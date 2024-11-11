#ifndef _APISTATS_H_INCLUDED
#define _APISTATS_H_INCLUDED

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
*   apistats.h
*
*     This include file contains the indexes and lengths
*     into the data portion of the ClientMessageEvent for
*     the data passed between ce and the api.  The message
*     is always the value of the atom "CE_CCLIST" and
*     the format is always 8 since we handle big endian/
*     little endian conversions ourselves.
*
*     To use from the api, create a ClientMessageEvent
*     with any value for serial number.  Put the api window
*     id in the winbdow parameter so Ce knows where to send
*     the data back to.  Intern the CE_CCLIST atom name and
*     put the atom id in the message_type field and set the
*     format to 8.  Send this to the main window of ce.
*
*     Wait for the message to be sent back.  It will contain
*     the data in the data portion of the screen.  You can
*     access the data elements using the defines in this
*     include file.
*
*     For example, to access the total number of lines in
*     the main window access  event_union.TOTAL_LINES_INFILE
*
*     You need to include
*     #include <netinet/in.h>
*     and use ntohl and ntohs on the members.
*
*     Format of the message
*
*           l[0]              l[1]         s[4]   s[5]       l[3]               l[4]
*     +----------------+----------------+----------------+----------------+----------------+
*     |                |                |       |        |                |                | 
*     |  total_lines   |  current_line  |  col  | write  |                |                | 
*     |                |                |       |        |                |                | 
*     +----------------+----------------+----------------+----------------+----------------+
*     
*
***************************************************************/

#define   TOTAL_LINES_INFILE   xclient.data.l[0]
#define   CURRENT_LINENO       xclient.data.l[1]
#define   CURRENT_COLNO        xclient.data.s[4]
#define   PAD_WRITABLE         xclient.data.s[5]

#endif

