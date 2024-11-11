/*static char *sccsid = "%Z% %M% %I% - %G% %U% ";*/
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
*  module reload.c
*
*  Routines:
*     dm_reload           - Reload the file
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */


#include "dmwin.h"
#include "display.h"
#include "getevent.h"
#include "ind.h"
#include "kd.h"
#include "lock.h"
#include "prompt.h"
#include "parms.h"
#include "pw.h"
#include "reload.h"
#include "windowdefs.h"

#ifndef WIN32
#ifndef OMVS
#include <sys/param.h>      /* /usr/include/sys/param.h  */
#endif
#endif /* WIN32 */
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif

/************************************************************************

NAME:      dm_reload

PURPOSE:    This routine causes the file being edited/browsed to be 
            reloaded from disk.  It is only valid in non-pad mode.
            It does not make sense in cetemm.  It can take a an optional
            parameter which may be 'y', 'f', or 'n'.
            'n' is a no-op from "reload -n".  'y' or 'f' are the same.
            and cause the reload to occur.  No parameter causes the
            dirty (file modified) bit to be checked.  If the file is
            changed, a prompt is issues for yes or no (-y or -n).

PARAMETERS:

   1.  dmc            - pointer to DMC (INPUT)
                        This is the command structure for the reload command.
                        It contains the data from any dash options.

   2.  dspl_descr     - pointer to DISPLAY_DESCR (INPUT / OUTPUT)
                        This is the description for the display.
                        It is used to create new mem-init structure and start the
                        reload.

   3.  edit_file      - pointer to char (INPUT)
                        This is the file being edited and thus reloaded.


FUNCTIONS :

   1.   Switch on the type of the reload.

   2.   If no parms were specified, check to see if the file is dirty.  If so
        set up a prompt and return.  The result of the prompt will be a dash option.
        A yes reply will cause a reload -y to be generated, which we interpret here as a
        confirmed reload.

   3.   If -y or -f was specified, delete the memory copy of the file, open it and
        reload it.

OUTPUTS:
   redraw -  The mask anded with the type of redraw needed is returned.

*************************************************************************/

int  dm_reload(DMC             *dmc,                    /* input  */
               DISPLAY_DESCR   *dspl_descr,             /* input  / output */
               char            *edit_file)              /* input  */
{
int              really_reload = False;
DMC             *prompt;
char             work[MAXPATHLEN+100];
int              rc;
int              redraw_needed = 0;
int              open_for_write;



/***************************************************************
*  
*  Deal with the case where this is the second call after a prompt
*  is executed.  We expect either a -y or -n answer.
*  
***************************************************************/

switch (dmc->reload.type)
{
case 'y':
case 'f':
   really_reload = True;
   break;

case '\0':
#ifdef PAD
   if (dspl_descr->pad_mode)
      {
         dm_error("(reload) No reload in ceterm", DM_ERROR_BEEP);
         break;
      }
#endif
   if (!dirty_bit(dspl_descr->main_pad->token))
      really_reload = True;
   else
      {
         strcpy(work, "reload -&'File modified, OK to reload? ' ");
         prompt = prompt_prescan(work, True, dspl_descr->escape_char);  /* in kd.c */
         if (prompt == NULL)
            dm_error("(reload) Unable to reload ", DM_ERROR_LOG);
         else
            redraw_needed = dm_prompt(prompt, dspl_descr);
      }
   break;

case 'n':
   dspl_descr->reload_off = True;
   break;


default:
   break;

} /* end of switch on type */


/***************************************************************
*  
*  If all has gone well, we delete the old memory copy and start
*  a new one.
*  
***************************************************************/

if (really_reload)
   {
      if (dspl_descr->pad_mode)
         {
            dm_error("(reload) No reload in ceterm", DM_ERROR_BEEP);
         }
      else
         if (strcmp(edit_file, STDIN_FILE_STRING) == 0)
            {
               dm_error("(reload) Cannot reload STDIN", DM_ERROR_BEEP);
            }
         else
            {
               dspl_descr->reload_off = False;
               if (instream)
                  {
                     if (LSF)
                        lsf_close(instream, dspl_descr->ind_data);
                     else
                        fclose(instream);
                  }

               if (LSF)
                  instream = filter_in(&(dspl_descr->ind_data), dspl_descr->hsearch_data, edit_file, "r");
               else
                  instream = fopen(edit_file, "r");

               if (instream == NULL)
                  {
                     snprintf(work, sizeof(work), "(reload) Can't reopen %s (%s)", edit_file, strerror(errno));
                     dm_error(work, DM_ERROR_BEEP);
                     return(FULL_REDRAW);  /* could not reopen, this is not good */
                  }

               open_for_write = WRITABLE(dspl_descr->main_pad->token); /* save writeable state */

               mem_kill(dspl_descr->main_pad->token);
               dspl_descr->main_pad->token = NULL;

               dspl_descr->main_pad->token     = mem_init(75, False);  /* r/w fill blocks 75 %full  in memdata.c */

               WRITABLE(dspl_descr->main_pad->token) = open_for_write;

               change_background_work(dspl_descr, MAIN_WINDOW_EOF, False);
               change_background_work(dspl_descr, BACKGROUND_READ_AHEAD, READAHEAD);
               load_enough_data(dspl_descr->main_pad->first_line + dspl_descr->main_pad->lines_displayed); /* in getevent.c */
               redraw_needed = (MAIN_PAD_MASK & FULL_REDRAW);
               file_has_changed(edit_file);  /* in lock.c */
            }
   }

return(redraw_needed);

}  /* end of dm_reload */


