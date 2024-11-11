#ifndef _XERRORPOS_H_INCLUDED
#define _XERRORPOS_H_INCLUDED

/*  "%Z% %M% %I% - %G% %U%  */

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
*   This macro is used to assist in debugging x errors in ce.
*   The macro is placed before each X call which goes to the
*   server.  It saves __line__ and __file__ to a set of global
*   variables which are used by xerror.
*   The macro only runs when DEBUG9 is on, this is the X sync
*   macro.
*
*   DEBUG9(XERRORPOS)
*
***************************************************************/

#ifdef _MAIN_

int             Xcall_lineno;
char           *Xcall_file;

#else
extern int      Xcall_lineno;
extern char    *Xcall_file;
#endif

#define XERRORPOS Xcall_lineno = __LINE__; Xcall_file = __FILE__;

#endif

