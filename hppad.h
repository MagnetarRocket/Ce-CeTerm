#ifndef _PAD_INCLUDED
#define _PAD_INCLUDED

/* static char *pad_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

#include "memdata.h"

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
*  Routines in pad.c
*     pad_init              -  Allocate and initialize a pad window to a shell.
*     shell2pad             -  Reads shell output and puts it in the output pad
*     pad2shell             -  Reads pad commands and hands them to the shell
*     kill_child            -  Kill the child shell
*     vi_set_size           -  Register the screen size with the kernel
*     ce_getcwd             -  Get the current dir of the shell
*     close_shell           -  Force a close on the socket to the shell
*     dump_tty              -  Dump tty info (DEBUG)
*     hex_dump              -  Dump a memory region in hex/ascii
*     tty_echo              -  Is echo mode on ~(su, passwd, telnet, vi ...)
*  
***************************************************************/

#include "buffer.h"

#define  LINES_BEFORE_REFRESH 1

/**************************************************************
*  Defines for values returned by pad2shell
***************************************************************/
#define  PAD_SUCCESS   0
#define  PAD_FAIL     -1
#define  PAD_BLOCKED   1
#define  PAD_PARTIAL   2

/*
 *   TTY characteristics
 */

#define TTY_INT   0
#define TTY_QUIT  1
#define TTY_ERASE 2
#define TTY_KILL  3
#define TTY_EOF   4
#define TTY_START 5
#define TTY_STOP  6
#define TTY_BRK   7

/*               
 *   exported (Global) variables
 */              
                 
#ifdef _PAD_     
char ce_tty_char[16];
int  tty_echo_mode = -1;
#else
extern char ce_tty_char[];
extern int  tty_echo_mode;
#endif

#define TTY_ECHO_MODE ((tty_echo_mode >= 0) ? tty_echo_mode : (tty_echo_mode = tty_echo()))

/*
 *   Routines provided
 */

int  pad_init(char *shell, int *rfd);

int  shell2pad(BUFF_DESCR  *cursor_buff,
               PAD_DESCR   *main_window_cur_descr,
               int         *prompt_changed);

int  pad2shell(char *line, int newline);

int  kill_shell(int sig);

int  tty_reset(int tty);

int  tty_echo(void);

int  vi_set_size(int fd,      /* file descriptor of stream */
                 int lines,   /* min lines that will fit on screen */
                 int columns, /* min columns (char/line) that will fit on screen */
                 int hpix,    /* number of horizontal pixels available */
                 int vpix);   /* number of verticle pixels */

char *ce_getcwd(char *buf, int size);

void close_shell(void);

void dump_tty(void);

void hex_dump(FILE *ofd, char *header, char *line, int length);

#endif
