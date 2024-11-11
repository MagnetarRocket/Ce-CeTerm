#ifndef _MVCURSOR_H_INCLUDED
#define _MVCURSOR_H_INCLUDED

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
*  Routines in mvcursor.c
*     dm_pv       -   Move pad vertically
*     dm_ph       -   Move Pad Horizontally
*     dm_pt       -   Scroll to top of pad
*     dm_tt       -   Put cursor on first line in window
*     dm_pb       -   Scroll to bottom of the pad
*     pad_to_bottom - Move a pad to the bottom
*     pb_ff_scan  -   Scan last lines in pad for a form feed.
*     ff_scan     -   Scan forward for a form feed
*     dm_dollar   -   Scroll to bottom of the pad, position at last line col 0
*     dm_bt       -   Put cursor on bottom line in window
*     dm_tr       -   Put cursor on right most char on line
*     dm_tl       -   Put cursor on left most char on line
*     dm_au       -   Move cursor up one line
*     dm_al       -   Move cursor up one line
*     dm_ad       -   Move cursor down one line
*     dm_ar       -   Move cursor right one character
*     dm_al       -   Move cursor left one character
*     dm_num      -   Move cursor to the requested file line number
*     dm_corner   -   Position the window upper left corner over the file
*     dm_twb      -   Move cursor to the requested window border
*     dm_sic       -  Set Insert Cursor for Mouse Off mode
*     dm_tn       -   tn (to next) and ti (to next input) window commands
*     dm_wh       -   Take window into and out of hold mode
*     dm_position -   Position the cursor to a given file row and col
*
*     After return from these routines. A screen redraw may
*     be required, and or a warp cursor.
*
***************************************************************/

#include   "dmc.h"          /* needed for DMC        */
#include   "buffer.h"     /* needed for DATA_TOKEN */


/***************************************************************
*  
*  Command prototypes
*  
***************************************************************/


int   dm_pv(BUFF_DESCR   *cursor_buff,
            DMC          *dmc,
            int          *warp_needed);

int   dm_ph(BUFF_DESCR       *cursor_buff,
            DMC              *dmc);

int   dm_pt(BUFF_DESCR       *cursor_buff);

int   dm_tt(BUFF_DESCR       *cursor_buff);

int   dm_pb(BUFF_DESCR       *cursor_buff);

int   pad_to_bottom(PAD_DESCR        *pad);

int   pb_ff_scan(PAD_DESCR        *pad,
                 int               new_row);

void ff_scan(PAD_DESCR        *pad,
             int               first_row);

int   dm_tb(BUFF_DESCR       *cursor_buff);

int   dm_tr(BUFF_DESCR       *cursor_buff);

int   dm_tl(BUFF_DESCR       *cursor_buff);

void  dm_au(BUFF_DESCR       *cursor_buff,
            PAD_DESCR        *main_window_cur_descr,
            PAD_DESCR        *dmoutput_cur_descr,
            PAD_DESCR        *unixcmd_cur_descr);

void  dm_ad(BUFF_DESCR       *cursor_buff,
            PAD_DESCR        *dmoutput_cur_descr,
            PAD_DESCR        *dminput_cur_descr,
            PAD_DESCR        *unixcmd_cur_descr);

int   dm_ar(BUFF_DESCR       *cursor_buff,
            PAD_DESCR        *dmoutput_cur_descr,
            int               wrap_mode);

int   dm_al(BUFF_DESCR       *cursor_buff,
            PAD_DESCR        *dminput_cur_descr,
            int               wrap_mode);

int dm_num(DMC               *dmc,
           BUFF_DESCR        *cursor_buff,
           int                find_border);

int dm_corner(DMC               *dmc,
              BUFF_DESCR        *cursor_buff);

void  dm_twb(BUFF_DESCR       *cursor_buff,
             DMC              *dmc);

int dm_sic(XEvent       *event,
           BUFF_DESCR   *cursor_buff,
           PAD_DESCR    *main_window_cur_descr,
           int           wrap_mode);

int  dm_tn(DMC          *dmc,
           BUFF_DESCR   *cursor_buff,
           PAD_DESCR    *dminput_cur_descr,
           PAD_DESCR    *unixcmd_cur_descr);

int  dm_wh(DMC            *dmc,
           DISPLAY_DESCR  *dspl_descr);

int dm_position(BUFF_DESCR   *cursor_buff,
                PAD_DESCR    *target_win_buff,
                int           target_line,
                int           target_col);


#endif

