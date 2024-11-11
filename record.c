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
*  module record.c
*
*  Routines:
*     dm_rec              - Turn on/off event recording to a file or pastebuff
*     rec_cmds            - Record the commmand lists.
*     rec_position        - Record the cursor position after dm cmds
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */

#ifndef WIN32
#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#endif

#include "debug.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "parsedm.h"
#include "emalloc.h"
#include "memdata.h"     /* needed for MAX_LINE */
#include "pastebuf.h"
#include "record.h"

/***************************************************************
*  
*  Structure with static data used by dm_rec.  This data is
*  malloc'ed an attachhed to the display description.
*  
***************************************************************/

typedef struct {
   FILE              *pastebuf_file;          /* place we are saving the cmds */
   int                motion;                 /* Flag to show normal mouse motion has taken place */
   int                last_file_line_no;      /* last known position */
   int                last_file_col_no;       /* last known position */
   int                which_window;           /* last known position */
} REC_DATA;


/************************************************************************

NAME:      dm_rec - Turn on/off event recording to a file or pastebuff

PURPOSE:    This routine performs the window close operation.
            It is the shut down command for the editor.
            Unless a prompt is generated to confirm quiting a changed file,
            or the autoclose mode is being changed, this routine does not return.

PARAMETERS:

   1.  dmc            - pointer to DMC (INPUT)
                        This is the command structure for the rec command.
                        It contains the the flag saying whether we are
                        turning recording on or off and the name of the
                        file or paste buffer to put it in.
                        dmc can be NULL in which case recording is turned off if it
                        was on.  This is used in ce, cp, and so on.

   2.  dspl_descr     - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                        The display description is used to access fields cmd_record_data and
                        This field holds the static data we need associated with the display.
                        If the recording is being turned off or the open of the target
                        for the recording fails, cmd_record_data will be NULL upon exit.


FUNCTIONS :

   1.   If the recording is being turned off, close the paste buffer
        and get rid of the recording data.

   2.   If the recording is being turned on, malloc area for the
        recording data and save it in the display description.

   3.   If the recording is being turned on, generate an xc command 
        and use it to open the paste buffer.  This avoids changing
        the paste buffer commands to recogize the record command.


*************************************************************************/

void   dm_rec(DMC            *dmc,
              DISPLAY_DESCR  *dspl_descr)
{
REC_DATA          *rec_data;
DMC_xc             xc_dmc;
int                rc;
#define            MAX_PASTE_REC_SIZE  8192

/***************************************************************
*  If recording is on, turn it off.  We either want to turn
*  it off or we are turning it on and need to turn it off first.
***************************************************************/
if (dspl_descr->cmd_record_data != NULL)
   {
      DEBUG17(fprintf(stderr, "dm_rec: Turning recording off\n");)
      /***************************************************************
      *  We are turning the recording off.  Close the buffer and
      *  free the data.
      ***************************************************************/
      rec_data = (REC_DATA *)dspl_descr->cmd_record_data;
      ce_fclose(rec_data->pastebuf_file);
      free(rec_data);
      dspl_descr->cmd_record_data = NULL;
   }
else
   if (dmc && (!dmc->rec.dash_p && !dmc->rec.path))
      DEBUG17(fprintf(stderr, "dm_rec: Command recording already off\n");)

/***************************************************************
*  If -p or a path or both were specified, we are turning
*  recording on.
***************************************************************/

if (dmc && (dmc->rec.dash_p || dmc->rec.path))
   {
      /***************************************************************
      *  Get a data area and initialize it.
      ***************************************************************/
      DEBUG17(fprintf(stderr, "dm_rec: Turning recording on\n");)
      rec_data = (REC_DATA *)CE_MALLOC(sizeof(REC_DATA));
      if (!rec_data)
         return;
      memset((char *)rec_data, 0, sizeof(REC_DATA));
      /***************************************************************
      *  Build an xc command to use to open the paste buffer.  This
      *  allows us to open it for output.  This technique avoids
      *  having to make open_paste_buffer know about the rec command.
      ***************************************************************/
      xc_dmc.next   = NULL;
      xc_dmc.cmd    = DM_xc;
      xc_dmc.temp   = False;
      xc_dmc.dash_r = False;
      if (dmc->rec.dash_p)
         xc_dmc.dash_f = False;
      else
         xc_dmc.dash_f = 'f';
      xc_dmc.dash_a = False;
      xc_dmc.dash_l = False;
      xc_dmc.path   = dmc->rec.path;
      /***************************************************************
      *  Access the paste buffer area.  If we cannot, bail out.
      ***************************************************************/
      rec_data->pastebuf_file = open_paste_buffer((DMC *)&xc_dmc,
                                                  dspl_descr->display,
                                                  dspl_descr->main_pad->x_window,
                                                  CurrentTime,
                                                  dspl_descr->escape_char);
      if (rec_data->pastebuf_file == NULL)
         {
            free(rec_data);
            return;
         }
      /***************************************************************
      *  If we are writing into a paste buffer, allocate space for it.
      ***************************************************************/
      if (dmc->rec.dash_p)
         {
            rc = setup_x_paste_buffer(rec_data->pastebuf_file,
                                      NULL,
                                      1, 1,
                                      MAX_PASTE_REC_SIZE);
            if (rc != 0)
               {
                  ce_fclose(rec_data->pastebuf_file);
                  free(rec_data);
                  return;
               }
         }
      /***************************************************************
      *  Show recording is on in the display description and save the
      *  current position.
      ***************************************************************/
      dspl_descr->cmd_record_data = (void *)rec_data;
      rec_position(dspl_descr);
   }

}  /* end of dm_rec */


/************************************************************************

NAME:      rec_cmds - Record the commmand lists.

PURPOSE:    This routine performs the actual recording of dm commands.

PARAMETERS:

   1.  dm_list        - pointer to DMC (INPUT)
                        This is the command structure for the command(s) to
                        be recorded.

   2.  dspl_descr     - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                        The display description is used to access fields cmd_record_data and
                        This field holds the static data we need associated with the display.
                        If the recording is being turned off or the open of the target
                        for the recording fails, cmd_record_data will be NULL upon exit.



FUNCTIONS :

   1.   If motion has been detected, generate the appropriate number
        of ar, al, au, and ad command to position the cursor.  Changes
        in which window we are in are affected by tdm, tmw, tdmo commands
        followed by a window position command.  Write this buffer to the
        place the file points to.

   2.   Put the key definitions in a buffer.

   3.   Write the buffer.


*************************************************************************/

void   rec_cmds(DMC              *dm_list,
                DISPLAY_DESCR    *dspl_descr)
{
REC_DATA          *rec_data;
#ifdef DebuG
int                last_line;
int                last_col;
#endif
char               buff[MAX_LINE+1];
int                rc;
int                delta_line;
int                delta_col;
char              *p;

if ((rec_data = (REC_DATA *)dspl_descr->cmd_record_data) == NULL)
   {
      DEBUG(fprintf(stderr, "rec_cmds called when command recording is turned off\n");)
      return;
   }

/***************************************************************
*  If there is no dm_list, return
***************************************************************/
if (!dm_list)
   return;

/***************************************************************
*  If this is a turn recording off command by itself, do not
*  output it.
***************************************************************/
if ((dm_list->any.cmd == DM_rec) && (dm_list->next == NULL) && (!dm_list->rec.dash_p && !dm_list->rec.path))
   return;

/***************************************************************
*  Generate au, ad, al, and ar commands as necessary.
*  To get the cursor in the right place.
***************************************************************/
#ifdef DebuG
last_line = rec_data->last_file_line_no;
last_col  = rec_data->last_file_col_no;
#endif

if (rec_data->which_window == dspl_descr->cursor_buff->which_window)
   {
      delta_line = dspl_descr->cursor_buff->current_win_buff->file_line_no - rec_data->last_file_line_no;
      delta_col  = dspl_descr->cursor_buff->current_win_buff->file_col_no  - rec_data->last_file_col_no;

      if (delta_line || delta_col)
         {
            snprintf(buff, sizeof(buff), "[%+d,%+d]", delta_line, delta_col);
            rec_data->last_file_line_no = dspl_descr->cursor_buff->current_win_buff->file_line_no;
            rec_data->last_file_col_no  = dspl_descr->cursor_buff->current_win_buff->file_col_no;
         }
      else
         buff[0] = '\0';
   }
else
   {
      switch(dspl_descr->cursor_buff->which_window)
      {
      case  MAIN_PAD:
         strcpy(buff, "tmw;");
         break;
      case  DMINPUT_WINDOW:
         strcpy(buff, "tdm;");
         break;
      case  DMOUTPUT_WINDOW:
         strcpy(buff, "tdmo;");
         break;
      case  UNIXCMD_WINDOW:
         strcpy(buff, "tmw;ti;");
         break;
      }
      p = &buff[strlen(buff)]; /* point to the null at the end */
      snprintf(p, (sizeof(buff)-(p-buff)), "[%d,%d]", dspl_descr->cursor_buff->current_win_buff->file_line_no, dspl_descr->cursor_buff->current_win_buff->file_col_no);
      rec_data->last_file_line_no = dspl_descr->cursor_buff->current_win_buff->file_line_no;
      rec_data->last_file_col_no  = dspl_descr->cursor_buff->current_win_buff->file_col_no;
      rec_data->which_window = dspl_descr->cursor_buff->which_window;
   }


/***************************************************************
*  If we had movement commands, output them.
***************************************************************/
if (buff[0])
   {
      DEBUG17(
         fprintf(stderr, "rec_cmds:  positioning from file [%d,%d] to [%d,%d]:  \"%s\"\n",
                         last_line, last_col, dspl_descr->cursor_buff->current_win_buff->file_line_no, dspl_descr->cursor_buff->current_win_buff->file_col_no, buff);
      )
      strcat(buff, "\n");
      rc = ce_fputs(buff, rec_data->pastebuf_file);
      if (rc < 0)
         {
            ce_fclose(rec_data->pastebuf_file);
            free(rec_data);
            dspl_descr->cmd_record_data = NULL;
            return;
         }
   }

buff[0] = '\0';
dump_kd(NULL, dm_list,  False, False, buff, dspl_descr->escape_char);
if (buff[0])
   {
      DEBUG17(fprintf(stderr, "rec_cmds:  \"%s\"\n", buff);)
      strcat(buff, "\n");
      rc = ce_fputs(buff, rec_data->pastebuf_file);
      if (rc < 0)
         {
            ce_fclose(rec_data->pastebuf_file);
            free(rec_data);
            dspl_descr->cmd_record_data = NULL;
            return;
            
         }
   }


}  /* end of rec_cmds */


/************************************************************************

NAME:      rec_position - Record the cursor position after dm cmds

PURPOSE:    This routine is called when in record mode after a set of
            commmands is executed, before drawing the screen and going
            back to the user.  It save the current cursor position.

PARAMETERS:

   1.  dspl_descr     - pointer to DISPLAY_DESCR (INPUT/OUTPUT)
                        The display description is used to access fields cmd_record_data and
                        This field holds the static data we need associated with the display.
                        If the recording is being turned off or the open of the target
                        for the recording fails, cmd_record_data will be NULL upon exit.



FUNCTIONS :

   1.   Save the current window postion in the display data.


*************************************************************************/

void   rec_position(DISPLAY_DESCR   *dspl_descr)
{
REC_DATA          *rec_data;

if ((rec_data = (REC_DATA *)dspl_descr->cmd_record_data) != NULL)
   {
      rec_data->which_window      = dspl_descr->cursor_buff->which_window;
      rec_data->last_file_line_no = dspl_descr->cursor_buff->current_win_buff->file_line_no;
      rec_data->last_file_col_no  = dspl_descr->cursor_buff->current_win_buff->file_col_no;
   }

}  /* end of rec_position */




