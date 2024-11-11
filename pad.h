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
*     tty_reset             -  Reset the tty
*     pad_init              -  Allocate and initialize a pad window to a shell.
*     shell2pad             -  Reads shell output and puts it in the output pad
*     pad2shell             -  Reads pad commands and hands them to the shell
*     pad2shell_wait        -  Wait for the shell to be ready for more input.
*     kill_child            -  Kill the child shell
*     vi_set_size           -  Register the screen size with the kernel
*     ce_getcwd             -  Get the current dir of the shell
*     close_shell           -  Force a close on the socket to the shell
*     dump_tty              -  Dump tty info (DEBUG)
*     hex_dump              -  Dump a memory region in hex/ascii
*     tty_echo              -  Is echo mode on ~(su, passwd, telnet, vi ...)
*     dead_child            -  Receives death of child and handles it
*     dscpln                -  Change line discepline on the fly
*     kill_utmp             -  Remove the utmp entry for this process
*     isolate_process       -  Isolate the process by making it its own process group
*  
*     send_control_c        -  WIN32 only, Send interupt to the shell
*
*     lock_display_descr    -  WIN32 only, Mutex lock the display description
*     unlock_display_descr  -  WIN32 only, Mutex unlock the display description
*     do_save_file_dialog   -  WIN32 only, Use windows dialog to get edit filename
*     do_open_file_dialog   -  WIN32 only, Use windows dialog to get edit filename
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
#ifdef WIN32
/*#define SHELL "E:\\Cygnus\\B19\\H-i386-cygwin3\\bin\\bash"*/
#define SHELL "bash"
#else
#ifdef apollo
#define SHELL "/sys5/bin/sh"
#else
#define SHELL "/bin/sh"
#endif
#endif

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
unsigned char ce_tty_char[16]; /* was signed /9/2/93 */
int  tty_echo_mode = -1;
#else
extern unsigned char ce_tty_char[];  /* was signed /9/2/93 */

extern int  tty_echo_mode;
#endif

/* RES 8/30/97 Added DOTMODE to tty_echo parms */
#define TTY_DOT_MODE  ((tty_echo_mode >= 0) ? tty_echo_mode : (tty_echo_mode = tty_echo(0, DOTMODE))) /* DOTMODE in parms.h */
#define TTY_ECHO_MODE ((tty_echo_mode >= 0) ? tty_echo_mode : (tty_echo_mode = tty_echo(1, NULL)))

/*
 *   Routines provided
 */
#ifdef WIN32
int  pad_init(char *shell, int *rfd, char *host, int login_shell,DISPLAY_DESCR   *dspl_descr);

void send_control_c(void);

void lock_display_descr(DISPLAY_DESCR *dspl_descr, char  *title);

void unlock_display_descr(DISPLAY_DESCR *dspl_descr, char  *title);

int do_open_file_dialog(char   *edit_file_ret,
                        int     size_of_edit_file_ret,
                        int     new_file_ok);

int do_save_file_dialog(char   *edit_file_ret,
                        int     size_of_edit_file_ret);

void tickle_main_thread(DISPLAY_DESCR *dspl_descr);

#else
int  pad_init(char *shell, int *rfd, char *host, int login_shell);

int  shell2pad(DISPLAY_DESCR   *dspl_descr,
               int             *prompt_changed,
               int              initial_select_needed); /* True / False */
#endif

int  pad2shell(char *line, int newline);

int  pad2shell_wait(void);

int  kill_shell(int sig);

int  tty_reset(int tty);

int  tty_echo(int echo_only, char *dotmode);

int  vi_set_size(int fd,         /* file descriptor of stream */
                 int vt100_mode, /* Flag showing vt100 mode, if false, show 1024 columns */
                 int lines,      /* min lines that will fit on screen */
                 int columns,    /* min columns (char/line) that will fit on screen */
                 int hpix,       /* number of horizontal pixels available */
                 int vpix);      /* number of verticle pixels */

char *ce_getcwd(char *buf, int size);
void ce_setcwd(char *buf); /* sun brain damage */

void close_shell(void);

void /* where prohibited */ dead_child();

void dump_tty(int fd);
void dump_ttya(int tty, char *title);
void dscpln(void);
void kill_utmp(void);
void get_stty(void);
void isolate_process(int chid);
void cp_stty(void);
void signal_handler();



/*
 *   Type for the set parameter sent to the select
 *   system call.  HP requires a funny type compared
 *   to everyone else.  This changes at HP/UX 11 and 11i.
 *   take the funny party out since HP/UX 10 is no longer supported.
 */

#if defined(_INCLUDE_HPUX_SOURCE) && !defined(_XOPEN_SOURCE_EXTENDED) && defined(blah) /* 10/14/2004 RES, blah added */
#define HT (int *)
#else
#define HT (fd_set *)
#endif


#endif
