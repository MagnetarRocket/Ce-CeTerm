#ifndef _RECORD_H_INCLUDED
#define _RECORD_H_INCLUDED

/* static char *record_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in record.c
*     dm_rec              - Turn on/off event recording to a file or pastebuff
*     rec_cmds            - Record the commmand lists.
*     rec_position        - Record the cursor position after dm cmds
*
***************************************************************/

#include "dmc.h"
#include "buffer.h"

/***************************************************************
*
*  Prototypes
*
***************************************************************/

void   dm_rec(DMC            *dmc,
              DISPLAY_DESCR  *dspl_descr);

void   rec_cmds(DMC              *dm_list,
                DISPLAY_DESCR    *dspl_descr);

void   rec_position(DISPLAY_DESCR   *dspl_descr);

#endif

