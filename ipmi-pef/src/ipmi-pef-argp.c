/* 
   $Id: ipmi-pef-argp.c,v 1.12 2007-05-17 05:06:15 chu11 Exp $ 
   
   ipmi-pef-argp.c - Platform Event Filtering utility.
   
   Copyright (C) 2005 FreeIPMI Core Team
   
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
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
*/

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#if STDC_HEADERS
#include <string.h>
#endif /* STDC_HEADERS */
#include <argp.h>
#include <ctype.h>
#include <errno.h>

#include "argp-common.h"
#include "ipmi-pef.h"
#include "ipmi-pef-argp.h"

#include "freeipmi-portability.h"

static error_t parse_opt (int key, char *arg, struct argp_state *state);

const char *argp_program_version = 
"IPMI PEF [ipmi-pef-" PACKAGE_VERSION "]\n"
"Copyright (C) 2003-2005 FreeIPMI Core Team\n"
"This program is free software; you may redistribute it under the terms of\n"
"the GNU General Public License.  This program has absolutely no warranty.";

const char *argp_program_bug_address = "<freeipmi-devel@gnu.org>";

static char doc[] = "IPMI PEF - PEF configuration utility.";

static char args_doc[] = "";

static struct argp_option options[] = 
  {
    ARGP_COMMON_OPTIONS_INBAND,
    ARGP_COMMON_OPTIONS_OUTOFBAND,
    ARGP_COMMON_OPTIONS_AUTHTYPE,
    ARGP_COMMON_OPTIONS_PRIVLEVEL_ADMIN,
#ifndef NDEBUG
    ARGP_COMMON_OPTIONS_DEBUG,
#endif /* NDEBUG */
    {"info",       INFO_KEY,       0, 0, 
     "Show general information about PEF.", 18},
    {"checkout",   CHECKOUT_KEY,   0, 0,
     "Action is to GET the PEF event filter tables", 19},
    {"commit",     COMMIT_KEY,     0, 0,
     "Action is to UPDATE the PEF event filter tables", 20},
    /* XXX: and diff */
    {"community-string", COMMUNITY_STRING_KEY, 0, 0,
     "Checkout Community String", 21},
    {"lan-alert-destinations", LAN_ALERT_DESTINATIONS_KEY, 0, 0, 
     "Checkout of LAN Alert Destinations.", 22},
    {"alert-policy-table", ALERT_POLICY_TABLE_KEY, 0, 0, 
     "Checkout of Alert Policy Table.", 23},
    {"event-filter-table", EVENT_FILTER_TABLE_KEY, 0, 0,
     "Checkout Event Filter Table", 24},
    {"verbose", VERBOSE_KEY, 0, 0,  
     "Produce verbose output", 25},
    {"filename", FILENAME_KEY, "FILENAME", 0,
     "use FILENAME in checkout or commit", 26},
    { 0 }
  };

static struct argp argp = { options, parse_opt, args_doc, doc };

static error_t 
parse_opt (int key, char *arg, struct argp_state *state)
{
  struct ipmi_pef_arguments *cmd_args = state->input;
  
  switch (key)
    {
    case INFO_KEY:
      cmd_args->info_wanted = 1;
      break;
    case CHECKOUT_KEY:
      cmd_args->checkout_wanted = 1;
      break;
    case COMMIT_KEY:
      cmd_args->commit_wanted = 1;
      break;
    case EVENT_FILTER_TABLE_KEY:
      cmd_args->event_filter_table_wanted = 1;
      break;
    case ALERT_POLICY_TABLE_KEY:
      cmd_args->alert_policy_table_wanted = 1;
      break;
    case LAN_ALERT_DESTINATIONS_KEY:
      cmd_args->lan_alert_destinations_wanted = 1;
      break;
    case COMMUNITY_STRING_KEY:
      cmd_args->community_string_wanted = 1;
      break;
    case VERBOSE_KEY:
      cmd_args->verbose_wanted = 1;
      break;
    case FILENAME_KEY:
      if (cmd_args->filename) /* If specified more than once */
        free (cmd_args->filename);
      if (!(cmd_args->filename = strdup (arg)))
        {
          perror("strdup");
          exit(1);
        }
      break;
    case ARGP_KEY_ARG:
      /* Too many arguments. */
      argp_usage (state);
      break;
    case ARGP_KEY_END:
      break;
    default:
      return common_parse_opt (key, arg, state, 
			       &(cmd_args->common));
    }
  
  return 0;
}

void 
ipmi_pef_argp_parse (int argc, char **argv, struct ipmi_pef_arguments *cmd_args)
{
  init_common_cmd_args (&(cmd_args->common));
  cmd_args->info_wanted = 0;
  cmd_args->checkout_wanted = 0;
  cmd_args->commit_wanted = 0;
  cmd_args->community_string_wanted = 0;
  cmd_args->lan_alert_destinations_wanted = 0;
  cmd_args->alert_policy_table_wanted = 0;
  cmd_args->event_filter_table_wanted = 0;
  cmd_args->verbose_wanted = 0;
  cmd_args->filename = NULL;

  /* ADMIN is minimum for ipmi-pef b/c its needed for many of the
   * ipmi cmds used
   */
  cmd_args->common.privilege_level = IPMI_PRIVILEGE_LEVEL_ADMIN;
  argp_parse (&argp, argc, argv, ARGP_IN_ORDER, NULL, cmd_args);
}

