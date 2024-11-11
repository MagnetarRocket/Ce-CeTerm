#ifndef HAVE_USLEEP
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
*  module usleep.c
*
*  The dec ultrix box does not support the usleep call but does
*  have setitimer.  From this we build our own usleep. Note that
*  this module is only included in the makefile for dec.
*
*  Routines:
*         usleep                - sleep for the passed number of microseconds
*
*  Internal:
*         catch_alarm           - Dummy signal catcher.
*
***************************************************************/

#include <stdio.h>
#include <sys/types.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <signal.h>   /* /usr/include/signal.h /usr/include/sys/signal.h */
#endif
#include <errno.h>


#ifndef WIN32
void catch_alarm()
{
} /* return */

int usleep(unsigned int micro_secs)
{
struct itimerval   new, old;
#ifdef USE_BSD_SIGNALS
struct sigvec      new_vec, old_vec;
int            old_mask;
#else
struct sigaction    new_vec, old_vec;
#endif

   if (micro_secs == 0)
       return(0);

   new.it_interval.tv_sec = 0;
   new.it_interval.tv_usec = 0; /* We only want one tick */
   new.it_value.tv_sec = micro_secs / 1000000;
   new.it_value.tv_usec = micro_secs % 1000000;

#ifdef USE_BSD_SIGNALS
   new_vec.sv_handler = catch_alarm;
   new_vec.sv_mask= 0;
   new_vec.sv_flags = 0;

   old_mask = sigblock(sigmask(SIGALRM));
   sigvec(SIGALRM, &new_vec, &old_vec);
#else
   new_vec.sa_handler = catch_alarm;
   sigemptyset(&new_vec.sa_mask);
   new_vec.sa_flags = 0;
   sighold(SIGALRM);
   sigaction(SIGALRM, &new_vec, &old_vec);
#endif

   setitimer(ITIMER_REAL, &new, &old);

#ifdef USE_BSD_SIGNALS
   sigpause(0);
   sigvec(SIGALRM, &old_vec, (struct sigvec *)0);
   sigsetmask(old_mask);
#else
   sigpause(SIGALRM);
/* sigaction(SIGALRM, &old_vec, (struct sigaction *)0);  KBP */
/* sigrelse(SIGALRM);                                         */
   sigignore(SIGALRM);                                         
#endif

/*   setitimer(ITIMER_REAL, &old, (struct itimerval *)0); KBP */
   return(0);

} /* end of usleep */

#else  /* is WIN32 */
int usleep(unsigned int micro_secs)
{
HANDLE          timer_handle;

/* create an event handle which is non-signaled */
timer_handle = CreateEvent(NULL, TRUE, FALSE, NULL);
if (!timer_handle)
   return(-1);

/* wait for the handle, which will never be set, and allow a timeout */
WaitForSingleObject(timer_handle, (micro_secs + 500) / 1000);

CloseHandle(timer_handle);
return(0);

} /* end of usleep */

#endif
#endif /* HAVE_USLEEP not defined */

