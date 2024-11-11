#ifndef _WC__INCLUDED
#define _WC__INCLUDED

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
*  Routines in wc.c
*     dm_wc              - Window close
*
***************************************************************/

#include "kd.h"
#include "drawable.h"  /* needed for def of DRAWABLE_DESCR and WINDOW_LINE */
#include "memdata.h"
#include "dmc.h"          /* needed for DMC        */
#include "buffer.h"     /* needed for BUFF_DESCR */

                                            
int  dm_wc(DMC             *dmc,                    /* input  */
           DISPLAY_DESCR   *dspl_descr,             /* input  / output */
           char            *edit_file,              /* input  */
           DISPLAY_DESCR  **display_deleted,        /* output */
           int              no_prompt,              /* input  */
           int              make_utmp);             /* input  */


#endif

