#ifndef _DMSEARCH_H_INCLUDED
#define _DMSEARCH_H_INCLUDED

#include "memdata.h"

/* static char *dmsearch_h_sccsid = "%Z% %M% %I% - %G% %U% ";  */

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

/***************************************************************
*  
*  This is a static copy of the last search pattern. It is
*  only modified in by search().  It is globally visable 
*  because the main routine needs it for message generation
*  when there is a find in progress and it is a repeat find.
*  
***************************************************************/

#define EBUF_MAX 256

#ifndef NBRA
#define NBRA 9
#endif

/***************************************************************
*  
*  Special values returned in the found line parm of 
*  dm_find.  If the value is non-negative,
*  the pattern was found.  Otherwise one of the following
*  two defined values is returned.
*  
***************************************************************/

#define  DM_FIND_NOT_FOUND     -1
#define  DM_FIND_IN_PROGRESS   -2
#define  DM_FIND_ERROR         -3

#define REPEAT_SUBS (char *)1 /* a reateat substitute is requested with the previous pattern */

#define UNIX_RE  1
#define AEGIS_RE 0

void  search(   DATA_TOKEN *token,           /* input  */
                int        from_line,        /* input  */
                int        from_col,         /* input  */
                int        to_line,          /* input  */
                int       *to_col,           /* input  */
                int        reverse_find,     /* input  */
                char      *pattern,          /* input  */
                char      *substitute,       /* input  */
                int        case_s,           /* input  */
                int       *found_line,       /* output */
                int       *found_col,        /* output */
                int       *newlines,         /* input/output */
                int       rectangular,       /* input */
                int       rec_start_col,     /* input */
                int       rec_end_col,       /* input */
                int      *subs_made,         /* input */
                int       so,                /* input */
                char      escape_char,       /* input */
                void    **sdata);            /* input/output  */
                                          
void process_inserted_newlines(DATA_TOKEN *token,       /* input */
                                       int from_line,   /* input */
                                       int to_line,     /* input */
                                       int *row,        /* input/output */
                                       int *column);    /* input/output */

int a_to_re(char *t, int replace); 

void compile_error(int rc);

void lower_case(char *string);

#endif

