#ifndef _SCROLL_INCLUDED
#define _SCROLL_INCLUDED

/* static char *scroll_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  This routine is involved in scrolling data on the main pad.
*  It actually will work for any pad, but the main one is the
*  only one it makes sense for.
*
*  Routines in scroll.c
*         scroll_redraw         - Shift a pad up one or more lines.
*
***************************************************************/

#include "buffer.h"

/***************************************************************
*  
*  Prototypes for the functions
*  
***************************************************************/


void  scroll_redraw(PAD_DESCR         *main_window_cur_descr,
                    DRAWABLE_DESCR    *lineno_subarea,
                    int                lines_to_scroll);


#endif

