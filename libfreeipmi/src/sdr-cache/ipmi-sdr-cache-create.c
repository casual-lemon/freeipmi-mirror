/*****************************************************************************\
 *  $Id: ipmi-sdr-cache-create.c,v 1.25 2009-03-04 18:07:37 chu11 Exp $
 *****************************************************************************
 *  Copyright (C) 2007-2009 Lawrence Livermore National Security, LLC.
 *  Copyright (C) 2006-2007 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Albert Chu <chu11@llnl.gov>
 *  UCRL-CODE-222073
 *
 *  This file is part of Ipmimonitoring, an IPMI sensor monitoring
 *  library.  For details, see http://www.llnl.gov/linux/.
 *
 *  Ipmimonitoring is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  Ipmimonitoring is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Ipmimonitoring.  If not, see <http://www.gnu.org/licenses/>.
\*****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#if STDC_HEADERS
#include <string.h>
#endif /* STDC_HEADERS */
#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */
#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <assert.h>
#include <errno.h>

#include "freeipmi/sdr-cache/ipmi-sdr-cache.h"
#include "freeipmi/api/ipmi-sdr-repository-cmds-api.h"
#include "freeipmi/cmds/ipmi-sdr-repository-cmds.h"
#include "freeipmi/debug/ipmi-debug.h"
#include "freeipmi/record-format/ipmi-sdr-record-format.h"
#include "freeipmi/spec/ipmi-comp-code-spec.h"

#include "ipmi-sdr-cache-common.h"
#include "ipmi-sdr-cache-defs.h"
#include "ipmi-sdr-cache-trace.h"
#include "ipmi-sdr-cache-util.h"

#include "libcommon/ipmi-fiid-util.h"

#include "freeipmi-portability.h"
#include "debug-util.h"
#include "fd.h"

#define IPMI_SDR_CACHE_MAX_SDR_RECORD_LENGTH    1024
#define IPMI_SDR_CACHE_MAX_RESERVATION_ID_RETRY 4

#define IPMI_SDR_CACHE_BYTES_TO_READ_START      16
#define IPMI_SDR_CACHE_BYTES_TO_READ_DECREMENT  4

static int
_sdr_cache_header_write (ipmi_sdr_cache_ctx_t ctx,
                         ipmi_ctx_t ipmi_ctx,
                         int fd,
                         unsigned int *total_bytes_written,
                         uint8_t sdr_version,
                         uint16_t record_count,
                         uint32_t most_recent_addition_timestamp,
                         uint32_t most_recent_erase_timestamp)
{
  char sdr_cache_magic_buf[4];
  char sdr_cache_version_buf[4];
  char record_count_buf[2];
  char most_recent_addition_timestamp_buf[4];
  char most_recent_erase_timestamp_buf[4];
  ssize_t n;

  assert (ctx);
  assert (ctx->magic == IPMI_SDR_CACHE_CTX_MAGIC);
  assert (ipmi_ctx);
  assert (fd);
  assert (total_bytes_written);

  sdr_cache_magic_buf[0] = IPMI_SDR_CACHE_FILE_MAGIC_0;
  sdr_cache_magic_buf[1] = IPMI_SDR_CACHE_FILE_MAGIC_1;
  sdr_cache_magic_buf[2] = IPMI_SDR_CACHE_FILE_MAGIC_2;
  sdr_cache_magic_buf[3] = IPMI_SDR_CACHE_FILE_MAGIC_3;

  if ((n = fd_write_n (fd, sdr_cache_magic_buf, 4)) < 0)
    {
      SDR_CACHE_ERRNO_TO_SDR_CACHE_ERRNUM (ctx, errno);
      return (-1);
    }
  if (n != 4)
    {
      SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_SYSTEM_ERROR);
      return (-1);
    }
  (*total_bytes_written) += 4;

  sdr_cache_version_buf[0] = IPMI_SDR_CACHE_FILE_VERSION_0;
  sdr_cache_version_buf[1] = IPMI_SDR_CACHE_FILE_VERSION_1;
  sdr_cache_version_buf[2] = IPMI_SDR_CACHE_FILE_VERSION_2;
  sdr_cache_version_buf[3] = IPMI_SDR_CACHE_FILE_VERSION_3;

  if ((n = fd_write_n (fd, sdr_cache_version_buf, 4)) < 0)
    {
      SDR_CACHE_ERRNO_TO_SDR_CACHE_ERRNUM (ctx, errno);
      return (-1);
    }
  if (n != 4)
    {
      SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_SYSTEM_ERROR);
      return (-1);
    }
  (*total_bytes_written) += 4;

  if ((n = fd_write_n (fd, (char *)&sdr_version, 1)) < 0)
    {
      SDR_CACHE_ERRNO_TO_SDR_CACHE_ERRNUM (ctx, errno);
      return (-1);
    }
  if (n != 1)
    {
      SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_SYSTEM_ERROR);
      return (-1);
    }
  (*total_bytes_written) += 1;

  /* Store record count little-endian */
  record_count_buf[0] = (record_count & 0x00FF);
  record_count_buf[1] = (record_count & 0xFF00) >> 8;

  if ((n = fd_write_n (fd, record_count_buf, 2)) < 0)
    {
      SDR_CACHE_ERRNO_TO_SDR_CACHE_ERRNUM (ctx, errno);
      return (-1);
    }
  if (n != 2)
    {
      SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_SYSTEM_ERROR);
      return (-1);
    }
  (*total_bytes_written) += 2;

  /* Store most recent addition timestamp little-endian */
  most_recent_addition_timestamp_buf[0] = (most_recent_addition_timestamp & 0x000000FF);
  most_recent_addition_timestamp_buf[1] = (most_recent_addition_timestamp & 0x0000FF00) >> 8;
  most_recent_addition_timestamp_buf[2] = (most_recent_addition_timestamp & 0x00FF0000) >> 16;
  most_recent_addition_timestamp_buf[3] = (most_recent_addition_timestamp & 0xFF000000) >> 24;

  if ((n = fd_write_n (fd, most_recent_addition_timestamp_buf, 4)) < 0)
    {
      SDR_CACHE_ERRNO_TO_SDR_CACHE_ERRNUM (ctx, errno);
      return (-1);
    }
  if (n != 4)
    {
      SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_SYSTEM_ERROR);
      return (-1);
    }
  (*total_bytes_written) += 4;

  /* Store most recent erase timestamp little-endian */
  most_recent_erase_timestamp_buf[0] = (most_recent_erase_timestamp & 0x000000FF);
  most_recent_erase_timestamp_buf[1] = (most_recent_erase_timestamp & 0x0000FF00) >> 8;
  most_recent_erase_timestamp_buf[2] = (most_recent_erase_timestamp & 0x00FF0000) >> 16;
  most_recent_erase_timestamp_buf[3] = (most_recent_erase_timestamp & 0xFF000000) >> 24;

  if ((n = fd_write_n (fd, most_recent_erase_timestamp_buf, 4)) < 0)
    {
      SDR_CACHE_ERRNO_TO_SDR_CACHE_ERRNUM (ctx, errno);
      return (-1);
    }
  if (n != 4)
    {
      SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_SYSTEM_ERROR);
      return (-1);
    }
  (*total_bytes_written) += 4;

  return (0);
}

static int
_sdr_cache_reservation_id (ipmi_sdr_cache_ctx_t ctx,
                           ipmi_ctx_t ipmi_ctx,
                           uint16_t *reservation_id)
{
  fiid_obj_t obj_cmd_rs = NULL;
  uint64_t val;
  int rv = -1;

  assert (ctx);
  assert (ctx->magic == IPMI_SDR_CACHE_CTX_MAGIC);
  assert (ipmi_ctx);
  assert (reservation_id);

  if (!(obj_cmd_rs = fiid_obj_create (tmpl_cmd_reserve_sdr_repository_rs)))
    {
      SDR_CACHE_ERRNO_TO_SDR_CACHE_ERRNUM (ctx, errno);
      goto cleanup;
    }

  if (ipmi_cmd_reserve_sdr_repository (ipmi_ctx, obj_cmd_rs) < 0)
    {
      SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_IPMI_ERROR);
      goto cleanup;
    }

  *reservation_id = 0;
  if (sdr_cache_fiid_obj_get (ctx,
                              obj_cmd_rs,
                              "reservation_id",
                              &val) < 0)
    {
      ERR_TRACE (ipmi_sdr_cache_ctx_errormsg (ctx), ipmi_sdr_cache_ctx_errnum (ctx));
      goto cleanup;
    }
  *reservation_id = val;

  rv = 0;
 cleanup:
  FIID_OBJ_DESTROY (obj_cmd_rs);
  return (rv);
}

static int
_sdr_cache_get_record (ipmi_sdr_cache_ctx_t ctx,
                       ipmi_ctx_t ipmi_ctx,
                       uint16_t record_id,
                       uint8_t *record_buf,
                       unsigned int record_buf_len,
                       uint16_t *reservation_id,
                       uint16_t *next_record_id)
{
  fiid_obj_t obj_cmd_rs = NULL;
  fiid_obj_t obj_sdr_record_header = NULL;
  int32_t sdr_record_header_length = 0;
  int32_t record_length = 0;
  int rv = -1;
  uint32_t bytes_to_read = IPMI_SDR_CACHE_BYTES_TO_READ_START;
  uint32_t offset_into_record = 0;
  unsigned int reservation_id_retry_count = 0;
  uint64_t val;

  assert (ctx);
  assert (ctx->magic == IPMI_SDR_CACHE_CTX_MAGIC);
  assert (ipmi_ctx);
  assert (record_buf);
  assert (record_buf_len);
  assert (reservation_id);
  assert (next_record_id);

  if (!(obj_cmd_rs = fiid_obj_create (tmpl_cmd_get_sdr_rs)))
    {
      SDR_CACHE_ERRNO_TO_SDR_CACHE_ERRNUM (ctx, errno);
      goto cleanup;
    }

  if (!(obj_sdr_record_header = fiid_obj_create (tmpl_sdr_record_header)))
    {
      SDR_CACHE_ERRNO_TO_SDR_CACHE_ERRNUM (ctx, errno);
      goto cleanup;
    }

  if ((sdr_record_header_length = fiid_template_len_bytes (tmpl_sdr_record_header)) < 0)
    {
      SDR_CACHE_ERRNO_TO_SDR_CACHE_ERRNUM (ctx, errno);
      goto cleanup;
    }

  reservation_id_retry_count = 0;
  while (!record_length)
    {
      uint8_t record_header_buf[IPMI_SDR_CACHE_MAX_SDR_RECORD_LENGTH];
      int sdr_record_header_len;

      if (ipmi_cmd_get_sdr (ipmi_ctx,
                            *reservation_id,
                            record_id,
                            0,
                            sdr_record_header_length,
                            obj_cmd_rs) < 0)
        {
          if (ipmi_ctx_errnum (ipmi_ctx) != IPMI_ERR_BAD_COMPLETION_CODE)
            {
              SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_IPMI_ERROR);
              goto cleanup;
            }
          else
            {
              if (sdr_cache_fiid_obj_get (ctx,
                                          obj_cmd_rs,
                                          "comp_code",
                                          &val) < 0)
                {
                  ERR_TRACE (ipmi_sdr_cache_ctx_errormsg (ctx), ipmi_sdr_cache_ctx_errnum (ctx));
                  goto cleanup;
                }

              if (val == IPMI_COMP_CODE_RESERVATION_CANCELLED
                  && (reservation_id_retry_count < IPMI_SDR_CACHE_MAX_RESERVATION_ID_RETRY))
                {
                  if (_sdr_cache_reservation_id (ctx,
                                                 ipmi_ctx,
                                                 reservation_id) < 0)
                    goto cleanup;
                  reservation_id_retry_count++;
                  continue;
                }

              SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_IPMI_ERROR);
              goto cleanup;
            }
        }

      if ((sdr_record_header_len = fiid_obj_get_data (obj_cmd_rs,
                                                      "record_data",
                                                      record_header_buf,
                                                      IPMI_SDR_CACHE_MAX_SDR_RECORD_LENGTH)) < 0)
        {
          SDR_CACHE_FIID_OBJECT_ERROR_TO_SDR_CACHE_ERRNUM (ctx, obj_cmd_rs);
          goto cleanup;
        }

      if (sdr_record_header_len < sdr_record_header_length)
        {
          SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_IPMI_ERROR);
          goto cleanup;
        }

      if (fiid_obj_set_all (obj_sdr_record_header,
                            record_header_buf,
                            sdr_record_header_len) < 0)
        {
          SDR_CACHE_FIID_OBJECT_ERROR_TO_SDR_CACHE_ERRNUM (ctx, obj_sdr_record_header);
          goto cleanup;
        }

      if (sdr_cache_fiid_obj_get (ctx,
                                  obj_sdr_record_header,
                                  "record_length",
                                  &val) < 0)
        {
          ERR_TRACE (ipmi_sdr_cache_ctx_errormsg (ctx), ipmi_sdr_cache_ctx_errnum (ctx));
          goto cleanup;
        }

      record_length = val + sdr_record_header_length;
    }

  if (record_length > record_buf_len)
    {
      SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_INTERNAL_ERROR);
      goto cleanup;
    }

  if (sdr_cache_fiid_obj_get (ctx,
                              obj_cmd_rs,
                              "next_record_id",
                              &val) < 0)
    {
      ERR_TRACE (ipmi_sdr_cache_ctx_errormsg (ctx), ipmi_sdr_cache_ctx_errnum (ctx));
      goto cleanup;
    }
  *next_record_id = val;

  reservation_id_retry_count = 0;
  while (offset_into_record < record_length)
    {
      int32_t record_data_len;

      if ((record_length - offset_into_record) < bytes_to_read)
        bytes_to_read = record_length - offset_into_record;

      if (ipmi_cmd_get_sdr (ipmi_ctx,
                            *reservation_id,
                            record_id,
                            offset_into_record,
                            bytes_to_read,
                            obj_cmd_rs) < 0)
        {
          if (ipmi_ctx_errnum (ipmi_ctx) != IPMI_ERR_BAD_COMPLETION_CODE)
            {
              SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_IPMI_ERROR);
              goto cleanup;
            }
          else
            {
              if (sdr_cache_fiid_obj_get (ctx,
                                          obj_cmd_rs,
                                          "comp_code",
                                          &val) < 0)
                {
                  ERR_TRACE (ipmi_sdr_cache_ctx_errormsg (ctx), ipmi_sdr_cache_ctx_errnum (ctx));
                  goto cleanup;
                }

              if (val == IPMI_COMP_CODE_RESERVATION_CANCELLED
                  && (reservation_id_retry_count < IPMI_SDR_CACHE_MAX_RESERVATION_ID_RETRY))
                {
                  if (_sdr_cache_reservation_id (ctx,
                                                 ipmi_ctx,
                                                 reservation_id) < 0)
                    goto cleanup;
                  reservation_id_retry_count++;
                  continue;
                }
              else if  (val == IPMI_COMP_CODE_CANNOT_RETURN_REQUESTED_NUMBER_OF_BYTES
                        && bytes_to_read > sdr_record_header_length)
                {
                  bytes_to_read -= IPMI_SDR_CACHE_BYTES_TO_READ_DECREMENT;
                  if (bytes_to_read < sdr_record_header_length)
                    bytes_to_read = sdr_record_header_length;
                  continue;
                }

              SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_IPMI_ERROR);
              goto cleanup;
            }
        }

      if ((record_data_len = fiid_obj_get_data (obj_cmd_rs,
                                                "record_data",
                                                record_buf + offset_into_record,
                                                record_buf_len - offset_into_record)) < 0)
        {
          SDR_CACHE_FIID_OBJECT_ERROR_TO_SDR_CACHE_ERRNUM (ctx, obj_cmd_rs);
          goto cleanup;
        }

      offset_into_record += record_data_len;
    }

  rv = offset_into_record;
 cleanup:
  FIID_OBJ_DESTROY (obj_cmd_rs);
  FIID_OBJ_DESTROY (obj_sdr_record_header);
  return (rv);
}

static int
_sdr_cache_record_write (ipmi_sdr_cache_ctx_t ctx,
                         int fd,
                         unsigned int *total_bytes_written,
                         uint16_t *record_ids,
                         unsigned int *record_ids_count,
                         uint8_t *sensor_numbers,
                         unsigned int *sensor_numbers_count,
                         uint8_t *buf,
                         unsigned int buflen)
{
  ssize_t n;

  assert (ctx);
  assert (ctx->magic == IPMI_SDR_CACHE_CTX_MAGIC);
  assert (fd);
  assert (total_bytes_written);
  assert (!record_ids || (record_ids && record_ids_count));
  assert (!sensor_numbers || (sensor_numbers && sensor_numbers_count));
  assert (buf);
  assert (buflen);

  /* Record header bytes are 5 bytes */
  if (buflen < IPMI_SDR_CACHE_SDR_RECORD_HEADER_LENGTH)
    {
      SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_CACHE_CREATE_INVALID_RECORD_LENGTH);
      return (-1);
    }

  /* Record Length plus the header bytes should match buflen. */
  if ((((uint8_t)buf[IPMI_SDR_CACHE_SDR_RECORD_LENGTH_INDEX]) + IPMI_SDR_CACHE_SDR_RECORD_HEADER_LENGTH) != buflen)
    {
      SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_CACHE_CREATE_INVALID_RECORD_LENGTH);
      return (-1);
    }

  if (record_ids)
    {
      uint16_t record_id;
      int i;

      /* Record ID stored little endian */
      record_id = ((uint16_t)buf[IPMI_SDR_CACHE_SDR_RECORD_ID_INDEX_LS] & 0xFF);
      record_id |= ((uint16_t)buf[IPMI_SDR_CACHE_SDR_RECORD_ID_INDEX_MS] & 0xFF) << 8;

      for (i = 0; i < *record_ids_count; i++)
        {
          if (record_ids[i] == record_id)
            {
              SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_CACHE_CREATE_DUPLICATE_RECORD_ID);
              return (-1);
            }
        }
      record_ids[*record_ids_count] = record_id;
      (*record_ids_count)++;
    }

  /* Not all SDR entries may contain a sensor number, buf[3] indicates
   * SDR record type
   */
  if (sensor_numbers
      && (buf[IPMI_SDR_CACHE_SDR_RECORD_TYPE_INDEX] == IPMI_SDR_FORMAT_FULL_SENSOR_RECORD
          || buf[IPMI_SDR_CACHE_SDR_RECORD_TYPE_INDEX] == IPMI_SDR_FORMAT_COMPACT_SENSOR_RECORD
          || buf[IPMI_SDR_CACHE_SDR_RECORD_TYPE_INDEX] == IPMI_SDR_FORMAT_EVENT_ONLY_RECORD))
    {
      uint8_t sensor_number;
      int i;

      sensor_number = (uint8_t)buf[IPMI_SDR_CACHE_SDR_RECORD_SENSOR_NUMBER_INDEX];

      for (i = 0; i < *sensor_numbers_count; i++)
        {
          if (sensor_numbers[i] == sensor_number)
            {
              SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_CACHE_CREATE_DUPLICATE_SENSOR_NUMBER);
              return (-1);
            }
        }
      sensor_numbers[*sensor_numbers_count] = sensor_number;
      (*sensor_numbers_count)++;
    }

  if ((n = fd_write_n (fd, buf, buflen)) < 0)
    {
      SDR_CACHE_ERRNO_TO_SDR_CACHE_ERRNUM (ctx, errno);
      return (-1);
    }

  if (n != buflen)
    {
      /* Try to lseek back to our original spot */
      lseek (fd, SEEK_SET, *total_bytes_written);
      SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_SYSTEM_ERROR);
      return (-1);
    }
  (*total_bytes_written) += buflen;

  return (0);

}

int
ipmi_sdr_cache_create (ipmi_sdr_cache_ctx_t ctx,
                       ipmi_ctx_t ipmi_ctx,
                       char *filename,
                       int create_flags,
                       int validation_flags,
                       Sdr_Create_Callback create_callback,
                       void *create_callback_data)
{
  int open_flags;
  uint8_t sdr_version;
  uint16_t record_count, reservation_id, record_id, next_record_id;
  uint32_t most_recent_addition_timestamp, most_recent_erase_timestamp;
  unsigned int record_count_written = 0;
  unsigned int total_bytes_written = 0;
  uint16_t *record_ids = NULL;
  unsigned int record_ids_count = 0;
  uint8_t *sensor_numbers = NULL;
  unsigned int sensor_numbers_count = 0;

  int fd = -1;
  int rv = -1;

  if (!ctx || ctx->magic != IPMI_SDR_CACHE_CTX_MAGIC)
    {
      ERR_TRACE (ipmi_sdr_cache_ctx_errormsg (ctx), ipmi_sdr_cache_ctx_errnum (ctx));
      return (-1);
    }

  /* Version cannot be 0h according to the IPMI spec */
  if (!ipmi_ctx
      || !filename
      || (strlen (filename) > MAXPATHLEN)
      || (create_flags != IPMI_SDR_CACHE_CREATE_FLAGS_DEFAULT
          && create_flags != IPMI_SDR_CACHE_CREATE_FLAGS_OVERWRITE)
      || (validation_flags & ~(IPMI_SDR_CACHE_VALIDATION_FLAGS_DUPLICATE_RECORD_ID | IPMI_SDR_CACHE_VALIDATION_FLAGS_DUPLICATE_SENSOR_NUMBER)))
    {
      SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_PARAMETERS);
      return (-1);
    }

  if (ctx->operation != IPMI_SDR_CACHE_OPERATION_UNINITIALIZED)
    {
      if (ctx->operation == IPMI_SDR_CACHE_OPERATION_READ_CACHE)
        SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_CACHE_DELETE_CTX_SET_TO_READ);
      else
        SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_INTERNAL_ERROR);
      return (-1);
    }

  if (create_flags == IPMI_SDR_CACHE_CREATE_FLAGS_DEFAULT)
    open_flags = O_CREAT | O_EXCL | O_WRONLY;
  else
    open_flags = O_CREAT | O_TRUNC | O_WRONLY;

  if ((fd = open (filename, open_flags, 0644)) < 0)
    {
      if (create_flags == IPMI_SDR_CACHE_CREATE_FLAGS_DEFAULT
          && errno == EEXIST)
        ctx->errnum = IPMI_SDR_CACHE_ERR_CACHE_CREATE_CACHE_EXISTS;
      else if (errno == EPERM
               || errno == EACCES
               || errno == EISDIR
               || errno == EROFS)
        ctx->errnum = IPMI_SDR_CACHE_ERR_PERMISSION;
      else if (errno == ENAMETOOLONG
               || errno == ENOENT
               || errno == ELOOP)
        ctx->errnum = IPMI_SDR_CACHE_ERR_FILENAME_INVALID;
      else if (errno == ENOSPC
               || errno == EMFILE
               || errno == ENFILE)
        ctx->errnum = IPMI_SDR_CACHE_ERR_FILESYSTEM;
      else
        ctx->errnum = IPMI_SDR_CACHE_ERR_INTERNAL_ERROR;
      goto cleanup;
    }

  if (ipmi_sdr_cache_info (ctx,
                           ipmi_ctx,
                           &sdr_version,
                           &record_count,
                           &most_recent_addition_timestamp,
                           &most_recent_erase_timestamp) < 0)
    goto cleanup;

  if (!record_count)
    {
      SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_CACHE_CREATE_INVALID_RECORD_COUNT);
      goto cleanup;
    }

  if (_sdr_cache_header_write (ctx,
                               ipmi_ctx,
                               fd,
                               &total_bytes_written,
                               sdr_version,
                               record_count,
                               most_recent_addition_timestamp,
                               most_recent_erase_timestamp) < 0)
    goto cleanup;

  /* Version cannot be 0h according to the IPMI spec, but we accept it regardless */
  ctx->sdr_version = sdr_version;
  ctx->record_count = record_count;
  ctx->most_recent_addition_timestamp = most_recent_addition_timestamp;
  ctx->most_recent_erase_timestamp = most_recent_erase_timestamp;

  if (validation_flags & IPMI_SDR_CACHE_VALIDATION_FLAGS_DUPLICATE_RECORD_ID)
    {
      if (!(record_ids = (uint16_t *)malloc (ctx->record_count * sizeof(uint16_t))))
        {
          SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_OUT_OF_MEMORY);
          goto cleanup;
        }
      record_ids_count = 0;
    }

  if (validation_flags & IPMI_SDR_CACHE_VALIDATION_FLAGS_DUPLICATE_SENSOR_NUMBER)
    {
      if (!(sensor_numbers = (uint8_t *)malloc (ctx->record_count * sizeof(uint8_t))))
        {
          SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_OUT_OF_MEMORY);
          goto cleanup;
        }
      sensor_numbers_count = 0;
    }

  if (_sdr_cache_reservation_id (ctx,
                                 ipmi_ctx,
                                 &reservation_id) < 0)
    goto cleanup;

  next_record_id = IPMI_SDR_RECORD_ID_FIRST;
  while (next_record_id != IPMI_SDR_RECORD_ID_LAST)
    {
      uint8_t record_buf[IPMI_SDR_CACHE_MAX_SDR_RECORD_LENGTH];
      int record_len;

      if (record_count_written >= ctx->record_count)
        {
          SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_CACHE_CREATE_INVALID_RECORD_COUNT);
          goto cleanup;
        }

      record_id = next_record_id;
      if ((record_len = _sdr_cache_get_record (ctx,
                                               ipmi_ctx,
                                               record_id,
                                               record_buf,
                                               IPMI_SDR_CACHE_MAX_SDR_RECORD_LENGTH,
                                               &reservation_id,
                                               &next_record_id)) < 0)
        goto cleanup;

      if (record_len)
        {
          if (ctx->flags & IPMI_SDR_CACHE_FLAGS_DEBUG_DUMP)
            {
              const char *record_str;

              if ((record_str = ipmi_sdr_cache_record_type_str (ctx,
                                                                record_buf,
                                                                record_len)))
                {
                  char hdrbuf[IPMI_SDR_CACHE_DEBUG_BUFLEN];

                  debug_hdr_str (DEBUG_UTIL_TYPE_NONE,
                                 DEBUG_UTIL_DIRECTION_NONE,
                                 record_str,
                                 hdrbuf,
                                 IPMI_SDR_CACHE_DEBUG_BUFLEN);

                  ipmi_dump_sdr_record (STDERR_FILENO,
                                        ctx->debug_prefix,
                                        hdrbuf,
                                        NULL,
                                        record_buf,
                                        record_len);
                }
            }

          if (_sdr_cache_record_write (ctx,
                                       fd,
                                       &total_bytes_written,
                                       record_ids,
                                       &record_ids_count,
                                       sensor_numbers,
                                       &sensor_numbers_count,
                                       record_buf,
                                       record_len) < 0)
            goto cleanup;

          record_count_written++;

          if (create_callback)
            (*create_callback)(ctx->sdr_version,
                               ctx->record_count,
                               ctx->most_recent_addition_timestamp,
                               ctx->most_recent_erase_timestamp,
                               record_id,
                               create_callback_data);
        }
    }

  if (record_count_written != ctx->record_count)
    {
      SDR_CACHE_SET_ERRNUM (ctx, IPMI_SDR_CACHE_ERR_CACHE_CREATE_INVALID_RECORD_COUNT);
      goto cleanup;
    }

  rv = 0;
  ctx->errnum = IPMI_SDR_CACHE_ERR_SUCCESS;
 cleanup:
  if (fd >= 0)
    {
      /* If the cache create never completed, try to remove the file */
      if (rv < 0)
        unlink (filename);
      close (fd);
    }
  if (record_ids)
    free (record_ids);
  if (sensor_numbers)
    free (sensor_numbers);
  ipmi_sdr_cache_init_ctx (ctx);
  return (rv);
}


