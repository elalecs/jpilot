/* log.h
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 1999-2001 by Judd Montgomery
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __LOG_H__
#define __LOG_H__

#include <stdarg.h>

#define LOG_DEBUG  1    /*debugging info for programers, and bug reports */
#define LOG_INFO   2    /*info, and misc messages */
#define LOG_WARN   4    /*worse messages */
#define LOG_FATAL  8    /*even worse messages */
#define LOG_STDOUT 256  /*messages always go to stdout */
#define LOG_FILE   512  /*messages always go to the log file */
#define LOG_GUI    1024 /*messages always go to the gui window */

/* pipe communication commands */
//undo where?
#define PIPE_PRINT           100
#define PIPE_USERID          101
#define PIPE_USERNAME        102
#define PIPE_PASSWORD        103
#define PIPE_WAITING_ON_USER 104
#define PIPE_FINISHED        105
#define PIPE_SYNC_CONTINUE   200
#define PIPE_SYNC_CANCEL     201

extern int glob_log_file_mask;
extern int glob_log_stdout_mask;
extern int glob_log_gui_mask;

int jpilot_logf(int log_level, char *format, ...);
int jpilot_vlogf (int level, char *format, va_list val);
int write_to_parent(int command, char *format, ...);

#endif
