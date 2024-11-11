#ifndef _PW_INCLUDED
#define _PW_INCLUDED

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
*  Routines in pw.c
*         usage               - Print error message.
*         set_file_from_parm  - Get the file name from the parm string.
*         initial_file_open   - Open the file to be edited or browsed
*         initial_file_setup  - Check that we can open a file.
*         dm_ro               - Flip Flop the read only flag
*         ok_for_output       - Verify that a file can be opened for output.
*         ok_for_input        - Verify that a file can be opened for input.
*         dm_pn               - Pad Name / Rename command
*         dm_pw               - Pad Write (save) command
*         dm_cd               - Change Dir (for edit process)
*         dm_pwd              - print current DM working directory
*
***************************************************************/

#include "dmc.h"
#include "buffer.h"
#include "memdata.h"

#include <sys/stat.h>   /* /usr/include/sys/stat.h */

/***************************************************************
*  
*  Special value put in the edit file by initial_file_open
*  if stdin is the source of the data to browse.
*  
***************************************************************/

#define  STDIN_FILE_STRING  "<stdin>"
#define  CE_EDIT_DIR        "CE_EDIT_DIR"

void usage(char  *cmdname);

int  set_file_from_parm(DISPLAY_DESCR *dspl_descr,  /* input  */
                        int            argc,        /* input  */
                        char          *argv[],      /* input  */
                        char          *edit_file);  /* output */

FILE  *initial_file_open(char          *edit_file,   /* input  */
                         int           *need_write,  /* input  */
                         DISPLAY_DESCR *dspl_descr); /* input  */

int  initial_file_setup(char          *edit_file,
                        int           *need_write,
                        int           *file_exists,
                        struct stat   *file_stats,
                        DISPLAY_DESCR *dspl_descr);

void  dm_ro(DISPLAY_DESCR     *dspl_descr,            /* input   */
            int                mode,                  /* input   */
            int                read_locked,           /* input   */
            char              *edit_file);            /* input   */


int   ok_for_output(char          *edit_file,      /* input  */
                    int           *file_exists,    /* output */
                    DISPLAY_DESCR *dspl_descr);    /* input  */

int   ok_for_input(char          *edit_file,       /* input  */
                   DISPLAY_DESCR *dspl_descr);     /* input  */

void dm_pn(DMC           *dmc,                    /* input  */
           char          *edit_file,              /* input / output */
           DISPLAY_DESCR *dspl_descr);            /* input / output */

int   dm_pw(DISPLAY_DESCR *dspl_descr,         /* input  */
            char          *edit_file,          /* input  */
            int            from_crash_file);   /* input  */

void dm_cd(DMC        *dmc,                    /* input  */
           char       *edit_file);             /* input  */

void dm_pwd(void);


#endif

