#ifndef _TYPING_INCLUDED
#define _TYPING_INCLUDED

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
*  Routines in typing.c
*         dm_es               - es : enter source command)
*         dm_en               - en : enter command)
*         dm_ed               - ed : delete character under the cursor
*         dm_ee               - ee : delete character just before the cursor
*         insert_in_buff      - put chars in a buffer
*         delete_from_buff    - remove chars from a buffer
*         flush               - flush the work buffer back to the memory map file.
*         redraw_start_setup  - setup start line for partial window redraws
*
***************************************************************/

#include "buffer.h"     /* needed for BUFF_DESCR */
#include "dmc.h"


int    dm_es(BUFF_DESCR      *cursor_buff,
            DMC             *dmc);

int    dm_en(DMC            *dmc,           /* input  */
             DISPLAY_DESCR  *dspl_descr,    /* input / output */
             DMC           **new_dmc);      /* output */

int    dm_ed(BUFF_DESCR   *cursor_buff);

int    dm_ee(BUFF_DESCR   *cursor_buff,
             int           wrap_mode);

int  insert_in_buff(char  *buff,
                    char  *insert_str,
                    int    offset,
                    int    oinsert_mode,
                    int    max_len);

int  delete_from_buff(char  *buff,
                      int    len,
                      int    offset);

void flush(PAD_DESCR   *buffer);

int  redraw_start_setup(BUFF_DESCR      *cursor_buff);

#endif

