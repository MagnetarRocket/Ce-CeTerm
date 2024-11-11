#ifndef _DMFIND_H_INCLUDED
#define _DMFIND_H_INCLUDED

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
*  Routines in dmfind.c
*     dm_find           -  Do a find or reverse find.
*     dm_s              -  Do a substitute or substitute once (s or so)
*     dm_continue_find  -  Continue a find started by dm_start_find. (via dm_find).
*     dm_continue_sub   -  Continue a find started by dm_start_find. (via dm_s)
*     dm_re             -  Convert a regular expression from aegis to unix and display
*     find_border_adjust - Adjust first line to set border around the find.
*
***************************************************************/

#include "memdata.h"  /* needed for DATA_TOKEN */
#include "dmc.h"      /* need for DMC */
#include "buffer.h"   /* needed for BUFF_DESCR */

/***************************************************************
*  
*  Routines
*  
***************************************************************/



int   dm_find(DMC               *dm_list,               /* input   */
              FIND_DATA         *find_data,             /* input / output */
              BUFF_DESCR        *cursor_buff,           /* input / output  */
              int                case_sensitive,        /* input   */
              int               *search_failed,         /* output  */
              char               escape_char);          /* input  */

int   dm_s(DMC               *dm_list,               /* input   */
           FIND_DATA         *find_data,             /* input / output */
           MARK_REGION       *mark1,                 /* input   */
           BUFF_DESCR        *cursor_buff,           /* input   */
           int               *search_failed,         /* output  */
           int                rectangular,           /* input */
           char               escape_char);          /* input  */

void  dm_continue_find(FIND_DATA  *find_data,        /* input / output */
                       int        *found_line,       /* output */
                       int        *found_col,        /* output */
                       PAD_DESCR **found_pad,        /* output */
                       char        escape_char);     /* input  */



void  dm_continue_sub(FIND_DATA  *find_data,        /* input / output */
                      int        *found_line,       /* output */
                      int        *found_col,        /* output */
                      PAD_DESCR **found_pad,        /* output */
                      char        escape_char);     /* input  */

void dm_re(DMC  *dmc,
           char  escape_char);

int find_border_adjust(PAD_DESCR   *main_pad,
                       int          border);

#endif

