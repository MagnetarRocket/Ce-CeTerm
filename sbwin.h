#ifndef _SBWIN_INCLUDED
#define _SBWIN_INCLUDED

/* static char *sbwin_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in sbwin.c
*         get_sb_windows        - Create the horizontal and vertical scroll bars
*         resize_sb_windows     - Reposition scroll bar windows after main window resize
*         draw_vert_sb          - Draw the vertical scroll bar
*         draw_horiz_sb         - Draw the horizontal scroll bar
*         vert_button_press     - Process a button press in the vertical sb window
*         horiz_button_press    - Process a button press in the horizontal sb window
*         move_vert_slider      - Move vertical slider because of positioning
*         move_horiz_slider     - Move horizontal slider because of positioning
*         build_in_arrow_press  - Build a button press which appears to be in one of the 4 arrows
*         build_in_arrow_rlse   - Build a button release to turn off scrolling from build_in_arrow_press
*
***************************************************************/

#include "buffer.h"

/**************************************************************
*
*  Constants visible to the outside world
*
***************************************************************/

#define VERT_SCROLL_BAR_WIDTH    14
#define HORIZ_SCROLL_BAR_HEIGHT  13

/***************************************************************
*  
*  Prototypes for the functions
*  
***************************************************************/

void  get_sb_windows(PAD_DESCR        *main_pad,            /* input  */
                     Display          *display,             /* input  */
                     SCROLLBAR_DESCR  *sb_win,              /* input / output */
                     char             *sb_colors,           /* input */
                     char             *sb_horiz_base_width);/* input */

void  resize_sb_windows(SCROLLBAR_DESCR  *sb_win,           /* input / output  */
                        DRAWABLE_DESCR   *main_window,      /* input   */
                        Display          *display);         /* input   */

int  draw_vert_sb(SCROLLBAR_DESCR  *sb_win,
                  PAD_DESCR        *main_pad,
                  Display          *display);

int  draw_horiz_sb(SCROLLBAR_DESCR  *sb_win,
                   PAD_DESCR        *main_pad,
                   Display          *display);

int  vert_button_press(XEvent           *event,
                       DISPLAY_DESCR    *dspl_descr);

int  horiz_button_press(XEvent           *event,
                        DISPLAY_DESCR    *dspl_descr);

int  move_vert_slider(XEvent           *event,
                      DISPLAY_DESCR    *dspl_descr);

int  move_horiz_slider(XEvent           *event,
                       DISPLAY_DESCR    *dspl_descr);

int  build_in_arrow_press(XEvent           *event,
                          DISPLAY_DESCR    *dspl_descr);

int  build_in_arrow_rlse(DISPLAY_DESCR    *dspl_descr);

#endif

