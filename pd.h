#ifndef _PD_INCLUDED
#define _PD_INCLUDED

/* static char *pd_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in pd.c
*         get_menubar_window    - Create the menubar X window
*         resize_menu_bar       - Resize the menu bar
*         dm_mi                 - Menu Item command (builds menus)
*         dm_pd                 - Pull Down command (initiates a pulldown)
*         dm_pdm                - Toggle the menu bar on and off
*         pd_remove             - Get rid of one or more pulldowns.
*         pd_window_to_idx      - Find pulldown entry given the X window id.
*         change_pd_font        - Change the font in the pulldown to match the main window.
*         draw_menu_bar         - draw the top menu area
*         draw_pulldown         - draw a pulldown window
*         pd_adjust_motion      - Adjust motion events when pulldown is active
*         pd_button             - Process a button press or release when pulldowns are active
*         pd_button_motion      - Process button motion events when pulldowns are active
*         check_menu_items      - Make sure certain menu items exist
*         is_menu_item_key      - Test is a keykey is a menu item keykey
*         dm_tmb                - To menu bar
*
***************************************************************/

#include "buffer.h"

/***************************************************************
*  
*  Prototypes for the functions
*  
***************************************************************/

void  get_menubar_window(Display          *display,
                         PD_DATA          *pd_data,
                         char             *mousecursor,
                         DRAWABLE_DESCR   *main_window,
                         Window            main_x_window);


void  resize_menu_bar(Display          *display,              /* input   */
                      Window            menubar_x_window,     /* input   */
                      DRAWABLE_DESCR   *main_window,          /* input   */
                      DRAWABLE_DESCR   *title_subarea,        /* input   */
                      DRAWABLE_DESCR   *menubar_win);         /* input / output  */

int   dm_mi(DMC               *dmc,
            int                update_server,
            DISPLAY_DESCR     *dspl_descr);

void  dm_pdm(DMC               *dmc,
             DISPLAY_DESCR     *dspl_descr);

void  dm_pd(DMC               *dmc,
            XEvent            *event,
            DISPLAY_DESCR     *dspl_descr);

void pd_remove(DISPLAY_DESCR     *dspl_descr,
               int                new_nest_level);

int  pd_window_to_idx(PD_DATA     *pd_data,
                      Window       w);

void change_pd_font(Display          *display,
                    PD_DATA          *pd_data,
                    Font              fid);

void  draw_menu_bar(Display          *display,            /* input   */
#ifdef WIN32
                    Drawable          x_pixmap,
#else
                    Pixmap            x_pixmap,           /* input   */
#endif
                    void             *hsearch_data,       /* input   */
                    PD_DATA          *pd_data);

void  draw_pulldown(Display          *display,            /* input   */
                    void             *hsearch_data,       /* input   */
                    int               nest_level,         /* input   */
                    PD_DATA          *pd_data);           /* input   */

void  pd_adjust_motion(XEvent            *event,
                       PAD_DESCR         *main_pad,
                       PD_DATA           *pd_data);

DMC *pd_button(XEvent           *event,            /* input  */
               DISPLAY_DESCR    *dspl_descr);      /* input / output */

void  pd_button_motion(XEvent             *event,            /* input  */
                       DISPLAY_DESCR      *dspl_descr);      /* input / output */

void check_menu_items(DISPLAY_DESCR     *dspl_descr);

int   is_menu_item_key(char          *keykey);

int   dm_tmb(Display          *display,            /* input   */
             PD_DATA          *pd_data);           /* input   */

#endif

