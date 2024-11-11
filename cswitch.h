#ifndef _CSWITCH__INCLUDED
#define _CSWITCH__INCLUDED

/* "%Z% %M% %I% - %G% %U% " */

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
*  Routines in cswitch.c
*     cswitch                 - Main command switch
*
***************************************************************/

#ifdef DebuG
#define DEBUG_ENV_NAME "CEDEBUG"
#endif

int  cswitch(DISPLAY_DESCR  *dspl_descr,         /* in/out */
             DMC            *dm_list,            /* input  */
             DMC           **new_dmc,            /* output */
             XEvent         *event_union,        /* input  KeyPress, KeyRelease, ButtonPress, ButtonRelease */
             int             read_locked,        /* input  */
             int            *warp_needed,        /* output */
             DISPLAY_DESCR **wc_display_deleted, /* output */
             int            *stop_dm_list,       /* output */
             char           *arg0);              /* input  */


#endif

