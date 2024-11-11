#ifndef _TRANSLATE_KEYSYM_INCLUDED
#define _TRANSLATE_KEYSYM_INCLUDED    

/* static char *translate_keysym_h_sccsid = "%Z% %M% %I% - %G% %U% ";   */

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

/*****************************************************************************
*
*   translate_keysym     Translate an X11 keysym value into its text string
*
*   PARAMETERS:
*
*      key    -   KeySym  (an int)  (INPUT)
*
*   FUNCTIONS:
*
*   1.  switch on the key value and assign the approriate text string.
*
*   RETURNED VALUE:
*
*   name    -   pointer to char
*               This is the text string name of the keysym
*
*   NOTES:
*
*   1.   This program was generated from X11/Keysymdef.h
*
*   2.   The strings returned are static, they may be used indefinitly
*
*********************************************************************************/



char  *translate_keysym(KeySym  key);

KeySym  keyname_to_sym(char    *name);


#endif
