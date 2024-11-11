#ifndef _XSMP_H_INCLUDED
#define _XSMP_H_INCLUDED

/* static char *xsmp_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in xsmp.c
*     xsmp_init              -   Initial connection to xsmp
*     xsmp_fdset             -   Update an fdset for the xsmp connections
*     xsmp_fdisset           -   Check the fdset and process
*     xsmp_close             -   Close the xsmp connection and free the private data
*     xsmp_register          -   Register the with the X server that XSMP is being used
*     xsmp_display_check     -   Check saved dspl_descr when one is being deleted.
*
***************************************************************/

/***************************************************************
*  
*  Includes needed for types used in prototypes.
*  
***************************************************************/

#ifndef FD_ZERO
#include <sys/select.h>
#endif

#include "buffer.h"



/***************************************************************
*  
*  Prototypes for routines in xsmp.c
*  
***************************************************************/

int  xsmp_init(DISPLAY_DESCR     *dspl_descr,  /* returns true if xsmp is active */
               char              *edit_file,   /* creates xsmp_private_data_p */
               char              *arg0);    

int  xsmp_fdset(void        *xsmp_private_data_p,  /* input */
                fd_set      *ice_fdset);           /* output */

void   xsmp_fdisset(void        *xsmp_private_data_p,  /* input */
                    fd_set      *ice_fdset);           /* input */


void   xsmp_close(void       **xsmp_private_data_p);  /* input/output */

void   xsmp_register(DISPLAY_DESCR       *dspl_descr);

void   xsmp_display_check(DISPLAY_DESCR     *dspl_descr_going_away,
                          DISPLAY_DESCR     *Replacement_dspl_descr);

#endif

