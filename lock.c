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
*  module lock.c
*
*  Routines:
*     lock_init              -   Set up initial lock data for a file
*     lock_file              -   Issue advisory lock on a file
*     update_lock_data       -   Update stats on a file when it is rewritten
*     unlock_file            -   Release advisory lock on a file
*     lock_rename            -   Deal with pad name
*     save_file_stats        -   Save stats to detect file changed.
*     detect_changed_file    -   See if the passes stat buff matches the last saved
*     file_has_changed       -   Get stats for current file and compare against the saved stats
*
* Internal:
*
*
* Lock Overview:
* The lockf system call is used to lock the file while it is in 
* read/write mode.
* 
* The init call is issued early on to save the
* stats buffer for the file if it already exists.  If it does not
* exist, the routine is not called and the locking enabled flag
* is left set to false.
* 
* The lock_file routine is used to put the file into lock mode when 
* it is put into r/w mode.  If the file descriptor held local to this
* module is not open, it is opened in read/write mode as is required
* for use with lockf.  A stat call is used prior to the lock and
* the size and date of maintenance are checked to see if the file
* has changed.  If so, the lock is not issued and a message indicating
* that the file has changed is output.  If the file has not
* been changed by someone else, an attempt is made to lock the file
* with lockf starting at offset zero for the entire file.  The
* non-blocking lock is used and if it fails, an error message about
* being in use is produced and locking fails.
* 
* Unlock file releases the lock when ro mode changes to read only.
* 
* update_lock_data is called after a pad write.  It gets the new
* date maintained and size.  AFS has problems getting the updated
* information to the program which would cause problems when an
* attempt is made to go into r/w mode.  Hopefully using fstat against
* the open file descriptor will help.  If not, an attempt to 
* identify AFS files and disable locking will be made in lock_init.
* The device number for AFS seems to be x'00000001' on HP, Solaris,
* SunOS, and AIX.  I could use that value to supress the locking.
* Supression is ok since AFS does not support locking anyway.
* If the file did not exist before the first pad write, the initialization
* sequence is used to establish locking.
* 
* lock_rename is used when a pad name command (pn) is issued.  If
* the file is actually being renamed, the new stat data is obtained.
* If a -c is used to create a new file, the locking will be turned
* off till the file is created.
* 
* ***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <fcntl.h>          /* /usr/include/fcntl.h */
#include <limits.h>         /* /usr/include/limits.h     */
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif
#include <sys/types.h>    /* /usr/include/sys/types.h   */
#include <sys/stat.h>     /* /usr/include/sys/stat.h    */
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>       /* /usr/include/unistd.h      */
#endif


#ifdef ARPUS_CE
#include "debug.h"
#include "dmwin.h"
#else
#define dm_error(m,j) fprintf(stderr, "%s\n", m)
#define DEBUG27(x)  {}
#ifndef True
#define True 1
#endif
#ifndef False
#define False 0
#endif
#endif
#include "lock.h"

/***************************************************************
*  
*  Static data related to file locking.  Only one per file.
*  Thus it is not put in the display description.
*
*  Global (to this module) data descriptions:  
*  locking_enabled   -  Boolean flag
*                       This flag is set if file locking is enabled.
*                       The LOCKF value from the argument list and/or
*                       X resource is not involved.  It is handled by
*                       the caller which has access to the display
*                       description containing this information.  The
*                       C preprocessor definition DO_AFS is used to
*                       turn file locking on and off for AFS.  AFS does not
*                       support file locking, but the file changed checking
*                       could be useful.
*
*  file_locked       -  Boolean flag
*                       This flag is set when the file is actually in the
*                       locked state.
*
*  lock_fd           -  File descriptor
*                       This file descriptor is opened r/w on the file
*                       and is used with the lockf call.  It is set to -1
*                       when closed.
*
*  lock_file_stats   -  Stat buffer
*                       This is set by lock_init and update_lock_data
*                       to the stats we know about.  It is used by lock_file
*                       to detect if the file changed.
*
*  lock_file_stats   -  Stat buffer
*                       This is set by save_file_stats and update_lock_data
*                       to the stats we know about.  It is used by 
*                       detect_changed_file to see if the file changed.
*
***************************************************************/

static int            locking_enabled = False;
static int            file_locked = False;
static int            lock_fd = -1;
static struct stat    lock_file_stats;
static struct stat    saved_file_stats;

#define DO_AFS


/************************************************************************

NAME:      lock_init              -   Set up initial lock data for a file

PURPOSE:    This routine initializes lock data.

PARAMETERS:

   1.  file_stats -  pointer to struct stat (INPUT)
                     These are the file stats for the file being edited.  The
                     file does exist.

FUNCTIONS :

   1.   Check for the AFS device number.  If this is an AFS file,
        locking does not work and is thus disabled.

   2.   Copy the passed statistics to the static buffer.

   3.   Show that locking is enabled.

*************************************************************************/

void  lock_init(struct stat    *file_stats)
{

#if !defined(FREEBSD) && !defined(WIN32)
#ifndef DO_AFS
if (file_stats->st_dev == 1) /* AFS */
   {
      locking_enabled = False;
      return;
   }
#endif

DEBUG27(fprintf(stderr, "lock_init: locking enabled\n");)
memcpy((char *)&lock_file_stats, (char *)file_stats, sizeof(lock_file_stats));
locking_enabled = True;
#endif

} /* end of lock_init */


/************************************************************************

NAME:      lock_file              -   Issue advisory lock on a file


PURPOSE:    This routine performs file locking when shifting into 
            read/write (edit) mode in ce.

PARAMETERS:
   1.  file_stats -  pointer to struct stat (INPUT)
                     These are the file stats for the file being edited.  The
                     file does exist.

FUNCTIONS :
   1.   If locking is disabled, return that True to show the
        file is as locked as it is going to get.

   2.   If the locking file descriptor is not open, open it for
        read/write as required by lockf(2).  If this fails
        issue a message and return False.

   3.   Show that locking is enabled.

RETURNED VALUE:
   locked   -   Int flag
                True  -  File locked, if locking was enabled.
                False -  File could not be locked.

*************************************************************************/

int   lock_file(char    *edit_file)
{
#ifndef WIN32
struct stat     current_stats;
char            msg[1500];
#endif


/***************************************************************
*  Locking must be enabled,
*  Disabled for AFS and new files.  
***************************************************************/
if ((!locking_enabled) || file_locked)
   return(True);

#if !defined(FREEBSD) && !defined(WIN32)
/***************************************************************
*  Get the current stats.  If the file has been changed,
*  disallow the lock.
***************************************************************/
if (stat(edit_file, &current_stats) < 0)
   {
      snprintf(msg, sizeof(msg), "Could not get file stats for locking (%s)", strerror(errno));
      dm_error(msg, DM_ERROR_LOG);
      return(False);
   }

if (detect_changed_file(&current_stats))
   {
      dm_error("File has been changed since you opened it, cannot change to write mode", DM_ERROR_BEEP);
      if (lock_fd >= 0)
         {
            close(lock_fd);
            lock_fd = -1;
         }
      return(False);
   }

/***************************************************************
*  Open the locking file descriptor if needed.
*  Failure of the open is failure.
***************************************************************/
if (lock_fd < 0)
   if ((lock_fd = open(edit_file, O_RDWR)) < 0)
      {
         snprintf(msg, sizeof(msg), "Could not open file for locking (%s)", strerror(errno));
         dm_error(msg, DM_ERROR_LOG);
         return(False);
      }

/***************************************************************
*  Now try the lock.
*  Lock the whole file.
***************************************************************/
if (lockf(lock_fd, F_TLOCK, 0) < 0)
   {
      DEBUG27(fprintf(stderr, "lock_file, lockf failed, errno = %d, file %s\n", errno, edit_file);)
      if ((errno == EAGAIN) || (errno == EACCES))
         snprintf(msg, sizeof(msg), "File in use %s (%s)", edit_file, strerror(errno));
      else
         snprintf(msg, sizeof(msg), "Cannot lock %s (%s)", edit_file, strerror(errno));
      dm_error(msg, DM_ERROR_LOG);
      return(False);
   }
else
   {
      DEBUG27(fprintf(stderr, "File %s locked\n", edit_file);)
      file_locked = True;
   }

#endif
return(True);

} /* end of lock_file */


/************************************************************************

NAME:      unlock_file              -   Release advisory lock on a file


PURPOSE:    This routine releases the advisory lock when leaving r/w mode.

PARAMETERS:
   None

FUNCTIONS :
   1.   If we are not locked. Do nothing.

   2.   Release the lock.

*************************************************************************/

void  unlock_file(void)
{
#if !defined(FREEBSD) && !defined(WIN32)
char            msg[1500];


/***************************************************************
*  If we are not locked, there is nothing to do.
***************************************************************/
if (!locking_enabled || (lock_fd < 0) || !file_locked)
   return;

/***************************************************************
*  Unlock the file.
***************************************************************/
if (lockf(lock_fd, F_ULOCK, 0) < 0)
   {
      snprintf(msg, sizeof(msg), "Cannot unlock file (%s)", strerror(errno));
      dm_error(msg, DM_ERROR_LOG);
   }
else
   {
      DEBUG27(fprintf(stderr, "File unlocked\n");)
      file_locked = False;
   }
#endif

} /* end of unlock_file */


/************************************************************************

NAME:      update_lock_data       -   Update stats on a file when it is rewritten


PURPOSE:    This routine reloads the new stats after a pad write so we can
            go back into r/w mode on it.

PARAMETERS:
   none

FUNCTIONS :

   1.   Get the stats for the file into the global stats buffer.

RETURNED VALUE:
   update_worked  -  int flag
                     True  - The statistics were updated
                     False - There was trouble, switch to read only mode

*************************************************************************/

void  update_lock_data(char    *edit_file)
{
#if !defined(WIN32)
struct stat     current_stats;
char            msg[1500];
int             update_worked;

if (stat(edit_file, &current_stats) < 0)
   {
      snprintf(msg, sizeof(msg), "Cannot load updated file statistics (%s)", strerror(errno));
      dm_error(msg, DM_ERROR_LOG);
      return;
   }

#if !defined(FREEBSD) 
/***************************************************************
*  
*  If the inode has changed, someone else has been messing with
*  the file.
*  
***************************************************************/

if (locking_enabled && (current_stats.st_ino != lock_file_stats.st_ino))
   {
      snprintf(msg, sizeof(msg), "Someone changed the file and we overwrote it, %s", edit_file);
      dm_error(msg, DM_ERROR_LOG);
      unlock_file();
      if (lock_fd >= 0)
         {
            close(lock_fd);
            lock_fd = -1;
         }
   }

DEBUG27(fprintf(stderr, "Lock data updated %s\n", edit_file);)
lock_init(&current_stats);

if (!file_locked)
   lock_file(edit_file);
#endif /* not freebsd */

save_file_stats(&current_stats);
#endif /* not win32 */

} /* end of update_lock_data */


/************************************************************************

NAME:      lock_rename            -   Deal with pad name

PURPOSE:    This routine is called when a pn command creates a new file.
            File locking is turned off if it was on

PARAMETERS:
   1.   new_edit_file   -  pointer to char (INPUT)
                           This is the new file name.

FUNCTIONS :

   1.   Unlock the file if it was locked.

   2.   Close the file descriptor if it was open.

   3.   Get the stats for the new file and put them in the target file.



*************************************************************************/

void  lock_rename(char    *new_edit_file)
{
#if !defined(WIN32)
struct stat     current_stats;
char            msg[1500];

DEBUG27(fprintf(stderr, "@lock_rename: new file %s\n", new_edit_file);)
#if !defined(FREEBSD)
unlock_file();

if (lock_fd >= 0)
   {
      close(lock_fd);
      lock_fd = -1;
   }
#endif

if (stat(new_edit_file, &current_stats) < 0)
   {
      snprintf(msg, sizeof(msg), "Cannot load new file statistics for %s (%s)", new_edit_file, strerror(errno));
      dm_error(msg, DM_ERROR_LOG);
   }
else
   {
      lock_init(&current_stats);
      save_file_stats(&current_stats);
   }
#endif

} /* end of lock_rename */


/************************************************************************

NAME:      save_file_stats  -   Save stats to detect file changed.

PURPOSE:    This routine saves the stats in case the file changes
            under us.

PARAMETERS:

   1.  file_stats -  pointer to struct stat (INPUT)
                     These are the file stats for the file being edited.  

FUNCTIONS :

   2.   Copy the passed statistics to the static buffer.

*************************************************************************/

void  save_file_stats(struct stat    *file_stats)
{
#if !defined(WIN32)

memcpy((char *)&saved_file_stats, (char *)file_stats, sizeof(saved_file_stats));

#endif
} /* end of save_file_stats */


/************************************************************************

NAME:      detect_changed_file  -   See if the passes stat buff matches the last saved

PURPOSE:    This routine saves the stats in case the file changes
            under us.

PARAMETERS:

   1.  file_stats -  pointer to struct stat (INPUT)
                     These are the file stats for the file being edited. 

FUNCTIONS :

   2.   See if the file has changed.

RETURNED VALUE:
   changed_file  -  int flag
                    True  - The file has changed.
                    False - The file hast not changed.

*************************************************************************/

int  detect_changed_file(struct stat    *file_stats)
{
#if !defined(WIN32)
int             changed_file = False;

if ((file_stats->st_size  != saved_file_stats.st_size)  ||
    (file_stats->st_mtime != saved_file_stats.st_mtime) ||
    (file_stats->st_ino   != saved_file_stats.st_ino))
   changed_file = True;

return(changed_file);

#endif
} /* end of detect_changed_file */


/************************************************************************

NAME:      file_has_changed  -   Get stats for current file and compare against the saved stats

PURPOSE:    This routine saves the stats in case the file changes
            under us.

PARAMETERS:

   1.  edit_file -  pointer to struct stat (INPUT)
                     These are the file stats for the file being edited.  The
                     file does exist.

FUNCTIONS :

   2.   Copy the passed statistics to the static buffer.

RETURNED VALUE:
   changed_file  -  int flag
                    True  - The file has changed.
                    False - The file hast not changed.

*************************************************************************/

int  file_has_changed(const char *edit_file)
{
#if !defined(WIN32)
int             changed_file = False;
struct stat     current_stats;
char            msg[1500];

if (stat(edit_file, &current_stats) < 0)
   {
#ifdef blah
      /* Causes error when used as pager */
      if (saved_file_stats.st_size != 0)
         {
            snprintf(msg, sizeof(msg), "File has been deleted or renamed for %s (%s)", edit_file, strerror(errno));
            dm_error(msg, DM_ERROR_LOG);
         }
#endif

      memset(&current_stats, 0, sizeof(current_stats));
   }
else
   changed_file = detect_changed_file(&current_stats);

save_file_stats(&current_stats); /* only one complaint per change */

return(changed_file);

#endif
} /* end of detect_changed_file */



