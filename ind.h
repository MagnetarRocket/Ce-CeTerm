#ifndef _IND_H_INCLUDED
#define _IND_H_INCLUDED

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
*  Routines in ind.c
*     dm_ind       -   Do indent processing
*     ind_init     -   Initialize an ind structure
*     filter_in    -   Returns a file pointer to input stream
*     filter_out   -   Returns a file pointer to output stream
*     ind_stat     -   Return stat struct on file
*     get_ind      -   Get the entire .Cetypes file and return a pointer for X (#includes done)
*     dm_lsf       -   Language Sensitive Filter
*     is_virtual   -   determine if the file is virtual or not
*
*     After return from these routines. A screen redraw may
*     be required, and or a warp cursor.
*
***************************************************************/
#include "buffer.h"
#include "dmc.h"
#include <sys/stat.h>

/*typedef enum { ABOVE, BELOW, CURRENT } pat_direction; */


typedef struct ind_data{
    FILE *fpi;
    FILE *fpo;
    int rule;
} IND_DATA;

/***************************************************************
*  
*  Command prototypes
*  
***************************************************************/

DMC *dm_ind(DISPLAY_DESCR *dspl_descr);
char   *get_ind(char *file);  /* load the .Cetypes file pointed to by *file */
FILE   *filter_in (void **data, void *hsearch_data, char *file, char* mode);
FILE   *filter_out(void **data, void *hsearch_data, char *file, char* mode);
int     ind_stat  (void **data, void *hsearch_data, char *file, struct stat *buf);
int     is_virtual(void **data, void *hsearch_data, char *file);
int     lsf_access(void **data, void *hsearch_data, char *file, int mode);

void    free_ind(void *data); 
void    lsf_close(FILE *fp, void *data); 

int   dm_lsf(DMC               *dmc,
             int                update_server,
             DISPLAY_DESCR     *dspl_descr);

int    is_lsf_key(char          *keykey);

#endif

