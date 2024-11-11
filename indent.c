static char *sccsid = "%Z% %M% %I% - %G% %U% ";
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
*  module indent.c
*
*  Routines:
*     dm_ind              - indent command
*     dm_aind             - Turn auto indenting off and on
*
*  Internal:
*     read_indent         - scroll the main pad one line.
*
*  Overview of ind command:  Rules based indenting.  
*  
*  dm command ind (indent)
*
*  1.  positions cursor on the current line.
*  2.  looks are rules of the form   /regx/  {indent | "DM;CMDS"" }
*                              or    \regx\  {indent | "DM;CMDS"" }
*  3.  Finds the preceding and following non-blank lines.
*  4.  Applies the rules in order till a match is found.
*  5.  Positions cursor to start position of that line
*      + or minus the indent or runs the list of dm cmds.
*  6.  Indent examples:   +5    indent 5 beyond the start
*                               of the matched line
*                         -3    indent 3 before the start of
*                               the matched line
*                          6    put the cursor in column 6
*  7.  A Null regular expression means match the previous/next
*      non-blank line.
*  8.  The command has an option to ignore rules and position
*      based on the previous or next non-blank line.
*  9.  ind [-on | -off] [-p] [-n] [-c] [<indent_num>]
*      specifying no options causes rules to be checked.
*      -p -> position based on the previous non-blank lines start point
*      -n -> position based on the next non-blank lines start point
*      -c -> position based on the current lines start point
*      start point is the first non-blank.
* 10.  Rules are read in first time used based on suffix of file.
*      Rules stored in a directory.  Default ~/.Ce_autoindent
*      Special name in directory .DEFAULT matches is used if no
*      name matches.
*
*  Auto-indenting causes dm_ind to be called with no parms after each
*  enter in the main window in non-pad mode.
*
*  dm_ind is a no-op in pad_mode.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */


#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */

#include "debug.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "emalloc.h"
#include "memdata.h"     /* needed for MAX_LINE */

/***************************************************************
*  
*  Structure with static data used by dm_ind.  This data is
*  malloc'ed an attachhed to the display description.
*  
***************************************************************/

struct RULE  ;

struct RULE {
        struct RULE            *next;              /* next rule in list                          */
        char                   *search;            /* search command                             */
        char                   *cmds;              /* command list                               */
        int                     relative;          /* flag for relative or absolute offsets      */
        int                     offset;            /* length of the data in this segment         */
  };

typedef struct RULE RULE;

typedef struct {
   RULE              *list_head;              /* linked list of rules */
} INDENT_DATA;


/************************************************************************

NAME:      dm_ind - indent command

PURPOSE:    This routine performs the window close operation.
            It is the shut down command for the editor.
            Unless a prompt is generated to confirm quiting a changed file,
            or the autoclose mode is being changed, this routine does not return.

PARAMETERS:

   1.  dmc            - pointer to DMC (INPUT)
                        This is the command structure for the indent command.

   2.  edit_file      - pointer to char (INPUT)
                        This is the current file being edited.  It is needed
                        to get the suffix off in case we have to load the file.

   3.  isuffix_dir    - pointer to char (INPUT)
                        This is the name of the directory holding the indenting rules.

   4.  cursor_buff    - pointer to BUFF_DESCR (INPUT/OUTPUT)
                        This is the current position description.  It is known
                        to point to the main pad.

   5.  indent_data    - pointer to pointer to void (INPUT/OUTPUT)
                        This is the address of where indent puts it static data.
                        It points to a null pointer when the data area has
                        not yet been initialized.

maybe pass only dspl_descr after the option_values are merged with the display description.


FUNCTIONS :

   1.   If the recording is being turned off, close the paste buffer
        and get rid of the recording data.

   2.   If the recording is being turned on, malloc area for the
        recording data and save it in the display description.

   3.   If the recording is being turned on, generate an xc command 
        and use it to open the paste buffer.  This avoids changing
        the paste buffer commands to recogize the record command.


*************************************************************************/

void   dm_ind(DMC            *dmc,
              char           *edit_file,
              char           *isuffix_dir,
              BUFF_DESCR     *cursor_buff,
              void          **indent_data)
{
} /* end of dm_ind  */




