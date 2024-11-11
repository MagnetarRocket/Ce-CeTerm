#ifndef _PARSEDM_INCLUDED
#define _PARSEDM_INCLUDED

/* static char *parsedm_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in parsedm.c
*
*     parse_dm_line    -  parse a line with DM commands
*     free_dmc         -  Free a DMC command structure.
*     dump_kd          -  Format a nd print a DMC command structure.
*     iln              -  special itoa for line_numbers
*     add_escape_chars -  Add escape characters to a line
*     copy_with_delim  -  Copy a file taking into account escape chars and a delimiter (internal)
*
***************************************************************/

#include "dmsyms.h"
#include "dmc.h"


#define  DMC_COMMENT ((DMC *)1)

DMC      *parse_dm_line(char   *cmd_line,    /* input  */
                        int     temp_flag,   /* input  */
                        int     line_no,     /* input  */
                        char    escape_char, /* input  */
                        void   *hash_table); /* input  */

void   free_dmc(DMC   *dmc);

char *iln(int val);

void dump_kd(FILE    *stream,      /* input   */
             DMC     *first_dmc,   /* input   */
             int      one_only,    /* input   */
             int      kd_special,  /* input   */
             char    *target_str,  /* output  */
             char     escape_char);/* input  */

void add_escape_chars(char   *outname,
                      char   *inname,
                      char   *special_chars,
                      char    escape_char,
                      int     add_quotes,
                      int     kd_special);

char *copy_with_delim(char    *from,
                      char    *to,
                      char     delim,
                      char     escape_char,
                      int      missing_end_delim_ok);


#endif

