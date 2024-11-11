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
*  module timeout.c
*
*  These routines deal with timing events.  For example, if
*  the mouse button is pressed down in the scroll bar window,
*  and held, an additional button event is generated every
*  so many milliseconds.  This involves setting the timeout
*  on the select in unixpad.c and then generating the button
*  press.
*
*  Routines:
*         timeout_set           - Set the period of the timeout
*
*  Internal:
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h         */
#include <string.h>         /* /usr/include/string.h        */
#include <errno.h>          /* /usr/include/errno.h         */
#include <sys/types.h>      /* usr/include/sys/types.h      */
#ifdef WIN32
#include <time.h>
#include <winsock.h>
#else
#include <sys/time.h>       /* /usr/include/sys/time.h      */
#endif

#include "debug.h"
#include "dmwin.h"
#include "sendevnt.h"
#include "timeout.h"
#include "xerror.h"         /* needed for CURRENT_TIME */


/***************************************************************
*  
*  Local prototypes
*  
***************************************************************/

#ifdef WIN32
static void gettimeofday(struct timeval *any_time, void *junk)
{
unsigned long     ticks;

ticks = GetTickCount();
any_time->tv_sec  = ticks / 1000;         /* take whole number of seconds */
any_time->tv_usec =(ticks % 1000) * 1000; /* convert to microseconds */

} /* end of gettimeofday */
#endif


/***************************************************************
*  
*  Structure used for doing timeout
*   
*  lc_time_out is the amount of time till the next license server
*  update call is needed.
*
*  last_tod is the time of day timeval from the last call to timeout set.
*  it it used to calculate how much time elapsed in since the last call.
*  
***************************************************************/


static struct timeval        last_tod;

static struct timeval        zero_tod = {0,0};



/************************************************************************

NAME:      timeout_set           - Set the period of the timeout

PURPOSE:    This routine calculates the timeout for the select in the
            unixpad.c routine wait_for_input.  It also triggers events
            caused by timeouts such as scroll window button presses and
            license server calls.

PARAMETERS:

   1.   dspl_descr   - pointer to DISPLAY_DESCR (INPUT)
                       This is a display description in the circular
                       linked list.  They are all looked at.

GLOBAL DATA:

   1.  last_tod   -    struct timeval (INPUT/OUTPUT)
                       If a timeout is needed, the time of day is saved here.
                       This is the time before the select.  Otherwise, the
                       value is set to zero.  If the value is non-zero on
                       entry, the interval since the last call is calculated
                       from this value.

   2.  lc_time_out  -  struct timeval (INPUT)
                       If the license server is active, this is the amount
                       of time till the next license server udpate call.

FUNCTIONS:
   1.  Check the display descriptions to see if there are any scroll bars active.
       If so, save their timeout values.

   2.  If the license server is active, factor in the time till the next 
       call to update server.

   3.  If a timeout is set, save the current time of day.


RETURNED VALUE:
   time_out  -  pointer to struct timeval
                This structure is what is passed to select.
                If appropriate, NULL is returned.
                The pointer is to a static structure.

*************************************************************************/

struct timeval *timeout_set(DISPLAY_DESCR   *dspl_descr)
{
struct timeval        *time_ptr = NULL;
DISPLAY_DESCR         *walk_dspl;
int                    curr_microseconds = INT_MAX;
int                    last_interval;
int                    work_microseconds;
int                    lcl_pad_mode = False;
char                   msg[256];
struct timeval         curr_tod;

static int             beep_mode = 0;
static struct timeval  time_out;

msg[0] = '\0';

/***************************************************************
*  
*  If the last_tod is not zero, then we had a timeout before.
*  Thus we will need the elapsed microseconds.
*  
***************************************************************/

if ((last_tod.tv_sec != 0) || (last_tod.tv_usec != 0))
   {
      gettimeofday(&curr_tod, NULL);
      if (curr_tod.tv_usec < last_tod.tv_usec)
         {
            curr_tod.tv_usec += 1000000;
            curr_tod.tv_sec--;
         }
      last_interval = ((curr_tod.tv_sec - last_tod.tv_sec) * 1000000) + (curr_tod.tv_usec - last_tod.tv_usec); 
   }
else
   {
      curr_tod = zero_tod;
      last_interval = 0;
   }
 

/***************************************************************
*  
*  Scan to see if we have any display descriptions with
*  waiting scroll bar or pulldown activity.
*  
***************************************************************/
walk_dspl = dspl_descr;

do
{
   lcl_pad_mode |= walk_dspl->pad_mode;
   if (walk_dspl->sb_data->sb_microseconds)
   {
      if (last_interval >= walk_dspl->sb_data->sb_microseconds)
         {
            DEBUG22(fprintf(stderr, "timeout_set: Triggering SB button push for display %d\n", walk_dspl->display_no);)
            ce_XSendEvent(walk_dspl->sb_data->button_push.xbutton.display,
                          walk_dspl->sb_data->button_push.xbutton.window,
                          True,
                          ButtonPressMask,
                          &walk_dspl->sb_data->button_push);
            walk_dspl->sb_data->sb_microseconds = 0;
         }
      else
         {
            walk_dspl->sb_data->sb_microseconds -= last_interval;
            DEBUG22(fprintf(stderr, "timeout_set: %d microseconds to go till SB button push for display %d\n", walk_dspl->sb_data->sb_microseconds, walk_dspl->display_no);)
            time_ptr = &time_out;
            if (curr_microseconds > dspl_descr->sb_data->sb_microseconds)
               {
                  time_out.tv_sec  = walk_dspl->sb_data->sb_microseconds / 1000000;
                  time_out.tv_usec = walk_dspl->sb_data->sb_microseconds % 1000000;
                  curr_microseconds = dspl_descr->sb_data->sb_microseconds;
               }
         }
   }
   if (walk_dspl->pd_data->pd_microseconds)
   {
      if (last_interval >= walk_dspl->pd_data->pd_microseconds)
         {
            DEBUG29(fprintf(stderr, "timeout_set: Triggering pulldown button push for display %d\n", walk_dspl->display_no);)
            ce_XSendEvent(walk_dspl->pd_data->button_push.xbutton.display,
                          walk_dspl->pd_data->button_push.xbutton.window,
                          True,
                          ButtonPressMask,
                          &walk_dspl->pd_data->button_push);
            walk_dspl->pd_data->pd_microseconds = 0;
         }
      else
         {
            walk_dspl->pd_data->pd_microseconds -= last_interval;
            DEBUG29(fprintf(stderr, "timeout_set: %d microseconds to go till PD button push for display %d\n", walk_dspl->pd_data->pd_microseconds, walk_dspl->display_no);)
            time_ptr = &time_out;
            if (curr_microseconds > dspl_descr->pd_data->pd_microseconds)
               {
                  time_out.tv_sec  = walk_dspl->pd_data->pd_microseconds / 1000000;
                  time_out.tv_usec = walk_dspl->pd_data->pd_microseconds % 1000000;
                  curr_microseconds = dspl_descr->pd_data->pd_microseconds;
               }
         }
   }

   walk_dspl = walk_dspl->next;

} while(walk_dspl != dspl_descr);

DEBUG22(
   if (time_ptr)
      fprintf(stderr, "timeout_set: Scrollbar or Pulldown search found something\n");
)

/***************************************************************
*  
*  If we set a timer, we will need the time of day for next time.
*  
***************************************************************/

if (time_ptr)
   if (last_interval != 0)
      last_tod = curr_tod;
   else
      gettimeofday(&last_tod, NULL);
else
   last_tod = zero_tod;
   

DEBUG22(
   if (time_ptr)
      fprintf(stderr, "timeout_set: Timeout : %d secs, %d microsecs\n", time_out.tv_sec, time_out.tv_usec);
)

if (beep_mode)
   ce_XBell(dspl_descr, 0);

return(time_ptr);

} /* end of timeout_set */



