/* 
   Copyright (C) 2006 FreeIPMI Core Team
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.  
*/

#ifndef _IPMIMONITORING_H
#define _IPMIMONITORING_H

#include <freeipmi/freeipmi.h>

#include "ipmi_monitoring.h"

#include "tool-cmdline-common.h"
#include "tool-sdr-cache-common.h"
#include "hostmap.h"
#include "pstdout.h"

#define IPMIMONITORING_MAX_RECORD_IDS           256
#define IPMIMONITORING_MAX_GROUPS               256
#define IPMIMONITORING_MAX_GROUPS_STRING_LENGTH 256

enum ipmimonitoring_argp_option_keys
  { 
    REGENERATE_SDR_CACHE_KEY = 'r', /* legacy */
    QUIET_READINGS_KEY = 'q',
    SDR_INFO_KEY = 'i', 
    LIST_GROUPS_KEY = 'L', 
    GROUPS_KEY = 'g', 
    SENSORS_LIST_KEY = 's', 
    CACHE_DIR_KEY = 'c',            /* legacy */
  };

struct ipmimonitoring_arguments
{
  struct common_cmd_args common;
  struct sdr_cmd_args sdr;
  struct hostrange_cmd_args hostrange;
  int regenerate_sdr_cache_wanted;
  int quiet_readings_wanted;
  int sdr_info_wanted;
  int list_groups_wanted;
  int groups_list_wanted;
  char groups_list[IPMIMONITORING_MAX_GROUPS][IPMIMONITORING_MAX_GROUPS_STRING_LENGTH+1];
  unsigned int groups_list_length;
  int sensors_list_wanted;
  unsigned int sensors_list[IPMIMONITORING_MAX_RECORD_IDS];
  unsigned int sensors_list_length;

  struct ipmi_monitoring_ipmi_config conf;
  int ipmimonitoring_flags;
  unsigned int ipmimonitoring_groups[IPMIMONITORING_MAX_GROUPS];
  unsigned int ipmimonitoring_groups_length;
};

typedef struct ipmimonitoring_prog_data
{
  char *progname;
  struct ipmimonitoring_arguments *args;
  hostmap_t hmap;
} ipmimonitoring_prog_data_t;

typedef struct ipmimonitoring_state_data
{
  ipmimonitoring_prog_data_t *prog_data;
  ipmi_ctx_t ipmi_ctx;
  pstdout_state_t pstate;
  char *hostname;
  ipmi_sdr_cache_ctx_t ipmi_sdr_cache_ctx;
  ipmi_monitoring_ctx_t ctx;
} ipmimonitoring_state_data_t;

#endif
