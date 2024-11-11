#ifndef _DUMP_XEVENT_INCLUDED
#define _DUMP_XEVENT_INCLUDED    

/* static char *dumpxevent_h_sccsid = "%Z% %M% %I% - %G% %U% "; */


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
*  Prototype for routine dump_Xevent
*
*   This routine will format and dump an X window system
*   event structure to the file descriptor passed.
*
* PARAMETERS:
*
*   stream      -   Pointer to File (INPUT)
*                   This is the stream file to write to.
*                   It must already be open.  Values such
*                   as stderr or stdout work fine.  Or you
*                   may fopen a file and pass the returned
*                   pointer.
*
*   event       -   Pointer to XEvent (INPUT)
*                   The event to dump.  This is the event
*                   structure returned bu a call to XNextEvent,
*                   An event handler in motif, or the event
*                   structure from a calldata parm in a motif
*                   widget callback routine.
*
*   level       -   int (INPUT)
*                   This parm indicates how much stuff to dump
*                   Values:
*                   0  -  Do nothing, just return
*                   1  -  Dump just the name of the event
*                   2  -  Dump the major interesting fields
*                   3  -  Dump all the fields
*  
***************************************************************/


void dump_Xevent(FILE     *stream,
                 XEvent   *event,
                 int       level);

void translate_buttonmask(unsigned int     state,
                          char            *work);


#endif
