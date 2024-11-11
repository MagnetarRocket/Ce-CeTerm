/*static char *sccsid = "%Z% %M% %I% - %G% %U%";*/
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
*  Routines in emalloc.c
*     malloc_error       - Handle malloc failure. Called from macro CE_MALLOC
*     malloc_init        - CE_MALLOC / malloc_error setup.
*     malloc_copy        - Copy a string into malloc'ed area.
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h       */
#include <string.h>          /* /usr/include/string.h      */
#include <signal.h>         /* /usr/include/signal.h      */
#include <stdlib.h>         /* needed on linux for exit */

#ifdef WIN32
#include <stdlib.h>
#endif

#include "debug.h"
#include "dmwin.h"
#include "getevent.h"
#include "emalloc.h"
#include "memdata.h"
#include "undo.h"
#include "vt100.h"   /* needed for free_drawable */
#include "windowdefs.h"
#include "xerror.h"

#define MIN_RECOVERED_STORAGE 4096

static void nop();

static char *pre_saved_storage = NULL;
static int   recursive_flag = False;


/************************************************************************

NAME:      malloc_error

PURPOSE:    This routine is called by macro CE_MALLOC (in "malloc.h") when
            the system malloc(3) function returns NULL.  This routine
            does a garbage collection and if enough storage was recovered,
            retries the request.  If the retry fails, it sees if there is
            enough space retrieved and fails the current malloc request.
            If it is dead out of space, it attempts to create a crash file
            and then exits.

PARAMETERS:

   1.  size   - int (INPUT)
                This is the size which was to be malloc'ed.

   2.  file -  pointer to char (INPUT)
               This is the file name __FILE__ of the routine which called
               malloc_error.

   3.  line -  int (INPUT)
               This is the line in the file (__LINE__) of the routine which called
               malloc_error.


FUNCTIONS :

   1.   Free the undo lists and the dm_output memdata area.

   2.   Build a new empty dm_output memdata area.

   3.   If we got back alot of space, retry the request to malloc.  If it works,
        return the malloc'ed space with a message output to the dm window.

   4.   Turn off any background work.

   5.   If we got enough storage back, reverse video the screen and 
        return failure (NULL) with a message about being out of space.

   6.   If we are really dead out of room, free the storage allocated
        by malloc_init and try to create a crash file.  Then exit.

RETURNED VALUE
   area  -  pointer to char
            If the retry of the malloc worked, the area requested is
            returned.  Otherwise, NULL is returned.

*************************************************************************/

void *malloc_error(int size, char *file, int line)
{
int total;
char *area;

DEBUG(fprintf(stderr, "CE_MALLOC_ERROR!: of %d bytes in line %d of %s.\n", size, line, file);)

if (!recursive_flag)
   {
      recursive_flag = True;

      /***************************************************************
      *  
      *  Free as much space as we can.
      *  
      ***************************************************************/

      total = kill_event_dlist(dspl_descr->main_pad->token); 
      total += kill_event_dlist(dspl_descr->dminput_pad->token); 
      total += mem_kill(dspl_descr->dmoutput_pad->token);
      dspl_descr->dmoutput_pad->token = NULL;

      DEBUG(fprintf(stderr, "Recovered %d bytes in garbage collection\n", total);)

      dspl_descr->dmoutput_pad->token = mem_init(100, False);
      WRITABLE(dspl_descr->dmoutput_pad->token) = False;

      /***************************************************************
      *  
      *  If we got back a fair amount of space, try the malloc again.
      *  If it works, return it.
      *  
      ***************************************************************/

      if (total > ((MIN_RECOVERED_STORAGE * 2) + size))
         if ((area = (char *)malloc(size)) != NULL)
            {
               dm_error("Ce: ran out of memory, undo list deleted, garbage collection worked", DM_ERROR_BEEP);
               recursive_flag = False;
               return(area);
            }


      /***************************************************************
      *  
      *  Turn off any background work going on.
      *  
      ***************************************************************/

      change_background_work(NULL, BACKGROUND_FIND, False);
      change_background_work(NULL, BACKGROUND_SUB, False);
      change_background_work(NULL, BACKGROUND_KEYDEFS, False);
      change_background_work(NULL, MAIN_WINDOW_EOF, True);
   }

/***************************************************************
*  
*  If we have received a reasonable amount of storage back
*  through garbage collection, give the user a chance to save
*  the file.  Otherwise, free the emergency ration of storage
*  and use it to try to create a crash file.
*  
***************************************************************/

if (total >= MIN_RECOVERED_STORAGE)
   {
      dm_error("Out of memory, try to save the file. (Out of disk swap space)\n", DM_ERROR_LOG); 
      recursive_flag = False;
      return(NULL);
   }
else
   {
      if (pre_saved_storage)
         {
           free(pre_saved_storage);
           pre_saved_storage = NULL;
#ifdef OMVS
           create_crash_file();
           exit(32);
#else
           if (create_crash_file() == 0){
#ifdef WIN32
              fprintf(stderr, "Ce: Cannot find any swap space!, Crash File Created\n"); 
               exit(32);
#else
              fprintf(stderr, "Ce: Cannot find any swap space! I will sleep until you can clear sufficient space.\n"); 
              fprintf(stderr, "    I will resume after being sent a SIGCONT signal (my pid is %d).\n", getpid()); 
              signal(SIGCONT, nop);                                  
              pause(); /* exit(32); */
              return(malloc_error(size, file, line));
#endif
           }
#endif
         }
      else
         {
            fprintf(stderr, "Ce: out of swap space! (Can not load file)\n");
            exit(32);
         }
   }
return(NULL);
} /* end of malloc_error */

/************************************************************************

NAME:      malloc_init

PURPOSE:    This routine is called during initialization.  It mallocs an
            amount of storage to be hoarded away in case we run out.
            If we completely run out of memory, the storage is freed and
            used by the routine attempting to create a crash file.

PARAMETERS:

   1.  size   - int (INPUT)
                This is the size which is to be malloc'ed an hoarded away.

FUNCTIONS :

   1.   Malloc the presaved storage.


*************************************************************************/

void malloc_init(int size)
{
  pre_saved_storage = (char *)malloc(size);
}

static void nop(int sig){}


/************************************************************************

NAME:      malloc_copy            -   Copy a string into malloc'ed area.

PURPOSE:    This routine checks to see that the atom ids needed by wdf have
            been gotten from the server.

PARAMETERS:

   1.  str         - pointer to char (INPUT)
                     This is the string to copy

FUNCTIONS :

   1.   Malloc space for the new string.

   2.   If the malloc worked, copy the string over.  
        On malloc failure, dm_error has already been called.

RETURNED VALUE:
   copy   -   pointer to char
              A pointer to the copy of the input string in malloc'ed
              storage is returned.

*************************************************************************/

char *malloc_copy(char   *str)
{
char          *copy;

if (str == NULL)
   return(NULL);

copy = CE_MALLOC(strlen(str)+1);
if (copy)
   strcpy(copy, str);

return(copy);

} /* end of malloc_copy */



