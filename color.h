#ifndef _COLOR__INCLUDED
#define _COLOR__INCLUDED

/* color_h_sccsid = "%Z% %M% %I% - %G% %U% " */
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

/***************************************************************
*
*  rotuines in module color.c
*
*         dm_inv              - Invert the foreground and background window colors
*         dm_bgc              - Change the background or forground color of the windows.
*         dm_fl               - Load a new font
*         set_initial_colors  - Turn the initial background and foreground colors to pixels
*
***************************************************************/

#include "buffer.h"   /* needed for PROPERTIES */
#include "dmc.h"      /* needed for DMC */
#include "xutil.h"    /* needed for DRAWABLE_DESCR */


void dm_inv(DISPLAY_DESCR  *dspl_descr);

void dm_bgc(DMC            *dmc,
            DISPLAY_DESCR  *dspl_descr);

void dm_fl(DMC            *dmc,
           DISPLAY_DESCR  *dspl_descr);

void set_initial_colors(DISPLAY_DESCR  *dspl_descr,  /* input  */
                        char           *back_color,  /* input  */
                        unsigned long  *back_pixel,  /* output */
                        char           *fore_color,  /* input  */
                        unsigned long  *fore_pixel); /* output */

void change_title(DISPLAY_DESCR     *dspl_descr,
                  char              *window_name);

#endif

