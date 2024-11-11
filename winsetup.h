#ifndef _WINSETUP_H_INCLUDED
#define _WINSETUP_H_INCLUDED

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
*  Routines in winsetup.c
*     win_setup               - Set up a new set of windows
*     bring_up_to_snuff       - Synchronize the cursor and pad buffers
*
***************************************************************/

#include "buffer.h"

/***************************************************************
*  
*  Prototypes for Routines
*  
***************************************************************/

int  win_setup(DISPLAY_DESCR   *dspl_descr,
               char            *edit_file,
               char            *resource_class,
               int              open_for_write,
               char            *cmdpath);

void bring_up_to_snuff(DISPLAY_DESCR   *dspl_descr);


#endif

