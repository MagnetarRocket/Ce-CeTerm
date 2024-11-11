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
*  module prompt.c
*
*  Routines:
*     dm_prompt           - Put a prompt request on the stack
*     prompt_in_progress  - Query if a prompt is requested      
*     complete_prompt     - Fill in the response to a prompt
*     process_prompt      - complete the prompt process and return the command
*     prompt_prescan      - Prescan a command line in a keydef for prompts.
*     clear_prompt_stack  - Remove all entries from the prompt stack.
*     prompt_target_merge - Merge target strings in key defs with multiple prompts.
*     
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h    */
#include <errno.h>          /* /usr/include/errno.h      */

#ifndef WIN32
#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#endif

#include "borders.h"
#include "dmc.h"
#include "dmwin.h"
#include "emalloc.h"
#include "mark.h"
#include "memdata.h"   /* needed for MAX_LINE */
#include "mvcursor.h"
#include "parsedm.h"
#include "prompt.h"

/***************************************************************
*  
*  Structure for linked list of pending prompts.
*  
***************************************************************/

struct PROMPT  ;

struct PROMPT {
        struct PROMPT *next;            /* next cmd if chained                         */
        int            resp_required;   /* count of how many responses are required    */
        int            resp_received;   /* count of how many responses recieved so far */
        MARK_REGION    prompt_mark;     /* mark where cursor was when prompt was requested */
        DMC           *dmc;             /* string of cmds associated with this prompt  */
  };

typedef struct PROMPT PROMPT;


#define PROMPT_STACK ((PROMPT *)dspl_descr->prompt_data)


/************************************************************************

NAME:      dm_prompt  - (DM prompt command (internal)) 
                        Put a prompt request on the stack

PURPOSE:    This routine is called when the main command loop detects a
            command string which has prompts in it.  It is also called
            by commands which sometimes issue a prompt to put the command
            back on the queue after the prompt is resolved.

PARAMETERS:

   1.  dmc            - pointer to DMC (INPUT)
                        This is the command which requests the prompt.
                        The prompt field must be non-null, otherwise a
                        serious error is detected.

   2.   dspl_descr  -  pointer to DISPLAY_DESCR  (INPUT)
                       The display description contains the prompt stack.
                       It is used to get to the main pad, dminput pad, and cursor buff.

OTHER INPUTS:
       prompt_stack   -  The prompt stack (list of pending prompts) is added
                         to by this routine.


FUNCTIONS :

   1.   Make sure the command has a prompt.

   2.   Allocate a prompt structure and fill in the fields. Put this
        structure on the stack.

   3.   Set the dm prompt to the requested value.

OUTPUTS:
   redraw -  The mask anded with the type of redraw needed is returned.


NOTES:
   When called, this command is special because it eats all the commands on
   the DM list.  The main command loop will set the list pointer to null to
   force completion of the keystroke until the prompt is satisfied.


*************************************************************************/

int    dm_prompt(DMC             *dmc,                      /* input  */
                 DISPLAY_DESCR   *dspl_descr)               /* input  */
{
PROMPT          *new_prompt;
DMC             *pr;
int              redraw;
int              window_changed;

/***************************************************************
*  
*  Get a new prompt structure and put it on the stack.
*  
***************************************************************/

new_prompt = (PROMPT *)CE_MALLOC(sizeof(PROMPT));
if (!new_prompt)
   return(0);
memset((char *)new_prompt, 0, sizeof(PROMPT));

/* PROMPT_STACK is a macro pointing into the dspl_descr */
new_prompt->next = PROMPT_STACK;
dspl_descr->prompt_data  = (void *)new_prompt;
new_prompt->dmc  = dmc;


/***************************************************************
*  
*  Count the number of prompts required.
*  
***************************************************************/

pr = dmc;

/* second test added 10/9/95 RES, for prompts with trailing commands */
while ((pr != NULL) && (pr->any.cmd == DM_prompt))
{
   (new_prompt->resp_required)++;
   pr = pr->next;
}


/***************************************************************
*  Set the new prompt and move to the DM input window.
***************************************************************/
set_dm_prompt(dspl_descr, dmc->prompt.prompt_str);
window_changed = dm_tdm(&PROMPT_STACK->prompt_mark, dspl_descr->cursor_buff, dspl_descr->dminput_pad, dspl_descr->main_pad);
if (!window_changed)
   redraw = (DMINPUT_MASK & FULL_REDRAW);
else
   redraw = 0;  /* if window was reconfigured, do not redraw or move the cursor */

return(redraw);

}  /* end of dm_prompt */


/************************************************************************

NAME:      prompt_in_progress - Query if a prompt is requested      


PURPOSE:    This routine is called by the DM enter command when it completes
            a command in the DM input window.  The command line is either
            a response or an independent command.
            

PARAMETERS:

   1.   dspl_descr  -  pointer to DISPLAY_DESCR  (INPUT)
                       The display description contains the prompt stack.


FUNCTIONS :

   1.   If the prompt stack is empty, return false.

   2.   If the top prompt on the stack is complete, return false.

   3.   Otherwise return true.

RETURNED VALUE:
   0 - no prompt in progress
   1 - prompt in progress


*************************************************************************/

int    prompt_in_progress(DISPLAY_DESCR   *dspl_descr)
{


if (PROMPT_STACK == NULL)
   return(False);

if (PROMPT_STACK->resp_required == PROMPT_STACK->resp_received)
   return(False);
else
   return(True);

}  /* end of prompt_in_progress */




/************************************************************************

NAME:      complete_prompt - put in the response to the request


PURPOSE:    This routine is called by the DM enter command when it completes
            a command in the DM input window and has determined that there is
            an outstanding prompt. The response to the prompt is passed here.
            

PARAMETERS:

   1.   dspl_descr  -  pointer to DISPLAY_DESCR  (INPUT)
                       The display description contains the prompt stack.

   2.   response    -  pointer to char  (INPUT)
                       The response is passed in this parm.

   3.   prompt_mark -  pointer to MARK_REGION  (OUTPUT)
                       The position to return to is set if a prompt is completed.


FUNCTIONS :

   1.   If the prompt stack is empty or complete, flag an internal error.

   2.   Find the prompt to reply to.  It is the first unreplied to prompt.

   3.   Fill in the reply to the prompt.

   4.   Set the DM prompt to the next prompt request or back to the default.
        If we are setting this back to the default.  Set the prompt_mark to
        show where to put the cursor.

OUTPUTS:
   redraw -  The mask anded with the type of redraw needed is returned.



*************************************************************************/

int    complete_prompt(DISPLAY_DESCR   *dspl_descr,     /* input  */
                       char            *response,       /* input  */
                       MARK_REGION     *prompt_mark)    /* output */

{

int              i;
DMC             *dmc;

prompt_mark->mark_set     = False;

/***************************************************************
*  
*  Make sure there is a valid place to put the prompt.
*  
***************************************************************/

if (PROMPT_STACK == NULL || PROMPT_STACK->resp_required == PROMPT_STACK->resp_received)
   {
      dm_error("Internal error in complete_prompt", DM_ERROR_LOG);
      return(DMINPUT_MASK & FULL_REDRAW);
   }

/***************************************************************
*  
*  Find the prompt to be filled in.
*  
***************************************************************/

dmc = PROMPT_STACK->dmc;

for (i = 0; i < PROMPT_STACK->resp_received; i++)
   dmc = dmc->next;

(PROMPT_STACK->resp_received)++;

/* RES 8/8/95 if quote char was grave quote, leave blanks in place */
if (dmc->prompt.quote_char != '`')
   while(*response == ' ') response++;


dmc->prompt.response = CE_MALLOC(strlen(response)+1);
if (!dmc->prompt.response)
   return(0);
strcpy(dmc->prompt.response, response);

/***************************************************************
*  
*  See if there are more responses needed for this prompt.
*  If not, go to the next one if there is one.
*  
***************************************************************/

if (PROMPT_STACK->resp_required > PROMPT_STACK->resp_received)
   {
      dmc = dmc->next;
      set_dm_prompt(dspl_descr, dmc->prompt.prompt_str);
   }
else
   if (PROMPT_STACK->next != NULL)
      {
         dmc = PROMPT_STACK->next->dmc;
         set_dm_prompt(dspl_descr, dmc->prompt.prompt_str);
      }
   else
      {
         set_dm_prompt(dspl_descr, "");
         if (PROMPT_STACK->prompt_mark.mark_set)
            {
               set_dm_prompt(dspl_descr, "");
               prompt_mark->mark_set     = True;
               prompt_mark->col_no       = PROMPT_STACK->prompt_mark.col_no;
               prompt_mark->file_line_no = PROMPT_STACK->prompt_mark.file_line_no;
               prompt_mark->buffer       = PROMPT_STACK->prompt_mark.buffer;
            }
      }
   

return(DMINPUT_MASK & FULL_REDRAW);

}  /* end of complete_prompt */


/************************************************************************

NAME:      process_prompt - process a completed prompt.


PURPOSE:    This routine is called by the DM main loop to see if there are
            any completed prompts.  If there are, the commands are parsed and
            the results returned.
            

PARAMETERS:

   1.   dspl_descr  -  pointer to DISPLAY_DESCR  (INPUT)
                       The display description contains the prompt stack.


FUNCTIONS :

   1.   If the prompt stack is not ready, return NULL

   2.   Take the ready prompt and build a command line by filling in the
        responses where required.

   3.   parse the generated command line and return the results.

*************************************************************************/

DMC   *process_prompt(DISPLAY_DESCR   *dspl_descr)
{

#define MAIN_STATE  0
#define RESP_STATE  1

DMC             *dmc;
DMC             *delayed_dmcs;
DMC             *pr;
PROMPT          *next_prompt;
char             work_cmd[MAX_LINE+1];
char            *target;
char            *from_main;
char            *from_resp;
int              state = MAIN_STATE;
int              done;

/* PROMPT_STACK is a macro pointing into the dspl_descr */
if (PROMPT_STACK == NULL || PROMPT_STACK->resp_required > PROMPT_STACK->resp_received)
   return(NULL);

work_cmd[0] = '\0';

dmc = PROMPT_STACK->dmc;
from_main = dmc->prompt.target_strt;
target    = work_cmd;
done      = 0;

while (!done)
{
   switch(state)
   {
   case MAIN_STATE:
      if (dmc->prompt.target == from_main)
         {
            from_main++; /* skip the '&' */
            state = RESP_STATE;
            from_resp = dmc->prompt.response;
         }
      else
         {
            if (*from_main == '\0')
               done = 1;
            *target++ = *from_main++;
         }
      break;

   case RESP_STATE:
      if (*from_resp == '\0')
         {
            state = MAIN_STATE;
            if (dmc->next != NULL)
               dmc = dmc->next;
         }
      else
         *target++ = *from_resp++;
      break;


   } /* end of switch */
}    /* end of while  */


/***************************************************************
*  
*  Get the list of delayed DMC's off the end of the prompt list.
*  
***************************************************************/

delayed_dmcs = dmc->prompt.delayed_cmds;
dmc->prompt.delayed_cmds = NULL;

/***************************************************************
*  
*  Get rid of the old prompt stack entry (pop the stack)
*  
***************************************************************/

/* PROMPT_STACK is a macro pointing into the dspl_descr */
if (PROMPT_STACK->dmc->any.temp)
   {
      /***************************************************************
      *  if the end of the prompt points to anything non-temp,
      *  don't accidentally free it.
      ***************************************************************/
      pr = PROMPT_STACK->dmc;
      while(pr->next && pr->next->any.temp)
         pr = pr->next;
      pr->next = NULL;
      free_dmc(PROMPT_STACK->dmc);
   }
next_prompt = PROMPT_STACK->next;
free((char *)PROMPT_STACK);
dspl_descr->prompt_data  = (void *)next_prompt;

/***************************************************************
*  
*  Parse the resultant structure and return the results.
*  Bad commands get the errors generated and NULL is returned.
*  
***************************************************************/

dmc = parse_dm_line(work_cmd,
                    True, /* temp DMC structures returned */
                    0,    /* no line number               */
                    dspl_descr->escape_char,
                    dspl_descr->hsearch_data);

/***************************************************************
*  
*  If there are any delayed commands (from a cmdf), put them on
*  the end of the list.
*  
***************************************************************/

if (delayed_dmcs != NULL)
   if (dmc != DMC_COMMENT && dmc != NULL)
      {
         pr = dmc;
         while (pr->next != NULL)
            pr = pr->next;
         pr->next = delayed_dmcs;
      }
   else
      if (delayed_dmcs->any.temp)
         free_dmc(delayed_dmcs);

return(dmc);
   
}  /* end of process_prompt */


/************************************************************************

NAME:      prompt_prescan  - Prescan a key defintion for a DM prompt.


PURPOSE:    If a key definition contains prompts, which could theoretically
            be the command name, the command cannot yet be parsed.  Thus an
            internal prompt command (or list of them) is generated to 
            collect the required information which will be parsed later.

PARAMETERS:

   1. cmd_in       -  pointer to char  (INPUT)
                      This the key definition to be prescanned.

   2. temp_flag    -  int  (INPUT)
                      If a prompt dmc is created, this value is put in
                      the temp field to identify this as a temporary or
                      permanent(kd) dmc.
   3. escape_char  -  char (INPUT)
                      The current escape character is passed in.

FUNCTIONS :

   1.  Check if there are and '&' characters in the defintion.  If not,
       there can be no prompts.

   2.  Walk the defintion with a finite state machine and watch for the
       prompt positions ('&') and the prompt strings.  Build up one
       prompt command structure for each prompt.

RETURNED VALUE:
   dmc   -   pointer to DMC
             If a prompt is found and processed, the prompt command structure
             is returned.  Otherwise NULL is returned.

NOTES:
   1.  This technique is used because the prompts are collected asynchronously.
       Once a prompt is issued, the user can do other things in the window
       before responding to the prompt.

*************************************************************************/

DMC  *prompt_prescan(char   *cmd_in,
                     int     temp_flag,
                     char    escape_char)
{
#define      INIT_STATE     0
#define      FOUND_PROMPT   1
#define      IN_STRING      2
#define      IN_PROMPT      3

int          state = INIT_STATE;
DMC         *prompt = NULL;
DMC         *last_prompt;
DMC         *temp_prompt;
char        *prompt_char = NULL;
char        *cmd_out;
char        *cmd_out_start;
char        *cmd_in_start = cmd_in;
int          multiple_prompt = False;
char         sep;

/***************************************************************
*  
*  If there are no '&' characters, there is no chance of a prompt.
*  
***************************************************************/

if (strchr(cmd_in, '&') == NULL)
   return(NULL);

/***************************************************************
*  
*  Check first is this is a kd command.  If the next thing is
*  not a prompt, we bail out.  If the key definition we have
*  been passed is a kd, we only process a prompt if it is
*  The letter.  The other prompts get parsed when processing the kd.
*  
***************************************************************/

cmd_out = cmd_in;
while ((*cmd_out == ' ') || (*cmd_out == '\t')) cmd_out++;
if (*cmd_out == '#') /* comment check */
   return(NULL);
if (((*cmd_out | 0x20) == 'k') && ((*(cmd_out+1) | 0x20) == 'd'))
   {
      cmd_out += 2;
      while ((*cmd_out == ' ') || (*cmd_out == '\t')) cmd_out++;
      if (*cmd_out != '&')
         return(NULL);
   }


cmd_out = CE_MALLOC(strlen(cmd_in)+1);
if (!cmd_out)
   return(NULL);
cmd_out_start = cmd_out;

while(*cmd_in)
{
   switch(state)
   {
   case INIT_STATE:
      /***************************************************************
      *  In the initial state, we watch for the unescaped &.  Otherwise
      *  we copy the data to the target.
      ***************************************************************/
      if (*cmd_in == '&')
         {
            /***************************************************************
            *  We found a prompt.  Create the prompt structure and put it
            *  on the list.  Set the pointers to where the prompt is inserted.
            ***************************************************************/
            state = FOUND_PROMPT;
            temp_prompt = (DMC *)CE_MALLOC(sizeof(DMC));
            if (!temp_prompt)
               return(NULL);
            memset((char *)temp_prompt, 0, sizeof(DMC));
            temp_prompt->any.cmd = DM_prompt;
            temp_prompt->any.temp = temp_flag;
            temp_prompt->prompt.mult_prmt = multiple_prompt;
            multiple_prompt = True;
            /* put the structure on the list, can be more than one */
            if (prompt == NULL)
               {
                  prompt = temp_prompt;
                  last_prompt = temp_prompt;
               }
            else
               {
                  last_prompt->next = temp_prompt;
                  last_prompt = temp_prompt;
               }
            temp_prompt->prompt.target = cmd_out;
            temp_prompt->prompt.target_strt = cmd_out_start;
            *cmd_out++ = *cmd_in; /* copy the '&' */
         }
      else
         /* only process escaped '&' */
         if ((*cmd_in == escape_char) && (*(cmd_in+1) == '&'))
            {
               *cmd_out++ = *(++cmd_in);
               if (*cmd_in == '\0')
                  return(prompt);
            }
         else
            {
               *cmd_out++ = *cmd_in;
            }
      break;

   case FOUND_PROMPT:
      /***************************************************************
      *  Check for start of prompt string.  If we find it, we are
      *  will read it in.  Otherwise, we will return to the inital state.
      ***************************************************************/
      if ((*cmd_in == '\'') || (*cmd_in == '`'))
         {
            /* start of a prompt string */
            temp_prompt->prompt.prompt_str = CE_MALLOC(strlen(cmd_in)+1);
            if (!temp_prompt->prompt.prompt_str)
               return(NULL);
            prompt_char = temp_prompt->prompt.prompt_str;
            temp_prompt->prompt.quote_char = *cmd_in;
            sep = *cmd_in;
            state = IN_PROMPT;
         }
      else
         {
            /* null prompt string */
            temp_prompt->prompt.prompt_str = CE_MALLOC(1);
            if (!temp_prompt->prompt.prompt_str)
               return(NULL);
            prompt_char = temp_prompt->prompt.prompt_str;
            temp_prompt->prompt.prompt_str[0] = '\0';
            *cmd_out++ = *cmd_in;
            state = INIT_STATE;
         }
      break;

   case IN_STRING:
      *cmd_out++ = *cmd_in;
      if (*cmd_in == '\'')
         state = INIT_STATE;
      break;

   case IN_PROMPT:
      if (*cmd_in == sep)
         {
            state = INIT_STATE;
            *prompt_char = '\0';
         }
      else
         *prompt_char++ = *cmd_in;
      break;

   } /* end of switch on state */

   cmd_in++;
} /* end of while not end of cmd_string */

/***************************************************************
*  
*  If we found a prompt, complete it. Otherwise free the
*  work string.
*  
***************************************************************/

if (prompt_char)
   *prompt_char = '\0'; /* take care of missing close quote on the last prompt */

*cmd_out = '\0';
strcpy(cmd_in_start, cmd_out_start);

if (prompt == NULL)
   free((char *)cmd_out_start);

return(prompt);

} /* end of prompt_prescan */


/************************************************************************

NAME:      clear_prompt_stack - Empty the prompt stack (after an interupt or some such)


PURPOSE:    This routine gets rid of any pending prompts.
            

PARAMETERS:

   1.   dspl_descr  -  pointer to DISPLAY_DESCR  (INPUT)
                       The display description contains the prompt stack.

FUNCTIONS :

   1.   Free any and all prompts on the stack.

   2.   If any of the DMC_prompts are temporary, free them.

OUTPUTS:
   redraw -  The mask anded with the type of redraw needed is returned.


*************************************************************************/

void  clear_prompt_stack(DISPLAY_DESCR   *dspl_descr)
{
PROMPT          *next_prompt;

/* PROMPT_STACK is a macro pointing into the dspl_descr */
while (PROMPT_STACK != NULL)
{
   if (PROMPT_STACK->dmc->prompt.temp)
      free_dmc(PROMPT_STACK->dmc);

   next_prompt = PROMPT_STACK->next;
   free((char *)PROMPT_STACK);
   dspl_descr->prompt_data  = (void *)next_prompt;
}

return;

} /* end of clear_prompt_stack */


/************************************************************************

NAME:      prompt_target_merge - Merge target strings in key defs with multiple prompts.


PURPOSE:    This routine is called when key definitions are read in from
            the X server.  If there is a key definition with multiple
            prompts, the definition is stored in the X server with multiple
            copies of the prompt string.  
            

PARAMETERS:

   1.   dspl_descr  -  pointer to DISPLAY_DESCR  (INPUT)
                       The display description contains the prompt stack.

FUNCTIONS :

   1.   Walk the list looking for prompts.

   2.    If a prompt is found and the next DMC is also a prompt and the
         text matches, fix up the second prompt to refer to the first.
         This includes:
         a.  Re-adjusting the target pointer 
         b.  Set the mult_prmt flag
         c.  change the target start pointer
         d.  free the old target string.

*************************************************************************/

void   prompt_target_merge(DMC             *dmc)       /* input  */
{
DMC      *current;
DMC      *next;
int       offset;

for (current = dmc; current; current = current->next)
{
   if ((current->any.cmd == DM_prompt) &&
       (current->next)                 &&
       (current->next->any.cmd == DM_prompt) &&
       (strcmp(current->prompt.target_strt, current->next->prompt.target_strt) == 0))
   {
      next = current->next;
      offset = next->prompt.target - next->prompt.target_strt;
      free(next->prompt.target_strt); /* get rid of duplicate copy of string */
      next->prompt.target_strt = current->prompt.target_strt;
      next->prompt.target = next->prompt.target_strt + offset;
      next->prompt.mult_prmt = True;
   }
}

}  /* end of prompt_target_merge */

