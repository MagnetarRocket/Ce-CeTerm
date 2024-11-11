#ifndef _XC_H_INCLUDED
#define _XC_H_INCLUDED

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
*  Routines in xc.c
*     dm_xc               -   Do a copy range of text
*     dm_xp               -   Do a paste
*     dm_undo             -   Undo processing and redo (un-undo) processing
*     dm_case             -   Case processing on a range of text (upper, lower, swap)
*     case_line           -   change the case (upper, lower, swap) of a line of text
*     dm_xl               -   Copy liternal command to a paste buffer (xl)
*
***************************************************************/

#include "dmc.h"
#include "buffer.h"

/***************************************************************
*  
*  Prototypes for Routines
*  
***************************************************************/

 
int   dm_xc(DMC               *dmc,                /* input   */
            MARK_REGION       *mark1,              /* input   */
            BUFF_DESCR        *cursor_buff,        /* input   */
            Display           *display,
            Window             main_x_window,
            int                echo_mode,          /* input   */
            Time               timestamp,          /* input - X lib type */
            char               escape_char);       /* input   */

int   dm_xp(DMC               *dmc,                /* input   */
            BUFF_DESCR        *cursor_buff,        /* input   */
            Display           *display,
            Window             main_x_window,
            char               escape_char);       /* input   */

int   dm_undo(DMC               *dmc,                /* input   */
              BUFF_DESCR        *cursor_buff);       /* input / output  */

int   dm_case(DMC               *dmc,                /* input   */
              MARK_REGION       *mark1,              /* input   */
              BUFF_DESCR        *cursor_buff,        /* input   */
              int                echo_mode);         /* input   */

int case_line(char    func,
              char   *work,
              int     from_col,
              int     to_col);

void  dm_xl(DMC               *dmc,                /* input   */
            Display           *display,
            Window             main_x_window,
            Time               timestamp,          /* input - X lib type */
            char               escape_char);       /* input   */

void  dm_xa(DMC               *dmc,                /* input   */
            Display           *display,
            Window             main_x_window,
            Time               timestamp,          /* input - X lib type */
            char               escape_char);       /* input   */

#endif

