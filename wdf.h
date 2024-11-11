#ifndef _WDF_H_INCLUDED
#define _WDF_H_INCLUDED

/* static char *wdf_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in wdf.c
*     dm_wdf                 -   Add or replace an entry in the WDF or WDC list.
*     wdf_get_geometry       -   Get the next window geometry.
*     wdc_get_colors         -   Get the next window color set
*     wdf_save_last          -   Save the current window geometry and color set on exit
*
***************************************************************/

/***************************************************************
*  
*  Includes needed for types used in prototypes.
*  
***************************************************************/

#include "buffer.h"
#include "dmc.h"

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/X.h"         /* /usr/include/X11/X.h */
#include "X11/Xatom.h"     /* /usr/include/X11/Xatom.h */
#endif
#else
#include <X11/X.h>         /* /usr/include/X11/X.h */
#include <X11/Xatom.h>     /* /usr/include/X11/Xatom.h */
#endif


/***************************************************************
*  
*  Prototypes for routines in wdf.c
*  
***************************************************************/

void  dm_wdf(DMC         *dmc,
             Display     *display,
             PROPERTIES  *properties);

char  *wdf_get_geometry(Display     *display,
                        PROPERTIES  *properties);

void   wdc_get_colors(Display     *display,       /* input  */
                      char       **bgc,           /* output */
                      char       **fgc,           /* output */
                      PROPERTIES  *properties);   /* input  */


void   wdf_save_last(DISPLAY_DESCR   *dspl_descr);

#endif

