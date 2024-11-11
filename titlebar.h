#ifndef _TITLEBAR_INCLUDED
#define _TITLEBAR_INCLUDED

/* static char *titlebar_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in titlebar.c
*         get_titlebar_window - Initialize the title bar subwindow drawable description
*         write_titlebar      - Write the titlebar
*         resize_titlebar     - Handle a resize of the main window in the titlebar
*
*
***************************************************************/

#include "xutil.h"

/***************************************************************
*  
*                                flags 0 1 2 3 4
*  title                               W I H S A  
*  title                                 R M   Lineno
*
*  Edit windows can have I, R, or blank in flag 1 and lineno
*  right justified in pos 4
*  
***************************************************************/



void  get_titlebar_window(Display          *display,              /* input   */
                          Window            main_x_window,        /* input   */
                          DRAWABLE_DESCR   *main_drawable,        /* input   */
                          DRAWABLE_DESCR   *menubar_drawable,     /* input   */
                          char             *titlebarfont,         /* input   */
                          DRAWABLE_DESCR   *titlebar_subwin);     /* output  */

void  write_titlebar(Display          *display,            /* input   */
#ifdef WIN32
                     Drawable          x_pixmap,           /* input   */
#else
                     Pixmap            x_pixmap,           /* input   */
#endif
                     DRAWABLE_DESCR   *titlebar_subwin,    /* input  */
                     char             *text,               /* input  */
                     char             *node_name,          /* input  */
                     int               line_no,            /* input  */
                     int               col_no,             /* input  */
                     int               insert_mode,        /* input  */
                     int               ro_mode,            /* input  */
                     int               hold_mode,          /* input  */
                     int               scroll_mode,        /* input  */
                     int               autohold_mode,      /* input  */
                     int               vt100_mode,         /* input  */
                     int               window_wrap,        /* input  */
#define CMD_RECORD_BOX
#ifdef CMD_RECORD_BOX
                     int               recording_mode,     /* input  */
#endif
                     int               modified);          /* input  */


void  resize_titlebar(DRAWABLE_DESCR   *main_drawable,        /* input   */
                      DRAWABLE_DESCR   *menubar_drawable,     /* input   */
                      DRAWABLE_DESCR   *titlebar_subwin);     /* input / output  */


#endif

