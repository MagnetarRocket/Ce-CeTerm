#ifndef _WINDOW_H_INCLUDED
#define _WINDOW_H_INCLUDED

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
*  Routines in window.c
*     dm_wg                   - Resize a window
*     dm_wm                   - move a window
*     dm_icon                 - iconify the window
*     dm_geo                  - Resize and move a window, new command
*     dm_wp                   - window pop
*     dm_rs                   - refresh screen
*     process_geometry        - Process a geometry string, set window values.
*     window_is_obscured      - Return flag showing if the window is at least partially obscured.
*     raised_window           - Sets flag to show window is raised
*     obscured_window         - Sets flag to show window is at least partially obscured.
*     main_subarea            - Calculates size of text region of the main window
*     window_crossing         - Mark the passage of the cursor into and out of the window structure
*     focus_change            - Mark the gaining and losing the input focus.
*     in_window               - Show which window the mouse cursor is in.
*     force_in_window         - Force the in window flag to true.
*     force_out_window        - Force the in window flag to False
*     get_root_location       - Get the root x and y coordinates for a window
*     reconfigure             - Generate a configurenotify
*     fake_focus              - Generate a fake focuse event
*
***************************************************************/

#include "buffer.h"
#include "dmc.h"


/***************************************************************
*  
*  Prototypes for Routines
*  
***************************************************************/

int   dm_geo(DMC             *dmc,
             DISPLAY_DESCR   *dspl_descr);

void  dm_wp(DMC              *dmc,
            PAD_DESCR        *main_window_cur_descr);

void  dm_icon(DMC              *dmc,
              Display          *display,
              Window            main_x_window);

void  dm_rs(Display          *display);


int  process_geometry(DISPLAY_DESCR    *dspl_descr,       /* input / output */
                      char             *geometry_string,  /* input */
                      int               default_size);    /* input */

int window_is_obscured(DISPLAY_DESCR       *dspl_descr);  /* returns True or False */

void raised_window(DISPLAY_DESCR       *dspl_descr);  /* sets flag */

void obscured_window(DISPLAY_DESCR       *dspl_descr); /* sets flag */

int  main_subarea(DISPLAY_DESCR    *dspl_descr);

void window_crossing(XEvent              *event,
                     DISPLAY_DESCR       *dspl_descr);

void focus_change(XEvent              *event,
                  DISPLAY_DESCR       *dspl_descr);

int in_window(DISPLAY_DESCR       *dspl_descr);

void force_in_window(DISPLAY_DESCR       *dspl_descr);

void force_out_window(DISPLAY_DESCR       *dspl_descr);

void  get_root_location(Display      *display,
                        Window        window,
                        int          *root_x,
                        int          *root_y);

void reconfigure(PAD_DESCR        *main_pad,
                 Display          *display);

void fake_focus(DISPLAY_DESCR    *dspl_descr, int focus_in);

#endif

