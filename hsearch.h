#ifndef _SEARCH_INCLUDED
#define _SEARCH_INCLUDED

/* static char *hsearch_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in hsearch.c
*         hcreate         - Create the hash table for Ce key defs
*         hdestroy        - Destroy a Ce hash table created by hcreate (contains Ce dependent code)
*         hsearch         - Search or insert an item into the hash table.
*         hlinkhead       - Get start of linked list of entries
*
***************************************************************/

struct ENTRY ;

typedef struct entry {
	char          *key;
	char          *data;
   struct ENTRY  *link; /* not modified by user, maintained by hsearch */
} ENTRY;

typedef enum {
	FIND,	/* Find, if present. */
	ENTER	/* Find; enter if not present. */
} ACTION;

#ifdef  __cplusplus
    extern "C" {
#endif

void  *hcreate(unsigned int   size);
void hdestroy(void *table_p);
ENTRY *hsearch(void *table_p, ENTRY item, ACTION action);
ENTRY *hlinkhead(void *table_p);

#ifdef  __cplusplus
    }
#endif

#endif /* _SEARCH_INCLUDED */
