#ifndef _TAB__INCLUDED
#define _TAB__INCLUDED

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
*  Routines in tab.c
*
*     dm_ts                 -   Set Tab stops
*     dm_th                 -   Tab Horizontal (right and left)
*     untab                 -   expand tab characters for drawing
*     tab_cursor_adjust     -   adjust cursor position withing a window
*     tab_pos               -   Calculate tab position in a line
*     set_window_col_from_file_col - set window col and x value from file col
*     dm_hex                -   Turn hex mode display on an off.
*     dm_untab              -   Change tabs to blanks according to the current tab stops.
*     window_col_to_x_pixel -   Calculate 'x' pixel position for a given window column
*
***************************************************************/

#include "dmc.h"          /* needed for DMC           */
#include "buffer.h"       /* needed for BUFF_DESCR   */

#define TAB      '\t'
#define FORMFEED '\f'

                                            
/***************************************************************
*  
*  Prototypes
*  
***************************************************************/

int  dm_ts(DMC             *dmc);

void dm_th(int              cmd_id,
           BUFF_DESCR      *cursor_buff);

#define TAB_TAB_FOUND             1
#define TAB_FORMFEED_FOUND  (1 << 1)


int untab(char          *from,
          char          *to,
          unsigned int   limit_len,
          int            hex_mode);


/**************************************************************
*  
*  TAB_WINDOW_COL    Translates a window position from X,Y to window col adjusted for tabs
*                    Used when tracking the mouse cursor to determine where to put the
*                    text cursor when moving around tabs.
*  TAB_LINE_OFFSET   Used to bring the cursor buff up to snuff.  It takes the window
*                    column and returns the col on the line of text.
*  TAB_EXPAND_POS    Translates a line col to a line col with tabs expanded
*                    Same functionality as TAB_EXPAND in tab_pos.
*                    Converts a window column to a file column.
*  TAB_PADDING       Translates a window position from X,Y to the number of chars to pad.
*                    Tells the number of characters to pad (not currently used).
*  
***************************************************************/

#define TAB_WINDOW_COL  0
#define TAB_LINE_OFFSET 1
#define TAB_EXPAND_POS  2
#define TAB_PADDING     3

int  tab_cursor_adjust(char           *line,            /* input  */
                       int             offset,          /* input  */
                       int             first_char,      /* input  */
                       int             mode,            /* input  TAB_WINDOW_COL, TAB_LINE_OFFSET, TAB_EXPAND_POS, TAB_PADDING */
                       int             hex_mode);       /* input  */

/**************************************************************
*  
*  TAB_EXPAND        returns position of pos offset in line with tabs expanded (line contains unexpanded tabs)
*  TAB_CONTRACT      returns position of pos offset in line with tabs unexpanded (line contains unexpanded tabs, this is the inverse of EXPAND_TABS)
*  TAB_PADDING       returns number of spaces to skip for this character, always 1 unless we are on a tab.
*  
***************************************************************/

#define TAB_EXPAND      0
#define TAB_CONTRACT    1

int  tab_pos(char           *line,            /* input  */
             int             offset,          /* input  */
             int             mode,            /* input  TAB_EXPAND, TAB_CONTRACT, TAB_PADDING */
             int             hex_mode);


int set_window_col_from_file_col(BUFF_DESCR      *cursor_buff); /* input / output */

int dm_hex(DMC             *dmc);

int   dm_untab(MARK_REGION       *mark1,              /* input   */
               BUFF_DESCR        *cursor_buff,        /* input   */
               int                echo_mode);         /* input   */

int window_col_to_x_pixel(char         *buff_ptr,
                          int           win_col,
                          int           first_char,
                          int           subwindow_x,
                          XFontStruct  *font,
                          int           hex_mode);


#endif

