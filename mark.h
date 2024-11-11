#ifndef _MARK_H_INCLUDED
#define _MARK_H_INCLUDED

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
*  Routines in mark.c
*     dm_tdm              -   Move cursor to the dm window. Leave mark
*     dm_tdmo             -   Move to the DM output window
*     dm_tmw              -   Move to the main window
*     dm_dr               -   define region command
*     dm_gm               -   go to mark command
*     mark_point_setup    -   Get a position from marks.
*     dm_comma            -   Comma processing for 1,$s etc.
*     dm_num              -   move to a specified line number
*     range_setup         -   Turn MARK_REGION's into top and bottom rows and cols.
*     toggle_switch       -   Flip Flop a DM_ON, DM_OFF, DM_TOGGLE flag
*     check_and_add_lines -   Add padding lines if neccesary because of past eof or form feeds
*
***************************************************************/

#include "dmc.h"
#include "buffer.h"

/***************************************************************
*  
*  Routines
*  
***************************************************************/

int   dm_tdm(MARK_REGION  *tdm_mark,
             BUFF_DESCR   *cursor_buff,
             PAD_DESCR    *dminput_cur_descr,
             PAD_DESCR    *main_pad);


int  dm_tdmo(BUFF_DESCR   *cursor_buff,
             PAD_DESCR    *dmoutput_cur_descr,
             PAD_DESCR    *main_pad);

int  dm_tmw(BUFF_DESCR   *cursor_buff,
            PAD_DESCR    *main_window_cur_descr);

void  dm_dr(MARK_REGION       *mark1,
            BUFF_DESCR        *cursor_buff,
            int                echo_mode);


int   dm_gm(MARK_REGION       *mark1,
            BUFF_DESCR        *cursor_buff);

void  mark_point_setup(MARK_REGION       *tdm_mark,           /* input  */
                       BUFF_DESCR        *cursor_buff,        /* input  */
                       int               *from_line,          /* input  */
                       int               *from_col,           /* output */
                       PAD_DESCR        **window_buff);       /* output */

void  dm_comma(MARK_REGION       *tdm_mark,               /* output   */
               BUFF_DESCR        *cursor_buff,            /* input / output  */
               MARK_REGION       *comma_mark);            /* input    */

int  range_setup(MARK_REGION       *mark1,              /* input   */
                 BUFF_DESCR        *cursor_buff,        /* input   */
                 int               *top_line,           /* output  */
                 int               *top_col,            /* output  */
                 int               *bottom_line,        /* output  */
                 int               *bottom_col,         /* output  */
                 PAD_DESCR        **buffer);            /* output  */ 

int  toggle_switch(int      mode,      /* input - DM_TOGGLE, DM_ON, or DM_OFF */
                   int      cur_val,   /* input */
                   int     *changed);  /* output */

int check_and_add_lines(BUFF_DESCR   *cursor_buff);

#endif

