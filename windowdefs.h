#ifndef _WINDOWDEFS_INCLUDED
#define _WINDOWDEFS_INCLUDED

/*  "%Z% %M% %I% - %G% %U% " */

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
*  Include windowdefs.h
*
*  Definition and declaration of the window descriptions.
*
*  This include file allows global access to the
*  display description for the current window.
*
*  It is meant to be used by clients who need access to
*  everything, but not too often.
*
***************************************************************/

/**************************************************************

    Layout of the windows and sub areas within the windows.

+----------------------------------------------------------------+
| + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +|
| |   titlebar sub area                                          |
| + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +|
| + - - - - ++ - - - - - - - - - - - - - - - - - - - - - - - - -+|
| '         ''                                                  '|
| '         ''                                                  '|
| ' lineno  ''           text sub area                          '|
| '  sub    ''                                                  '|
| '  area   ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| '         ''                                                  '|
| + - - - - ++ - - - - - - - - - - - - - - - - - - - - - - - - -+|
+------------------------------+---------------------------------+
|   dm input window            |   dm output window              |
| + - - - - - - - - - - - - - +|+ - - - - - - - - - - - - - - - +|
| '                           '|'                               '|
| ' dm input sub area         '|'  dm output sub area           '|
| + - - - - - - - - - - - - - +|+ - - - - - - - - - - - - - - - +|
+------------------------------+---------------------------------+
+----------------------------------------------------------------+


There is one text sub area in each window.  This is the sub
area where text can be written.  The titlebar sub area and
lineno sub area are not writable.


***************************************************************/


#include "buffer.h"
#include "dmwin.h"
#include "memdata.h"  /* needed for MAX_LINE */
#ifdef PAD
#include "unixwin.h"  /* needed for MAX_UNIX_LINES */
#endif

/***************************************************************
*  
*  GLOBAL DATA IN Ce
*
*  We have attempted to avoid global data as much as possible.
*  However in recovery from errors (async error handlers) we
*  have to be able to get the edit file name and the current
*  display discription.  
*
*  There is some other error recovery global data in xerror.h
*  
***************************************************************/


#ifdef _MAIN_

DISPLAY_DESCR        *dspl_descr;
char                 *resource_class = "Ce";             /* Lookup class for X resources  eg:  Ce.background */
char                  edit_file[MAX_LINE*2];              /* extra long for cmdname + parms  (pad mode)       */
FILE                 *instream;

#else

extern DISPLAY_DESCR        *dspl_descr;
extern char                 *resource_class;
extern char                  edit_file[MAX_LINE*2];
extern FILE                 *instream;

#endif 

#endif
