#ifndef _LINENO_INCLUDED
#define _LINENO_INCLUDED

/* static char *lineno_h_sccsid = "%Z% %M% %I% - %G% %U% ";  */

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
*  Routines in lineno.c
*         get_lineno_window   - Initialize the title bar subwindow drawable description
*         write_lineno        - Write the lineno subarea
*         dm_lineno           - Toggle the display line number state
*
*
***************************************************************/

#include "xutil.h"
#include "dmc.h"



void  setup_lineno_window(DRAWABLE_DESCR   *main_drawable,        /* input   */
                          DRAWABLE_DESCR   *title_subarea,        /* input  */
                          DRAWABLE_DESCR   *dminput_window,       /* input  */
                          DRAWABLE_DESCR   *unixcmd_window,       /* input   */
                          DRAWABLE_DESCR   *lineno_subarea);      /* output  */

void  write_lineno(Display           *display,            /* input  */
                   Drawable           drawable,           /* input  */
                   DRAWABLE_DESCR    *lineno_subarea,     /* input  */
                   int                first_line,         /* input  */
                   int                overlay,            /* input  */
                   int                skip_lines,         /* input  */
                   DATA_TOKEN        *token,              /* input  */
                   int                formfeed_in_window, /* input  */
                   WINDOW_LINE       *win_lines);         /* input  */

int   dm_lineno(DMC              *dmc,                /* input  */
                PAD_DESCR        *main_pad,           /* input  */
                Display          *display,            /* input  */
                int              *show_lineno);       /* output */


#endif

