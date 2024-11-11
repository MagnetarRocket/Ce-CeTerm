#ifndef _ALIAS_INCLUDED
#define _ALIAS_INCLUDED

/* static char *alias_h_sccsid = "%Z% %M% %I% - %G% %U% ";  */
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
*  Routines in alias.c
*         lookup_alias         -   Check the hash table for an alias name.
*         alias_dollar_vars    - Process the dollar sign variables in an alias.
*         dm_alias             - Do the alias command
*         is_alias_key         - Test is a keykey is an alias keykey
*
*
***************************************************************/

#include "buffer.h"
#include "dmc.h"


/***************************************************************
*  
*  Max length of an alias name
*  
***************************************************************/

#define  MAX_ALIAS_NAME  8

/***************************************************************
*  
*  Function prototypes
*  
***************************************************************/

char     *lookup_alias(char   *name,
                       void   *hash_table);

int   alias_dollar_vars(char     *inbuff,
                        int       argc,
                        char     *argv[],
                        char      escape_char,
                        int       max_out,
                        char     *outbuff);

int   dm_alias(DMC               *dmc,
               int                update_server,
               DISPLAY_DESCR     *dspl_descr);

int   is_alias_key(char          *keykey);

#endif

