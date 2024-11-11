#ifndef _REDRAW_INCLUDED
#define _REDRAW_INCLUDED

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
*  Routines in redraw.c
*     process_redraw          - handle redrawing the screen after the action of DM commands
*     highlight_setup         - Perform the setup for text highlighting.
*     redo_cursor             - Generate a warp to reposition the cursor after a window size change.
*     fake_cursor_motion      - Fudge up a motion event
*     off_screen_warp_check   - Adjust warps to areas on the physical screen.
*
***************************************************************/

#include "buffer.h"

/**************************************************************
*  Defines for warp needed flag.  Normally just True (1)
*  and False (0) are needed.  In the special case where
*  we want to force a real warp pointer where one would not
*  normally occur, we use the special flag.
***************************************************************/
#define REDRAW_NO_WARP         False
#define REDRAW_WARP            True
#define REDRAW_FORCE_REAL_WARP 2

void process_redraw(DISPLAY_DESCR  *dspl_descr,
                    int             redraw_needed,
                    int             warp_needed);

void  highlight_setup(MARK_REGION    *mark1,
                      int             echo_mode);

void redo_cursor(DISPLAY_DESCR  *dspl_descr,
                 int             force_fake_warp);

void fake_cursor_motion(DISPLAY_DESCR   *dspl_descr);

int off_screen_warp_check(Display            *display,
                          DRAWABLE_DESCR     *main_window,
                          DRAWABLE_DESCR     *target_window,
                          int                *x,
                          int                *y);


#endif

