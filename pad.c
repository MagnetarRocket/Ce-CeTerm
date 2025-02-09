/*.pad.c
/phx/bin/e pad.c;exit
*/
/*static char *sccsid = "%Z% %M% %I% - %G% %U% ";*/
#ifdef PAD
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

/***********************************************************
*
*   pad_init  -  start a shell up and set up async I/O and I/O signal
*
*      set up signal handelers, provide communications with the
*      shell, get rid of current control terminal, set up new control terminal
*
*   Note: on Apollo, all child must close fds[0] and only work with fds[1]
*                    parent closes fds[1] and only work with fds[0].
*         
*   Returns -1 on error, and child pid on success.
*
*   Parms:
*
* shell  -  The shell (including args/parms to be invoked
*
*   rfd  - The X socket passed in and the shell socket out
*
*  host  - host we display on (NULL implies not needed (No utmp_entry wanated))
*
*                                                                                
* External Interfaces:
*
*     tty_reset             -  Reset the tty
*     pad_init              -  Allocate and initialize a pad window to a shell.
*     shell2pad             -  Reads shell output and puts it in the output pad
*     pad2shell             -  Reads pad commands and hands them to the shell
*     pad2shell_wait        -  Wait for the shell to be ready for more input.
*     kill_shell            -  Kill the child shell
*     vi_set_size           -  Register the screen size with the kernel
*     ce_getcwd             -  Get the current dir of the shell
*     close_shell           -  Force a close on the socket to the shell
*     dump_tty              -  Dump tty info (DEBUG)
*     tty_echo              -  Is echo mode on ~(su, passwd, telnet, vi ...)
*     dscpln                -  Change line discepline on the fly
*     kill_utmp             -  Remove the utmp entry for this process
*     get_stty              -  Get the stty values (especially INT and EOF 
*     cp_stty               -  Cp the stty values to ultrix
*     isolate_process       -  Isolate the process by making it its own process group
*                      
* Internal Routines:     
*
*    dead_child             -  Routine called asynchronously for SIGCHLD
*    remove_backspaces      -  Remove backspaces from a line
*    print_msg              -  <DIAG> print a line instead of putting it in memdata
*    tty_getmode            -  Get the tty characteristics                         
*    tty_setmode            -  Set the tty characteristics                         
*    tty_raw                -  Put the tty in raw mode                         
*    pty_master             -  Connect to the tty master            
*    pty_slave              -  Connect to the tty slave            
*    insert_in_line         -  Insert a string into the begining of a string                         
*    get_pty                -  HP required pty routine
*    check_save             -  Check if slave is compatable with master
*    solaris_check_save     -  Check if solaris slave is compatable with master
*    pts_convert            -  Create a pts from a pty
*    build_utmp             -  Build a utmp entry for this process
*    init_utmp_ent          -  Initialize a utmp data structure.
*    dump_utmp              -  dump a utmp to stderr
*    ioctl_setup            -  set up the pty
*    shell_out_select       -  Select to look for more input from the shell
*    get_ttyname_from_child -  Wait for tty name from child process
*    null_signal_handler    -  NOOP
*
*   Optimizations:
* 
* 1  check if all #includes are needed
* 2  Buffer strm
* 3  stop flushing strm
* 4  combine pty_masters for SYS5 and regular
* 
****************************************************************************/

#define _PAD_  1

/* #define BSD_COMP */  /* get BSD compatability on Solaris */
#define itoa(v, s) ((sprintf(s, "%d", (v))), (s))
#define EOF_LINE "*** EOF ***"   /* is printed instead of '^D' */

#include <stdio.h>          /* usr/include/stdio.h      */ 
#include <string.h>         /* usr/include/strings.h    */ 
#include <errno.h>          /* usr/include/errno.h      */ 
#include <limits.h>         /* usr/include/limits.h     */ 
#include <signal.h>

#ifndef NSIG
#define NSIG 256
#endif
#include <ctype.h>
#include <fcntl.h>
#include <pwd.h>

/* moved 6/19/2001 for HP/UX 11 */
#if defined(_INCLUDE_HPUX_SOURCE) || defined(solaris)
#define  SYS5  1  
#else
#define _BSD   1   /* to get BSD stuff included on Apollo */
#endif

#define _NO_PROTO    /* DEC DEPRIVATION */
#include <sys/wait.h>
#undef  _NO_PROTO

#ifdef FREEBSD
#define USE_OLD_TTY
#  include <sys/ttydev.h>
#undef USE_OLD_TTY
#include <sys/ioctl_compat.h>
#include <sys/ttydefaults.h>
#ifndef O_EVENP
#  define O_EVENP EVENP
#  define O_ODDP  ODDP
#  define O_TAB1  TAB1
#  define O_TAB2  TAB2
#endif
/* #include <sys/termios.h> */
#endif /* freebsd */

#if defined(__alpha) || defined(ultrix)
#define DECBOX
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#ifdef solaris
#include <unistd.h>
#endif

#ifdef linux
#include <unistd.h>
#include <sgtty.h>
#include <rpcsvc/rex.h>
#define O_RAW RAW
#define O_XTABS XTABS
#define O_ECHO ECHO
#define O_CRMOD CRMOD
#define TIOCGETP TIOCGETD
#define TIOCSETP TIOCSETD
#endif

#ifndef solaris
#include <sys/ioctl.h>   /* /usr/include/sys/ioctl.h  */
#endif

#include <sys/socket.h>    /* /bsd4.3/usr/include/sys/socket.h  */
#include <sys/time.h> 
#include <utmp.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif

#ifndef FOPEN_MAX
#define FOPEN_MAX   64
#endif


#ifdef SYS5 
#include <termio.h>
#include <termios.h>
#ifndef linux
#include <sys/ttold.h>
#endif

#ifdef _INCLUDE_HPUX_SOURCE
#include <sys/bsdtty.h>
#endif

#ifndef IMAXBEL
#define IMAXBEL 0
#endif

#ifndef TCGETS
#define TCGETS TCGETA
#define TCSETS TCSETA
#endif

#ifdef linux
#define USE_XOPEN 1
#define USE_XOPEN_EXTENDED 1
#define USE_GNU 1
extern char *ptsname (int __fd) __THROW;
#endif


#include <stdlib.h>

#ifdef _INCLUDE_HPUX_SOURCE
#include <sys/ptyio.h>
#endif

/* #include <codelibs/ptyopen.h> OLD HP */

#endif  /* HPSUX */ /******************** HPSUX ****************************/

#ifndef FD_ZERO
#include <sys/select.h>    /* /usr/include/sys/select.h  */
#endif

#if defined(_INCLUDE_HPUX_SOURCE) || defined(solaris)
/*ifdef solaris RES 1/6/1999 */
#include <stropts.h>    /* for I_PUSH /usr/include/sys/stropts.h/ */
#endif

#include "cc.h"                                          
#include "debug.h"                                          
#include "dmwin.h"  
#include "getevent.h"
#include "hexdump.h"
#include "pad.h"
#include "parms.h"
/*#include "parsedm.h"*/
#include "str2argv.h"
#include "unixwin.h"  
#include "undo.h"
#include "vt100.h"  

#ifndef HAVE_STRLCPY
#include "strl.h"
#endif

char *malloc_copy(char   *str);  /* declare malloc_copy by itself here because we cannot include emalloc.h because of system include file conflicts */

#define D16 (D_BIT16 & debug)

static void remove_backspaces(DISPLAY_DESCR   *dspl_descr, char *line);
static int  print_msg(void);
static int  tty_getmode(int oldfd);
static int  tty_setmode(int newfd);
static int  tty_raw(int fd);
static int  pty_master(void);
static int  pty_slave(int master_pty_fd);
static void insert_in_line(char *line, char *text);
static char *pts_convert(char *pty);
static int  solaris_check_slave();

static int  check_slave();
static int  get_pty(void);
static int  online(char *str1, char *str2);
static void build_utmp(char *host);
static int  ioctl_setup(int master_pty_fd);
void sigusr1_hander();
static int  shell_out_select(int timeout_microseconds);
static void get_ttyname_from_child(int    shell_fd);

#if !defined(solaris) && !defined(linux)  && !defined(IBMRS6000)
char *getcwd(char *buf, int size);
#endif

char  *ttyname(int fd);

static int  fds[2];
static char msg[512];
static FILE *strm;
static int telnet_mode = 0;

static char pty_name[32];  /* master name */
static char tty_name[32];  /* slave name  */
#ifdef solaris
static char pts_name[32];  /* pts name  */
#endif

static char intr_char[3];  /* printable interupt character (usually: ^C) */

static int chid = 0;    /* shell's (child) pid [used as fork() completion flag!] */
       int pid;         /* ceterm's pid */

int tty_fd = 0;   /* file descripter of accessible control terminal {0, 1, 2, shell }  */
static int sigusr1_accepted;

extern int errno;
void  (*state)();  /* funny declare for signal handeler temp pointer */

#ifdef DEVTTY_DEBUG
void devtty(void);
#endif

#if (defined(sun) && !defined(solaris)) || defined(ultrix) /* otherwise declared in utmp.h */
struct utmp *getutid(struct utmp *utmp);
void pututline(struct utmp *utmp);
void removeutline(struct utmp *utmp);
void setutent();
void endutent();
#else
extern struct utmp *getutid();
extern void setutent();
extern void endutent();
#ifdef __alpha
extern struct utmp *pututline();
#else
/* if !defined(solaris) && !defined(_XOPEN_SOURCE_EXTENDED) RES 5/6/98 */
#if !defined(solaris) && !defined(_INCLUDE_HPUX_SOURCE)
extern struct utmp *pututline();
#endif /* solaris */
#endif /* alpha */
#endif /* !SVR4 */
    
int utmp_setup();

struct utmp utmp;
int    utmp_set = False;

void init_utmp_ent(struct utmp *utptr, int type, char *host);
#ifdef DebuG
void dump_utmp(struct utmp *utmp, char *header);
#endif
#if defined(SYS5) && defined(DebuG)
void dump_ttya(int tty, char *title);
#endif

#ifndef NTTYDISC
#define NTTYDISC 2
#endif

#if defined(_INCLUDE_HPUX_SOURCE) || defined(solaris) || defined(linux)
/* #ifdef solaris RES 1/6/1999 */
int slave_tty_fd = -1;
#endif

/**********************************************************************/
/*************************** MAIN *************************************/
/**********************************************************************/

int pad_init(char *shell, int *rfd, char *host, int login_shell)
{

int i;
#if defined(DSCPLN) || defined(DebuG) || defined(ultrix)
int ldisc = NTTYDISC;  
#endif
int rc = -1;
int lcl_argc;

char *ptr;
char **lcl_argvp;
char *p;

DEBUG16(fprintf(stderr, "\nPAD_INIT(%s,%d,%s,%d):uid = %d, euid = %d\n", (shell ? shell : "<NULL>"), *rfd, (host ? host : "<NULL>"), login_shell, getuid(), geteuid());)

#ifdef solaris
if (((tty_fd = open("/dev/tty", O_RDWR, 0)) >= 0) ||
    ((tty_fd = open("/dev/console", O_RDWR, 0)) >= 0)){
       DEBUG16(dump_ttya(tty_fd, "Before fork, Open of /dev/tty or /dev/console");)
       rc = tty_getmode(tty_fd);
       close(tty_fd);
       DEBUG16(fprintf(stderr, "tty_getmode rc for /dev/[console|tty] = %d\n", rc);)
}else
       DEBUG16(fprintf(stderr, "(1)Can not open /dev/[tty/console]! (%s)\n", strerror(errno));)
#endif

if ((fds[0] = pty_master()) < 0){
    dm_error("Could not open pseudo-terminal.", DM_ERROR_LOG);
    return(-1);
}

tty_fd = fds[0]; /* 7/18/94 SOLARIS SU problem */

#if defined(__alpha)
if ((tty_fd = open("/dev/console", O_RDWR)) >= 0) 
     rc = tty_getmode(tty_fd);
else
    DEBUG16(fprintf(stderr, "Can not open /dev/console (%s)\n", strerror(errno));)
#endif

if (rc < 0)
    rc = tty_getmode(tty_fd); 

if (rc < 0){
    DEBUG16(fprintf(stderr, "first tty_getmode on fd %d failed rc = %d\n", tty_fd, rc);)
    tty_fd = 0;
    while (tty_fd < 32){
           if ((rc = tty_getmode(tty_fd)) >= 0) break;
           tty_fd++;
    }
}

if (rc < 0){ 
    DEBUG(fprintf(stderr, "Can't get tty attributes, attempting defaults! (%s)\n", strerror(errno));)   
    tty_fd = fds[0]; /* try anyway */
}

DEBUG16(fprintf(stderr, "ttyfd=%d\n", tty_fd);)

get_stty();
cp_stty();

#ifdef SIGCLD           /* apollo has CLD and CHLD backwrds */
#if (SIGCLD != SIGCHLD)
signal(SIGCLD, dead_child);                               
#endif
#endif

#ifdef SIGCHLD
signal(SIGCHLD, dead_child);                              
#endif

/* signal handler used in child, signal send by parent */
sigusr1_accepted = False;
#ifdef USE_BSD_SIGNALS   /* sunos 4.1.* and linux only */
signal(SIGUSR1, sigusr1_hander);
#else
sigset(SIGUSR1, sigusr1_hander);
#endif

#ifdef SIGUSR2
signal(SIGUSR2, SIG_IGN);
#endif

#ifdef SIGTTOU
signal(SIGTTOU, dead_child); /* DEC_ULTRIX goes infinite otherwise! */                             
#endif 

#if defined(SIGTTIN) && defined(DebuG)
signal(SIGTTIN, signal_handler); /* Solaris Backgrnd contrl terminal (SU)!T */                             
#endif 

#if !defined(ultrix) && !defined(SYS5)
if (tty_raw(tty_fd) < 0){
    if (tty_raw(fds[0]) < 0){
        DEBUG16(snprintf(msg, sizeof(msg), "Can't set raw mode(%s))", strerror(errno));   
        dm_error(msg, DM_ERROR_LOG);)
    }
}
#endif

/*
 * See if we are requested to do utmp processing, if so, see if we have
 * permission to do the utmp processing.  If not, cancel utmp processing.
 */

if (host)
   if (utmp_setup() != 0)
       host = NULL;

pid = getpid();

#if defined(ultrix)
fds[1] = ioctl_setup(tty_fd); /* on all other boxes, executed in child! */
#endif

if ((chid = fork()) < 0){ 
     snprintf(msg, sizeof(msg), "Can't fork when preparing to start shell (%s)", strerror(errno));   
     dm_error(msg, DM_ERROR_LOG);
     return(-1);
}
 
if (chid){  /***********************************************/ 

    /*
     * parent which returns to ceterm
     */

#ifdef blah_INCLUDE_HPUX_SOURCE
     /* KBP 8/11/95 Fixes HP problem where terms did not come up correctly */
     nice(39);  usleep(1);
#endif

     DEBUG16( fprintf(stderr, " @Pad_init, PID=%d, CHPID, %d\n", pid, chid);)

     *rfd = fds[0];

     strm = fdopen(fds[0], "w");  

     if (!strm){
          snprintf(msg, sizeof(msg), "Can't fdopen(stream(%s))", strerror(errno));   
          dm_error(msg, DM_ERROR_LOG);
          return(-1);
     }

/*   if (strm)  setvbuf(strm, NULL, _IONBF, 0); */    

#ifdef SYS5
     if (tty_reset(tty_fd)) 
         if (tty_reset(fds[0])){
             DEBUG16(snprintf(msg, sizeof(msg), "Can't reset tty(%s)", strerror(errno));   
             dm_error(msg, DM_ERROR_LOG);)
         }
#endif

     DEBUG25(
        for (i=1; i<256; i++){
             errno = 0;
             state = signal(i, signal_handler);
             if (errno == EINVAL) continue;
             if (state != SIG_DFL){
                 signal(i, state); /* reset it */
                 fprintf(stderr, "Signal already trapped: %d\n", i); 
             } /* else  fprintf(stderr, "Grabbing unallocated signal: %d\n", i); */
        }  /* for */
     )  /* debug */
    
#ifdef FIONBIO
     i = 1;
     ioctl(fds[0], FIONBIO, &i);
#endif

/* #ifdef _INCLUDE_HPUX_SOURCE   */
#if defined(FIONOBLK) && defined(SYS5)
     DEBUG16(fprintf(stderr, "Gonna try FIONOBLK!\n");)   
     {int on=1;ioctl(fds[0], FIONOBLK, &on);}
#endif

     get_ttyname_from_child(fds[0]);

#ifdef blah_INCLUDE_HPUX_SOURCE
     nice(0); 
#endif
     return(chid);

}  /* end parent **********************************************/

/* 
 *  Child (Which is the shell being invoked)
 */

#ifdef linux
/* RES 12/16/2002 clear this signal right away */
#ifdef SIGCHLD
signal(SIGCHLD, SIG_DFL);                              
#endif
#endif

close(*rfd);   /* close off X so we don't ioctl its brains out */

chid = getpid();  /* chid now has the same value in the parent and child */

for (i=1; i<NSIG; i++)   /* reset all signal handelers */
     if (i != SIGUSR1)
        signal(i, SIG_DFL);

#if !defined(ultrix)
fds[1] = ioctl_setup(fds[0]);
#endif

#if defined(_INCLUDE_HPUX_SOURCE) || defined(solaris)
/*ifdef solaris  RES 1/6/1999 */
close(fds[0]); /* makes /dev/ptmx->/dev/tty->getpass(2)->su(1) work! */
#endif

cc_ttyinfo_send(fds[1]);

/* wait for main task to register */
DEBUG16(fprintf(stderr, "CHILD:  Pausing while main task registers\n");)
#ifdef USE_BSD_SIGNALS   /* sunos 4.1.* and linux only */
if (!sigusr1_accepted)
   pause();
#else
sighold(SIGUSR1);
while(!sigusr1_accepted)
   sigpause(SIGUSR1);
#endif

i = char_to_argv(NULL, shell, "ceterm", shell, &lcl_argc, &lcl_argvp, '\\' /* unix always uses backslash as an esacpe char */);
ptr = strrchr(lcl_argvp[0], '/');
strlcpy(shell, lcl_argvp[0], MAX_LINE+1);
if (ptr){
    if (login_shell){
        lcl_argvp[0] = ptr;
        *ptr = '-';
    }else
        lcl_argvp[0] = ptr + 1;
       
}else{
    if (login_shell){
        snprintf(msg, sizeof(msg), "-%s", lcl_argvp[0]);
        free(lcl_argvp[0]);
        lcl_argvp[0] = malloc_copy(msg);
    }
}

if (i < 0){
      fprintf(stderr, "Cannot process %s\n", shell);
      exit(1);
}

#ifndef SYS5
DEBUG16(
ioctl(tty_fd, TIOCGETD, (char *)&ldisc);    
fprintf(stderr, "Line discip(%d) = '%d'\n",tty_fd, ldisc);
ioctl(fds[1], TIOCGETD, (char *)&ldisc);  
fprintf(stderr, "Line discip(tty) = '%d'\n", ldisc);
)
#endif

#ifdef DSCPLN
ptr = strrchr(lcl_argvp[0], '/');
if (!ptr) ptr = lcl_argvp[0];
if (!strcmp("csh", ptr)){
   ldisc = NTTYDISC;
   ioctl(fds[1], TIOCSETD, (char *)&ldisc);  /* new tty discipline */
}else{
   ldisc = OTTYDISC;
   DEBUG16(fprintf(stderr, "Line discip(tty)set to: '%d'\n", ldisc);)
   ioctl(fds[1], TIOCSETD, (char *)&ldisc);  /* old tty discipline */
}
#endif

#ifdef ultrix
   ldisc = NTTYDISC;
   DEBUG16(fprintf(stderr, "Line discip(tty)set to: '%d'\n", ldisc);)
   ioctl(fds[1], TIOCSETD, (char *)&ldisc);  /* old tty discipline */
#endif

if (host) 
   build_utmp(host);

cp_stty();

#ifdef linux
/***************************************************************
*  RES 2/22/1999
*  In Linux, make sure blocking is turned on for the shell.  
*  Otherwise, shell writes ahead and line discipine throws away
*  output.
***************************************************************/
i = fcntl(fds[1], F_GETFL);
if (i == -1)
   DEBUG16(fprintf(stderr, "Could not fcntl fd %d  in child to F_GETFL (%s)\n", fds[1], strerror(errno));)
else{
    i &= ~O_NONBLOCK;
    if (fcntl(fds[1], F_SETFL, i) == -1)
       DEBUG16(fprintf(stderr, "Could not fcntl fd %d  in child to turn off O_NONBLOCK (%s)\n", fds[1], strerror(errno));)
}
#endif

#ifdef sun
nice(1);  /* acctually helps the sun a great deal, but slows apollo a lot */
#endif

DEBUG16( 
fprintf(stderr, " Going to shell: '%s'\n", shell);
for (i=0; lcl_argvp[i] ; i++)
    fprintf(stderr, " argv[%d]='%s'\n", i, lcl_argvp[i]);
)

if (getuid() && !geteuid()){
    DEBUG16(fprintf(stderr, "Setting uid to %d in child %d\n", getuid(), chid);)
    setuid(getuid());
}

DEBUG16(
fprintf(stderr, "fd[0]= open(%s)\n", tty_name);
)

close(0);     /* stdin becomes my output */
close(1);     /* stdout becomes my input */
close(2);     /* stderr become my input */

#ifdef SYS5
open(tty_name, O_RDWR); /* HPTERM */
/* open(tty_name, O_RDONLY); 7/10/97 tsim recommendataion */  /* HPTERM */
if ((dup(fds[1]) != 1) || (dup(fds[1]) != 2)){
#else
if ( (dup(fds[1]) != 0) || (dup(fds[1]) != 1) || (dup(fds[1]) != 2)){
#endif
    dm_error("Dup failed!, can not exec shell.\n", DM_ERROR_LOG);
    exit(1);
}

if ((char *)getenv(DMERROR_MSG_WINDOW) != NULL)
   {
      snprintf(msg, sizeof(msg), "%s=", DMERROR_MSG_WINDOW);
      p = malloc_copy(msg);
      if (p)
         putenv(p);
   }

/* RES 8/10/95 added env variable */
snprintf(msg, sizeof(msg), "CE_SH_PID=%d", chid);
p = malloc_copy(msg);
if (p)
   putenv(p);


for (i = 3; i < FOPEN_MAX; i++)
    close(i); 

#ifdef  BLAH
#ifdef FIONBIO
     i = 0;
     fprintf(stderr, "FIONBIO!\n");
     ioctl(0, FIONBIO, &i);
#endif
#endif

execvp(shell, lcl_argvp); 

dm_error("Exec failed!", DM_ERROR_LOG); /* should never get here */
_exit(1);
return(-1); /* make lint happy */
} /* pad_init() */

/****************************************************************** 
*
*  devtty -  Determine if the tty cloning device still works
*
******************************************************************/
#ifdef DEVTTY_DEBUG
void devtty()
{
FILE *ttyfp;char ttyl[1024]; 
ttyfp = fopen("/dev/tty", "r");
if (!ttyfp){ fprintf(stderr, "Could not open /dev/tty!\n");return;}
if (ttyfp && !fgets(ttyl, 1024, ttyfp)){ perror("dt");fprintf(stderr, "Could not read /dev/tty!\n"); return;}
if (ttyfp) fclose(ttyfp);
fprintf(stderr, "/dev/tty OK!\n");
}  /* devtty() */
#endif

/****************************************************************** 
*
*  sigusr1_hander -  Routine called to stop the pause in the child
*
******************************************************************/

 /*ARGSUSED*/
void sigusr1_hander(int sig){
sigusr1_accepted = True;
}

/****************************************************************** 
*
*  dead_child -  Routine called asynchronously by shell output
*
******************************************************************/

void dead_child(int sig)
{ 

int wpid;

int status_data;

 /*
  *  On the ibm, the signal catcher has to be reset .
  */

DEBUG16( fprintf(stderr, "@dead_child signal=%d, current pid = %d\n", sig, getpid());)

#ifdef SIGTTOU
if (sig == 22) signal(SIGTTOU, SIG_IGN); /* DEC_ULTRIX goes infinite otherwise! */                             
#endif 

#ifdef solaris
wpid = wait(&status_data);
#else
wpid = wait3(&status_data, WNOHANG, NULL);
            /* NOOP */ ;
#endif

if (!wpid)
    return;  /* false alarm! */

#ifdef  DebuG
if (debug & D_BIT16 ){
   fprintf(stderr, "Process [%d] died!, shell is:%d, signal was:%d, status:0x%08X\n", wpid, chid, sig, status_data);
#ifndef IBMRS6000
   if (wpid < 0)
      fprintf(stderr, "Wait terminated by signal (%s)\n", strerror(errno));
   else
      if (WIFEXITED(status_data))
         fprintf(stderr, "Process exited, code %d\n", WEXITSTATUS(status_data));
      else
         if (WIFSIGNALED(status_data))
            fprintf(stderr, "Process signaled, signal code %d\n", WTERMSIG(status_data));
         else
            if (WIFSTOPPED(status_data))
               fprintf(stderr, "Process stopped, signal code %d\n", WSTOPSIG(status_data));
            else
#ifdef WIFCONTINUED
               if (WIFCONTINUED(status_data))
                  fprintf(stderr, "Process continued, status = %d\n", status_data);
               else
#endif
                  fprintf(stderr, "Unknown cause of termination status = %d\n", status_data);

#ifdef WCOREDUMP
     if (WCOREDUMP(status_data))
        fprintf(stderr, "Core was dumped\n");
#endif
#endif
}
#endif

#ifdef ultrix
if ((wpid != chid) && (sig != 22) && chid){
#else
if ((wpid != chid) && chid){   /* !chid means parent did not wake up yet */
#endif
    DEBUG16( fprintf(stderr, "Something happened, but it was not shell %d dying, it was %d !\n", chid, wpid);)
    kill_unixcmd_window(0);  /* 0="Not real kill */
}else{
   DEBUG16(if (!chid) fprintf(stderr, "chid=0 assuming child died before parent returned from fork.\n");)
   kill_unixcmd_window(1);
   kill(-chid, SIGHUP);
}

}  /* dead_child() */

/****************************************************************** 
*
*  kill_shell -  Routine terminates child
*
******************************************************************/

int kill_shell(int sig)
{ 
int rc;

DEBUG16( fprintf(stderr, "@kill_child (%d) signal:%d\n", chid, sig);)

#ifdef blah
if (fds[0])
    close(fds[0]);
if (fds[1])
    close(fds[1]);
#endif

rc = kill(chid, sig); 

if (rc){
    DEBUG16(fprintf(stderr, "Can't signal process: %s", strerror(errno));)   
    return(rc);
}

return(0);

} /* kill_shell() */

/****************************************************************** 
*
*  shell2pad - Reads shell output and puts it in the output pad
*              returns number of lines read from shell or -1
*              on i/o error from the shell socket.
*
******************************************************************/
#define MAX_BLOCKS_PER_CALL 10

int  shell2pad(DISPLAY_DESCR   *dspl_descr,
               int             *prompt_changed,
               int              initial_select_needed)   /* True / False */
{

int rc;
int lines = 0;
char *ptr, *lptr;
char *bptr, *cptr;
int block_count = 0;
int bytes = 1;
static char *save_vt100;
int preread_data = False;


#ifdef blah
fd_set rfds_bits;
struct timeval time_out;
#endif

char color_line[MAX_LINE+1];
char buff[MAX_LINE+1];
char buff_out[(MAX_LINE*2)+1];

*prompt_changed = 0;

/* RES 12/22/1998 added for dq -f, flush to/from shell */
if (initial_select_needed)
   if ((bytes = shell_out_select(20000)) <= 0)
       return(bytes);

#ifndef solaris
DEBUG16(
   if (ioctl(fds[0], FIONREAD, &bytes) < 0){ 
      snprintf(msg, sizeof(msg), "Can't ioctl(FIONREAD[1](%s))", strerror(errno));   
      dm_error(msg, DM_ERROR_LOG);
      return(-1);
   }
   fprintf(stderr, " @S2P#1 '%d' bytes to read: \n", bytes);
   bytes = 1;  /* just incase the FIONREAD Fails (happens on DEC!) */
)
#endif

/* vtmode ? */
if (dspl_descr->vt100_mode && bytes){
  errno = 0;
  while(((rc = read(fds[0], buff, MAX_LINE)) < 0) && (errno == EINTR)){
      DEBUG16(fprintf(stderr, "Read[1] interupted, retrying\n");) /* Retry reads when an interupt occurs */
  }      
  if (rc < 0) return(rc);
  buff[rc]='\0';  
  tty_echo_mode = -1; /* invalidate known echo mode */
  DEBUG16(hex_dump(stderr, "VT100 data", (char *)buff, rc);)
  if (!autovt_switch()) {
     vt100_parse(rc, buff, dspl_descr);
     *prompt_changed = True;
     return(0);
  }
  else {
     preread_data = True;
     DEBUG16(fprintf(stderr, "Switching out of VT100 mode: len %d\n", rc);)
  }
}  /* vt100 mode */

while (bytes){
   errno = 0;
   if (!preread_data)
       while(((rc = read(fds[0], buff, MAX_LINE)) < 0) && (errno == EINTR)){
           DEBUG16(fprintf(stderr, "Read[1] interupted, retrying\n");) /* Retry reads when an interupt occurs */
       }
   if ((rc < 0) && (errno == EAGAIN))
      {
         DEBUG16(fprintf(stderr, "Read[1] - no more data\n");)
         break; /* no more data (non-blocking I/O) */
      }
   if (rc < 0){
       DEBUG16(fprintf(stderr, "Can't read from shell (%s)%d\n", strerror(errno), errno);)
       if (tty_reset(tty_fd)) 
           if (tty_reset(fds[0])){
               DEBUG16(snprintf(msg, sizeof(msg), "Can't reset tty(%s)", strerror(errno));   
               dm_error(msg, DM_ERROR_LOG);)
           }
       /* return(lines); */  /* would patch around (kludge) select() fail bug in sun */
       kill_unixcmd_window(1);
       kill(-chid, SIGHUP);
       return(rc);
   }

   preread_data = False;
   buff[rc]='\0';  
   if (rc > 0) /* show wait_for_input that we read something even if backspaces kill if off */
       *prompt_changed = True;
 
   DEBUG16(hex_dump(stderr, "S2P-IO:", (char *)buff, rc);)


   if ((buff[0] == VT_ESC) || ((rc < 100) && strchr(buff, VT_ESC))){
       tty_echo_mode = -1; /* invalidate known echo mode */
       if (autovt_switch()){
           DEBUG16(fprintf(stderr, "Switching to VT100 mode: len %d\n", rc);)
           if (save_vt100){
              vt100_parse(strlen(save_vt100), save_vt100, dspl_descr);
              free(save_vt100);
              save_vt100 = NULL;
           }
           vt100_parse(rc, buff, dspl_descr);
           *prompt_changed = True;
           return(0);
       }
       else{
           ptr = strchr(buff, VT_ESC);
           if (!save_vt100)
               save_vt100 = malloc_copy(ptr);
           else{
               strlcpy(buff_out, save_vt100, sizeof(buff_out));
               strlcat(buff_out, ptr, sizeof(buff_out));
               free(save_vt100);
               save_vt100 = malloc_copy(buff_out);
           }
       }
   }
   else
      if (save_vt100){
          free(save_vt100);
          save_vt100 = NULL;
      }

   if (MANFORMAT){
      remove_backspaces(dspl_descr, buff); 
      vt100_eat(buff, buff); /* does work in place */
   }else
      remove_backspaces(dspl_descr, buff); 

   DEBUG16(fprintf(stderr, "I/O is: [%d] bytes:'%s'\n", rc, buff);)

   /* get the prompt and any pre-newline stuff and write it out */

   ptr = get_unix_prompt();
   strlcpy(buff_out, ptr, sizeof(buff_out));
   if (*ptr)
       *prompt_changed = 1;

   ptr = buff;
   lptr = "";
   while (ptr){

       bptr = ptr;
       while (bptr = strchr(bptr, 07)){  /* process bells */
             ce_XBell(dspl_descr, 0);
             cptr = bptr;
             while (*cptr){  /* remove the bell */
                    *cptr = *(cptr+1); 
                     cptr++;
             }
             bptr++;
       }

       if (ptr[0] == ce_tty_char[TTY_EOF]) /* EOF on line */
           insert_in_line(ptr, EOF_LINE);

       if (ptr[0] == ce_tty_char[TTY_INT])  /* Interupt on line */
           insert_in_line(ptr, intr_char);

       lptr = ptr;
       ptr = strchr(ptr, '\n');

       if (ptr){     /* we got (mo)  data [multiple lines]*/
           *ptr = '\0';
           ptr++;
           if (buff_out[0]){
               strcat(buff_out, lptr);
               lptr = buff_out;
           }
                 
           if (!ceterm_prefix_cmds(dspl_descr, lptr))
              {
                 /* RES 7/8/2003 Added call to process vt100 color lines returned from Linux 'ls' command */
                 if (vt100_color_line(dspl_descr, lptr, color_line, sizeof(color_line)) != False)  /* in vt100.c */
                    {
                       put_line_by_num(dspl_descr->main_pad->token, total_lines(dspl_descr->main_pad->token) - 1, lptr, INSERT);
                       put_color_by_num(dspl_descr->main_pad->token, total_lines(dspl_descr->main_pad->token) - 1, color_line);
                    }
                 else
                    put_line_by_num(dspl_descr->main_pad->token, total_lines(dspl_descr->main_pad->token) - 1, lptr, INSERT);
#define linelim
#ifdef linelim
                 if (dspl_descr->linemax)
                    while (total_lines(dspl_descr->main_pad->token) > dspl_descr->linemax)
                        delete_line_by_num(dspl_descr->main_pad->token, 0, 1);
#endif
    
                 lines++;
                 buff_out[0] = '\0';
              }
           else /* RES 8/28/96 added to allow prefix commands in middle of line */
              if (*lptr)
                 strlcpy(buff_out, lptr, sizeof(buff_out));
              else
                 buff_out[0] = '\0';

       }  /* if (mo data ) */

   }  /* while (data) */

   block_count++;
        
   if (*lptr){   /* do we need to set a prompt */
       strcat(buff_out, lptr);
       set_unix_prompt(dspl_descr, buff_out);
       *prompt_changed = 1;
       if (online(buff_out, "pas") && (tty_echo(0, DOTMODE),telnet_mode)){ /* Royal kloodge!!! */
           DEBUG16(fprintf(stderr, "s2p: Forcing tty_echo_mode to False\n");)
           tty_echo_mode = 0;
           telnet_mode = 0;
       }else
           tty_echo_mode = -1; /* invalidate known echo mode */
   }else{
       set_unix_prompt(dspl_descr, NULL);
       tty_echo_mode = -1; /* invalidate known echo mode */
   }

   if (block_count > MAX_BLOCKS_PER_CALL)
       break;

#ifndef linux
   DEBUG30(break;)  /* skips usleep() when in debugger (DDE) */
   usleep(2000); 

   bytes = shell_out_select(2000);
#else
   bytes = shell_out_select(2000);
#endif
   if (bytes < 0)
       return(-1);

#ifdef blah
   FD_ZERO(&rfds_bits);
   FD_SET(fds[0], &rfds_bits);
   time_out.tv_sec = 0;
   time_out.tv_usec = 2000;
   while ((bytes = select(fds[0]+1, HT &rfds_bits, NULL, NULL, &time_out)) == -1) 
         if (errno != EINTR)  /* on an interupted select, try again */
              break;
   if (bytes < 0){
       snprintf(msg, sizeof(msg), "Select() failed(%s))", strerror(errno));   
       dm_error(msg, DM_ERROR_LOG);
       return(-1);
   }

   DEBUG16(fprintf(stderr, " @S2P#2 data %sReady to read: \n", bytes ? "" : "Not ");)
#endif

}   /* while bytes */

return(lines);

}  /* shell2pad() */

/****************************************************************** 
*
*  shell_out_select - Check for data waiting to be written to the
*                     pad.
*  RETURNS:  -1     on error
*            0      on select timeout
*            >0     return code from select (nfds)
*
******************************************************************/

static int  shell_out_select(int timeout_microseconds)
{
int    nfds;
fd_set rfds_bits;
struct timeval time_out;

FD_ZERO(&rfds_bits);
FD_SET(fds[0], &rfds_bits);

time_out.tv_sec = timeout_microseconds / 1000000;
time_out.tv_usec = timeout_microseconds % 1000000;

while ((nfds = select(fds[0]+1, HT &rfds_bits, NULL, NULL, &time_out)) == -1) 
      if (errno != EINTR)  /* on an interupted select, try again */
           break;

if (nfds < 0){
   snprintf(msg, sizeof(msg), "Select() failed(%s))", strerror(errno));   
   dm_error(msg, DM_ERROR_LOG);
}

DEBUG16(fprintf(stderr, " @S2P-select TO(%d) data %sReady to read: \n", timeout_microseconds, ((nfds > 0) ? "" : "Not "));)

return(nfds);

}  /* shell_out_select() */


/****************************************************************** 
*
*  get_ttyname_from_child - In main process wait for tty name from
*                           child process.
*
******************************************************************/
                
static void get_ttyname_from_child(int    shell_fd)
{
fd_set    rfds_bits;
int       count;
char      buff[MAX_LINE+1];
int       rc;
char     *ptr;
char     *tty_name_start;
int       done = False;
struct timeval time_out;

while (!done){
    FD_ZERO(&rfds_bits);
    FD_SET(shell_fd, &rfds_bits);
    time_out.tv_sec = 5;
    time_out.tv_usec = 0;
    while ((count = select(shell_fd+1, HT &rfds_bits, NULL, NULL, &time_out)) < 0) 
            if (errno != EINTR){  /* on an interupted select, try again */
                DEBUG16(fprintf(stderr, "get_ttyname_from_child: Error in select (%s)\n", strerror(errno));)
                return;
            }

    if (count > 0) { /* something was selected */
        errno = 0;
        while(((rc = read(shell_fd, buff, MAX_LINE)) < 0) && (errno == EINTR)){
             DEBUG16(fprintf(stderr, "ttyinfo read interupted, retrying\n");) /* Retry reads when an interupt occurs */
        }      
        if (rc < 0){
            DEBUG16(fprintf(stderr, "get_ttyname_from_child: Error in read (%s)\n", strerror(errno));)
            return;
        }
        buff[rc]='\0';  
        DEBUG16(hex_dump(stderr, "tty processing", (char *)buff, rc);)
        tty_name_start = strchr(buff, CC_TTYINFO_CHAR);
        if (tty_name_start){
            ptr = strchr(tty_name_start, '\n');
            if (ptr){
               *ptr = '\0';
               if (*(ptr-1) == '\r')
                   *(ptr-1) = '\0';
            }
            strlcpy(tty_name, tty_name_start+1, sizeof(tty_name));
            done = True;
        } /* tty_name_start is not NULL */
    } else {
       /* must be zero, negative case handled in while loop */
       DEBUG16(fprintf(stderr, "ttyinfo select timeout\n");)
       return;
    }
} /* while not done */

#if defined(_INCLUDE_HPUX_SOURCE) || defined(solaris) || defined(linux)
/*#ifdef solaris RES 1/6/1999 */
    slave_tty_fd = open(tty_name, O_RDWR, 0);
    DEBUG16(
         if (slave_tty_fd < 0) 
             fprintf(stderr, "Slave open from parent failed! (%s)\n", strerror(errno));
         else
             fprintf(stderr, "Slave open from parent succeded!\n", strerror(errno));
    )
#endif
    cc_ttyinfo_receive(&buff[1]);
} /* get_ttyname_from_child() */


/****************************************************************** 
*
*  pad2shell - Write to the shell. If the shell can not take all,
*              buffer it until next call, if the shell did not take
*              any, return:
*
*                   1 (try again another day), 
*                  -1 error;
*                   0 successfull line sent
*
******************************************************************/

static char p2s_buf[MAX_LINE] = "";
int p2s_len = 0;

int pad2shell(char *line, int newline)
{

int rc, len;
char *ptr1;
char *ptr2;
char MEOF = EOF;
char buf[MAX_LINE];

DEBUG16(
if (!p2s_len) p2s_buf[0] = '\0';
if (*line)
    fprintf(stderr, " @pad2shell([%d]%s)nl:%s[p2sbuf: '%s']\n", *line, line, (newline ? "True" : "False"), p2s_buf);
else
    fprintf(stderr, " @pad2shell(NULL[FLUSH buffers])\n");
)

/*
 * Have we got buffered stuff from last time ?
 */

if (line && 
    (line[0] == ce_tty_char[TTY_INT]) &&
    (line[1] == '\0')){
      DEBUG16(fprintf(stderr, "INTERUPT!\n");)
      p2s_len = 0;
      p2s_buf[0] = '\0';
}

errno = 0;
if (p2s_len){   /* we have buffered stuff from last time */
    rc = write(fds[0], p2s_buf, p2s_len); 
    DEBUG16(if (errno) fprintf(stderr, "ERRNO: '%s'\n",  strerror(errno));)
    if ((rc == -1) && (errno == EWOULDBLOCK)) rc = 0; /* apollo bug */
    if (rc == -1){
        DEBUG16(fprintf(stderr, "Can't write to shell (%s)[1]\n", strerror(errno));)
        if (tty_reset(tty_fd)) 
            if (tty_reset(fds[0])){
                DEBUG16(snprintf(msg, sizeof(msg), "Can't reset tty(%s)", strerror(errno));   
                dm_error(msg, DM_ERROR_LOG);)
            }
        kill_unixcmd_window(1);
        kill(-chid, SIGHUP);
        return(PAD_FAIL);
    }
    if (strm) fflush(strm);
    DEBUG16( fprintf(stderr, "Old data written %d of %d\n", rc, p2s_len);)
    p2s_len -= rc;
    if (!p2s_len) p2s_buf[0] = '\0';
    if (p2s_len){       /* socket still full */
        ptr1 = p2s_buf;        
        ptr2 = p2s_buf + rc;        
        while (*ptr2)         /* shift down any that did copy */
               *(ptr1++) = *(ptr2++); 
        *ptr1 = '\0';
        DEBUG16( fprintf(stderr, "Overflow of %d\n", p2s_len);)
        return(PAD_BLOCKED);
    }
}  /* old buffered data */

if (line && *line == MEOF){ /* EOF request */
   buf[0] = ce_tty_char[TTY_EOF]; buf[1] = '\0';
   DEBUG16( fprintf(stderr, "Writing'EOF':x%02X\n", buf[0]);)
   errno = 0;
   rc =  write(fds[0], buf, 1); 
   DEBUG16(if (errno) fprintf(stderr, "ERRNO: '%s'\n",  strerror(errno));)
   if ((rc == -1) && (errno == EWOULDBLOCK)) rc = 0; /* apollo bug */
   if (rc == -1){
       snprintf(msg, sizeof(msg), "Can't write to shell (%s)[2]", strerror(errno));
       dm_error(msg, DM_ERROR_LOG);  
       return(PAD_FAIL);
   }
   return(PAD_SUCCESS); 
}

/*
 * Lets copy as much as we can of the new line
 */

if (!line) return(PAD_SUCCESS);  /* flush request */

strlcpy(buf, line, sizeof(buf));
if (newline){
    if (setprompt_mode())
       strcat(buf, "\n"); /* Fix setprompt mode in slim 11/18/97 */
    else
       strcat(buf, "\r"); /* fixed tandem vt -on 6/6/96 might break others! */
}
len = strlen(buf);

/* #define tandembug
#ifdef tandembug
DEBUG16(fprintf(stderr, "Tandem! \n");)
if (ptr1 == strrchr(buf, 0x0A) *ptr1 = 0x0D;
#endif  */

errno = 0;
rc = write(fds[0], buf, len); 
DEBUG16(if (errno) fprintf(stderr, "ERRNO: '%s'\n",  strerror(errno));)
if ((rc == -1) && (errno == EWOULDBLOCK)) rc = 0; /* apollo bug */
DEBUG16( fprintf(stderr, "Wrote %d char of %d \"%s\"\n", rc, len, buf);)

if (strm) fflush(strm);

if (rc == -1){
    snprintf(msg, sizeof(msg), "Can't write to shell (%s)[3]", strerror(errno));
    dm_error(msg, DM_ERROR_LOG);  
    return(PAD_FAIL);
}

p2s_len = len - rc;

if (!p2s_len) p2s_buf[0] = '\0';
if (p2s_len){         /* did not write everything */
    DEBUG16( fprintf(stderr, "Not every thing wrote %d of %d\n", rc, len);)
    /* if (!rc) return(PAD_BLOCKED); 3/2/94 p2s_len should be set to 0 */ /* failed, blocked socket */
    strlcpy(p2s_buf, buf + rc, sizeof(p2s_buf));
    return(PAD_PARTIAL);
}

return(PAD_SUCCESS);

} /* pad2shell */

/****************************************************************** 
*
*  pad2shell_wait - Wait for the shell to be ready for more input.
*
*                   0 select timed out
*                  -1 error;
*                   1 shell is ready for more input
*
******************************************************************/

int pad2shell_wait(void)
{
fd_set wfds_bits;
struct timeval time_out;
int nfound;

FD_ZERO(&wfds_bits);
FD_SET(fds[0], &wfds_bits);
time_out.tv_sec = 2;
time_out.tv_usec = 0;

nfound = select(fds[0]+1, NULL, HT &wfds_bits, NULL, &time_out);

return(nfound);

} /* pad2shell_wait */

/****************************************************************** 
*
*  remove_backspaces - removes backspaces and carage returns
*                      does not let a person backup over a \n or
*                      begining of buffer
*
******************************************************************/
static int last_char_was_cr = 0;
static int last_char_was_blank = 0;

static void remove_backspaces(DISPLAY_DESCR   *dspl_descr, char *line)
{

char *bptr, *tptr;
char *last_0a = NULL;
       static char special_string2[] = { 0x02, 0x02, 0 };

/* RES 9/12/95 special ce_isceterm check */
if ((line[0] == 0x02) && (line[1] == 0x02) &&
    (line[2] == 0x08) && (line[3] == 0x08)){
    DEBUG16(fprintf(stderr, "ce_isceterm_chk: special string matched(0)\n");)
    pad2shell(special_string2, False);
}

bptr = tptr = line;
if (last_char_was_cr && *bptr != '\n')
   {
      DEBUG16(fprintf(stderr, "remove_backspaces: Erased UNIX prompt due to stand alone \\r split across blocks\n");)
      set_unix_prompt(dspl_descr, NULL); /* RES 11/09/2012, deal with Solaris format */
   }
while (*bptr){
   /* RES 05/13/2013 Watch for line ending in a blank if the next char is a \r, we won't clear the prompt */
   if ((*bptr == ' ') && (*(bptr+1) == '\0'))
      last_char_was_blank = 1;
   else
      last_char_was_blank = 0;
      

   /* RES 12/16/2002 watch for carriage return followed by erase to eof used by bash, clear prompt */
   if (*bptr == '\r')    /* carriage returns */
      {
         if (*(bptr+1) == '\0')
            {
               last_char_was_cr = 1;
            }
         else
            {
               last_char_was_cr = 0;
            }
         /* ignore the carriage return */
         /* if the next two chars are 1B5B4B (escape [ K) clear the UNIX prompt */
         if ((*(bptr+1) == 0x1B) && (*(bptr+2) == '[') && (*(bptr+3) == 'K'))
            {
               set_unix_prompt(dspl_descr, NULL);
               bptr += 3; /* skip the escape K */
               DEBUG16(fprintf(stderr, "remove_backspaces: Erased UNIX prompt\n");)
            }
         else
#ifdef linux
            if ((last_char_was_blank || (*(bptr-1) == ' ')) && (*(get_unix_prompt()) != '\0'))
               {
                  DEBUG16(fprintf(stderr, "remove_backspaces: Stand alone \\r ignored because it is preceeded by a blank \n");)
                  /* this was wrong anyway because of the '==' / *  *bptr == '\n'; * treat it like a new line */
                  tptr--;  /* get rid of the blank as well */
               }
            else
#endif
               {
                  if ((*(bptr+1) != '\n') && (*(bptr+1) != '\r') && (*(bptr+1) != '\0'))
                     {
                        if (last_0a == NULL)
                           tptr = line;
                        else
                           tptr = last_0a + 1; 
                        DEBUG16(fprintf(stderr, "remove_backspaces: Erased UNIX prompt due to stand alone \\r (last_0a is %s)\n", (last_0a ? "Not NULL" : "NULL"));)
                        set_unix_prompt(dspl_descr, NULL); /* RES 09/24/2004, deal with Linux progress bars, this should have been here anyway */
                     }
                  else
                     last_0a = tptr;
               }
      }
   else
      {
         if (*bptr == '\b')
            {     /* back spaces */
               tptr--;
               if (*tptr == '\n') 
                  tptr++;    /* can't back over newlines */
            }
         else
            {
               if (*tptr != *bptr)
                  *tptr = *bptr;
               tptr++;
            }
      }

   /* RES 9/12/95 special ce_isceterm check */
   if (*bptr == '\n')
      if ((bptr[1] == 0x02) && (bptr[2] == 0x02) &&
          (bptr[3] == 0x08) && (bptr[4] == 0x08)){
          DEBUG16(fprintf(stderr, "ce_isceterm_chk: special string matched\n");)
          pad2shell(special_string2, False);
      }

   if (tptr < line)    /* incase we get more backspaces than characters! */
         tptr = line; 

   bptr++;
}

*tptr = '\0';

} /* remove_backspaces */

#ifdef PRINT_MSG

/****************************************************************** 
*
*  print_msg - Debug printer print data instead of sending it to mad dog
*
******************************************************************/

static int print_msg(void)
{

int rc;
int bytes;

char buff[MAX_LINE+1];

if (ioctl(fds[0], FIONREAD, &bytes) < 0){ 
   snprintf(msg, sizeof(msg), "Can't ioctl(FIONREAD[1](%s))", strerror(errno));   
   dm_error(msg, DM_ERROR_LOG);
   return(-1);
}

DEBUG16( fprintf(stderr, "'%d' bytes to read: ", bytes);)

while (bytes) {
   rc = read(fds[0], buff, MAX_LINE);
   buff[rc]=NULL;  
   fprintf(stderr, "I/O is:'%s'[%d]\n", buff, rc);

   if(rc == EOF) break;

   if ((rc = ioctl(fds[0], FIONREAD, &bytes)) < 0){
       snprintf(msg, sizeof(msg), "Can't ioctl(FIONREAD[2](%s))", strerror(errno));   
       dm_error(msg, DM_ERROR_LOG);
       return(-1);
   }
}

return(0);

}  /* print_msg() */

#endif  /* Debug */

/****************************************************************** 
*
*  tty_getmode - Get the modes of the terminal
*
******************************************************************/

#ifdef SYS5
#if defined(solaris) || defined(linux)
static struct termios tty_termio;
static struct termios deftio;
#else
static struct termio tty_termio;
static struct termio deftio;
#endif
#else
static struct sgttyb  tty_sgttyb = {
        0, 0, 0177, CKILL, EVENP|ODDP|ECHO|XTABS|CRMOD
};
static struct tchars  tty_tchars = {
        CINTR, CQUIT, CSTART,
        CSTOP, CEOF, -1,
};
static struct ltchars tty_ltchars = {
        CSUSP, CDSUSP, CRPRNT,
        CFLUSH, CWERASE, CLNEXT
};
static struct winsize tty_winsize;
static int tty_localmode = LCRTBS|LCRTERA|LCRTKIL|LCTLECH;
static int tty_ldisc = NTTYDISC;
#endif

static int tty_getmode(int tty)
{

#ifdef SYS5

if (ioctl(tty, TCGETS, (char *) &tty_termio) < 0){
    snprintf(msg, sizeof(msg), "Can't ioctl fd = %d (GETMODES-SYSV(%s))", tty,  strerror(errno));   
    DEBUG16(dm_error(msg, DM_ERROR_LOG);)
    return(-1);
}
                      
#define SYS5_DEFAULTS 1 /* kill this line if the next 100 lines gives trouble! */

#ifdef SYS5_DEFAULTS
#ifdef linux
   tty_termio.c_cflag &= ~(CBAUD);
#endif
	tty_termio.c_iflag = ICRNL|IXON|IMAXBEL; /* added 4/19/94 */
	tty_termio.c_oflag = OPOST|ONLCR|TAB3;
#ifdef BAUD_0
    	tty_termio.c_cflag = CS8|CREAD|PARENB|HUPCL;
#else	/* !BAUD_0 */
    	tty_termio.c_cflag = B9600|CS8|CREAD|PARENB|HUPCL;
#endif	/* !BAUD_0 */
       tty_termio.c_cflag &= ~(CLOCAL); /* from new xterm */
    	tty_termio.c_lflag = ISIG|ICANON|ECHO|ECHOE|ECHOK;
#ifndef solaris
	tty_termio.c_line = 0;
#endif
	tty_termio.c_cc[VINTR] = 0x7f;		/* DEL  */
	tty_termio.c_cc[VQUIT] = '\\' & 0x3f;	/* '^\'	*/
	tty_termio.c_cc[VERASE] = '#';		/* '#'	*/
	tty_termio.c_cc[VKILL] = '@';		/* '@'	*/
    	tty_termio.c_cc[VEOF] = 'D' & 0x3f;		/* '^D'	*/
	tty_termio.c_cc[VEOL] = '@' & 0x3f;		/* '^@'	*/
#ifdef VSWTCH
	tty_termio.c_cc[VSWTCH] = '@' & 0x3f;	/* '^@'	*/
#endif	/* VSWTCH */
	/* now, try to inherit tty settings */
	{
	    int i;

	    for (i = 0; i <= 2; i++) {
		if (ioctl (i, TCGETS, &deftio) == 0) {
		    tty_termio.c_cc[VINTR] = deftio.c_cc[VINTR];
		    tty_termio.c_cc[VQUIT] = deftio.c_cc[VQUIT];
		    tty_termio.c_cc[VERASE] = deftio.c_cc[VERASE];
		    tty_termio.c_cc[VKILL] = deftio.c_cc[VKILL];
		    tty_termio.c_cc[VEOF] = deftio.c_cc[VEOF];
		    tty_termio.c_cc[VEOL] = deftio.c_cc[VEOL];
#ifdef VSWTCH
		    tty_termio.c_cc[VSWTCH] = deftio.c_cc[VSWTCH];
#endif /* VSWTCH */
		    break;
		}                   
	    }
	}
/*	tty_termio.c_cc[VSUSP] = '\000';
	tty_termio.c_cc[VDSUSP] = '\000';
	tty_termio.c_cc[VREPRINT] = '\377';
	tty_termio.c_cc[VDISCARD] = '\377';
	tty_termio.c_cc[VWERASE] = '\377';
	tty_termio.c_cc[VLNEXT] = '\377';  */
#ifdef TIOCLSET_BLAH
	d_lmode = 0;
#endif	/* TIOCLSET */
#endif  /* macII || CRAY (SYS5_DEFAULTS) */

/* end of ifdef SYS5 */
#else

DEBUG16(fprintf(stderr, " @tty_getmode(tty=%d)\n", tty);)

if (ioctl(tty, TIOCGETP,   (char *) &tty_sgttyb) < 0){
    DEBUG16(dm_error("Could not GETMODES(TIOCGETP)", DM_ERROR_LOG);)
    return(-1);
}

if (ioctl(tty, TIOCGLTC,   (char *) &tty_ltchars) < 0){
    DEBUG16(dm_error("Could not GETMODES(TIOCGLTC)", DM_ERROR_LOG);)
    return(-1);
}
#ifdef TIOCSLTC_BLAH
        tty_ltchars.t_suspc = '\000';		/* t_suspc */
        tty_ltchars.t_suspc.t_dsuspc = '\000';	/* t_dsuspc */
        tty_ltchars.t_rprntc = '\377';	/* reserved...*/
        tty_ltchars.t_flushc = '\377';
        tty_ltchars.t_werasc = '\377';
        tty_ltchars.t_lnextc = '\377';
#endif	/* TIOCSLTC */

if (ioctl(tty, TIOCGETC,   (char *) &tty_tchars) < 0){
    DEBUG16(dm_error("Could not GETMODES(TIOCGETC)", DM_ERROR_LOG);)
    return(-1);
}

if (ioctl(tty, TIOCLGET,   (char *) &tty_localmode) < 0){
    DEBUG16(dm_error("Could not GETMODES(TIOCLGET)", DM_ERROR_LOG);)
    return(-1);
}

if (ioctl(tty, TIOCGETD,   (char *) &tty_ldisc) < 0){
    DEBUG16(dm_error("Could not GETMODES(TIOCGETD)", DM_ERROR_LOG);)
    return(-1);
}

if (ioctl(tty, TIOCGWINSZ, (char *) &tty_winsize) < 0){
    DEBUG16(dm_error("Could not GETMODES(TIOCGWINSZ)", DM_ERROR_LOG);)
    return(-1);
}
#endif

return(0);

} /* tty_getmode() */

/****************************************************************** 
*
*  tty_setmode - Set the mode of the terminal
*
******************************************************************/
#ifdef SYS5
/***** SYSV ******/
#ifdef linux
#define sg_flags flags
#define sg_ispeed chars[0]
#define sg_ospeed chars[1]
#endif
static int tty_setmode(int tty)
{
struct sgttyb sg;

DEBUG16(fprintf(stderr, " @tty_setmode(tty=%d)\n", tty);)
if (ioctl(tty, TCSETS, (char *) &tty_termio) < 0){    
     snprintf(msg, sizeof(msg), "Can't ioctl(TCSETS(%s))", strerror(errno));   
     dm_error(msg, DM_ERROR_LOG);
     return(-1);
} 

if(ioctl(tty, TIOCGETP, (char *)&sg) < 0){
     snprintf(msg, sizeof(msg), "Can't ioctl(TIOCGETP(%s))-set/modes", strerror(errno));   
     dm_error(msg, DM_ERROR_LOG);
     return(-1);
}

/* sg.sg_flags &= ~(ALLDELAY | XTABS | RAW); */
#ifdef linux
sg.sg_flags &= ~(O_XTABS | /* CBREAK | */ O_RAW);
#else
sg.sg_flags &= ~(IOCTYPE | O_XTABS | /* CBREAK | */ O_RAW);
#endif
sg.sg_flags |= O_ECHO | O_CRMOD;
/* make sure speed is set on pty so that editors work right*/
sg.sg_ispeed = B9600;
sg.sg_ospeed = B9600;

if (ioctl (tty, TIOCSETP, (char *)&sg) == -1){
     snprintf(msg, sizeof(msg), "Can't ioctl(TIOCSETP(%s))", strerror(errno));   
     dm_error(msg, DM_ERROR_LOG);   
     return(-1);
}

return(0);

} /* tty_setmode() */

#else

/***** BSD ******/
static int tty_setmode(int tty)
{

 /*
  * from xterm/main.c I don't know why 
  */

tty_tchars.t_brkc = -1;  

#ifdef TIOCSCTTY
DEBUG16(fprintf(stderr, "ioctl(TIOCSCTTY)\n");)
if (ioctl(tty, TIOCSCTTY, 0) < 0)
     DEBUG16(fprintf(stderr, "Can't ioctl(TIOCSCTTY(%s))\n", strerror(errno));)   
#endif


tty_localmode = LCRTBS|LCRTERA|LCRTKIL|LCTLECH; /* regular */

 /*
  *  set the tty characteristics
  */

tty_sgttyb.sg_flags |= CRMOD | ECHO;  /**** ^C ****/                     
tty_sgttyb.sg_flags &= ~(NOHANG | ANYP | RAW | CBREAK);

#ifdef sun
tty_sgttyb.sg_flags |= O_TAB1 | O_TAB2 | O_ODDP | O_EVENP;
tty_sgttyb.sg_ospeed = 0x0F;   /* makes '/usr/ucb/more' work */
#endif

if ((ioctl(tty, TIOCSETP,    (char *) &tty_sgttyb) < 0) ||   
    (ioctl(tty, TIOCSETC,    (char *) &tty_tchars) < 0) ||   
    (ioctl(tty, TIOCSLTC,    (char *) &tty_ltchars) < 0) ||  
    (ioctl(tty, TIOCLSET,    (char *) &tty_localmode) < 0) ||
    (ioctl(tty, TIOCSETD,    (char *) &tty_ldisc) < 0) ||    
    (ioctl(tty, TIOCSWINSZ,  (char *) &tty_winsize) < 0)){   
       snprintf(msg, sizeof(msg), "Can't ioctl(SETMODES(%s))", strerror(errno));   
       dm_error(msg, DM_ERROR_LOG);
       return(-1);
}

return(0);

} /* tty_setmode() */

#endif

/****************************************************************** 
*
*  tty_raw - Set to raw mode
*
******************************************************************/

#ifdef SYS5

/******* SYSV *****/

#if defined(solaris) || defined(linux)
static struct termios tty_mode;
#else
static struct termio tty_mode;
#endif

static int tty_raw(int fd)
{
#if defined(solaris) || defined(linux)
struct termios temp_mode;
#else
struct termio temp_mode;
#endif

if (ioctl(fd, TCGETS, (char *) &temp_mode) < 0){
   DEBUG16(snprintf(msg, sizeof(msg), "Can't ioctl(TCGETA(%s))", strerror(errno));   
           dm_error(msg, DM_ERROR_LOG);)
   return(-1);
}

tty_mode = temp_mode;

temp_mode.c_iflag = IMAXBEL; /* was 0 4/19/94 */
temp_mode.c_oflag &= ~OPOST;
temp_mode.c_lflag &= ~(ISIG | ICANON | ECHO | XCASE);

temp_mode.c_cflag &= ~(CSIZE | PARENB);
temp_mode.c_cflag |= CS8;
temp_mode.c_cc[VMIN] = 1;
temp_mode.c_cc[VTIME] = 1;

if (ioctl(fd, TCSETS, (char *) &temp_mode) < 0){
   DEBUG16(snprintf(msg, sizeof(msg), "Can't ioctl[R](TCSETS(%s))", strerror(errno));   
           dm_error(msg, DM_ERROR_LOG);)
   return(-1);
}

return(0);

} /* tty_raw() */

#else

/******* BSD *****/
static struct sgttyb tty_mode;

static int tty_raw(int fd)
{

static struct sgttyb temp_mode;

DEBUG16(fprintf(stderr, "tty fd[%d] set to RAW\n", fd);)   

if (ioctl(fd, TIOCGETP, (char *) &temp_mode) < 0){   
   DEBUG16(snprintf(msg, sizeof(msg), "Can't ioctl(GRAW(%s)[%d])", strerror(errno), fd);   
           dm_error(msg, DM_ERROR_LOG);)
   return(-1);
}

tty_mode = temp_mode;

   /*
    * from xterm/main.c
    */

temp_mode.sg_flags &= ~(ALLDELAY | XTABS | CBREAK | RAW); 
temp_mode.sg_flags |= ECHO | CRMOD;                       

temp_mode.sg_ispeed = B9600;
temp_mode.sg_ospeed = B9600;

   /*
    * Impose our views on the tty
    */

if (ioctl(fd, TIOCSETP, (char *) &temp_mode) < 0){   
    DEBUG16(snprintf(msg, sizeof(msg), "Can't ioctl(SRAW(%s)[%d])", strerror(errno), fd);   
            dm_error(msg, DM_ERROR_LOG);)
    return(-1);
}

return(0);

}  /* tty_raw() */

#endif
/****************************************************************** 
*
*  tty_reset - Reset from raw back to previos state CBREAK/COOKED
*
******************************************************************/

 /*ARGSUSED*/
int tty_reset(int fd)
{

#if defined(ultrix) || defined(SYS5)
return(0);
#else

#ifdef SYS5
if (ioctl(fd, TCSETS, (char *) &tty_mode) < 0){   
#else
if (ioctl(fd, TIOCSETP, (char *) &tty_mode) < 0){   
#endif
       DEBUG16(snprintf(msg, sizeof(msg), "Can't ioctl(SRAWBII(%s)[%d])", strerror(errno), fd);   
               dm_error(msg, DM_ERROR_LOG);)
       return(-1);
}

DEBUG16(fprintf(stderr, "Resetting tty(%d)\n", fd);)

return(0);

#endif   /* ultrix */
} /* tty_reset() */

/****************************************************************** 
*
*  tty_echo - return whether we should display characters or not.
*             this routine checks if we are echoing characters and returns that
*             if called with (1), and factors in telnet_mode otherwise
*             this is incase we are telneted in.
*  RETURNED VALUE:           
*             True, echo characters is enabled.  Not dot mode.
*             False, use dot mode, echoing is disabled.
*
******************************************************************/
#include "dumptermios.h"
int tty_echo(int   echo_only,
             char *dotmode) /* Added 8/30/97 RES */
{
char  *prompt;

#if defined(SYS5)/* && !defined(solaris)  */

int tty;

#if defined(solaris) || defined(linux)
struct termios tty_echo_bits;
#else
struct termio tty_echo_bits;
#endif

struct sgttyb tty_sg_bits;

if (!echo_only){
   if (dotmode && (dotmode[0] == '0'))
      return(1);

   if (dotmode && (dotmode[0] == '1')){
      prompt = get_unix_prompt();
      if (dotmode[1]) /* if value was supplied for dotmode 1 */
         if (online(prompt, &dotmode[1]))
            return(0);
         else
            return(1);
      else
         if (online(prompt, "pas"))
            return(0);
         else
            return(1);

   }
}



tty = fds[0];

#if defined(_INCLUDE_HPUX_SOURCE) || defined(solaris) || defined(linux)
/*ifdef solaris RES 1/6/1999 */
if (slave_tty_fd >= 0) tty = slave_tty_fd;
#endif

if (ioctl(tty, TCGETS, (char *) &tty_echo_bits) < 0){   
       DEBUG16(fprintf(stderr, "TTY_ECHO: Can't ioctl(TCGETA(%s), fd = %d)\n", strerror(errno), tty);)   
       return(-1);
}  	

#ifdef sgi
if(!echo_only && ioctl(tty, TCGETS, (char *)&tty_sg_bits) < 0){
     DEBUG16(fprintf(stderr, "TTY_ECHO: Can't ioctl(TCGETS(%s), fd = %d)", strerror(errno), tty);)   
     return(-1);
}
#else
if(!echo_only && ioctl(tty, TIOCGETP, (char *)&tty_sg_bits) < 0){
     DEBUG16(fprintf(stderr, "TTY_ECHO: Can't ioctl(TIOCGETP(%s), fd = %d)", strerror(errno), tty);)   
     return(-1);
}
#endif

/* RES 1/31/2003 use both flags to support ssh, which triggered dot mode on non-unix systems */
if(!echo_only)
    telnet_mode = !(tty_echo_bits.c_lflag & ECHO) && (!(tty_echo_bits.c_lflag & ICANON) || !(tty_echo_bits.c_lflag & O_CRMOD));

#ifdef blah
#ifdef linux
/* RES 4/3/2002 Added different telnet mode code for linux */
if(!echo_only)
    telnet_mode = !(tty_echo_bits.c_lflag & ECHO) && !(tty_echo_bits.c_lflag & ICANON);
#else
if(!echo_only)
    telnet_mode = !(tty_echo_bits.c_lflag & ECHO) && !(tty_sg_bits.sg_flags & O_CRMOD);
#endif
#endif

DEBUG16(fprintf(stderr, "fd[%d] echo_only:%d\n", tty, echo_only);)
DEBUG16(fprintf(stderr, "#define ECHO=%d, CRMOD=%d\n", ECHO, O_CRMOD);)
DEBUG16(fprintf(stderr, "telnet_mode:%d\n", telnet_mode);)
DEBUG16(fprintf(stderr, "termio.c_lflag:0x%X\n", tty_echo_bits.c_lflag );)
DEBUG16(fprintf(stderr, "sgtty.sg_flags:0x%X\n", tty_sg_bits.sg_flags);)
DEBUG16(fprintf(stderr, "(termio & ECHO) = %d, (sgtty & CRMOD) = %d\n", tty_echo_bits.c_lflag & ECHO, tty_sg_bits.sg_flags & O_CRMOD);)
DEBUG16(fprintf(stderr, "RC=%d\n", (tty_echo_bits.c_lflag & ECHO) || telnet_mode);)
DEBUG16(dump_termios(&tty_echo_bits, stderr);)

if (echo_only) return (tty_echo_bits.c_lflag & ECHO);

return((tty_echo_bits.c_lflag & ECHO) || telnet_mode);
 
#else /* BSD style! */

static struct sgttyb tty_echo_bits; 

if (!echo_only){
   if (dotmode && (dotmode[0] == '0'))
      return(1);

   if (dotmode && (dotmode[0] == '1')){
      prompt = get_unix_prompt();
      if (dotmode[1]) /* if value was supplied for dotmode 1 */
         if (online(prompt, &dotmode[1]))
            return(0);
         else
            return(1);
      else
         if (online(prompt, "pas"))
            return(0);
         else
            return(1);

   }
}

if (ioctl(fds[0], TIOCGETP, (char *) &tty_echo_bits) < 0){   
       if (ioctl(tty_fd, TIOCGETP, (char *) &tty_echo_bits) < 0){  /* 10/1/93 solaris */ 
           DEBUG16(fprintf(stderr, "TTY_ECHO: Can't ioctl(TIOCGETP(%s))\n", strerror(errno));)   
           return(-1);
       }
}

telnet_mode = !(tty_echo_bits.sg_flags & ECHO) && !(tty_echo_bits.sg_flags & CRMOD);

DEBUG16(fprintf(stderr, "ttyecho(ECHO=%d,CRMOD=%d,TELNET=%d)RC=%d\n", tty_echo_bits.sg_flags & ECHO, tty_echo_bits.sg_flags & CRMOD, telnet_mode, (tty_echo_bits.sg_flags & ECHO) || telnet_mode);)

if (echo_only) return(tty_echo_bits.sg_flags & ECHO);

return((tty_echo_bits.sg_flags & ECHO) || telnet_mode);

#endif               

} /* tty_echo() */


/****************************************************************** 
*
*  pty_master - Connect to the tty master
*
******************************************************************/

#ifdef _INCLUDE_HPUX_SOURCE
#define PTY_MASTER	"/dev/ptym/ptyxx"
/*
static char ptychar[] = "zyxwvutsrqp";
static char hexdigit[] = "fedcba9876543210";
*/
#else
#define PTY_MASTER	"/dev/ptyxx"
/*static char ptychar[] = "pqrs";*/
#endif
static char ptychar[] = "pqrs";
static char hexdigit[] = "0123456789abcdef";

static int pty_master(void)
{

int i, rc, len, master_fd;
char *ptr;

/* added defined(linux) RES 9/9/2004 for Fedora Core 2 */
#if defined(_INCLUDE_HPUX_SOURCE) || defined(solaris) || defined(HAVE_GETPT)

#if defined(HAVE_GETPT)
master_fd = getpt(); 
DEBUG16(fprintf(stderr, "getpt Master opened fd = %d\n", master_fd);)
if (master_fd < 0)
{
    snprintf(msg, sizeof(msg), "getpt failed (%s)\n", strerror(errno));   
    fprintf(stderr, msg);
    return(-1);
}

#else

strcpy(pty_name, "/dev/ptmx");

#ifdef sgi
strcpy(tty_name, _getpty(&master_fd, O_RDWR|O_NDELAY, 0600, 0));
strcpy(pty_name, "/dev/ptc");
DEBUG16(fprintf(stderr, "PI: getting master & slave SGI way(%s,)\n", tty_name );)
#else
master_fd = open(pty_name, O_RDWR, 0);
#endif

DEBUG16(fprintf(stderr, "Master opened (%s) fd = %d\n", pty_name, master_fd);)
if (master_fd < 0){
    snprintf(msg, sizeof(msg), "Can't open /dev/ptmx(%s)", strerror(errno));   
    dm_error(msg, DM_ERROR_LOG);
    return(-1);
}

#endif /* else not linux */

/* RES 9/9/2004 added tests for error */
rc = grantpt(master_fd);
#if !defined(solaris)
if (rc < 0)
{
    snprintf(msg, sizeof(msg),  "Can't grantpt pty %s (%s)\n", pty_name, strerror(errno));   
    dm_error(msg, DM_ERROR_LOG);
    return(-1);
}
#endif
rc = unlockpt(master_fd);
if (rc < 0)
{
    snprintf(msg, sizeof(msg),  "Can't unlockpt pty %s (%s)\n", pty_name, strerror(errno));   
    dm_error(msg, DM_ERROR_LOG);
    return(-1);
}

/*rc = ioctl(master_fd, I_PUSH, "pckt");
DEBUG16(fprintf(stderr, "Had to I_PUSH \"pckt\" RC=%d\n", rc);)*/

/* !defined(linux) added 9/9/2004 for Fedora Core 2 */
#if !defined(_INCLUDE_HPUX_SOURCE) && !defined(linux)
rc = ioctl(master_fd, I_FIND, "sockmod");
if (rc<0){
    rc = ioctl(master_fd, I_PUSH, "sockmod");
    DEBUG16(fprintf(stderr, "Had to I_PUSH \"sockmod\" RC=%d\n", rc);)
}
#endif
i = fcntl(master_fd, F_GETFL, 0); /* gleened out of truss of xterm */
fcntl(master_fd, F_SETFL, O_NDELAY | ((i>0) ? i : 0));

return(master_fd);
#else

/* struct stat statbuf; */

strcpy(pty_name, PTY_MASTER);

len = strlen(PTY_MASTER) -1;
for (ptr = ptychar; *ptr != 0; ptr++){
    pty_name[len-1] = *ptr;
    pty_name[len] = hexdigit[0];

    for (i=0; i<16; i++){
        pty_name[len] = hexdigit[i];
        if ((master_fd = open(pty_name, O_RDWR | O_NDELAY)) >= 0){
            DEBUG16(fprintf(stderr, "Master opened (%s) fd = %d\n", pty_name, master_fd);)
#if defined(SYS5)
            rc = chown (pty_name, getuid(), getgid());
            rc |= chmod (pty_name, 0600);
            if (rc){
                fprintf(stderr,  "Can't unlock pty %s (%s)\n", pty_name, strerror(errno));   
               /* close(master_fd); return(-1);  */
            }
#endif
#if defined(SYS5) /* && !defined(solaris) */
            return(master_fd);
#else
#ifdef solaris /* obsoleted at 2.3 */
            if (solaris_check_slave())
#else
            if (check_slave())
#endif
                return(master_fd);
            else{
                DEBUG16(fprintf(stderr, "Discarding Master since slave is incompatable!\n");)
                close(master_fd);
            }
#endif
        } /* if (open()) */
    }

} /* for */

if (i == 16)
  snprintf(msg, sizeof(msg), "All pty devices are used!");   
else
  snprintf(msg, sizeof(msg), "Can't open master tty: %s (%s)", pty_name, strerror(errno));   

dm_error(msg, DM_ERROR_LOG);
 
return(-1);

#endif /* HP & solaris  */

} /* pty_master() */

#ifdef solaris_obsolete

/****************************************************************** 
*
*  solaris_check_slave - Check if a viable solaris slave is available
*
******************************************************************/

static int solaris_check_slave()
{
int rc;

rc = open (pts_convert(pty_name), O_RDWR | O_NDELAY /*| O_NOCTTY*/);

DEBUG16(
   if (rc == -1)
       fprintf(stderr, "Slave is is not accessible.\n");
   else
       fprintf(stderr, "Slave is compatible.\n");
)

if (rc == -1)
   return(0);
else{
   close(rc);
   return(1);
}

} /* solaris_check_slave() */

/****************************************************************** 
*
*  pts_convert - Build a pts out of a pty
*
******************************************************************/
static char hextrans[16] = "0123456789abcdef";

static char *pts_convert(char *pty)
{
char *ptr;

strcpy(pts_name, pty);
ptr = strrchr(pts_name, '/');

if (ptr && (ptr > pts_name)){
    *(ptr+3) =  's';     /* /dev/ptsq7 */
    itoa(((*(ptr+4) - 'p') * 16) + 
         (strchr(hextrans, *(ptr+5)) - hextrans), ptr+5); /* /dev/ptsp23 */
    *(ptr+4) = '/';  /* /dev/pts/23 */
}

DEBUG16( fprintf(stderr," pts_convert[%s]\n",pts_name);) 
return(pts_name);

} /* pts_convert() */

#endif

/****************************************************************** 
*
*  check_slave - Check if a viable slave is available
*
******************************************************************/

static int check_slave()
{
int rc;

pty_name[5] = 't';
rc = access(pty_name, R_OK | W_OK);
pty_name[5] = 'p';

DEBUG16(
   if (rc)
       fprintf(stderr, "Slave is is not accessible.\n");
   else
       fprintf(stderr, "Slave is compatible.\n");
)

return(!rc); /* access backward logic! */

} /* check_slave() */

/****************************************************************** 
*
*  pty_slave - Connect to the tty slave
*
******************************************************************/

#if defined(SYS5)

/***** SYSV ******/
static int pty_slave(int master_pty_fd)
{

int rc;
int slave_fd;
char *ptr;
char *ptr1;

DEBUG16( fprintf(stderr," @pty_slave: master_pty_fd %d, ptyname(%s) ", master_pty_fd,  pty_name);) 

/*
 *  An example of a HP tty master names is:
 *  /dev/ptym/ptysb
 *  We want to change this to
 *  /dev/pty/ttysb
 *  That is, take out the m on the second qualifer 
 *  and change the pty to tty in the last qualifier.
 *
 *  for SOLARIS we go:
 *
 *         /dev/ptyp7 -> /dev/pts/7
 *         /dev/ptyq7 -> /dev/pts/23
 *         /dev/ptyr7 -> /dev/pts/39
 */

#ifndef sgi
strcpy(tty_name, pty_name);
ptr = strrchr(tty_name, '/');
#endif

/* added defined(linux) RES 09/09/2004 for Fedora Core 2 */
#if defined(_INCLUDE_HPUX_SOURCE) || defined(solaris) || defined(linux)

#ifndef sgi
/* strcpy(tty_name, pts_convert(tty_name)); */ 

ptr = ptsname(master_pty_fd);
DEBUG16( fprintf(stderr," ttyname(%s)\n", ptr ? ptr : "ERROR!");) 
if (!ptr){
   snprintf(msg, sizeof(msg), "Slave tty could not be found for master fd %d (%s)\n", master_pty_fd, strerror(errno));
   dm_error(msg, DM_ERROR_LOG);
   return(-1);
 }
strcpy(tty_name, ptr);
#else
DEBUG16( fprintf(stderr,"\nSGI ttyname(%s)\n", tty_name);) 
#endif

slave_fd = open(tty_name, O_RDWR);
if (slave_fd < 0)
    return(-1);
DEBUG16( fprintf(stderr," tty slave fd = (%d)\n", slave_fd);) 

/* added !defined(linux) 9/9/2004 for Fedora Core 2 */
#if !defined(sgi) && !defined(linux)
#ifdef _INCLUDE_HPUX_SOURCE
if ((ioctl(slave_fd, I_PUSH, "ptem") < 0) ||
    (ioctl(slave_fd, I_PUSH, "ldterm") < 0)){
   snprintf(msg, sizeof(msg), "Slave tty could not push streams modules(%s)\n", strerror(errno));   
   dm_error(msg, DM_ERROR_LOG);
   return(-1);
}
#else
if ((ioctl(slave_fd, I_PUSH, "ptem") < 0) ||
    (ioctl(slave_fd, I_PUSH, "ldterm") < 0) ||
    (ioctl(slave_fd, I_PUSH, "ttcompat") < 0)){
   snprintf(msg, sizeof(msg), "Slave tty could not push streams modules(%s)\n", strerror(errno));   
   dm_error(msg, DM_ERROR_LOG);
   return(-1);
}
#endif
#endif

#ifdef blah
if (ioctl (slave_fd, I_PUSH, "ttcompat") < 0) 
   DEBUG16(fprintf(stderr, "Could not I_PUSH('ttcompat'): %s\n",  strerror(errno));)
#endif

return(slave_fd);

#else

#ifdef _INCLUDE_HPUX_SOURCE
DEBUG16( fprintf(stderr," HP-UX/");) 
if (ptr && (ptr > tty_name)){
    ptr1 = ptr - 1;
    while (*ptr1)
       *ptr1++ = *ptr++;
}else{
     fprintf(stderr,  "pty_slave:  Cannot parse tty name %s\n", tty_name);
     return(-1);
}
#endif

#ifndef solaris
tty_name[strlen(tty_name)-5] = 't';  /* change pty to tty */
#endif

DEBUG16( fprintf(stderr," ttyname(%s)\n", tty_name);) 

if ((slave_fd = open (tty_name, O_RDWR | O_NDELAY /*| O_NOCTTY*/)) >= 0) {
      rc = chown(tty_name, getuid(), getgid());
      rc |= chmod(tty_name, 0600);
      if (rc){
          fprintf(stderr,  "Can't unlock tty %s (%s)\n", tty_name, strerror(errno));   
          return(-1);
      }
      DEBUG16(
         fprintf(stderr,"pty_slave: Slave opened: '%s'.fd %d\n", tty_name, slave_fd);
         ptr = (char *)ttyname(slave_fd);
         if (!ptr)
             fprintf(stderr,  "pty_slave: Can't get tty name for fd %d (%s)\n", slave_fd, strerror(errno));   
      )
      return(slave_fd);
}else
    fprintf(stderr,  "pty_slave: Can't open tty %s (%s)\n", tty_name, strerror(errno));   

return(-1);
#endif /* solaris */

} /* pty_slave() */

#else  /* BSD/SYSV */

/***** BSD ******/
static int pty_slave(int master_pty_fd)
{

int slave_fd;

pty_name[5] = 't';

if ((slave_fd = open(pty_name, O_RDWR)) < 0){
    return(-1);
}
pty_name[5] = 'p';

strcpy(tty_name, (char *)ttyname(slave_fd));
DEBUG16( fprintf(stderr,"Slave[BSD] opened: '%s' / tty:%s.\n", pty_name, tty_name);) 
return(slave_fd);

} /* pty_slave() */

#endif

/****************************************************************** 
*
*  vi_set_size - Register the size of the screen with the kernel
*                so programs like vi can be used
*
******************************************************************/

int vi_set_size(int fd, int vt100_mode, int lines, int columns, int hpix, int vpix)
{

struct winsize winsiz;

winsiz.ws_row =  lines;
winsiz.ws_col =  columns;
winsiz.ws_xpixel =  hpix;
winsiz.ws_ypixel =  vpix;

/*if (!vt100_mode)
   winsiz.ws_col =  MAX_LINE;*/


DEBUG16(fprintf(stderr, "chid=%d in window size\n", chid);)

if ((ioctl(fd, TIOCSWINSZ, &winsiz) < 0) && chid){
    snprintf(msg, sizeof(msg), "Could not reset Kernel window size(%s))", strerror(errno));   
    DEBUG16(fprintf(stderr, "%s\n", msg);)
    return(-1);
}

DEBUG16(fprintf(stderr, "Window size changed to char:(%d,%d) pix:(%d,%d)\n", lines, columns, hpix, vpix);)

return(0);

} /* vi_set_size() */

/****************************************************************** 
*
*  ce_getcwd - Get the working directory for the subshell
*
******************************************************************/
static char sun_getcwd_storage[MAXPATHLEN+5] = "PWD=";
static char *sun_getcwd_bug = sun_getcwd_storage +4;

char *ce_getcwd(char *buf, int size)
{

if (buf && (size > (int)strlen(sun_getcwd_bug))) 
   strcpy(buf, sun_getcwd_bug);

#ifdef GETCWD_DEBUG
fprintf(stderr, "@ce_getcwd(%s,%d)strlen(sgb)=[%d], = %s\n", buf, size, (int)strlen(sun_getcwd_bug), sun_getcwd_bug);
#endif


return(sun_getcwd_bug);

}  /* ce_getcwd() */

/****************************************************************** 
*
*  ce_setcwd - Set the working directory for the subshell
*              Try the ksh $PWD and see if its ok if so
*              preserve it since the logical path is better
*
******************************************************************/

void ce_setcwd(char *buf)
{
char  *ptr;
char path[MAXPATHLEN+1];
static int wasSet = 1;
struct stat sbuf1, sbuf2;

#ifdef GETCWD_DEBUG
fprintf(stderr, "@ce_setcwd(\"%s\")\n", buf ? buf : "");
#endif

if (!buf){
    buf = (char *)getenv("PWD");
    ptr = getcwd(path, sizeof(path));
    if (!ptr)
        ptr = "/"; /* handles case of cd into a directory, remove the directory, fire up ce */
#ifdef GETCWD_DEBUG
    fprintf(stderr, "getenv(%s)\n", buf ? buf : "");
    fprintf(stderr, "getcwd(%s)\n", ptr ? ptr : "");
#endif
    if (!buf){
        buf = ptr;
        wasSet = 0;
    }else{
       if (stat(buf, &sbuf1))
           buf = ptr;     /* PWD failed */
       else{
          if (!stat(ptr, &sbuf2)) /* getcwd failed */
              if (!((sbuf1.st_dev == sbuf2.st_dev) && 
                    (sbuf1.st_ino == sbuf2.st_ino)))
                       buf = ptr;
       }
    }
}

strcpy(sun_getcwd_bug, buf);
if (wasSet){
#ifdef GETCWD_DEBUG
   fprintf(stderr, "putenv(%s)\n", sun_getcwd_storage);
#endif
   putenv(sun_getcwd_storage);
} 

} /* ce_setcwd() */

/****************************************************************** 
*
*  close_shell - Force a close on the socket to the shell
*
******************************************************************/

void close_shell(void)
{
close(fds[0]);
#if defined(_INCLUDE_HPUX_SOURCE) || defined(solaris)  || defined(linux)
/*ifdef solaris RES 1/6/1999 */
if (slave_tty_fd >= 0)
   close(slave_tty_fd);
#endif

}

#ifdef DebuG

/****************************************************************** 
*
*  signal_handler - Detect All signals
*
******************************************************************/

void signal_handler(int sig)
{

fprintf(stderr, "Signal accepted: pad!: sig(%d)\n", sig);

}
#endif  /* DebuG */

/****************************************************************** 
*
*  insert_in_line - replace an EOF
*
******************************************************************/
static void insert_in_line(char *line, char *text)
{
char eof_buf[MAX_LINE];

DEBUG16(fprintf(stderr, " @insert_in_line(%s,%s)\n", line, text);)
DEBUG16(hex_dump(stderr, "  LINE:", (char *)line, strlen(line));)

strlcpy(eof_buf, line+1, sizeof(eof_buf));
strcpy(line, text);
strcat(line, "\n");
strcat(line, eof_buf);

}  /*  insert_in_line() */

#ifdef DebuG
#ifdef SYS5
struct sgttyb Dtty_sg;
#if defined(solaris) || defined(linux)
static struct termios Dtty_termio;
#else
static struct termio Dtty_termio;
#endif
#else
static struct sgttyb  Dtty_sgttyb; 
static struct tchars  Dtty_tchars; 
static struct ltchars Dtty_ltchars;
static struct winsize Dtty_winsize;
static int Dtty_localmode;
static int Dtty_ldisc;
#endif
/****************************************************************** 
*
*  dump_tty - Dump tty info to screen
*
******************************************************************/
void dump_tty(int tty)
{

int ttty, ctty, i, i1, i2=0, i3=0, i4=0, i5=0, i6=0;

if (tty < 0)
   tty = fds[0];

fprintf(stderr, "Dumping Tty File Descriptor[%d]\n", tty);

#ifdef SYS5

ttty = open("/dev/tty", O_RDONLY, 0);
ctty = open("/dev/console", O_RDONLY, 0);

for (i = 0; i < 32; i++){

fprintf(stderr, "Dumping SYS5 File Descriptor[%d]\n", i);

if (ioctl(i, TCGETS, (char *) &Dtty_termio) < 0){    
     snprintf(msg, sizeof(msg), "Can't ioctl(TCGETA(%s))", strerror(errno));   
     dm_error(msg, DM_ERROR_LOG);
     /* return; */ continue;
} 

if(ioctl(i, TIOCGETP, (char *)&Dtty_sg) < 0){
     snprintf(msg, sizeof(msg), "Can't ioctl(TIOCGETP(%s))", strerror(errno));   
     dm_error(msg, DM_ERROR_LOG);
     /* return; */ continue;
}
 
hex_dump(stderr, "tty_termio", (char *)&Dtty_termio, sizeof(Dtty_termio));
hex_dump(stderr, "tty_sg", (char *)&Dtty_sg, sizeof(Dtty_sg));
 
}  /* for () */

close(ttty);close(ctty);

#else

if (((i1 = ioctl(tty, TIOCGETP,   (char *) &Dtty_sgttyb)) < 0) ||
    ((i2 = ioctl(tty, TIOCGETC,   (char *) &Dtty_tchars)) < 0) ||
    ((i3 = ioctl(tty, TIOCGLTC,   (char *) &Dtty_ltchars)) < 0) ||
    ((i4 = ioctl(tty, TIOCLGET,   (char *) &Dtty_localmode)) < 0) ||
    ((i5 = ioctl(tty, TIOCGETD,   (char *) &Dtty_ldisc)) < 0) ||
    ((i6 = ioctl(tty, TIOCGWINSZ, (char *) &Dtty_winsize)) < 0)){
       snprintf(msg, sizeof(msg), "Could not dump tty info(GETMODES(%s)): %d %d %d %d %d %d", strerror(errno), i1, i2, i3, i4, i5, i6);   
       dm_error(msg, DM_ERROR_LOG);
       return;
}

hex_dump(stderr, "tty_sgttyb - input-speed, output-speed, erase-character, kill-character, mode-flags", (char *)&Dtty_sgttyb, sizeof(Dtty_sgttyb));
hex_dump(stderr, "tty_tchars - interrupt, quit, start_output, stop_output, end-of-file, input_delimiter (like nl)", (char *)&Dtty_tchars, sizeof(Dtty_tchars));
hex_dump(stderr, "tty_ltchars - stop-process-signal, delayed-stop-process-signal, reprint-line, flush-output (toggles), word erase, literal-next-character", (char *)&Dtty_ltchars, sizeof(Dtty_ltchars));
hex_dump(stderr, "tty_localmode", (char *)&Dtty_localmode, sizeof(Dtty_localmode));
hex_dump(stderr, "tty_ldisc", (char *)&Dtty_ldisc, sizeof(Dtty_ldisc));
hex_dump(stderr, "tty_winsize - (all short's) rows-in-chars, cols-in-chars, horizontal-size-pixels, vertical-size-pixels", (char *)&Dtty_winsize, sizeof(Dtty_winsize));

#endif

}  /*  dump_tty() */

/****************************************************************** 
*
*  dscpln - change the current line discpline
*
******************************************************************/

static int current_dscpln = 0;
void dscpln(void)
{

int rc;

current_dscpln++;
current_dscpln %= 5;

#ifndef SYS5
if (rc = ioctl(fds[1], TIOCSETD, (char *)&current_dscpln) < 0){  
    snprintf(msg, sizeof(msg), "Could not set discipline! RC=[%d] D={%d} (%s)", rc, current_dscpln, strerror(errno));
    dm_error(msg, DM_ERROR_BEEP);
    return;
}
#endif

snprintf(msg, sizeof(msg),"Line discipline set to %d",  current_dscpln);
dm_error(msg, DM_ERROR_BEEP);

} /* dscpln() */

#endif  /* DebuG */

/****************************************************************** 
*
*  get_stty - change the current line discpline
*             copy implies that on ultrix a copy of tty attributes should happen
*
******************************************************************/

#ifdef SYS5
#if defined(solaris) || defined(linux)
static struct termios gs_termio;
#else
static struct termio gs_termio;
#endif
#else
static struct sgttyb  gs_sgttyb = {
        0, 0, 0177, CKILL, EVENP|ODDP|ECHO|XTABS|CRMOD
};
static struct tchars  gs_tchars = {
        CINTR, CQUIT, CSTART,
        CSTOP, CEOF, -1,
};
#endif

void get_stty(void)
{
int fd, tfd1 = 0, tfd2 = 0;
int first_pass = 1;
int rc = -1;

fd = tty_fd;

#ifdef SYS5
 fd=0; /* tty_fd is bad place to start under solaris 2.3 10/27/94 */
 ioctl(fd, TCGETS, (char *) &gs_termio);  /* 10/27/94 */
              /* was (signed char) 10/27/93 */
while(rc || (((char)gs_termio.c_cc[VEOF] <=0) || ((char)gs_termio.c_cc[VINTR]<=0) || ((char)gs_termio.c_cc[VEOF]>63) || ((char)gs_termio.c_cc[VINTR]>63))){
    rc = 0;
    if (ioctl(fd, TCGETS, (char *) &gs_termio) < 0){
        snprintf(msg, sizeof(msg), "Can't get stty info ioctl(%d, GETMODES-SYSV(%s) failed.)", fd, strerror(errno));   
        DEBUG16(dm_error(msg, DM_ERROR_LOG);)
        rc++;
    }
    if (fd == 32) break;
    fd++;
    if (first_pass){
        first_pass = 0;
        fd = 0;
    }
    DEBUG16(fprintf(stderr, "get_stty(%d): rc=%d, int=%d, eof=%d\n", fd, rc, gs_termio.c_cc[VINTR] ,gs_termio.c_cc[VEOF]);)
    if (fd == 3){
       tfd1 = open("/dev/tty", O_RDONLY, 0);
       tfd2 = open("/dev/console", O_RDONLY, 0);
    }
}

if (fd != 32){
    ce_tty_char[TTY_EOF] = gs_termio.c_cc[VEOF];
    ce_tty_char[TTY_INT] = gs_termio.c_cc[VINTR];
    ce_tty_char[TTY_QUIT] = gs_termio.c_cc[VQUIT];
    ce_tty_char[TTY_ERASE] = gs_termio.c_cc[VERASE];
    ce_tty_char[TTY_KILL] = gs_termio.c_cc[VKILL];
}

if ((ce_tty_char[TTY_EOF] < 1) || (ce_tty_char[TTY_EOF] > 254)) 
    ce_tty_char[TTY_EOF] = 4;  /* sane default */

if ((ce_tty_char[TTY_INT] < 1) || (ce_tty_char[TTY_INT] > 254)) 
    ce_tty_char[TTY_INT] = 3;  /* sane default */

#else

while(rc || ((signed char)gs_tchars.t_intrc<=0) || ((signed char)gs_tchars.t_eofc<=0)){
    rc = 0;

    if (ioctl(fd, TIOCGETP, (char *) &gs_sgttyb) < 0){
        rc++;
        DEBUG16(dm_error("Could not GET_SGTTY(TIOCGETP)", DM_ERROR_LOG);)
    }

    if (ioctl(fd, TIOCGETC, (char *) &gs_tchars) < 0){
        rc++;
        DEBUG16(dm_error("Could not GET_STTY(TIOCGETC)", DM_ERROR_LOG);)
    }
    DEBUG16(fprintf(stderr, "get_stty(%d): rc=%d, int=%d, eof=%d\n", fd, rc, gs_tchars.t_intrc, gs_tchars.t_eofc);)
    if (fd == 32) break;
    fd++;
    if (first_pass){
        first_pass = 0;
        fd = 0;
    }else
        DEBUG16(fprintf(stderr, "Attempt to get INT/EOF on fd:%d\n", fd);)
    if (fd == 3){
       tfd1 = open("/dev/tty", O_RDONLY, 0);
       tfd2 = open("/dev/console", O_RDONLY, 0);
    }
} /* while can't get tty attributes */

if (rc || (!gs_tchars.t_intrc && !gs_tchars.t_eofc)){
    ce_tty_char[TTY_INT] = 3;
    ce_tty_char[TTY_EOF] = 4;  /* at least use some sane defaults */
    DEBUG16(fprintf(stderr, "Could not get INT/EOF characters assuming 03/04\n");)
    rc == -1;
}else{
    ce_tty_char[TTY_INT] = gs_tchars.t_intrc;
    ce_tty_char[TTY_QUIT] = gs_tchars.t_quitc;
    ce_tty_char[TTY_ERASE] = gs_sgttyb.sg_erase;
    ce_tty_char[TTY_KILL] = gs_sgttyb.sg_kill;
    ce_tty_char[TTY_EOF] = gs_tchars.t_eofc;
    ce_tty_char[TTY_START] = gs_tchars.t_startc;
    ce_tty_char[TTY_STOP] = gs_tchars.t_stopc;
    ce_tty_char[TTY_BRK] = gs_tchars.t_brkc;
}

#endif

if (tfd1) close(tfd1); 
if (tfd2) close(tfd2); 

intr_char[0] = '^';
intr_char[1] =  'A' -1 + ce_tty_char[TTY_INT]; 
intr_char[2] = '\0';

DEBUG16(fprintf(stderr, "Get_stty: TTY_EOF='%2d', TTY_INT='%2d'\n", ce_tty_char[TTY_EOF] ,ce_tty_char[TTY_INT] );)

} /* get_stty() */

/****************************************************************** 
*
*  	cp_stty - copy implies that on ultrix a copy of tty attributes should happen
*            get_stty must be run first!
*
******************************************************************/

void cp_stty(void)
{

#ifdef ultrix

DEBUG16(fprintf(stderr, "making copy of tty attributes\n");)

if (ioctl(tty_fd, TIOCSETP, (char *) &gs_sgttyb) < 0)
    DEBUG16(fprintf(stderr,"Could not CP_SGTTY(TIOCGETP)[tty_fd:%d]",tty_fd);)

if (ioctl(tty_fd, TIOCSETC, (char *) &gs_tchars) < 0)
    DEBUG16(fprintf(stderr,"Could not CP_STTY(TIOCGETC)[tty_fd:%d]", tty_fd);)

#endif   /* ultrix */

}  /* cp_stty() */

/**************************************************************
*
*   Routine online
*
*   Taken from liba.a, which is missing on this system.
*
*    This routine finds the first occurance of the second
*    string in the first and returns the offset of the
*    second string.
*
**************************************************************/

static int online(char *str1, char *str2)
{

char   c   = *str2;
char   *s1 = str1;
char   *s2 = str2;

int    len1 = strlen(s1);
int    len2 = strlen(s2);

while (len2 <= len1--){
   if (c == (*s1 | 040)){  /* case is independant */
      s2 = str2;
      while ((*s1 | 040) == *s2){s1++; s2++;}
      if (!*s2) return(1);/*  return(s1 - str1); */
   }
   s1++;
}

return(0);   /* (-1) */

} /* online() */

/**************************************************************
*
*   build_utmp - build a utmp entry
*
**************************************************************/
#ifndef UTMP_FILE
#define UTMP_FILE "/etc/utmp"
#endif
#ifndef WTMP_FILE
#define WTMP_FILE "/etc/wtmp"
#endif
#define PTYCHARLEN 2
/* the next two defines are for bsd systems which do not have a type field */
#ifndef DEAD_PROCESS
#define DEAD_PROCESS 8
#endif
#ifndef USER_PROCESS
#define USER_PROCESS 7
#endif

static void build_utmp(char *host)
{

int i, fd;
char   msg[256];
struct utmp *ut;

DEBUG16(fprintf(stderr, "@build_utmp, host=\"%s\"\n", (host ? host : "<NULL>"));)

init_utmp_ent(&utmp, DEAD_PROCESS, host);

setutent(); /* set up entry to search for */
ut = getutid(&utmp);  /* position to entry in utmp file */
if (ut){
    DEBUG16(dump_utmp(&utmp, "build_utmp");)
}else
    DEBUG16(fprintf(stderr, "No old utmp entry found\n");)

init_utmp_ent(&utmp, USER_PROCESS, host);
errno=0;
pututline(&utmp);
if (errno != 0){
    snprintf(msg, sizeof(msg), "Cannot create utmp entry, pututline failed (%s)", strerror(errno));
    dm_error(msg, DM_ERROR_LOG);
    return;
}
else
    DEBUG16(fprintf(stderr, "Utmp entry set.\n");)

#ifdef WTMP
       if ((fd = open(WTMP_FILE, O_WRONLY | O_APPEND)) >= 0) {
		    write(i, (char *)&utmp, sizeof(struct utmp));
		    close(i);
		}
#endif

endutent();		/* close the file */

} /* build_utmp() */

/**************************************************************
*
*   kill_utmp - remove a utmp entry
*
**************************************************************/

void kill_utmp(void)
{
int fd;
struct utmp utmp;
struct utmp *utptr;
#ifdef DebuG
int    rc = 1;
#endif

/* if we did not set the utmp entry, do not try to clear it */
if (!utmp_set)
    return;

if (geteuid()) 
#ifdef DebuG
   rc =
#endif
#ifdef _INCLUDE_HPUX_SOURCE
   setresuid(-1, 0, -1);
#else
   seteuid(0);
#endif
else
   DEBUG16(fprintf(stderr, "Already root!\n");)

DEBUG(if (rc != 0) fprintf(stderr, "Could not seteuid back to root (%s)\n", strerror(errno));)

DEBUG16(fprintf(stderr, "ID(%d, %d)\n", getuid(), geteuid());)

init_utmp_ent(&utmp, USER_PROCESS, NULL);
DEBUG16(dump_utmp(&utmp, "kill_utmp, looking for");)

setutent();
utptr = getutid(&utmp);

if (utptr){
    DEBUG16(dump_utmp(utptr, "kill_utmp, found");)
    utptr->ut_time = time(NULL);
#if (defined(sun) && !defined(solaris)) || defined(ultrix)
    removeutline(utptr);
#else
    utptr->ut_type = DEAD_PROCESS;
    errno=0;
    pututline(utptr);   /* was DEBUG0 */
    DEBUG16(if (errno != 0) fprintf(stderr, "Could not delete utmp entry, pututline failed (%s)\n", strerror(errno));)
    DEBUG16(if (errno == 0) fprintf(stderr, "Utmp entry cleared.\n");)
#endif


#ifdef WTMP /* set wtmp entry if wtmp file exists */
    if ((fd = open(WTMP_FILE, O_WRONLY | O_APPEND)) >= 0) {
        i = write(fd, utptr, sizeof(utmp));
        i = close(fd);
    }
#endif
}else      /* was DEBUG0 */
    DEBUG16(fprintf(stderr, "Could not find and clear utmp entry! (%s)\n", strerror(errno));)

endutent();

if (getuid())
#ifdef _INCLUDE_HPUX_SOURCE
   setresuid(-1, getuid(), -1);
#else
   seteuid(getuid());
#endif

} /* kill_utmp(void) */

/**************************************************************
*
*   init_utmp_ent - initialize the utmp data
*
**************************************************************/

void init_utmp_ent(struct utmp *utmp, int type, char *host)
{
static struct passwd *pw_ptr = NULL;
static struct passwd  pw;
int    len;
char  *ptr;
char   msg[256];


memset((char *)utmp, 0, sizeof(struct utmp));

if (!pw_ptr){
    setpwent();  /* make sure password file is rewound, cp, processing could cause this to be needed */

    pw_ptr = getpwuid(getuid());
    if (!pw_ptr){
        snprintf(msg, sizeof(msg), "Could not get passwd entry for uid %d (%s), utmp not created", getuid(), strerror(errno));
        dm_error(msg, DM_ERROR_LOG);
        return;
    }
   pw = *pw_ptr;
   endpwent();
}

/*#undef IBMRS6000  /* commenting this line out toggles to IBM alternate /dev method */
DEBUG16(fprintf(stderr, "init_utmp_ent: tty_name \"%s\"\n", tty_name);)
DEBUG16(fprintf(stderr, "init_utmp_ent: pty_name \"%s\"\n", pty_name);)

/***************************************************************
*  Set the id field of the utmp structure.  There is no id
*  field on Sun/OS 4.1.3, so this gets ifdef'ed out
***************************************************************/
len = strlen(tty_name);
#if defined(SYS5)
#   if defined(_INCLUDE_HPUX_SOURCE) || defined(solaris)
    /*ifdef solaris  RES 1/6/1999 */
        ptr = strrchr(tty_name, '/');
        if (ptr) strlcpy(utmp->ut_id, ptr+1, sizeof(utmp->ut_id));
#   else
        if (len>1) strlcpy(utmp->ut_id, &tty_name[len-2], sizeof(utmp->ut_id));
#   endif
#else  /* (BSD)  */
#    if defined(IBMRS6000)
        if (len>4) strlcpy(utmp->ut_id, tty_name + strlen("/dev/"), sizeof(utmp->ut_id));
#    else
#       if !defined(sun) && !defined(ultrix)
            if (len >= PTYCHARLEN) strlcpy(utmp->ut_id, pty_name + strlen(pty_name)-PTYCHARLEN, sizeof(utmp->ut_id));
#       endif
#   endif
#endif

/***************************************************************
*  Set the line field of the utmp structure.
***************************************************************/
#if defined(IBMRS6000) || defined(sun) || defined(_INCLUDE_HPUX_SOURCE)
    strlcpy (utmp->ut_line, tty_name + strlen("/dev/"), sizeof(utmp->ut_line));
#else
#   if defined(SYS5)
       strlcpy(utmp->ut_line, strrchr(tty_name,'/')+1, sizeof(utmp->ut_line));
#   else
       strlcpy (utmp->ut_line, pty_name + strlen("/dev/"), sizeof(utmp->ut_line));
#   endif
#endif

/***************************************************************
*  Set the user name field of the utmp structure.
***************************************************************/
strlcpy(utmp->ut_name, pw.pw_name, sizeof(utmp->ut_name));

#ifndef solaris
strlcpy(utmp->ut_host, (host ? host : ""), sizeof(utmp->ut_host));
#endif

#if (!defined(sun) && !defined(ultrix)) || defined(solaris)
utmp->ut_type = type;
#ifndef linux
utmp->ut_exit.e_exit = 0;
utmp->ut_exit.e_termination = 0;
#endif
strlcpy(utmp->ut_user, pw.pw_name, sizeof(utmp->ut_user));

utmp->ut_pid = chid;  /* chid is global, the process id of the child running the shell */

#endif

utmp->ut_time = time(NULL);

DEBUG16(dump_utmp(utmp, "init_utmp_ent");)

} /* end of init_utmp_ent */

#ifdef DebuG
/**************************************************************
*
*   dump_utmp - dump a utmp entry
*
**************************************************************/

void dump_utmp(struct utmp *utmp, char *header)
{
#if !defined(sun) && !defined(ultrix) && !defined(linux) || defined(solaris)
#ifdef solaris
fprintf(stderr, "%s: USER=\"%s\" id=\"%s\" line=\"%s\" pid=%d type=%d, exit=%d, termination=%d, time=%s",
#else
fprintf(stderr, "%s: USER=\"%s\" id=\"%s\" line=\"%s\" pid=%d type=%d, exit=%d, termination=%d, time=%s, host=\"%s\"",
#endif
        header, utmp->ut_name, utmp->ut_id, utmp->ut_line, 
        utmp->ut_pid, utmp->ut_type, utmp->ut_exit.e_exit, utmp->ut_exit.e_termination, ctime(&utmp->ut_time)
#ifndef solaris
, utmp->ut_host
#endif
);
#else
fprintf(stderr, "%s: line=\"%s\" type=\"%s\", host=\"%s\", time=%s\n",
                header, utmp->ut_line, utmp->ut_name, utmp->ut_host, ctime(&utmp->ut_time));
#endif
} /* end of dump_utmp */
#endif  /* DebuG */

#if (defined(sun) && !defined(solaris)) || defined(ultrix)
/**************************************************************
*
*   getutid... - These 5 calls are not on BSD boxes so I code them here
*
**************************************************************/

int ut_fd = -1;
int tslot = -1;
static struct utmp cur_utmp;

struct utmp *getutid(struct utmp *utmp)
{
if ((tslot < 0) || (ut_fd < 0)){
    DEBUG16(if (ut_fd < 0) fprintf(stderr, "Call to getutid with closed utmp file\n"); else  fprintf(stderr, "No tslot for this terminal\n");)
    return(NULL);
}
if (!ut_fd)
    setutent();

lseek(ut_fd, (long)(tslot * sizeof(struct utmp)), 0);
read(ut_fd, (char *)&cur_utmp, sizeof(struct utmp));
return(&cur_utmp);
}  /* getutid() */

/**************************************************************/

void pututline(struct utmp *utmp)
{
if (ut_fd > 0){
    lseek(ut_fd, (long)(tslot * sizeof(struct utmp)), 0);
    write(ut_fd, (char *)utmp, sizeof(struct utmp));
}else
    DEBUG16(fprintf(stderr, "Call to pututline with closed utmp file\n");)
} /* pututline() */

/**************************************************************/

void removeutline(struct utmp *utmp)
{
if (ut_fd < 0){
    DEBUG16(fprintf(stderr, "Call to removeutline with closed utmp file\n");)
    return;
}
memset((char *)utmp, 0, sizeof(struct utmp));
lseek(ut_fd, (long)(tslot * sizeof(struct utmp)), 0);
write(ut_fd, (char *)utmp, sizeof(struct utmp));
} /* removeutline() */

/**************************************************************/

void setutent()
{ 
char   msg[256];
tslot = ttyslot();
if (ut_fd < 0){
    ut_fd = open(UTMP_FILE, O_RDWR);
    if (ut_fd < 0){
        snprintf(msg, sizeof(msg), "Could not open '%s': %s", UTMP_FILE, strerror(errno));
        dm_error(msg, DM_ERROR_BEEP);
    }
}
DEBUG16(fprintf(stderr, "setutent: tslot=%d, utmp fd=%d\n", tslot, ut_fd);)

}  /* setutent() */

/**************************************************************/

void endutent()
{
if (ut_fd > 0){
    close(ut_fd);
    ut_fd = -1;
}
}  /* endutent() */

#endif /* getut.. */

/**************************************************************/

int utmp_setup()
{
int  rc;

if (geteuid() && (access(UTMP_FILE, 06) != 0)){  /* can we write on utmp? */
    DEBUG16(fprintf(stderr, "utmp_setup: Write access to %s denied, utmp option canceled (%s)\n", UTMP_FILE, strerror(errno));)
    rc = -1;
}else{
    utmp_set = True;
    rc = 0;
}

return(rc);

}  /* endutent() */

/*************************************************************
*
* ioctl_setup()  - does the appropriate IOCTL manipulations against
*                 the psuedo tty device
*
**************************************************************/

static int ioctl_setup(int master_pty_fd)
{

struct passwd *pw;
int            fds1;

/*  
 * setsid releases makes us the session leader and gets rid of the control terminal.
 */

/*#if !defined(ultrix) /* && !defined(solaris) */
/* executed in parent on ultrix and solaris */
isolate_process(chid);
/*#endif*/

 /* (fds[1] = pty_slave(tty_fd)) < 0) */
if ((fds1 = pty_slave(master_pty_fd)) < 0){
    fprintf(stderr, "Can't open slave tty: '%s' (%s)\n", tty_name, strerror(errno));   
    exit(-1);
}

#ifdef solaris
return(fds1);
#else

if (tty_setmode(fds1) < 0){
    fprintf(stderr, "Can't set slave terminal modes (%s)\n", strerror(errno));   
    exit(-1);
}

setpwent();  /* make sure password file is rewound, cp, processing could cause this to be needed */
if (!geteuid() && (pw = getpwuid(getuid())))
   initgroups(pw->pw_name, pw->pw_gid);
endpwent();

return(fds1);

#endif
} /* ioctl_setup() */

/*************************************************************
*
*  isolate_process()  - get a new process group
*
**************************************************************/

void isolate_process(int chid)
{

int i, fd;
char *ptr;

#ifdef __alpha
if (!chid) chid = getpid();
#endif

#if defined(ultrix)
setpgrp(0, 0);              /* IBM style */
close(open("/dev/tty", O_WRONLY, 0)); 
setpgrp (0, 0); /* second parm was chid */
#endif

#if defined(ultrix) && defined(SU_BLAH)
tcsetpgrp(fds[1], getpid());
#endif

i = setsid();
if (i < 0){	/* become session leader and lose controlling tty DEC style */
    DEBUG16(fprintf(stderr, "isolate_process: pid %d  Can't setsid (%s)\n", getpid(), strerror(errno));)   
}else
    DEBUG16(fprintf(stderr, "isolate_process: pid %d  setsid returned %d\n", getpid(), i);)   

#ifdef DECBOX
#ifdef TIOCSCTTY   /* DEC OSF tty(7) man page says this is needed */
fd = open("/dev/console", O_RDWR);
if ((fd >= 0) && (ioctl(fd, TIOCSCTTY, (char *)&chid) < 0))   /* BSD style */
    DEBUG16(fprintf(stderr, "Can't ioctl(TIOCSCTTY-II(%s))\n", strerror(errno));)   
#endif
#endif

#if !defined(ultrix) && !defined(SYS5)
DEBUG16(fprintf(stderr, "Attempting TIOCNOTTY()\n");)
fd = open("/dev/tty", O_RDWR, 0); 

if (fd < 0)  /* solaris 10/1/93 */
    fd = open("/dev/console", O_RDWR, 0);

if (fd >= 0){  /* dissassociate control terminal */
   if (ioctl(fd, TIOCNOTTY, (char *) 0) < 0)
       DEBUG16(fprintf(stderr, "Can't ioctl(TIOCNOTTY(%s))\n", strerror(errno));)   
   close(fd);
}else
   DEBUG16(fprintf(stderr, "Could not open(/dev/(tty:console)); No TIOCNOTTY(%s)\n", strerror(errno));)
#endif

#if defined(ultrix)
setpgrp(0, 0);               /* IBM style */
close(open("/dev/console", O_WRONLY, 0)); 
setpgrp (0, 0); /* second parm was chid */
#endif

#ifdef ultrix
if (!chid) chid = getpid();
#endif
#

} /* isolate_process() */

/*************************************************************
*
*  dump_ttya - bob's dump a sys5 tty
*
**************************************************************/

#if defined(SYS5) && defined(DebuG)
void dump_ttya(int tty, char *title)
{
#if defined(solaris) || defined(linux)
static struct termios tty_termios;
#endif
static struct termio  tty_termio;
int    i;

fprintf(stderr, "%s  ->  fd = %d\n", title, tty);

#ifdef solaris
if (ioctl(tty, TCGETS, (char *) &tty_termios) < 0)
   {
      fprintf(stderr, "Can't ioctl(TCGETS with termios (%s)), fd = %d\n", strerror(errno), tty);   
   }
else
   {
      fprintf(stderr, "TCGETS with termios, fd = %d\n", tty);
      fprintf(stderr, "%d, c_iflag  = 0x%08X\n", tty, tty_termios.c_iflag);
      fprintf(stderr, "%d, c_oflag  = 0x%08X\n", tty, tty_termios.c_oflag);
      fprintf(stderr, "%d, c_cflag  = 0x%08X\n", tty, tty_termios.c_cflag);
      fprintf(stderr, "%d, c_cc     = 0x", tty);
      for (i = 0; i < NCCS; i++)
         fprintf(stderr, "%02X", (unsigned int)tty_termios.c_cc[i]);
      fprintf(stderr, "\n\n");
   }
#endif


/* Now try TCGETA */


if (ioctl(tty, TCGETA, (char *) &tty_termio) < 0)
   {
      fprintf(stderr, "Can't ioctl(TCGETA with termio (%s)), fd = %d\n", strerror(errno), tty);   
   }
else
   {
      fprintf(stderr, "TCGETA with termio, fd = %d\n", tty);
      fprintf(stderr, "%d, c_iflag  = 0x%04X\n", tty, tty_termio.c_iflag);
      fprintf(stderr, "%d, c_oflag  = 0x%04X\n", tty, tty_termio.c_oflag);
      fprintf(stderr, "%d, c_cflag  = 0x%04X\n", tty, tty_termio.c_cflag);
      fprintf(stderr, "%d, c_lflag  = 0x%04X\n", tty, tty_termio.c_lflag);
      fprintf(stderr, "%d, c_line   = 0x%02X\n", tty, tty_termio.c_line);
      fprintf(stderr, "%d, c_cc     = 0x", tty);
      for (i = 0; i < NCC; i++)
         fprintf(stderr, "%02X", (unsigned int)tty_termio.c_cc[i]);
      fprintf(stderr, "\n\n");
   }

}  /*  dump_tty() */

#endif  /* SYS5 */

#endif  /* PAD */    

