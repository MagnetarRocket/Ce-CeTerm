#ifndef _DMWIN_INCLUDED
#define _DMWIN_INCLUDED

/* static char *dmwin_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in dmwin.c
*         get_dm_windows        - get the dm input and output windows.
*         draw_dm_prompt        - Draw the DM prompt in the dminput command window.
*         dm_error              - Write message to dm_output window and beep
*         dm_error_dspl         - Write message to dm_output specifying the display.
*         dm_clean              - Clean up dm error messages but not informatory data
*         resize_dm_windows     - handle a resize of the main window to the client.
*         set_dm_prompt         - Prompt for input at the DM input window.
*         dm_subarea            - Build the subarea drawing description for a DM window.
*         scroll_dminput_prompt - scroll the dm input window prompt.
*         get_dm_prompt         - Return the dm prompt address, set by set dm prompt.
*
***************************************************************/

#include "buffer.h"
#include "xutil.h"

/**************************************************************
*
*  Constants used as the second parm to dm_error.
*
***************************************************************/

#define  DM_ERROR_MSG    0
#define  DM_ERROR_BEEP   1
#define  DM_ERROR_LOG    2

#ifdef WIN32
#define  ERROR_LOG       "ce_edit_log.txt"
#else
#define  ERROR_LOG       "/tmp/ce_edit_log"
#endif
#define  DMERROR_MSG_WINDOW "CE_DMWIN_MSG"

/***************************************************************
*  
*  Prototypes for the functions
*  
***************************************************************/


int   get_dm_windows(DISPLAY_DESCR    *dspl_descr,           /* input / output */
                     int               format);              /* input   */

void draw_dm_prompt(DISPLAY_DESCR    *dspl_descr,
                    DRAWABLE_DESCR   *dminput_pix_subarea);

void  dm_error(char             *text,         /* input  */
               int               level);       /* input  */

void  dm_error_dspl(char             *text,         /* input  */
                    int               level,        /* input  */
                    DISPLAY_DESCR    *dspl_descr);  /* input  */

void  dm_clean(int clear_dmoutput);

void  resize_dm_windows(DRAWABLE_DESCR   *main_window,          /* input   */
                        DRAWABLE_DESCR   *dminput_window,       /* input / output  */
                        DRAWABLE_DESCR   *dmoutput_window,      /* input / output  */
                        Display          *display,              /* input   */
                        Window            main_x_window,        /* input   */
                        Window            dminput_x_window,     /* input   */
                        Window            dmoutput_x_window);   /* input   */

void  set_dm_prompt(DISPLAY_DESCR    *dspl_descr,
                    char             *text);

void  dm_subarea(DISPLAY_DESCR    *dspl_descr,
                 DRAWABLE_DESCR   *dm_window,    /* input/output  */
                 DRAWABLE_DESCR   *pix_subarea); /* output */

int scroll_dminput_prompt(int     scroll_chars);

char  *get_dm_prompt(void);

void ce_XBell(DISPLAY_DESCR    *dspl_descr,
              int               percent);

#endif

