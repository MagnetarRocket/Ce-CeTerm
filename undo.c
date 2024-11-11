 static char *sccsid = "%Z% %M% %I% - %G% %U% ";
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

/**************************************************************************
*
*   This is the Undo functions file.
*
*   Undo maintains a doubly linked list of memdata altering events. 
*   an 'un_do()'  walks the list backwards effecting the reverse of the event
*   on the memdata structure. Redo walks forward in the undo dlist.
*
* External Interfaces
*
*    un_do()        - undo to the last key stroke 
*    re_do()        - redo to the last key stroke 
*    event_do()     - Add an event to the undo dlist 
* 
* Internal Routines
*
*    rm_event()        - remove an event from the event dlist
*    last_event()      - take a step backwards in the event dlist
*    next_event()      - take a step forwards in the event dlist
*    free_forward()    - remove everything forward of the current pointer
*    remove_pwevents() - Remove any pw event in the linked list
* 
****************************************************************************/

#define _UNDO_  1

#include <stdio.h>          /* /usr/include/stdio.h      */ 
#include <errno.h>          /* /usr/include/errno.h      */ 
#include <limits.h>         /* /usr/include/limits.h     */ 
#include <string.h>         /* /usr/include/string.h     */ 


#include "debug.h"                                          
#include "dmwin.h"
#include "emalloc.h"
#include "memdata.h"                                          
#include "undo.h"


static int   free_forward(DATA_TOKEN *token);
static void  rm_event(event_struct *kill_event);
static void  next_event(DATA_TOKEN *token);
static void  last_event(DATA_TOKEN *token);
static char *print_event(int event);
static void  remove_pwevents(DATA_TOKEN *token);
void dump_event_list(DATA_TOKEN *token, int count);

void  exit(int code);

/***********************************************************
*
*   un_do -  move backward on key_strokeon the  dlist and 
*            effect the changes 
*
***********************************************************/
#ifdef DebuG
int Dcount = 0;
#endif

void un_do(DATA_TOKEN *token, int *line_out)
{

int len;
int line;
int dummy;
int last_line = -1;
char *ptr;

event_struct *event;

DEBUG0( if (token->marker != TOKEN_MARKER) {fprintf(stderr, "Bad token passed to un_do"); exit(8);})
DEBUG6( fprintf(stderr, " @Un_do \n"); dump_event_list(token, 500);)

*line_out = INT_MAX; 

if (!token->event_head || 
   ((token->event_head->type == KS_EVENT) && (token->event_head->last == NULL)) ||
   ((token->event_head->type == PW_EVENT) && (token->event_head->last != NULL) && (token->event_head->last->type == KS_EVENT) && (token->event_head->last->last == NULL) )){ 
       dm_error("Nothing left to UNDO", DM_ERROR_BEEP);
       return;
}

/*
 *  Walk backwards on the event dlist until a key_stroke is found
 */

if (token->event_head &&   /* if on a key_stroke get off before looking */
    (token->event_head->type == KS_EVENT) &&
     (token->event_head->last != NULL)) 
         last_event(token); /* backup over the key_stroke */

event = token->event_head;

undo_semafor = ON;  /* don't want the memdata(base) to log our changes! */

while (event && (event->type != KS_EVENT)){      

    DEBUG6( fprintf(stderr, " EVENT[%d]#%d\n", event->type, Dcount++);)

    if ((event->line < *line_out) && (event->line >= 0)) *line_out = event->line;
    if (event->line > last_line) last_line = event->line;

    switch(event->type){
         case  PC_EVENT: 
                        ptr = get_color_by_num(token, event->line, COLOR_CURRENT, &dummy);
                        if (!event->old_data){
                            len = strlen(ptr) + 1;
                            event->malloc_old_size = len;
                            event->old_data = (char *)CE_MALLOC(len);
                            if (!event->old_data) return; 
                            strcpy(event->old_data, ptr);
                        }
                        put_color_by_num(token, event->line, event->data);
                        break;

         case  PL_EVENT:
                         if (event->paste_type == INSERT){

                             line = event->line +1;
                             ptr = get_line_by_num(token, line);
                             if (!event->data){ 
                                 len = strlen(ptr) + 1;
                                 event->malloc_size = len;
                                 event->data = (char *)CE_MALLOC(len);
                                 if (!event->data) return; 
                                 strcpy(event->data, ptr);
                             }
                             delete_line_by_num(token, line, 1);

                         }else{   /* OVERWRITE */

                             ptr = get_line_by_num(token, event->line);
                             if (!event->old_data){
                                 len = strlen(ptr) + 1;
                                 event->malloc_old_size = len;
                                 event->old_data = (char *)CE_MALLOC(len);
                                 if (!event->old_data){
                               /*    dm_error("Cannot CE_MALLOC event->old_data.", DM_ERROR_LOG);  */
                                     return;
                                 }
                                 strcpy(event->old_data, ptr);
                             }
                             put_line_by_num(token, event->line, event->data, OVERWRITE);

                         }
                         break;
     
         case  DL_EVENT:
                         put_line_by_num(token, event->line -1, event->data, INSERT);
                         break;

         case  PW_EVENT:
                         dirty_bit(token) = 0;
                         break;
     
#ifdef PUTBLOCK

         case  RB_EVENT:
                         line = event->line +1;
                         ptr =  get_line_by_num(token, line);
                         len = strlen(ptr) + 1;
                         event->malloc_size = len;
                         event->data = (char *)CE_MALLOC(len);
                         if (!event->data){
                           /*  dm_error("Cannot CE_MALLOC event.data.", DM_ERROR_LOG); */
                             return;
                         }
                         strcpy(event->data, ptr);
                         delete_line_by_num(token, line, 1);
                         break;
     
#endif

#ifdef blah
         case  SL_EVENT:
                         ptr = get_line_by_num(token, event->line);
                         len = strlen(ptr) + strlen(event->data)+ 1;
                         event->malloc_old_size = len;
                         event->old_data = (char *)CE_MALLOC(len);
                         if (!event->old_data){
                           /*  dm_error("Cannot CE_MALLOC event.old_data.", DM_ERROR_LOG); */
                             return;
                         }
                         strcpy(event->old_data, ptr);
                         strcat(event->old_data, event->data);
                         put_line_by_num(token, event->line, event->old_data, OVERWRITE);
                         break;
       
#endif
         case  ES_EVENT:
         case  DS_EVENT:
         default:
                         dm_error("Program Error! (undo)", DM_ERROR_LOG);

    } /*switch */

    if (event->last == NULL) break; /* dont go off the end */
    last_event(token);
    event = token->event_head;

} /* while !key_stroke */

DEBUG6( fprintf(stderr, " @Un_do[END](%d)\n", *line_out);)

if ((token->event_head->type == PW_EVENT) ||
   ((token->event_head->type == KS_EVENT) && token->event_head->last && (token->event_head->last->type == PW_EVENT)))
          dirty_bit(token) = 0;

undo_semafor = OFF;

DEBUG6( fprintf(stderr, " #Un_do \n"); dump_event_list(token, 500);)

#ifdef old_dirty_bit
if (!token->event_head || 
   ((token->event_head->type == KS_EVENT) && 
    (token->event_head->last == NULL))){ 
       dirty_bit(token) = 0; /* file is no longer dirty */
       return;
}
#endif  

}   /* un_do() */

/***********************************************************
*
*   re_do -  move forward on key_stroke on the  dlist and 
*            effect the changes 
*
***********************************************************/

void re_do(DATA_TOKEN *token, int *line_out)
{

/*int line;*/
/*char *ptr;*/
int last_line = -1;

event_struct *event;

DEBUG0( if (token->marker != TOKEN_MARKER) {fprintf(stderr, "Bad token passed to re_do"); exit(8);})
DEBUG6( fprintf(stderr, " @Re_do: ");dump_event_list(token, 500);)

*line_out = INT_MAX;

if (!token->event_head ||  /* if on a key_stroke get off before looking */
    (token->event_head->next == NULL) ||
    ((token->event_head->type == PW_EVENT) && (token->event_head->next != NULL) && (token->event_head->next->next == NULL))){ 
       dm_error("Nothing to REDO", DM_ERROR_BEEP);
       return;
}

/*
 *  Walk forward on the event dlist until a key_stroke is found
 */

if (token->event_head && 
    (token->event_head->type == KS_EVENT) && 
    (token->event_head->next != NULL)) 
       next_event(token); /* go forward over the key_stroke */

event = token->event_head;

undo_semafor = ON;   /* don't wnat the memdata(base) to log our changes! */

while (event && (event->type != KS_EVENT)){

    if ((event->line < *line_out) && (event->line >= 0)) *line_out = event->line;
    if (event->line > last_line) last_line = event->line;

    DEBUG6( fprintf(stderr, " EVENT[%d]\n", event->type);)

    switch(event->type){
         case  PC_EVENT:  /* put more code here */
                         put_color_by_num(token, event->line, event->old_data);
                         break;
         case  PL_EVENT:
                         if (event->paste_type == INSERT){
                             put_line_by_num(token, event->line, event->data, INSERT);
                         }else{
                             put_line_by_num(token, event->line, event->old_data, OVERWRITE);
                         }
                         break;
     
         case  DL_EVENT:
                         delete_line_by_num(token, event->line, 1);
                         break;
     
         case  PW_EVENT:
                         dirty_bit(token) = 0;
                         break;
     
#ifdef PUTBLOCK

         case  RB_EVENT: 
                         put_line_by_num(token, event->line, event->data, INSERT);
                         free(event->data);
                         event->malloc_size = 0;
                         break;
     
#endif
#ifdef blah
         case  SL_EVENT:
                         event->old_data[event->column] = NULL;
                         put_line_by_num(token, event->line, event->old_data, OVERWRITE);
                         free(event->old_data);
                         event->malloc_old_size = 0;
                         break;   /* paste_type also implies a new line was created */
       
#endif
         case  ES_EVENT:
         case  DS_EVENT:
         default:
                         dm_error("Program Error! (redo)", DM_ERROR_LOG);
                         return;

    } /*switch */

    if ((event->next == NULL) && (event->type !=KS_EVENT)){ 
        event_do(token, KS_EVENT, 0, 0, 0, NULL); /* terminate the redo */
        break;   /* dont go off the end */
    }

    next_event(token); 
    event = token->event_head;



} /* while !key_stroke */

if ((token->event_head->type == PW_EVENT) ||
   ((token->event_head->type == KS_EVENT) && token->event_head->next && (token->event_head->next->type == PW_EVENT)))
        dirty_bit(token) = 0;

DEBUG6( fprintf(stderr, " @Re_do[END](%d)\n", *line_out);)
DEBUG6( fprintf(stderr, " #Re_do: ");dump_event_list(token, 500);)

undo_semafor = OFF;

} /* re_do() */

/***********************************************************
*
*   event_do -  Place an event on the dlist
*
***********************************************************/

void event_do(DATA_TOKEN *token,
              int event,
              int line,
              int column,   
              int paste_type,
              char *data)
{

int len;
event_struct *new_event;

DEBUG0( if (token->marker != TOKEN_MARKER) {fprintf(stderr, "Bad token passed to event_do"); exit(8);})
DEBUG6( fprintf(stderr, " @event_do[event=%s]H=%d\n", print_event(event), token->event_head);)

DEBUG6( fprintf(stderr, " BEFOR: ");dump_event_list(token, 500);)

if (!WRITABLE(token)){
         DEBUG6( fprintf(stderr, " event_do - Attempt to event to a readonly window. 0x%X\n", token);)
         return; 
}

if (token->event_head &&  event == KS_EVENT &&   /* no events on the dlist since last key_stroke */
    ((token->event_head->type == KS_EVENT) ||
    ((token->event_head->type == PW_EVENT) && token->event_head->last && (token->event_head->last->type == KS_EVENT)))){
         DEBUG6( fprintf(stderr, " event_do- \"event=%d is redundant!\"\n", event);)
         return; 
}
        
/* #678 */

/* merge mallocs for efficiency */
/* cut extra check in free forwrd */
new_event = (event_struct *)CE_MALLOC(sizeof(event_struct));
if (!new_event)  return;

if (data){
  len = strlen(data) + 1;
  new_event->data = (char *)CE_MALLOC(len);
  new_event->malloc_size = len;
  if (!new_event->data) return;
}

if (event == KS_EVENT) free_forward(token); /* moved down 10/7/92 from #678 */
if (event == PW_EVENT) remove_pwevents(token);

if ((event == PW_EVENT) || (event == KS_EVENT)) line = -1;

/*
 *  Put the new data in the event rec, and add the rec to the head of the dlist
 */

new_event->line = line;
new_event->column = column;
new_event->type = event;
new_event->paste_type = paste_type;
new_event->old_data = NULL;
new_event->malloc_old_size = 0; /* added 11/7/97 RES */
if (data)
     strcpy(new_event->data, data);
else
     new_event->data = NULL;

new_event->last = token->event_head;
if (token->event_head){ 
       new_event->next = token->event_head->next; /* new */
       if (token->event_head->next) token->event_head->next->last = new_event;
       token->event_head->next = new_event;
}else  /* else added */
        new_event->next = NULL;
token->event_head = new_event;

if (event != PW_EVENT) free_forward(token);

#ifdef blah
new_event->last = token->event_head;
if (token->event_head) 
       token->event_head->next = new_event;
new_event->next = NULL;
token->event_head = new_event;
#endif

DEBUG6( fprintf(stderr, " event{ADDRS(%d),line(%d)}\n", new_event, line);)
DEBUG6( fprintf(stderr, " @event_do[END]\n");)

DEBUG6( fprintf(stderr, " AFTER: ");dump_event_list(token, 500);)


} /* event_do() */

/***********************************************************
*
*   free_forward - Free up everything forward of the current 
*                  in the dlist
*
***********************************************************/

static int free_forward(DATA_TOKEN *token)
{
int mem_sum = 0;
event_struct *kill_event, *last_event;

DEBUG6( fprintf(stderr, " @Free_forward\n");)

if (!token->event_head) return(0);

kill_event = token->event_head->next;

while (kill_event){
   mem_sum += kill_event->malloc_size + kill_event->malloc_old_size + sizeof(event_struct);
   if (kill_event->type != KS_EVENT){
       if (kill_event->data)
           free(kill_event->data);
       if (kill_event->old_data)
           free(kill_event->old_data);
   }
   last_event = kill_event;
   kill_event = kill_event->next;
   free((char *)last_event);
}

/* All the elements we freed are gone - 10/7/93  RES */
token->event_head->next = NULL;

return(mem_sum);

} /* free_forward */

/***********************************************************
*
*   remove_pwevents - Remove any pw event in the linked list
*                     before placing the new one.
*
***********************************************************/

static void remove_pwevents(DATA_TOKEN *token)
{
int mem_sum = 0;
event_struct *kill_event, *last_event, *next_event;;

DEBUG6( fprintf(stderr, " @Remove PW Events()\n");)

if (!token->event_head) return;

kill_event = token->event_head;

while(kill_event->next)
    kill_event = kill_event->next;  /* get to the top of the dlist */

while (kill_event){
   next_event = kill_event->next;
   last_event = kill_event->last;
   if (kill_event->type == PW_EVENT){ 
      if (kill_event == token->event_head) token->event_head = kill_event->next;
      rm_event(kill_event);
      if (last_event && (last_event->type == KS_EVENT) &&
          next_event && (next_event->type == KS_EVENT)){ 
                    if (next_event == token->event_head) token->event_head = next_event->next;
                    rm_event(next_event);
                    next_event = last_event->next;
      }
   }
   kill_event = last_event;
}

DEBUG6( fprintf(stderr, " @Remove PW Events/Done\n");dump_event_list(token, 500);)
return;

} /* remove_pwevents */

/***********************************************************
*
*   rm_event - remove move an event from the dlist
*
*   NOTE: This routine does not free the memory from the 
*         data or old_data fields.
***********************************************************/

static void rm_event(event_struct *kill_event)
{

DEBUG6( fprintf(stderr, " @rm_event\n");)

if (kill_event->next) 
      (kill_event->next)->last = kill_event->last;

if (kill_event->last)
      (kill_event->last)->next = kill_event->next;

free((char *)kill_event);

} /* rm_event() */

/***********************************************************
*
*   next_event - move event_head on to the next event
*
***********************************************************/

static void next_event(DATA_TOKEN *token)
{

DEBUG6( fprintf(stderr, " @next_event\n");)

if (token->event_head) 
    token->event_head =  token->event_head->next;

} /* next _event */

/***********************************************************
*
*   last_event - move event_head back to the last event
*
***********************************************************/

static void last_event(DATA_TOKEN *token)
{

DEBUG6( fprintf(stderr, " @last_event\n");)

if (token->event_head) 
    token->event_head =  token->event_head->last;

} /* last _event */

/***********************************************************
*
*   undo_init - initialize the undo dlist
*
***********************************************************/

void undo_init(DATA_TOKEN *token)
{

kill_event_dlist(token);

token->event_head = NULL;

event_do(token, KS_EVENT, 0, 0, 0, NULL); /* init the undo dlist with a key stroke */
event_do(token, PW_EVENT, 0, 0, 0, NULL); /* init the undo dlist with a key stroke */

} /* undo_init() */

/***********************************************************
*
*   kill_event_dlist - kill all events on the dlist
*
***********************************************************/

int kill_event_dlist(DATA_TOKEN *token)
{

int freed;

event_struct *event;

event = token->event_head;

if (!event) return(0);

while (event->last)       /* get to the bottom of the list */
       event =  event->last;

token->event_head = event;

freed = free_forward(token);

DEBUG6( fprintf(stderr, " @kill_event_dlist freed(%d) bytes\n", freed);)

return(freed);

}  /* kill_event_dlist() */

#ifdef DebuG
/***********************************************************
*
*   dump_event_list - dah'   :-)
*
*                count is the max number of events to dump 
*                         (helps to reduce the output)
*
***********************************************************/

void dump_event_list(DATA_TOKEN *token, int count)
{


event_struct *event;

DEBUG6( fprintf(stderr, " Dump_event_list:\n");)

if (!token->event_head) return;

event = token->event_head;

while(event->next)
  event = event->next;  /* get to the top of the dlist */

while (event && count){       /* get to the bottom of the list */
       count--;
       if (event == token->event_head) fprintf(stderr, "HEAD->");
       fprintf(stderr, "   Event: %-20s(%d), ms(%d), mos(%d), l(%d), c(%d), pt(%d), d(%d), od(%d), n(%d), l(%d)\n", 
               print_event(event->type), event, event->malloc_size, event->malloc_old_size, event->line, event->column, event->paste_type, event->data, event->old_data, event->next, event->last);
       event = event->last;
}

}  /* dump__event_list() */

/***********************************************************
*
*   print_event - print an event number as a text string
*
***********************************************************/

static char *print_event(int event)
{

switch(event){

case  PL_EVENT:
               return("PUT_LINE_EVENT");
      break;         
               
case  DL_EVENT:
               return("DELETE_LINE_EVENT");
      break;         
               
case  RB_EVENT:
               return("READ_BLOCK_EVENT");
      break;         
               
case  KS_EVENT:
               return("KEY_STROKE_EVENT");
      break;         

case  SL_EVENT:
               return("SPLIT_LINE_EVENT");
      break;         

case  PC_EVENT:
               return("PUT_COLOR_EVENT");
      break;         

case  PW_EVENT:
               return("PAD_WRITE_EVENT");
      break;         
               
case  ES_EVENT:
case  DS_EVENT:
default:       
          dm_error("No Such event type.", DM_ERROR_LOG);
          return("NO_SUCH_EVENT");
}

} /* print_event() */

#endif
