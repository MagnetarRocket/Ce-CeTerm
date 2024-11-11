#ifndef _PASTEBUF_H_INCLUDED
#define _PASTEBUF_H_INCLUDED

/* static char *pastebuf_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in pastebuf.c
*     open_paste_buffer      -   Open a paste buffer (unnamed, named, or file)
*     init_paste_buff        -   Initialize default paste buffer name;
*     ce_fclose              -   Close a paste buffer
*     setup_x_paste_buffer   -   allocate memory for an X paste buffer, called from dm_xc
*     ce_fgets               -   Do an fgets, or the equivalent for the X paste buffer
*     ce_fputs               -   Do an fputs, or the equivalent for the X paste buffer
*     ce_fwrite              -   Do an fwrite, or the equivalent for the X paste buffer
*     clear_pastebuf_owner   -   clear the pastebuf ownership after receiving a selection clear
*     send_pastebuf_data     -   Send the paste buffer data to another program via a property
*     more_pastebuf_data     -   Send the pieces of the paste buffer data using INCR convention
*     closeup_paste_buffers  -   Save owned X paste buffers in the pastebuf files.
*     setup_paste_buff_dir   -   Verify the paste buffer directory exists and is accessible.
*     release_x_pastebufs    -   Release ownership flags for x paste buffers.
*     get_pastebuf_size      -   Return the total number of bytes in a paste buffer.
*     same_display           -   Determine if two displays are the same
*
***************************************************************/

#include "dmc.h"
#include "buffer.h"
#include "memdata.h"

/**************************************************************
*
*  This is the special file pointer returned when the X paste
*  buffer is used.
*
***************************************************************/

#define  X_PASTEBUF_MARKER  (unsigned long)0xFEEDFEED

#define  X_PASTE_STREAM(stream) (!stream || (*((unsigned long *)stream) == X_PASTEBUF_MARKER))

/**************************************************************
*
*  These are three special paste buffer names.  Special like
*  X, X2, and the unnamed paste buffer.  They are used by
*  the bang (!) command and always refer to files, never X
*  paste buffers.  ERR_PASTEBUF is the same but used by CPO
*  for stderr,
*
*  BANG_PASTEBUF_FIRST_CHAR is there for performance.
*
***************************************************************/

#define  BANG_PASTEBUF_FIRST_CHAR  'B'
#define  BANGIN_PASTEBUF           "BangIn"
#define  BANGOUT_PASTEBUF          "BangOut"
#define  ERR_PASTEBUF              "BangErr"

#ifdef _MAIN_
char            paste_buff_dir[256] = "";
#else
extern char     paste_buff_dir[256];
#endif




/***************************************************************
*
*  Prototypes
*
***************************************************************/

FILE *open_paste_buffer(DMC               *dmc,
                        Display           *display,
                        Window              main_x_window,
                        Time               time,
                        char               escape_char);

void  init_paste_buff(char             *paste_name,
                      Display          *display);

void  ce_fclose(FILE  *paste_stream);

int   setup_x_paste_buffer(FILE           *stream,
                           DATA_TOKEN     *token,
                           int             from_line,
                           int             to_line,
                           int             rectangular_width);

char *ce_fgets(char       *str,
               int         max_len,
               FILE       *stream);

int   ce_fputs(char      *str,
                FILE      *stream);

int   ce_fwrite(char     *str,
                int       size,
                int       nmemb,
                FILE     *stream);

void  clear_pastebuf_owner(XEvent    *event);

void  send_pastebuf_data(XEvent    *event);

int more_pastebuf_data(XEvent    *event);

void closeup_paste_buffers(Display    *display);

char *setup_paste_buff_dir(char              *cmd);

void release_x_pastebufs(void);

int   get_pastebuf_size(FILE   *stream);

int   same_display(Display  *dpy1, 
                   Display  *dpy2);

#endif

