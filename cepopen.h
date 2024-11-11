#ifndef _CEPOPEN__INCLUDED
#define _CEPOPEN__INCLUDED

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
*  Routines in cepopen.c
*     ce_popen           - Like _popen, but works under NT 4
*     ce_pclose          - Matches ce_popen
*
* These routines take the same arguments as popen and pclose on
* UNIX and _popen and _pclose on NT, but unlike the NT versions
* These work.  They can be macro'ed to _popen and _pclose when
* NT is fixed.
*
***************************************************************/

#ifdef  __cplusplus
extern "C" {
#endif


FILE *ce_popen(char *cmd, char *mode);

int ce_pclose(FILE *stream);


#ifdef  __cplusplus
}
#endif


#endif

