#ifndef _INIT_INCLUDED
#define _INIT_INCLUDED

/* static char *init_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in init.c
*         get_font_data                   - load the font and font data
*         setup_lineno_window             - set up font data for a window from the font parm
*         setup_window_manager_properties - set up the window manager hints data
*         setup_winlines                  - allocate a new winlines structure
*         change_icon                     - Change the icon name and or pixmap.
*         build_cmd_args                  - turn the option values into a argv/argc
*         save_restart_state              - Save the state so the window mangaer can restart us
*         get_execution_path              - Get the execution path of this program
*
***************************************************************/

#include "buffer.h"

char *get_font_data(char              *font_name,      /* input  */
                    DISPLAY_DESCR     *dspl_descr);    /* input/output  */

void setup_window_manager_properties(DISPLAY_DESCR     *dspl_descr,
                                     char              *window_name,
                                     char              *icon_name,
                                     char              *cmdname,
                                     int                start_iconic);

void setup_winlines(PAD_DESCR   *main_window);

void change_icon(Display           *display,
                 Window             main_x_window,
                 char              *icon_name,
                 int                pad_mode);

void build_cmd_args(DISPLAY_DESCR     *dspl_descr,
                    char              *edit_file,
                    char              *arg0,
                    int               *argc,
                    char             **argv);

void save_restart_state(DISPLAY_DESCR     *dspl_descr,
                        char              *edit_file,
                        char              *arg0);

char *get_execution_path(DISPLAY_DESCR     *dspl_descr,
                         char              *arg0);


#endif

