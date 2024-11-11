#ifndef _NEWDSPL_H_INCLUDED
#define _NEWDSPL_H_INCLUDED

/* static char newdspl_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in newdspl.c
*     open_new_dspl          -   Get the new display, options, and key definitions
*
***************************************************************/

/***************************************************************
*  
*  Includes needed for types used in prototypes.
*  
***************************************************************/

#include <X11/Xlib.h>         /* /usr/include/X11/X.h */


/***************************************************************
*  
*  Prototypes for routines in newdspl.c
*  
***************************************************************/

int   open_new_dspl(int             *argc,                   /* input/output  */
                    char            *argv[],                 /* input/output  */
                    Display         *curr_display,           /* input  */
                    Display        **new_display_ptr,        /* output */
                    char          ***new_option_values,      /* output */
                    char           **new_option_from_parm,   /* output */
                    PROPERTIES     **new_properties,         /* output */
                    void           **new_hash_table,         /* output */
                    char            *cmd_name);              /* input  */

#define FREE_NEW_OPTIONS(array, max) {int i;\
                                     for (i = 0; i<max; i++)\
                                         if ((array)[i]) free((char *)((array)[i]));\
                                     free((char *)(array));}

#endif

