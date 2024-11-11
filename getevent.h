#ifndef _GETEVENT__INCLUDED
#define _GETEVENT__INCLUDED

/* "%Z% %M% %I% - %G% %U% " */

/***************************************************************
*  
*  ARPUS/Ce text editor and terminal emulator modeled after the
*  Apollo(r) Domain systems.
*  Copyright 1988 - 2002 Enabling Technologies Group
*  Copyright 2003 - 2005 Robert Styma Consulting
*  
*  This program is free software; you can redistribute it and/or
*  modify it under the terms of the GNU General Public License
*  as published by the Free Software Foundation; either version 2
*  of the License, or (at your option) any later version.
*  
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*  
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*  
*  Original Authors:  Robert Styma and Kevin Plyler
*  Email:  styma@swlink.net
*  
***************************************************************/

/**************************************************************
*
*  Routines in getevent.c
*     get_next_X_event        - Get the next event, control background work.
*     expect_warp             - Mark warp data to show we are waiting for a warp
*     duplicate_warp          - Check if a potential warp is a duplicate
*     expecting_warp          - Check if a we are currently waiting on a warp
*     find_valid_event        - CheckMaskEvent compare routine to find an event given a mask
*     find_event              - CheckMaskEvent compare routine to find a single event type
*     change_background_work  - Turn on or off background tasks.
*     get_background_work     - Retrieve the current state of background task variables
*     autovt_switch           - Perform switching for AUTOVT mode
*     finish_keydefs          - Force the Keydefs to finish before continuing.
*     set_event_masks         - Set the input event masks, for the windows.
*     find_padmode_dspl       - Find the pad mode display description
*     set_edit_file_changed  - Find the pad mode display description
*     load_enough_data        - Make sure we are at eof or at least enough data is read in.
*     kill_unixcmd_window     - kKll of the unixcmd window when it is no longer needed.
*     window_to_buff_descr    - Convert X window id to the correct pad description
*     fake_keystroke          - Generate a fake keystroke event to trigger other things.
*     store_cmds              - Store commands to be triggered with a fake keystroke
*     set_global_dspl         - Set the current global display to a particular display.
*
***************************************************************/

#include "mvcursor.h"

void get_next_X_event(XEvent      *event_union,    /* output */
                      int         *shell_fd,       /* input  */
                      int         *cmd_fd);        /* input  */

void expect_warp(int             x,
                 int             y,
                 Window          window,
                 DISPLAY_DESCR  *dspl_descr);


int duplicate_warp(DISPLAY_DESCR    *dspl_descr, 
                   Window            window,
                   int               x,
                   int               y);

int expecting_warp(DISPLAY_DESCR    *dspl_descr);

/**************************************************************
*  Defined constants used by change_background_work
*  and get_background_work.  The exception is BACKGROUND_FIND_OR_SUB
*  which is the or of BACKGROUND_FIND and BACKGROUND_SUB.  This
*  field may only be used in calls to get_background_work and
*  is included for efficiency.
***************************************************************/
#define   BACKGROUND_FIND        1
#define   BACKGROUND_SUB         2
#define   BACKGROUND_KEYDEFS     3
#define   MAIN_WINDOW_EOF        4
#define   BACKGROUND_SCROLL      5
#define   BACKGROUND_READ_AHEAD  6
#define   BACKGROUND_FIND_OR_SUB 7

void change_background_work(DISPLAY_DESCR    *passed_dspl_descr,
                            int               type,
                            int               value);

int  get_background_work(int      type);

void finish_keydefs(void);

int autovt_switch(void);

Bool find_valid_event(Display        *display,
                      XEvent         *event,
                      char           *mask_ptr);


Bool find_event(Display        *display,
                XEvent         *event,
                char           *type);  /* really pointer to int */

void  set_event_masks(DISPLAY_DESCR    *dspl_descr,
                      int               force);

DISPLAY_DESCR *find_padmode_dspl(DISPLAY_DESCR   *passed_dspl_descr);

void set_edit_file_changed(DISPLAY_DESCR   *passed_dspl_descr, int true_or_false);

int load_enough_data(int          total_lines_needed);  /* input  */

PAD_DESCR *window_to_buff_descr(DISPLAY_DESCR    *dspl_descr, 
                                Window            window);

void kill_unixcmd_window(int real_kill);

void fake_keystroke(DISPLAY_DESCR    *passed_dspl_descr);

void store_cmds(DISPLAY_DESCR    *dspl_descr,
                char             *cmds,
                int               prefix_cmds);

void  set_global_dspl(DISPLAY_DESCR    *passed_dspl_descr);

#endif

