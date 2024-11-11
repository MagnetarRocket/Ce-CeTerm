#ifndef _CD__INCLUDED
#define _CD__INCLUDED

/* static char *cd_h_sccsid = "%Z% %M% %I% - %G% %U% "; */
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
*  Routines in cd.c
*     get_drawing_linestart   - Find the file lineno to display lines
*     cd_count_xed_lines      - Count x'ed lines in agroup.
*     lines_xed               - Calculate the number of x'ed file lines for a passed line
*     win_line_to_file_line   - translate a window line_no to a file line_no
*     file_line_to_win_line   - translate a file line_no to a window line_no
*     cd_add_remove           - Update cd data in memdata to add or delete chars
*     cdpad_add_remove        - Update pad cd data to add or delete chars
*     cdpad_curr_cdgc         - Get the current cdgc list for drawing
*     cd_join_line            - Update work cd data to accomadate a join
*     cd_flush                - Flush color data back to the memdata database
*
***************************************************************/

#include "buffer.h"    /* needed for TOKEN and PAD_DESCR */


#ifdef EXCLUDEP
int get_drawing_linestart(DATA_TOKEN      *token,             /* input  */
                          int              last_line_no,      /* input  */
                          int              count);            /* input  */

int cd_count_xed_lines(DATA_TOKEN      *token,             /* input  */
                       char            *cdgc,              /* input  */
                       int              cdgc_line,         /* input  */
                       int              line_no,           /* input  */
                       int             *first_line_no);    /* output */

int win_line_to_file_line(DATA_TOKEN      *token,             /* input  */
                          int              first_line,        /* input  */
                          int              win_line_no);      /* input  */

int file_line_to_win_line(DATA_TOKEN      *token,             /* input  */
                          int              first_line,        /* input  */
                          int              file_line_no);     /* input  */

int lines_xed(DATA_TOKEN      *token,             /* input  */
              int              file_line_no);     /* input  */

#endif

/* call just before or after the chars are added or removed */
void  cd_add_remove(DATA_TOKEN      *token,          /* input/output */
                    void           **curr_line_data, /* input/output */
                    int              lineno,         /* input */
                    int              col,            /* input */
                    int              chars);         /* input */

/* call just before or after the chars are added or removed */
void  cdpad_add_remove(PAD_DESCR      *pad,         /* input/output */
                       int             lineno,      /* input */
                       int             col,         /* input */
                       int             chars);      /* input */

FANCY_LINE  *cdpad_curr_cdgc(PAD_DESCR      *pad,           /* input  */
                             int             lineno,        /* input  */
                             int            *fancy_lineno); /* output */

/* call just before the lines are joined */
void  cd_join_line(DATA_TOKEN      *token,       /* input/output */
                   int              lineno,      /* input */
                   int              line_len);   /* input */

void  pulldown_overflow_color(PAD_DESCR      *pad,
                              int             to_line);


void  cd_flush(PAD_DESCR      *pad);          /* input/output */



#endif

