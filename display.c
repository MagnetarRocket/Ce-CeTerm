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
*  module display.c
*
*  These create and initialize and manipulate the display and
*  drawable structures.
*
*  Routines:
*         init_display     - build the display structure and the supporting structures.
*         add_display      - Add a display to the circular linked list of displays
*         del_display      - Delete a display from the circular linked list of displays
*
*  Internal routines
*         free_dspl        - free all allocated storage associated with the dspl
*         free_pad         - free all allocated storage associated with a pad
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */
#ifndef WIN32
#include <pwd.h>            /* /usr/include/pwd.h        */
#ifndef OMVS
#include <sys/param.h>      /* /usr/include/sys/param.h  */
#endif
#endif /* WIN32 */
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif


#include "debug.h"
#include "display.h"
#include "emalloc.h"
#include "ind.h"
#include "hsearch.h"
#include "memdata.h"
#include "parms.h"
#include "unixwin.h"  /* needed for MAX_UNIX_LINES */
#include "xsmp.h"

void   exit(int code);
#ifndef linux
void   gethostname(char  *target, int size);
#endif

/***************************************************************
*  
*  Declares for local routines.
*  
***************************************************************/

static void free_dspl(DISPLAY_DESCR *dspl, int last);
static void free_pad(PAD_DESCR *pad, int kill_token);

/***************************************************************
*  
*  Display counter
*  
***************************************************************/

static int     display_number = 0;

/************************************************************************

NAME:      init_display  - build the display structure and the supporting structures.

PURPOSE:    This routine will create the display structure and the 
            underlying buffer, pad, and drawable structures.

PARAMETERS:

   none

FUNCTIONS :

   1.   create and zero the display data

   2.   Create and zero each pad and put the address in the
        display data.

   3.   Create and zero all the drawable descriptions and
        put them in each pad description.  Initialize the
        pad description fields.

RETURNED VALUE:
   dspl  -  pointer to DISPLAY_DESCR
        The initialized structure is returned.  On malloc failure
        NULL is returned.

*************************************************************************/

DISPLAY_DESCR *init_display(void)
{
DISPLAY_DESCR  *dspl;
PAD_DESCR      *pad;

/***************************************************************
*  
*  Create the new display and initialize it with the pad descriptions.
*  
***************************************************************/

dspl = (DISPLAY_DESCR *)CE_MALLOC(sizeof(DISPLAY_DESCR));
if (!dspl)
   return(NULL);

memset((char *)dspl, 0, sizeof(DISPLAY_DESCR));
dspl->display_no = ++display_number;

dspl->cursor_buff = (BUFF_DESCR *)CE_MALLOC(sizeof(BUFF_DESCR));
if (!dspl->cursor_buff)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)dspl->cursor_buff, 0, sizeof(BUFF_DESCR));

dspl->main_pad = (PAD_DESCR *)CE_MALLOC(sizeof(PAD_DESCR));
if (!dspl->main_pad)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)dspl->main_pad, 0, sizeof(PAD_DESCR));

dspl->dminput_pad = (PAD_DESCR *)CE_MALLOC(sizeof(PAD_DESCR));
if (!dspl->dminput_pad)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)dspl->dminput_pad, 0, sizeof(PAD_DESCR));

dspl->dmoutput_pad = (PAD_DESCR *)CE_MALLOC(sizeof(PAD_DESCR));
if (!dspl->dmoutput_pad)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)dspl->dmoutput_pad, 0, sizeof(PAD_DESCR));

#ifdef PAD
dspl->unix_pad = (PAD_DESCR *)CE_MALLOC(sizeof(PAD_DESCR));
if (!dspl->unix_pad)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)dspl->unix_pad, 0, sizeof(PAD_DESCR));
#endif

dspl->title_subarea = (DRAWABLE_DESCR *)CE_MALLOC(sizeof(DRAWABLE_DESCR));
if (!dspl->title_subarea)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)dspl->title_subarea, 0, sizeof(DRAWABLE_DESCR));

dspl->lineno_subarea = (DRAWABLE_DESCR *)CE_MALLOC(sizeof(DRAWABLE_DESCR));
if (!dspl->lineno_subarea)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)dspl->lineno_subarea, 0, sizeof(DRAWABLE_DESCR));

/***************************************************************
*  
*  Initialize the display constants to their defaults.
*  
***************************************************************/

dspl->insert_mode = True;
dspl->scroll_mode = True;

/***************************************************************
*  
*  Initialize the main pad description
*  
***************************************************************/

pad = dspl->main_pad;

pad->which_window        = MAIN_PAD;
pad->file_line_no        = -1;
/*pad->first_line        =  0;*/
/*pad->file_col_no       =  0;*/
/*pad->first_char        =  0;*/
pad->work_buff_ptr       =  CE_MALLOC(MAX_LINE+1);
if (!pad->work_buff_ptr)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   pad->work_buff_ptr[0]    = '\0';
pad->buff_ptr               =  pad->work_buff_ptr;
/*pad->buff_modified        = 0;*/
pad->redraw_mask            = MAIN_PAD_MASK;
pad->display_data           = dspl;
pad->insert_mode            = &(dspl->insert_mode);
/*pad->echo_mode              = &(dspl->echo_mode); RES 9/8/97 */
pad->hold_mode              = &(dspl->hold_mode); 
pad->autohold_mode          = &(dspl->autohold_mode);
pad->vt100_mode             = &(dspl->vt100_mode);
pad->redraw_start_line      = -1;
pad->impacted_redraw_line   = -1;
pad->window = (DRAWABLE_DESCR *)CE_MALLOC(sizeof(DRAWABLE_DESCR));
if (!pad->window)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)pad->window, 0, sizeof(DRAWABLE_DESCR));

pad->pix_window = (DRAWABLE_DESCR *)CE_MALLOC(sizeof(DRAWABLE_DESCR));
if (!pad->pix_window)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)pad->pix_window, 0, sizeof(DRAWABLE_DESCR));

/***************************************************************
*  
*  Initialize the dminput pad description
*  
***************************************************************/

pad = dspl->dminput_pad;

pad->which_window        = DMINPUT_WINDOW;
pad->work_buff_ptr       =  CE_MALLOC(MAX_LINE+1);
if (!pad->work_buff_ptr)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   pad->work_buff_ptr[0]   = '\0';
pad->buff_ptr              = pad->work_buff_ptr;
pad->redraw_mask           = DMINPUT_MASK;
pad->display_data          = dspl;
pad->insert_mode           = &(dspl->insert_mode);
/*pad->echo_mode             = &(dspl->echo_mode); RES 9/8/97 */
pad->hold_mode             = &(dspl->hold_mode); 
pad->autohold_mode         = &(dspl->autohold_mode);
pad->vt100_mode            = &(dspl->vt100_mode);
pad->redraw_start_line      = -1;
pad->impacted_redraw_line   = -1;
pad->window = (DRAWABLE_DESCR *)CE_MALLOC(sizeof(DRAWABLE_DESCR));
if (!pad->window)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)pad->window, 0, sizeof(DRAWABLE_DESCR));

pad->pix_window = (DRAWABLE_DESCR *)CE_MALLOC(sizeof(DRAWABLE_DESCR));
if (!pad->pix_window)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)pad->pix_window, 0, sizeof(DRAWABLE_DESCR));

pad->win_lines_size  = 1;

pad->win_lines = (WINDOW_LINE *)CE_MALLOC(sizeof(WINDOW_LINE));
if (!pad->win_lines)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)pad->win_lines, 0, sizeof(WINDOW_LINE));

/***************************************************************
*  
*  Initialize the dmoutput_cur_descr pad description
*  
***************************************************************/

pad = dspl->dmoutput_pad;

pad->which_window        = DMOUTPUT_WINDOW;
pad->work_buff_ptr       =  CE_MALLOC(MAX_LINE+1);
if (!pad->work_buff_ptr)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   pad->work_buff_ptr[0] = '\0';
pad->buff_ptr               = pad->work_buff_ptr;
pad->redraw_mask            = DMOUTPUT_MASK;
pad->display_data           = dspl;
pad->insert_mode            = &(dspl->insert_mode);
/*pad->echo_mode              = &(dspl->echo_mode); RES 9/8/97 */
pad->hold_mode              = &(dspl->hold_mode); 
pad->autohold_mode          = &(dspl->autohold_mode);
pad->vt100_mode             = &(dspl->vt100_mode);
pad->redraw_start_line      = -1;
pad->impacted_redraw_line   = -1;
pad->window = (DRAWABLE_DESCR *)CE_MALLOC(sizeof(DRAWABLE_DESCR));
if (!pad->window)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)pad->window, 0, sizeof(DRAWABLE_DESCR));

pad->pix_window = (DRAWABLE_DESCR *)CE_MALLOC(sizeof(DRAWABLE_DESCR));
if (!pad->pix_window)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)pad->pix_window, 0, sizeof(DRAWABLE_DESCR));

pad->win_lines_size  = 1;

pad->win_lines = (WINDOW_LINE *)CE_MALLOC(sizeof(WINDOW_LINE));
if (!pad->win_lines)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)pad->win_lines, 0, sizeof(WINDOW_LINE));

#ifdef PAD
/***************************************************************
*  
*  Initialize the Unix cmd pad description
*  
***************************************************************/

pad = dspl->unix_pad;

pad->which_window        = UNIXCMD_WINDOW;
pad->work_buff_ptr       =  CE_MALLOC(MAX_LINE+1);
if (!pad->work_buff_ptr)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   pad->work_buff_ptr[0] = '\0';
pad->buff_ptr               = pad->work_buff_ptr;
pad->redraw_mask            = UNIXCMD_MASK;
pad->display_data           = dspl;
pad->insert_mode            = &(dspl->insert_mode);
/*pad->echo_mode              = &(dspl->echo_mode); RES 9/8/97 */
pad->hold_mode              = &(dspl->hold_mode); 
pad->autohold_mode          = &(dspl->autohold_mode);
pad->vt100_mode             = &(dspl->vt100_mode);
pad->redraw_start_line      = -1;
pad->impacted_redraw_line   = -1;
pad->window = (DRAWABLE_DESCR *)CE_MALLOC(sizeof(DRAWABLE_DESCR));
if (!pad->window)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)pad->window, 0, sizeof(DRAWABLE_DESCR));

pad->pix_window = (DRAWABLE_DESCR *)CE_MALLOC(sizeof(DRAWABLE_DESCR));
if (!pad->pix_window)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)pad->pix_window, 0, sizeof(DRAWABLE_DESCR));

pad->win_lines_size  = MAX_UNIX_LINES;

pad->win_lines = (WINDOW_LINE *)CE_MALLOC(sizeof(WINDOW_LINE)*MAX_UNIX_LINES);
if (!pad->win_lines)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)pad->win_lines, 0, sizeof(WINDOW_LINE)*MAX_UNIX_LINES);
#endif

/**************************************************************
*  
*  Allocate the option values area and the find data.
*  
***************************************************************/

dspl->find_data = (FIND_DATA *)CE_MALLOC(sizeof(FIND_DATA));
if (!dspl->find_data)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)dspl->find_data, 0, sizeof(FIND_DATA));

dspl->option_values = (char **)CE_MALLOC((sizeof(char *)) * OPTION_COUNT);
if (!dspl->option_values)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)dspl->option_values, 0, (sizeof(char *)) * OPTION_COUNT);

dspl->option_from_parm = (char *)CE_MALLOC(OPTION_COUNT);
if (!dspl->option_from_parm)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)dspl->option_from_parm, 0, OPTION_COUNT);

dspl->properties = (PROPERTIES *)CE_MALLOC(sizeof(PROPERTIES));
if (!dspl->properties)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)dspl->properties, 0, sizeof(PROPERTIES));

dspl->sb_data = (SCROLLBAR_DESCR *)CE_MALLOC(sizeof(SCROLLBAR_DESCR));
if (!dspl->sb_data)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)dspl->sb_data, 0, sizeof(SCROLLBAR_DESCR));

dspl->pd_data = (PD_DATA *)CE_MALLOC(sizeof(PD_DATA));
if (!dspl->pd_data)
   {
      free_dspl(dspl,True);
      return(NULL);
   }
else
   memset((char *)dspl->pd_data, 0, sizeof(PD_DATA));

return(dspl);
} /* end of init_display  */


/************************************************************************

NAME:      free_pad      - Free storage associated with a pad description

PURPOSE:    This routine will free up the storage malloc'ed to support
            a pad description.  It is called by free_dspl

PARAMETERS:

   1.   dspl            - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                          This is the description to free.

   2.   kill_token      - int (INPUT)
                          This flag specifies whether we are to delete the
                          memdata token associated this file.  During initialization
                          this irrelevant because the token has not been initialized yet.
                          Later this flag determines whether we want to delete the
                          main pad token which is shared across multiple 'cc' copies.

FUNCTIONS :

   1.   Free all the minor allocated structures.

   2.   Free the pad description itself.

*************************************************************************/

static void free_pad(PAD_DESCR *pad, int kill_token)
{

if (pad->work_buff_ptr)
   free((char *)pad->work_buff_ptr);
if (pad->window)
   free((char *)pad->window);
if (pad->pix_window)
   free((char *)pad->pix_window);
if (pad->win_lines)
   free((char *)pad->win_lines);
if (pad->token && kill_token)
   mem_kill(pad->token);
if (pad->cdgc_data)
   free((char *)pad->cdgc_data);
free((char *)pad);
} /* end of free_pad */


/************************************************************************

NAME:      free_dspl      - Free storage associated with a display

PURPOSE:    This routine will free up the storage malloc'ed to support
            a display description.

PARAMETERS:

   1.   dspl            - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                          This is the description to free.

   2.   last            - int (INPUT)
                          This flag specifies whether this is the last or
                          only description to be freed.  During initialization
                          this is always True.  The real question is whether
                          we want to delete the main pad token which is shared
                          across multiple 'cc' copies.

FUNCTIONS :

   1.   Free each of the pad descriptions.

   2.   Free all the minor allocated structures.

   3.   Free the display description itself.

*************************************************************************/

static void free_dspl(DISPLAY_DESCR *dspl, int last)
{
int                   i;
int                   j;
int                   found;

if (dspl->cursor_buff)
   free((char *)dspl->cursor_buff);
if (dspl->main_pad)
   free_pad(dspl->main_pad, last);
if (dspl->dminput_pad)
   free_pad(dspl->dminput_pad, True);
if (dspl->dmoutput_pad)
   free_pad(dspl->dmoutput_pad, True);
#ifdef PAD
if (dspl->unix_pad)
   free_pad(dspl->unix_pad, True);
#endif
if (dspl->title_subarea)
   free((char *)dspl->title_subarea);
if (dspl->lineno_subarea)
   free((char *)dspl->lineno_subarea);

if (dspl->txcursor_area)
   free((char *)dspl->txcursor_area);
if (dspl->cmd_record_data)
   free((char *)dspl->cmd_record_data);
if (dspl->indent_data)
   free((char *)dspl->indent_data);
if (dspl->warp_data)
   free((char *)dspl->warp_data);
if (dspl->dmwin_data)
   free((char *)dspl->dmwin_data);
if (dspl->current_font_name)
   free(dspl->current_font_name);
if (dspl->properties)
   free((char *)dspl->properties);
if (dspl->cursor_data)
   free((char *)dspl->cursor_data);

if (dspl->sb_data)
   {
      if (dspl->sb_data->sb_private)
         free((char *)dspl->sb_data->sb_private);
      free((char *)dspl->sb_data);
   }

if (dspl->pd_data)
   {
      if (dspl->pd_data->pd_private)
         free((char *)dspl->pd_data->pd_private);
      free((char *)dspl->pd_data);
   }

/* free of prompt data is handled in wc.c */

if (dspl->option_values)
   {
      for (i = 0; i < OPTION_COUNT; i++)
         if (dspl->option_values[i])
            {
               found = 0;
               for (j = 0; j < i && !found; j++)
                   if (dspl->option_values[i] == dspl->option_values[j])
                      found = 1;
               if (!found)
                  free((char *)dspl->option_values[i]);
            }
      free((char *)dspl->option_values);
   }

if (dspl->option_from_parm)
   free((char *)dspl->option_from_parm);

if (dspl->find_data)
   {
      if (dspl->find_data->private)
         free((char *)dspl->find_data->private);
      if (dspl->find_data->search_private)
         free((char *)dspl->find_data->search_private);
      free((char *)dspl->find_data);
   }

if (dspl->ind_data)
   free_ind(dspl->option_from_parm);

/* RES 7/11/2003, add VT_colors to display description */
for (i = 0; i < 8; i++)
   if (dspl->VT_colors[i])
      free(dspl->VT_colors[i]);


free((char *)dspl);

} /* end of free_dspl */


/************************************************************************

NAME:      add_display      - Add a display to the circular linked list of displays

PURPOSE:    This routine will add a passed display just after the
            base display (also passed).

PARAMETERS:

   1.   base_dspl       - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                          This is a display known to be in the
                          circular linked list of displays.

   2.   new_dspl        - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                          This is a new display structure to be added.

FUNCTIONS :

   1.   Put the base displays next field in the new displays next field.

   2.   Point the base displays next field to the new display.


*************************************************************************/

void add_display(DISPLAY_DESCR    *base_dspl,
                 DISPLAY_DESCR    *new_dspl)
{
new_dspl->next = base_dspl->next;
base_dspl->next = new_dspl;

} /* end of add_display */


/************************************************************************

NAME:      del_display      - Delete a display from the circular linked list of displays

PURPOSE:    This routine will delete the passed display from the circular
            linked list of displays and free the storage for the display.
            If the last display is being freed, it returns true.
            The display should already have been closed!

PARAMETERS:

   1.   dspl            - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                          This is a display to be deleted.

FUNCTIONS :

   1.   Check for this being the last display.  If so, free it and
        return true.

   2.   Point the base displays next field to the new display.

RETURNED VALUE:
   last   -   int
              True   -  The last display was deleted
              False  -  There are still more displays on the list.


*************************************************************************/

int del_display(DISPLAY_DESCR    *dspl)
{
int                   last;
DISPLAY_DESCR        *walk_dspl;
int                   dup;

if ((dspl->next == NULL) || (dspl->next == dspl))
   {
      last = True;
#ifdef  HAVE_X11_SM_SMLIB_H
      if (dspl->xsmp_active)
         {
            xsmp_close(&(dspl->xsmp_private_data));  /* input/output */
            dspl->xsmp_private_data = NULL;
         }
#endif
      if (dspl->hsearch_data)
         hdestroy(dspl->hsearch_data);
      dspl->hsearch_data = NULL;
   }
else
   {
      last = False;
#ifdef  HAVE_X11_SM_SMLIB_H
      if (dspl->xsmp_active)
         xsmp_display_check(dspl, dspl->next);
#endif

      /***************************************************************
      *  Only delete the hash table if this is the last display using it.
      *  This has to be done before we unlink the display description.
      ***************************************************************/
      if (dspl->hsearch_data)
         {
            dup = False;
            for (walk_dspl = dspl->next; walk_dspl != dspl; walk_dspl = walk_dspl->next)
            {
               if (walk_dspl->hsearch_data == dspl->hsearch_data)
                  dup = True;
            }
            if (!dup)
               hdestroy(dspl->hsearch_data);
         }

      /***************************************************************
      *  Find the display description which points to the passed one.
      *  We need this to reconnect the circular linked list.
      ***************************************************************/
      for (walk_dspl = dspl; walk_dspl->next != dspl; walk_dspl = walk_dspl->next)
          ; /* do nothing */

      walk_dspl->next = dspl->next; /* remove from the list */
   }

free_dspl(dspl, last);
return(last);

} /* end of del_display */



