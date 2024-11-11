#ifndef _SERVERDEF_INCLUDED
#define _SERVERDEF_INCLUDED

/* static char *serverdef_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in serverdef.c
*         save_server_keydefs  - save the key definitions in the X server property
*         load_server_keydefs  - load the keydefs from the server
*         change_keydef        - Update the keydef property to show a keydef change.
*         get_changed_keydef   - Handle an update to the key defintions detected by a keydef property change.
*         dm_delsd             - Delete server copy of the key definitions
*         dm_keys              - Display all the definitions in a separate window
*
***************************************************************/

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/Xatom.h"       /* /usr/include/X11/Xatom.h */
#endif
#else
#include <X11/Xatom.h>       /* /usr/include/X11/Xatom.h */
#endif

#include "dmc.h"
#include "hsearch.h"

/***************************************************************
*  
*  Function prototypes
*  
***************************************************************/

void      save_server_keydefs(Display     *display,
                              PROPERTIES  *properties,
                              char        *cekeys_atom_name,
                              void        *hash_table);

int  load_server_keydefs(Display     *display,
                         PROPERTIES  *properties,
                         char        *cekeys_atom_name,
                         void        *hash_table);

void change_keydef(ENTRY             *ent,
                   Display           *display,
                   char              *cekeys_atom_name,
                   PROPERTIES        *properties);

void  get_changed_keydef(XEvent      *event,
                         PROPERTIES  *properties,
                         void        *hash_table);

void dm_delsd(Display      *display,
              char         *cekeys_atom_name,
              PROPERTIES   *properties);

DMC *dm_keys(void             *hash_table,
             char              escape_char);



#endif

