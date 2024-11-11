#ifndef _GC__INCLUDED
#define _GC__INCLUDED

/* static char *gc_h_sccsid = "%Z% %M% %I% - %G% %U% "; */
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
*  Routines in gc.c
*     create_gcs         - Create the three graphics contents for a window
*     dup_gc             - Duplicate a Graphics Content with changes
*     dump_gc            - Dump Graphics Content info.
*     pix_value          - Get the pixel value under an X Y coordinate
*
***************************************************************/

#include "xutil.h"    /* needed for DRAWABLE_DESCR */


void   create_gcs(Display         *display,
                  Window           main_x_window,
                  DRAWABLE_DESCR  *window,
                  XGCValues       *gcvalues_parm);

GC   dup_gc(Display       *display,
            Drawable       drawable,
            GC             old_gc,
            unsigned long  new_valuemask,
            XGCValues     *new_gcvalues);


void dump_gc(FILE         *stream,                     /* input  */
             Display      *display,                    /* input  */
             GC            gc,                         /* input  */
             char         *header);                    /* input  */

unsigned long pix_value(Display         *display,
                        Window           main_x_window,
                        int              x,
                        int              y);


#endif

