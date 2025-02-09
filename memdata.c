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

9) optimize get_color_by_num with tokens->current_color_offset
10) split block & split header need to be looked over carefully and recalculate bits

*  
*  Routines in memdata.c
*     mem_init              - Allocate and initialize a memory data block
*     load_a_block          - Read a block of data
*     position_file_pointer - Position for next_line and prev_line
*     next_line             - Read sequentially forward
*     prev_line             - Read sequentially backward
*     get_line_by_num       - Read a line (position independent)
*     delete_line_by_num    - Delete a line
*     split_line            - Split a line into two adjacent lines
*     put_line_by_num       - Replace or insert a line
*     put_block_by_num      - Insert multiple lines from a file
*     save_file             - Write the file out
*     delayed_delete        - Mark a line to be deleted later
*     sum_size              - Sum the malloced sizes of a range of lines
*     encrypt_init          -  Initialize an encryption
*     encrypt_line          - encrypt a line
*     join_line             - join one line to the next one after it.
*     mem_kill              - kill a memdata structure.
*     vt100_eat             - Eat vt100 control sequences when in cv -man mode
*     print_color_bits      - Dump the color lines and the color bit patterns
*     print_color           - walk the data structure and print each color line
*     get_color_by_num      - Return forwrd/backward next color info
*     put_color_by_num      - Add/replace color data for a line
*     print_file            - same as save_file, but printing (DEBUGGING)
*     dump_ds               - same as print_file, but structure only (DEBUGGING)
*
*  Internal routines:
*     read_a_block          - Read a block into memory
*     hh_idx                - Return the 3 indexes to a line
*     last_line_in_block    - return the index of the last filled line in a block
*     remove_block          - Remove a block and pull up the following blocks
*     remove_header         - Remove a header and pull up the following blocks
*     sum_header            - Return the sum of the lines in a header
*     split_header          - Split a header and divide its data among the 2 halves
*     split_block           - Split a block and  "    "     "    "    "    "   "
*     data_sum              - Return the sum of the blocks in a header
*     wrap_input            - wrap input to put_block_by_num
*     prev_bit              - find the next bit to the left (inclusive)
*     next_bit              - find the next bit to the right
*     shift_right           - Shift bits in a color array one bit
*     shift_left            - Shift bits in a color array one bit
*     split_color           - Split a color array
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <errno.h>          /* /usr/include/errno.h      */
#include <limits.h>         /* /usr/include/limits.h     */
#include <string.h>         /* /usr/include/string.h     */
#include <signal.h>         /* /usr/include/signal.h     */


#define _MEMDATA_ 1

#include "cc.h"
#include "cdgc.h"
#include "dmwin.h"
#include "debug.h"
#include "emalloc.h"
#include "memdata.h"
#include "pastebuf.h"
#include "undo.h" 
#include "xerror.h"

/*
 *  
 *  MROUND is a macro which rounds a number to the next greater
 *  multiple of 8.  This is handy in doing mallocs.  Since the
 *  size malloc gets is a multiple of 8, we may as well use it
 *  all.
 *  
 */

#define BACKSPACE 8

#define MASK_8     (UINT_MAX & (UINT_MAX << 3))
#define MROUND(n)  ((n + 7) & MASK_8)

#ifdef WIN32
#define kill(a,b) exit(b)
#endif

/*
 *  
 *  Internal Prototypes
 *  
 */

static block_struct *read_a_block(DATA_TOKEN *token,
                                  FILE       *stream,
                                  int        *lines_put_in_block,     /*  output */
                                  int        *eof,                    /* input / output */
                                  int         line_no,                 /* input */
                                  int         eat_vt100,               /* input */
                                  int        *last_line_has_newline); /* output */

static void     hh_idx(DATA_TOKEN *token,         /* input  */
                       int        *data_idx,      /* output */
                       int        *header_idx,    /* output */
                       int        *line_idx,      /* output */
                       int         line_no);      /* input  */

static void     remove_block(DATA_TOKEN *token,
                             int         data_idx,
                             int         header_idx); 

static int      remove_header(DATA_TOKEN *token,
                              int         data_idx); 

static int sum_header(header_struct *header);

static int    split_header(DATA_TOKEN *token,
                           int         data_idx); 

static int prev_bit(uint32_t set[], int bit);
static int next_bit(uint32_t set[], int size, int bit);
static void shift_right(uint32_t set[], int size, int bit);
static void shift_left(uint32_t set[], int size, int bit);
static void split_color(uint32_t set[], uint32_t nset[], int size, int bit);


static header_struct   *split_block(DATA_TOKEN        *token,
                                    int                absolute_split_line,
                                    int                xtra_block);

void  exit(int retval);
void  wrap_input(DATA_TOKEN *token, int *line_no, int *len); 
int getpid(void);

/********************************************************************
*
*   Description of file held in memory:
*
*   data is an array of pointers to headers.
*   a header is an array of 1024 pointers each of which can point to
*   a line block.  A line block is an array of 100 pointers to char.
*   Each pointer to char points to a line of text.
*   It looks like this:
*
*  data                   headers
*  +--+        +--+--+--+--+--+--+--+--+--+--+--+--+
*  |  |------> |  |  |  |  |  |  |  |  |  |  |  |  | ...
*  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |
*  |  |        +--+--+--+--+--+--+--+--+--+--+--+--+
*  +--+
*  |  |        +--+--+--+--+--+--+--+--+--+--+--+--+
*  |  |------> |  |  |  |  |  |  |  |  |  |  |  |  | ...
*  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |
*  +--+        +--+--+--+--+--+--+--+--+--+--+--+--+
*   ...         |
*               |
*               V
*             +---+
*      blocks |   |------> "Line of text"
*             +---+
*             |   |------> "Line of text"
*             +---+
*             |   |------> "Line of text"
*             +---+
*             |   |
*             +---+
*             ...       
*
*
*     A token is an authenticatable structure to the perwindow data
*     of a window dminput dmoutput and textwindows
*
*
*************************************************************/

int HSP = (HEADER_SIZE / 2);   /* header split point */

/************************************************************************

NAME:    mem_init - generate a data structure token and init the structure 

************************************************************************/

DATA_TOKEN  *mem_init(int percent_full, int sequential_insert_strategy)
{
DATA_TOKEN   *token;

DEBUG3( fprintf(stderr, " @mem_init()\n");)

token = (DATA_TOKEN *)CE_MALLOC(sizeof(DATA_TOKEN));
                      
memset((char *)token, 0, sizeof(DATA_TOKEN));

token->marker                = TOKEN_MARKER;
token->current_line_number   = -1;
token->current_block_idx     = -1;
token->current_header_idx    = -1;
token->current_data_idx      = -1;
token->last_line_no          = -1;
token->line_load_level       = (percent_full * LINES_PER_BLOCK) / 100;
token->event_head            = NULL;
token->last_hh_block_idx     = 0;   /* optimization code */
token->last_hh_header_idx    = 0;
token->last_hh_data_idx      = 0;
token->last_hh_line          = 0;
token->colored               = 0;  /* has this memdata ever been colored on? */
token->writable              = True;
token->seq_insert_strategy   = sequential_insert_strategy;

undo_init(token);  /* initialize the undo dlist */

DEBUG3( fprintf(stderr, " mem_init(lll=%d)stop, returns 0x%X\n\n", token->line_load_level, token);)

#ifdef  Encrypt
if (ENCRYPT) encrypt_init(ENCRYPT);
#endif

return(token);

}  /* mem_init */

/************************************************************************

NAME:    mem_kill - Delete a mem_data structure and its token 

************************************************************************/

int mem_kill(DATA_TOKEN *token)
{

int free_sum = 0;
int i=0, j, k;
int line_count = 0;

header_struct *header_ptr;
block_struct *block_ptr;

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)){ fprintf(stderr, "Bad token passed to mem_kill\n"); kill(getpid(), SIGABRT);})

while ((i < DATA_SIZE) && (token->data[i].header != NULL) && (line_count < total_lines(token))){

    j=0;
    DEBUG3( fprintf(stderr, "DATA[%d](%d,&0x%x)\n", i, token->data[i].lines, token->data[i].header);)
    header_ptr = token->data[i].header;

    while ((j < HEADER_SIZE) && (header_ptr[j].block != NULL) && (line_count < total_lines(token))){

        k=0;
        DEBUG3( fprintf(stderr, "\tHEADER[%d](%d, &0x%x);\n", j, header_ptr[j].lines, header_ptr[j].block);)
        block_ptr = header_ptr[j].block;
        while ((k < header_ptr[j].lines) && (line_count < total_lines(token))){
            DEBUG3( fprintf(stderr,"\t\tLine[%d](dead);k(%d)\n",line_count, k);)
            free(block_ptr[k].text);
            free_sum +=  block_ptr[k].size;
            line_count++;
            k++;
        }
        j++;
    }
    i++;
}

kill_event_dlist(token); 

free((char *)token);

DEBUG3( fprintf(stderr, "MEM_KILL freed(%d) bytes\n", free_sum + sizeof(DATA_TOKEN));)
return(free_sum + sizeof(DATA_TOKEN));

}  /* mem_kill */


/************************************************************************

NAME:      load_a_block  - external entry to read a block of data

PURPOSE:   This routine loads a block of data.  A block of data is
           defined in this program as LINES_PER_BLOCK lines.
           The actual reading is done by read_a_block. This routine
           manipulates the data and header tables to organize
           access to the data after it is read in.
           Load a block always adds to the end of the list.

PARAMETERS:
   1.   token           -  pointer to DATA_TOKEN (opaque)
        This is the pointer to the top level header structure.  It is
        opaque to the caller and identifies the memory data object we
        are working with.  It is actually the pointer to the data_struct object.

   2.   stream          -  pointer to file (INPUT)
        This is the stream to read. 

   3.   eat_vt100       -  int (INPUT)
        If this flag is true, we will eat vt100 command sequences found in
        the data.  This is useful for processing manual pages spit out by the
        man command.

   4.   eof             -  pointer to int (INPUT / OUTPUT)
        This flag is set to true when eof is reached.

FUNCTIONS :

   1.   If eof is already set, just return.

   2.   Calculate the index into the data table and extract
        the pointer to the header table.  If we overflow (unlilkely),
        show eof.

   3.   If the header table is not yet created, create it.

   4.   Calculate this index into the header table.

   5.   Call read_a_block to read one block of data (LINES_PER_BLOCK lines)
        and save the block pointer in the header.

    
*************************************************************************/

void   load_a_block(DATA_TOKEN *token,
                    FILE       *stream,
                    int         eat_vt100,    /* input */
                    int        *eof)          /* input / output */
{
int  data_idx;
int  header_idx;
int  line_idx;    /* not used */
int  lines_put_in_block; 
int  last_line_has_newline;  /* not used */

char error_msg[64];

header_struct *header;   /* pointer to the array of double pointers */

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)) {fprintf(stderr, "Bad token passed to load_a_block\n"); kill(getpid(), SIGABRT);})

DEBUG3( fprintf(stderr, " @load_a_block(0x%X,%d,%d)\n", stream, total_lines(token), *eof);)

if (*eof)
   return;

 /*
  *  Get the indexes into the token.data, header, and
  *  line block for the line value passed in.
  *  Then bump the header index by 1 to show we are adding a
  *   new header.  The exception is the first time when there is nothing
  *   in the file.
  */

hh_idx(token, &data_idx, &header_idx, &line_idx, total_lines(token));

if (total_lines(token)) header_idx++;

if (header_idx >= HEADER_SIZE){
      header_idx = 0;
      data_idx++;
}                 

if (data_idx >= DATA_SIZE){
      snprintf(error_msg, sizeof(error_msg), "Too many lines of data, truncation of file at %d lines", total_lines(token));
      dm_error(error_msg, DM_ERROR_BEEP);
      *eof = 1;
      return;
}

DEBUG3( fprintf(stderr, "Loading a block at [%d,%d]\n",data_idx, header_idx);)

 /*
  *  Point to the current header in the top level index.  If it
  *  is null, we need a new one.  Get it. 
  */

header = token->data[data_idx].header;
if (header == NULL)
   {
      header = (header_struct *) CE_MALLOC(HEADER_SIZE * sizeof(header_struct));
      memset((char *) header, 0, HEADER_SIZE * sizeof(header_struct));
      header_idx = 0;
      token->data[data_idx].header = header;
      if (!header){
        /*  dm_error("Cannot malloc header.", DM_ERROR_LOG);  */
          *eof = 1;
          return;
      }
}

 /*
  *  Create a new lowest level index and read in a block of lines.
  *  Put that in the current block.  
  */

header[header_idx].block = read_a_block(token,
                                        stream, 
                                        &lines_put_in_block,
                                        eof,
                                        -1,   /* called from LAB */
                                        eat_vt100,
                                        &last_line_has_newline);

if (!header[header_idx].block){
    dm_error("Cannot read_a_block.", DM_ERROR_LOG);
    *eof = 1;
    return;
}

header[header_idx].lines       = lines_put_in_block;

token->data[data_idx].lines   += lines_put_in_block;
total_lines(token)            += lines_put_in_block;

 /*
  *  Point at the next header index and null it if it is not
  *  past the end of the list.
  */

header_idx++;
if (header_idx < HEADER_SIZE)
    header[header_idx].block = NULL;

DEBUG3( fprintf(stderr, " load_a_block(0x%X,%d,%d)\n\n", stream, total_lines(token), *eof);)

} /* end of load_a_block */


/************************************************************************

NAME:      read_a_block

PURPOSE:   This routine reads one block(LINES_PER_BLOCK lines) from
           the stream and saves it in memory.  An array of pointers
           to char is created and each one points to one line.

PARAMETERS:
   1.   token           -  pointer to DATA_TOKEN (opaque)
        This is the pointer to the top level header structure.  It is
        opaque to the caller and identifies the memory data object we
        are working with.  It is actually the pointer to the data_struct object.

   2.   stream          -  pointer to file (INPUT)
        This is the stream to read.

   3.   lines_put_in_block -  pointer to int (OUTPUT)
        The number of lines read into this block is returned in the parameter.

   4.   eof             -  pointer to int (INPUT / OUTPUT)
        This flag is set to true when eof is reached.

   5.   line_no  - -1 if called from load_a_block, N if from put_block.

   6.   eat_vt100       -  int (INPUT)
        If this flag is true, we will eat vt100 command sequences found in
        the data.  This is useful for processing manual pages spit out by the
        man command.

   7.   last_line_has_newline  -  pointer to int (INPUT / OUTPUT)
        This flag is set to true if the last line read has a newline on it.
        It is interesting to the routine which does the paste operation.

FUNCTIONS :

   1.   If eof is already set, return NULL.

   2.   Get a block of data to hold the pointers.

   3.   malloc data space to put the lines.

   4.   Read up to LINES_PER_BLOCK lines.

   5.   Copy the lines read to the data space and put a pointer to the
        start of each line in the block of pointers.

   6.   Return the address of the block of pointer.

RETURNS:
   block  -  pointer to block_struct
             The address of the block_struct with all the lines in it
             is returned.  NULL is returned if eof was previously read.
             An empty block is returned if there is an eof on the first read.
    
*************************************************************************/


static block_struct *read_a_block(DATA_TOKEN *token,
                                  FILE       *stream,
                                  int        *lines_put_in_block,     /*  output */
                                  int        *eof,                    /* input / output */
                                  int        line_no,                 /* input */
                                  int        eat_vt100,               /* input */
                                  int        *last_line_has_newline)  /* output */
{

block_struct   *block;

int i;
int len;

char *target;

char buff[MAX_LINE+2];

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)) {fprintf(stderr, "Bad token passed to read_a_block\n"); kill(getpid(), SIGABRT);})

*last_line_has_newline = 0;
*lines_put_in_block = 0;

if (*eof)
   return(NULL);

 /*
  *   Get the array we will put the pointers in.  Each pointer
  *   points to one line.  The block holds up to LINES_PER_BLOCK
  *   lines.
  */

block = (block_struct *) CE_MALLOC(LINES_PER_BLOCK * sizeof(block_struct));
if (!block)  /*   dm_error("Out of Memory! (Memdata/RAB[1])", DM_ERROR_LOG); */
      return((block_struct *) 0);

 /*
  *   Read the block till we get all the lines or hit eof.
  */

for (i = 0; i < token->line_load_level && !*eof; i++)
{
   errno = 0;
#ifdef MD_NULL
   if (get_line(buff, sizeof(buff), stream, &len) != NULL)
#else
   if (fgets(buff, sizeof(buff), stream) != NULL)
#endif
      {

         /*
          *   Bump the line count and get the line length.  Get rid
          *   of the newline at the end of the line but remember to
          *   account for the null when we copy to the permanent 
          *   area.
          */

         (*lines_put_in_block)++;
#ifndef MD_NULL
         len = strlen(buff);   
#endif
         if ((len == (MAX_LINE+1)) && (buff[MAX_LINE] != '\n')){   /* 1/15/93 */
              ungetc(buff[MAX_LINE], stream); 
              buff[MAX_LINE] = '\0';
              len--;
         }

         if (buff[len-1] == '\n'){   /* allows for line too long to warp */
               buff[len-1] ='\0';
               *last_line_has_newline = 1;
         }else{
               len++;
               *last_line_has_newline = 0;
         }

#ifdef Encrypt
         if (ENCRYPT) encrypt_line(buff);
#endif

         /*
          *   Put the line in more permanent storage.
          */

         target = (char *)CE_MALLOC(MROUND(len));
         if (!target){
            /*   dm_error("Out of Memory. (Memdata/RAB[2])", DM_ERROR_LOG); */
               return((block_struct *) 0);
         }

           /* the sema here is so changes are not added to the undo d-list if called from un_do() */ 
         if (!undo_semafor && (line_no != -1)) 
             event_do(token, RB_EVENT, line_no++, 0, 0, NULL);


         if (eat_vt100)  /* filter out back spaces and escape sequences */
             vt100_eat(target, buff);
         else             
             strcpy(target, buff);

         block[i].text = target;
         block[i].size = MROUND(len);
         block[i].color_data = NULL;

   }else{
       if (ferror(stream)){
            snprintf(buff, sizeof(buff), "I/O error reading file (%s)\n", strerror(errno));
            dm_error(buff, DM_ERROR_LOG);
            WRITABLE(token) = False; /* make file read only */
       }
       *eof = 1;
       DEBUG3( fprintf(stderr, "%d lines read in this block\n", *lines_put_in_block);)

   } /* fgets*/
} /* end of for loop */

return(block);

} /* end of read_a_block */



/************************************************************************

NAME:      position_file_pointer

PURPOSE:   This routine sets some global data (and data in token)
           to a line number. This allows calls to next() and prev()
           to be very efficient.

PARAMETERS:
   1.   token           -  pointer to DATA_TOKEN (opaque)
        This is the pointer to the top level header structure.  It is
        opaque to the caller and identifies the memory data object we
        are working with.  It is actually the pointer to the data_struct object.

   2.   line_no         -  int (INPUT)
        This is the line number to be positioned to.  If a number less
        than zero is specifed, the zero'th line is set.  If a number
        greater than the number of lines in the file is used,
        the pointer is set just past the last line.
        line_no is a zero based count of the total number of lines.


FUNCTIONS :

   1.   If the line number is out of range, set it to
        a reasonable value.

   2.   Use routine hh_idx to find the line.

   3.   Remember where the line was for future calls to next_line and prev_line.

*************************************************************************/

void position_file_pointer(DATA_TOKEN *token,       /* opaque */
                           int         line_no)     /* input  */
{

DEBUG3( fprintf(stderr, "postion(Token:0x%x, line:%d)\n", token, line_no);)
DEBUG0( if (!token || (token->marker != TOKEN_MARKER)) {fprintf(stderr, "Bad token passed to position_file_pointer\n"); kill(getpid(), SIGABRT);})

if (total_lines(token) == 0) {
   token->current_line_number = -1;
   return;
}

if (line_no > total_lines(token))
    line_no = total_lines(token);

 /*
  *  
  *  Set all the pointers.
  *  
  */

hh_idx(token, &token->current_data_idx, &token->current_header_idx, &token->current_block_idx, line_no);

token->current_header_ptr        = token->data[token->current_data_idx].header;
token->current_block_ptr         = token->current_header_ptr[token->current_header_idx].block;

token->current_line_number       = line_no;

}


/************************************************************************

NAME:      next_line

PURPOSE:   This routine returns the current line pointed to and bumps
           the pointers to the next line.

PARAMETERS:
   1.   token           -  pointer to DATA_TOKEN (opaque)
        This is the pointer to the top level header structure.  It is
        opaque to the caller and identifies the memory data object we
        are working with.  It is actually the pointer to the data_struct object.

INPUTS:

        The current line position as stored in the static position 
        variables is the input to this function.


FUNCTIONS :

   1.   Save the current line value to return.

   2.   Bump to the next line.  Handle the conditions of
        no more lines, end of line block, and end of header block.

OUTPUTS:
   RETURNED VALUE - pointer to char.
        The address of the current line is returned.  This is the memory
        address of the line and not a copy of the line.  Changes to the
        line, change the line.
   
*************************************************************************/

char    *next_line(DATA_TOKEN *token)
{
char *this_line;

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)) {fprintf(stderr, "Bad token passed to next_line\n"); kill(getpid(), SIGABRT);})
DEBUG3( fprintf(stderr, " @next_line(0x%x)\n", token);)


 /*
  *  
  *  If the position has not been established, return NULL.
  *  total_lines is global.  current_line_number starts at zero.
  *  
  */

if ((token->current_line_number >= total_lines(token)) || 
    (token->current_line_number < 0))
      return(NULL);

if (token->current_block_idx < 0)
      position_file_pointer(token, token->current_line_number);

 /*
  *  Get the current line pointer.  This is what we will return.
  *  Then bump to the next line.
  */

this_line = token->current_block_ptr[token->current_block_idx].text;

token->current_line_number++;
token->current_block_idx++;


/*
 *  Prepare for the next call to next_line.  If we at the last
 *  line this time, don't bother.  Remember:  At this point
 *  this_line contains what we will return.  We are setting
 *  up for the next call.
 *  
 *  Check to see if we are past the end of this block.  If so,
 *  bump to the next block.  If we bump the block pointer, check
 *  if we need to go to the next header.  If we bump the block
 *  pointer, watch for empty blocks.  Skip these.  Then we
 *  are in good shape for the next call.
 */

if (token->current_block_idx >= token->current_header_ptr[token->current_header_idx].lines &&
    token->current_line_number < total_lines(token)){
      token->current_block_idx = 0;
      token->current_header_idx++;
      if (token->current_header_idx >= HEADER_SIZE) /* breaking out of the header, normally false */
         {  /* broke out of current header */
            token->current_header_idx = 0;
            token->current_data_idx++;
            if (token->current_data_idx >= DATA_SIZE){
                   dm_error("File too Big!", DM_ERROR_BEEP);
                   return(NULL);
            }      
            token->current_header_ptr = token->data[token->current_data_idx].header;
         }

      /*
       *  Watch for empty line blocks.
       */

while((token->current_header_ptr[token->current_header_idx].lines == 0) ||
      !token->current_header_ptr[token->current_header_idx].block){  /* 1/14/93 fixes cupy of very large areas */
         token->current_block_idx = 0;
         token->current_header_idx++;
         if (token->current_header_idx >= HEADER_SIZE){ /* breaking out of the header, normally false */
               token->current_header_idx = 0;  /* broke out of current header */
               token->current_data_idx++;
               if (token->current_data_idx >= DATA_SIZE){
                      dm_error("End of file reached!", DM_ERROR_BEEP);
                      return(NULL);
               }      
               token->current_header_ptr = token->data[token->current_data_idx].header;
        }
      }

      token->current_block_ptr = token->current_header_ptr[token->current_header_idx].block;

}  /* broke out of current block */

DEBUG3( fprintf(stderr, " next_line(%s[%d,%d,%d](%d))\n", this_line, token->current_data_idx, token->current_header_idx, token->current_block_idx, token->current_line_number);)

return(this_line);

} /* end of next_line() */


/************************************************************************

NAME:      prev_line

PURPOSE:   This routine decrements the  current line pointer by 1 and 
           returns that line.

PARAMETERS:
   1.   token           -  pointer to DATA_TOKEN (opaque)
        This is the pointer to the top level header structure.  It is
        opaque to the caller and identifies the memory data object we
        are working with.  It is actually the pointer to the data_struct object.



FUNCTIONS :

   1.   Decrement to the next line.  Handle the conditions of
        no more lines, end of line block, and end of header block.

   2.   Return the current line.

OUTPUTS:
   RETURNED VALUE - pointer to char.
        The address of the current line is returned.  This is the memory
        address of the line and not a copy of the line.  Changes to the
        line, change the line.
   
*************************************************************************/

char    *prev_line(DATA_TOKEN *token)
{
char    *this_line;

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)) {fprintf(stderr, "Bad token passed to prev_line\n"); kill(getpid(), SIGABRT);})
DEBUG3( fprintf(stderr, " @prev_line(0x%x)\n",token);)


 /*
  *  If the position has not been established, return NULL.
  *  total_lines is global.  current_line_number starts at zero.
  */

if ((token->current_line_number > total_lines(token)) || 
    (token->current_line_number <= 0))
      return(NULL);

if (token->current_block_idx <= 0)
      position_file_pointer(token, token->current_line_number);

token->current_line_number--;

 /*
  *  Back up one line in the current block.
  *  Check to see if we are past the beginning of this block.  If
  *  so, decrement to the previous block.  If we decrement the
  *  block pointer check if we have fallen off the beginning of the
  *  header.
  */

token->current_block_idx--;

if (token->current_block_idx < 0)
   {
      token->current_header_idx--;
      if (token->current_header_idx < 0) /* normally false */
         {  /* broke out of current header */
            hh_idx(token, &token->current_data_idx, &token->current_header_idx, &token->current_block_idx, token->current_line_number);
#ifdef blah
            token->current_header_idx = HEADER_SIZE - 1;
            token->current_data_idx--; 
            if (token->current_data_idx < 0)  /* should never happen */
               {
                  DEBUG3( fprintf(stderr, "header position is out of sync with line numbers\n");)
                  position_file_pointer(token, 0);
                  return(NULL);
               }
#endif
            token->current_header_ptr = token->data[token->current_data_idx].header;
         }

      token->current_block_ptr = token->current_header_ptr[token->current_header_idx].block;
      token->current_block_idx = token->current_header_ptr[token->current_header_idx].lines-1;

   }  /* broke out of current block */

this_line = token->current_block_ptr[token->current_block_idx].text;

DEBUG3( fprintf(stderr, " prev_line(%s[%d,%d,%d](%d))\n", this_line, token->current_data_idx, token->current_header_idx, token->current_block_idx, token->current_line_number);)

return(this_line);

} /* end of prev_line */



/************************************************************************

NAME:      get_line_by_num

PURPOSE:   This routine gets the address on one line given the line
           number.  The current position is not affected.

PARAMETERS:
   1.   token           -  pointer to DATA_TOKEN (opaque)
        This is the pointer to the top level header structure.  It is
        opaque to the caller and identifies the memory data object we
        are working with.  It is actually the pointer to the data_struct object.

   2.   line_no         -  int (INPUT)
        This is the line number to be located.positioned to.  If a number less
        than zero is specifed, the zero'th line is set.  If a number
        greater than the number of lines in the file is used,
        the pointer is set just past the last line.
        line_no is a zero based count of the total number of lines.


FUNCTIONS :

   1.   If the line number is out of range, set it to
        a reasonable value.

   2.   Do some intial checks to optimize performance.

   3.   Use the line number to find the line and return it's address.

RETURNED VALUE:
   ptr - pointer to char
         pointer to the text of the requested line.
         If the requested line was not found, a pointer to a string of length zero is returned

*************************************************************************/

char    *get_line_by_num(DATA_TOKEN *token,       /* opaque */
                         int         line_no)     /* input  */
{
int                data_idx;
int                header_idx;
int                block_idx;
block_struct      *block_ptr;
header_struct     *header_ptr;

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)){ fprintf(stderr, "Bad token passed to get_line_by_num\n"); kill(getpid(), SIGABRT);})
DEBUG3( fprintf(stderr, " @get_line_by_number(token=0x%x,line=%d)\n", token, line_no);)


if ((line_no >= total_lines(token)) || (line_no < 0))
     return(""/* Bob wants this */); /* I don't know why? */

 /*
  *  Check if this is the current line or the same line as requested
  *  last time.
  */

if ((token->current_block_idx != -1) && (token->current_line_number == line_no))
   return(token->current_block_ptr[token->current_block_idx].text);

if (token->last_line_no == line_no)
   return(token->last_line);

/*
 *  Find the line
 */

hh_idx(token, &data_idx, &header_idx, &block_idx, line_no);

header_ptr = token->data[data_idx].header;
if (!header_ptr) return("");

block_ptr = header_ptr[header_idx].block;
if (!block_ptr) return("");

token->last_line    = block_ptr[block_idx].text;
token->last_line_no = line_no;

DEBUG3( fprintf(stderr, " get_line(%s[%d,%d,%d])\n", token->last_line, data_idx, header_idx, block_idx);)

return(token->last_line);

} /* get_line_by_num */
     

/************************************************************************

NAME:      delete_line_by_num  - given a line number, and a count of lines
                                 to be deleted, delete them.
                                 'count' is not currently used.

************************************************************************/

int      delete_line_by_num(DATA_TOKEN *token,       /* opaque */
                            int         line_no,     /* input  */
                            int         count)       /* output */ 
{

int            data_idx;
int            header_idx;
int            block_idx;

header_struct *header_ptr;
block_struct  *block_ptr;

/*if (cc_ce && !undo_semafor)  Changed 6/1/94 for new cc processing */
if (cc_ce) cc_dlbn(token, line_no, count);

DEBUG3(fprintf(stderr," @delete(token:0x%x,line:%d,tlines:%d,count:%d)\n",token, line_no, total_lines(token), count);)
DEBUG0( if (!token || (token->marker != TOKEN_MARKER)){ fprintf(stderr, "Bad token passed to delete_line_by_num\n"); kill(getpid(), SIGABRT);})

if (!total_lines(token)) return(0); /* can't delete from an empty file */

if ((line_no >= total_lines(token)) || (line_no < 0)){
      dm_error("No such line in file.", DM_ERROR_BEEP);
      return(-1);
}

hh_idx(token, &data_idx, &header_idx, &block_idx, line_no); /* locate */

header_ptr        = token->data[data_idx].header;         /* set ptrs */
block_ptr         = header_ptr[header_idx].block;

if (!undo_semafor) event_do(token, DL_EVENT, line_no, 0, 0, block_ptr[block_idx].text);

if (line_no <= token->last_line_no)
   token->last_line_no = -1;

if (line_no <= token->current_line_number) token->current_block_idx = -1;

if (block_ptr[block_idx].text){ 
   DEBUG3(fprintf(stderr," line being deleted: %s\n", block_ptr[block_idx].text);)
   free(block_ptr[block_idx].text);      /* free the unused memory */
}
block_ptr[block_idx].size = 0;    /* needed if deleting line #0 when it is the only line */
block_ptr[block_idx].text = NULL; /* needed if deleting line #0 when it is the only line */

shift_left(header_ptr[header_idx].color_bits, LINES_PER_BLOCK, block_idx); /* color bits need to be removed */

if (block_ptr[block_idx].color_data){
    free(block_ptr[block_idx].color_data);
    block_ptr[block_idx].color_data = NULL;
}

token->data[data_idx].lines--;         /* reduce the population count */
header_ptr[header_idx].lines--;
DEBUG3(fprintf(stderr," Dcount(%d)\n", header_ptr[header_idx].lines);)

if (!header_ptr[header_idx].lines && (total_lines(token) > 1)){     
     remove_block(token, data_idx, header_idx);   /* if blocks or headers become empty  kill them */
     if (-1 == prev_bit(token->data[data_idx].color_bits, HEADER_SIZE)){
                 clear_color_bit(token->color_bits, data_idx);                  /* color bits need to be removed */
                 if (-1 == prev_bit(token->color_bits, DATA_SIZE)) COLORED(token) = False;
     }
     if (!(token->data[data_idx].lines))  remove_header(token, data_idx);
}else{
      /* pull up the following lines to compress the whole out */
      while(block_idx < header_ptr[header_idx].lines){ 
         block_ptr[block_idx].text = block_ptr[block_idx+1].text;
         block_ptr[block_idx].size = block_ptr[block_idx+1].size;
         block_ptr[block_idx].color_data = block_ptr[block_idx+1].color_data;
         block_idx++;
      }

      if (-1 == prev_bit(header_ptr[header_idx].color_bits, LINES_PER_BLOCK)){
          clear_color_bit(token->data[data_idx].color_bits, header_idx);             /* color bits need to be removed */

          if (-1 == prev_bit(token->data[data_idx].color_bits, HEADER_SIZE)){
                 clear_color_bit(token->color_bits, data_idx);                  /* color bits need to be removed */
                 if (-1 == prev_bit(token->color_bits, DATA_SIZE)) COLORED(token) = False;
          }
      }
}
     
dirty_bit(token) = 1;     /* we have altered the file */
total_lines(token)--;

return(0);

} /* delete_line_by_num */

#define DELETE_TOKEN   -3333333  /* arbitrary invalid values */

/************************************************************************

NAME:      delayed_delete  - given a line number
                                          mark it to be deleted.

                                 if (delay == 0) then do the
                                      delete now, that was marked earlyer

************************************************************************/

int      delayed_delete(DATA_TOKEN *token,  /* opaque */
                        int     line_no,    /* input  */
                        int     delay,      /* input  LATER/NOW  */
                        int     *column,    /* input/ouput  new length */
                        int     join)       /* input  Join later */
{

int data_idx;
int header_idx;
int block_idx;

header_struct *header_ptr;
block_struct  *block_ptr;


DEBUG0( if (!token || (token->marker != TOKEN_MARKER)){ fprintf(stderr, "Bad token passed to delayed_delete\n"); kill(getpid(), SIGABRT);})
DEBUG3(fprintf(stderr," @delay_delete(token:0x%x,line:%d,tlines:%d, delay=%d)\n",token, line_no, total_lines(token), delay);)

if (!total_lines(token)) return(0); /* can't delete from an empty file */

if ((line_no >= total_lines(token)) || (line_no < 0)){
      dm_error("No such line in file.", DM_ERROR_BEEP);
      return(-1);
}

hh_idx(token, &data_idx, &header_idx, &block_idx, line_no); /* locate */

header_ptr        = token->data[data_idx].header;         /* set ptrs */
block_ptr         = header_ptr[header_idx].block;

if (delay == LATER){     /* LATER */
    if (join)
       block_ptr[block_idx].size = -ABSVALUE(block_ptr[block_idx].size);  /* both ABSs added 9/15/93 */
    else   
       block_ptr[block_idx].size = DELETE_TOKEN; 
    DEBUG3(fprintf(stderr, "line marked for deletion[%d}: %s\n", line_no, block_ptr[block_idx].text);)
}else{                   /* NOW */

   if (block_ptr[block_idx].size < 0){    /* NOW DELETE/JOIN */
       if (join && (block_ptr[block_idx].size != DELETE_TOKEN)){
           DEBUG13(fprintf(stderr,"Joining line(%d)len=%d\n", line_no, strlen(block_ptr[block_idx].text));)
           join_line(token, line_no);
           *column = strlen(block_ptr[block_idx].text)+1; /* 12/1/93 fixes cursor :222' s/@n/5/ */
        /* block_ptr[block_idx].size = ABSVALUE(block_ptr[block_idx].size); 8/21/96 fixes 1,$s/@n/ /;1 xc X  on 8/20/96  */
           block_ptr[block_idx].size = MROUND(strlen(block_ptr[block_idx].text)+1); 
       }else  
           delete_line_by_num(token, line_no, 1);
       return(1);
   }
} /* NOW/LATER */

return(0);

} /* delayed_delete() */

/************************************************************************

NAME:      put_line_by_num - given a line, and data place the new line in the 
                             data structure the flag is insert/overwrite

************************************************************************/

int      put_line_by_num(DATA_TOKEN *token,       /* opaque */
                         int         line_no,     /* input */
                         char       *line,        /* input */ 
                         int         flag)        /* input;   0=overwrite 1=insert after */
{
int len = strlen(line);
int block_idx, temp_idx;
int header_idx;
int data_idx;

int split_pos;

char *target;
char c;

header_struct *header_ptr;

block_struct  *block_ptr;

while (len > MAX_LINE){
     c = line[MAX_LINE];
     line[MAX_LINE] = '\0';
     dm_error("line too long, wrapped", DM_ERROR_BEEP);
     put_line_by_num(token, line_no++, line, flag);
     line[MAX_LINE] = c;
     line += MAX_LINE;
     len = strlen(line);
}

 /*
  *  Establish position with hh_idx
  */


/*if (cc_ce && !undo_semafor)  Changed 6/1/94 for new cc processing */
if (cc_ce)
   cc_plbn(token, line_no, line, flag);

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)) {fprintf(stderr, "Bad token passed to put_line_by_num\n"); kill(getpid(), SIGABRT);})
DEBUG3( fprintf(stderr, " @put_line(token=0x%x,lines=%d,tlines=%d,'%s')\n",token, line_no, total_lines(token), line);)

if ((line_no > total_lines(token)) || (line_no < -1)){
      fprintf(stderr, "Put Line error! line: %d\n", line_no);
      dm_error("No such line in file.", DM_ERROR_BEEP);
      return(-1);
}

hh_idx(token, &data_idx, &header_idx, &block_idx, line_no);

/* #define EQN */
#ifdef EQN
{
char *eqnptr;
char eqnrslt[256];
char cmd[4096];
FILE *eqnfp;
if (eqnptr = strstr(line, "EQN=")){ 
   eqnptr += 4;
   snprintf(cmd, sizeof(cmd), "echo '%s' | /usr/bin/bc -l", eqnptr); 
   eqnfp = popen(cmd, "r");
   fgets(eqnrslt, 256, eqnfp); 
   fclose(eqnfp);
   realloc(line, strlen(eqnrslt) + strlen(line));
   eqnptr -= 4;
   strcpy(cmd, eqnptr+3);
   *(eqnrslt + strlen(eqnrslt) -1) = NULL;
   strcpy(eqnptr, eqnrslt);
   strcat(eqnptr, cmd);
}
}
#endif

header_ptr = token->data[data_idx].header;         /* set ptrs */
if (header_ptr == NULL && total_lines(token) == 0){  /* if empty file initialize the first line */
      /* no lines in the file, get the first header and block */
      header_ptr = token->data[data_idx].header = (header_struct *) CE_MALLOC(HEADER_SIZE * sizeof(header_struct));
      if (!header_ptr) return(-1);                                
      memset((char *)header_ptr, 0, (HEADER_SIZE * sizeof(header_struct)));
      header_ptr[header_idx].block = (block_struct *) CE_MALLOC(LINES_PER_BLOCK * sizeof(block_struct));
      if (!header_ptr[header_idx].block) return(-1);                                
      header_ptr[header_idx].lines = 0;
      if (flag == INSERT)
         block_idx--; 
      else
         header_ptr[header_idx].block->size = 0;
   }

/* fprintf(stderr," Memory[%x] 0x%x {TOKEN=%d(%s), LINE=%d, TEXT= '%s'}\n", &header_ptr[header_idx].block[256].text,
               header_ptr[header_idx].block[256].text, token, flag == INSERT ? "INSERT" : "OVERWRITE", line_no, line);
if (header_ptr[header_idx].block[256].text == 0)
     block_ptr = NULL;       */

block_ptr  = header_ptr[header_idx].block;

if (flag == INSERT){
      /* insert after */

      if (line_no == -1) block_idx = -1; /* if undo of delete line 0, backup block index by 1 */

      if (!undo_semafor) event_do(token, PL_EVENT, line_no, 0, flag, NULL);
      DEBUG3( fprintf(stderr, " Doing an INSERT\n\n");)

      /*
       *  Since we are adding a line, invalidate the held pointer used
       *  by i_get_line_by_num since it may be no good now.
       */

      if (line_no <= token->last_line_no)
         token->last_line_no = -1;

      if (line_no <= token->current_line_number) token->current_block_idx = -1;
                 
      target = (char *)CE_MALLOC(MROUND(len + 1));   /* get the line space + '\0' */
      if (!target){
            /* dm_error("Out of Memory. (Memdata/PLBN[1])", DM_ERROR_LOG);  */
            return(-1);
      }

      strcpy(target, line);

      /*
       *  See if there is room for one more line.  If not split the
       *  block and recalculate our position.
       *  The split pos calculation splits half way between where we
       *  want to do the insert and the middle of the block.
       */

      if (header_ptr[header_idx].lines >= (LINES_PER_BLOCK -1)){
            if (token->seq_insert_strategy)  /* RES 01/07/2003 optimize pad mode  */
               split_pos = line_no-1; /* RES 01/07/2003 optimize pad mode  */
            else
               split_pos = (line_no - block_idx) + ((block_idx - (LINES_PER_BLOCK / 2)) / 2) + (LINES_PER_BLOCK / 2);
            if (!split_block(token, split_pos, 0))
                     return(-1);
            hh_idx(token, &data_idx, &header_idx, &block_idx, line_no);  /* recalculate where ? */
            header_ptr = token->data[data_idx].header;         /* set ptrs */
            block_ptr  = header_ptr[header_idx].block;
      }


      for (temp_idx = header_ptr[header_idx].lines;
           temp_idx > block_idx;
           temp_idx--){
         /* shift everything down including the null*/
         block_ptr[temp_idx+1].text =  block_ptr[temp_idx].text;
         block_ptr[temp_idx+1].size =  block_ptr[temp_idx].size;
         block_ptr[temp_idx+1].color_data =  block_ptr[temp_idx].color_data;
      }

      block_idx++; /* this positions us over the spot to overwrite */
      shift_right(header_ptr[header_idx].color_bits, LINES_PER_BLOCK, block_idx); /* changed to fix color not shifted down on a linedelete undelete, was below the next line */

      block_ptr[block_idx].text = target;  /* implant the new record */
      block_ptr[block_idx].size = MROUND(len+1);
      block_ptr[block_idx].color_data = NULL;

      header_ptr[header_idx].lines++; /* add one to each of the larger structures */
      token->data[data_idx].lines++;

      total_lines(token)++;
}  /* end of insert mode */
else{
      /* over write */
                                             
      if (!undo_semafor) event_do(token, PL_EVENT, line_no, 0, flag, block_ptr[block_idx].text);
      DEBUG3( fprintf(stderr, " Doing an OVERWRITE of (%s)\n\n", block_ptr[block_idx].text);)

      if (block_ptr[block_idx].size < len+1){         /* new line is longer */
            target = (char *)CE_MALLOC(MROUND(len + 1));               /* so get bigger */
            if (!target){
               /*  dm_error("Out of Memory. (Memdata/PLBN[2])", DM_ERROR_LOG);  */
                 return(-1);
            }

            if(block_ptr[block_idx].text)         /* get rid of the old buffer */
               free(block_ptr[block_idx].text);   
            block_ptr[block_idx].text = target;  /* put the new line in */

            if (block_ptr[block_idx].size >= 0) /* for delayed_delete preservation of negative sizes */
                block_ptr[block_idx].size = MROUND(len+1); 

            if (!total_lines(token)){
                total_lines(token)++;    /* empty file */
                token->data[0].lines++;
                header_ptr[0].lines++;
            }

            if (line_no == token->last_line_no) 
               token->last_line = target;
      }
      strcpy(block_ptr[block_idx].text, line);   /*  copy in new line */
/*      if (block_ptr[block_idx].color_data) put_color_by_num(token, line_no, NULL); */ /* added 6/9/97 to fix delete line/undelete line color propagation problem */

} /* if INSERT/OVERWRITE */

dirty_bit(token) = 1;       /* update globals */

return(0);

} /* put_line_by_num */



/************************************************************************

NAME:      put_block_by_num -  by using read_a_block, or putlines, read a 
                               file(paste-buffer) and inset it into the database.
                               the first line may be an estimate of size which 
                               allows us to use read_a_block. 

OUTPUTS:
   rc   -  Return code
           0  -  Successfull
          -1  -  Failure, msg already generated

************************************************************************/

void     put_block_by_num(DATA_TOKEN *token,       /* opaque */
                          int         line_no,     /* input  */
                          int         est_lines,   /* input  */
                          int         column_no,   /* input  */
                          FILE       *stream)      /* input  */
{

int eof = 0;
int len;
char    *bufin;
char    *bufout;
int offset; /* dummy variable */

int data_idx, header_idx, block_idx;

char *ptr;
char buf[MAX_LINE  * 2 + 4];
char head[MAX_LINE * 2 + 4];
char tail[MAX_LINE + 2];
char *lch, head_color[MAX_LINE+1], tail_color[MAX_LINE+1];

header_struct    *header_ptr;
block_struct     *block_ptr;

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)) {fprintf(stderr, "Bad token passed to put_block_by_num"); kill(getpid(), SIGABRT);})
DEBUG3( fprintf(stderr, " @put_block(token=0x%x, line=%d, est=%d, col=%d)\n",token, line_no, est_lines, column_no);)

head_color[0] = '\0';
tail_color[0] = '\0';

 /*
  *  
  *  Get the line at the insert point and establish position.  Set
  *  the header pointer and block pointer from this position info.
  *  
  */

if ((line_no > total_lines(token)) || (line_no < 0)){
      fprintf(stderr, "Put Block %d\n", line_no);
      dm_error("No such line in file.", DM_ERROR_BEEP);
      return;
}

hh_idx(token, &data_idx, &header_idx, &block_idx, line_no);

header_ptr = token->data[data_idx].header;
if (header_ptr){
    /* res 1/26/94, add checks for header_ptr and block_ptr being null (new memdata structure) */
    block_ptr = header_ptr[header_idx].block;
    if (block_ptr)
       ptr = block_ptr[block_idx].text;
    else
       ptr = NULL;
}else
   ptr = NULL;
 
 /*
  *  Since we are adding a line invalidate the held pointer used
  *  by i_get_line_by_num since it may be no good now.
  */

if (line_no <= token->last_line_no)  
   token->last_line_no = -1;

if (line_no <= token->current_line_number) token->current_block_idx = -1;

 /*
  *  Save the head and tail of the existing line.
  */

if (ptr){
    
    len = strlen(ptr);

    if (lch = get_color_by_num(token, line_no, COLOR_CURRENT, &offset)){
        cdgc_split(lch, column_no, 0, len, CDGC_SPLIT_BREAK, head_color, tail_color);
    } 
    if (column_no >= len){
       strcpy(head, ptr);
       while (len < column_no) head[len++] = ' ';
       head[len] = '\0';
       tail[0] = '\0';
    }else{
       if (column_no == 0){
           head[0] = '\0';
           strcpy(tail, ptr);
       }else{
           
           strncpy(head, ptr, column_no); 
           head[column_no] = '\0';
           strcpy(tail, ptr+column_no);
       }
    }

}else{              /* NULL line */
   head[0] = '\0';
   tail[0] = '\0';
}

if (ce_fgets(buf, MAX_LINE+2, stream) == NULL){
      eof = 1;
      buf[0] = '\0';
}

len = strlen(buf);

 /*
  *  Special case processing for zero lines.  This is a partial
  *  line being inserted into the middle of the line. 
  *
  *  A line more than 2 * MAX_LINE will fall into this catagory,
  *  but that is a trashed out file anyway, so we won't worry about it.
  */

if (((len == 0) || (buf[len-1] != '\n')) && (len < (MAX_LINE+1))){
      if (len > MAX_LINE){
            dm_error("Line too long.[1]", DM_ERROR_BEEP);
            buf[MAX_LINE] = '\0';
      }
      strcat(head, buf);
      len = strlen(head);
      if (len > MAX_LINE){
            dm_error("Line too long.[2]", DM_ERROR_BEEP);
            head[MAX_LINE] = '\0';
      }else{
            strcat(head, tail);
            len = strlen(head);
            if (len > MAX_LINE){
                  dm_error("Line too long.[3]", DM_ERROR_BEEP);
                  head[MAX_LINE] = '\0';
            }
      }

      put_line_by_num(token, line_no, head, OVERWRITE);

      if (lch = get_color_by_num(token, line_no, COLOR_CURRENT, &offset)){
          cdgc_split(lch, column_no, strlen(buf), len, CDGC_SPLIT_NOBREAK, head_color, NULL);
          if (*head_color) put_color_by_num(token, line_no, head_color);
      }

      if (len > MAX_LINE) wrap_input(token, &line_no, &len); 

      return;
}


 /*
  *  Back to the business at hand.  We have a multi line file to insert.
  *  Take the first line we read in and put it out with the header
  *  in on the front.
  */

buf[--len] = '\0'; /* get rid of the '\n' */

#ifdef Encrypt
if (ENCRYPT) encrypt_line(buf);
#endif

strcat(head, buf);
len = strlen(head);

put_line_by_num(token, line_no, head, OVERWRITE);
put_color_by_num(token, line_no, head_color);
if (len > MAX_LINE) wrap_input(token, &line_no, &len);

 /*
  *  If we have a lot of lines to process.  Load them a block
  *  at a time.  Split the block at the point of insertion and
  *  ask split_block for space for an extra block.  We get
  *  the block from read a block and drop it in place.
  *  Due to the way split_block works, we cannot split a block
  *  after the last line in the block.  Thus, in this unusual
  *  case, we will insert the lines the regular way.
  */

#ifdef LOADBLOCKS

* not color coordinated ! *

if (header_ptr[header_idx].lines != block_idx+1)
   while (est_lines > token->line_load_level){
      new_header_ptr = split_block(token, line_no, 1);
      if (!new_header_ptr) return;
      new_header_ptr->block = read_a_block(token, stream, &i, &eof, line_no, &last_line_has_newline);
      new_header_ptr->lines = i;
      line_no += i;  /* move pointer of where to insert */
      total_lines(token) += i;
      est_lines -= i;

      if (eof){
            if (!last_line_has_newline){ /* oops, not as many lines as we thought */
                  bufin = new_header_ptr->block[i-1].text;
                  snprintf(buf, sizeof(buf), "%s%s", bufin, tail);
                  if (*buf) put_line_by_num(token, line_no, buf, OVERWRITE);
            }else
                  if (*tail) put_line_by_num(token, line_no, tail, INSERT);
            return;
      }
   } /* end of while lots of lines to go */

#endif

 /*
  *  Read and process the est_lines in the file.  We use a read ahead
  *  one scheme to allow us to see eof one line before
  *  we get to it.
  *
  *  Use a 2 buffer system to read and insert the rest of
  *  the lines.  First we prime the output buffer.  EOF here
  *  makes everything else fall through.  Otherwise we read and
  *  write till we hit eof.  At eof, the last line is waiting to
  *  be output in bufout.  We attach the tail and we are done.
  */

bufin  = head;
bufout = buf;

if (ce_fgets(bufout, MAX_LINE+1, stream) == NULL){
      eof = 1;
      if (*tail){
            put_line_by_num(token, line_no, tail, INSERT);
            if (*tail_color) put_color_by_num(token, line_no+1, tail_color);
      }else
            put_line_by_num(token, line_no, "", INSERT);
}

while(!eof){

   if (ce_fgets(bufin, MAX_LINE+1, stream) == NULL) eof = 1;

   len = strlen(bufout); 

   if (!eof && (bufout[len-1] != '\n') && (bufin[0] == '\n') && (bufin[1] == '\0'))
           if (ce_fgets(bufin, MAX_LINE+1, stream) == NULL) eof = 1;

   if (eof){

         /*
          *  Check if the last line has a newline at the end.  If so,
          *  The tail goes on it's own line.
          */

         if (bufout[len-1] == '\n'){
             bufout[len-1] = '\0';
#ifdef Encrypt
             if (ENCRYPT) encrypt_line(bufout);
#endif
             put_line_by_num(token, line_no++, bufout, INSERT);
             put_line_by_num(token, line_no, tail, INSERT);
             if (*tail_color) put_color_by_num(token, line_no+1, tail_color);
             if (len > MAX_LINE) wrap_input(token, &line_no, &len);
         }else{             /* last line is in bufout */
#ifdef Encrypt
             if (ENCRYPT) encrypt_line(bufout);
#endif
             strcat(bufout, tail);
             len = strlen(bufout);
             put_line_by_num(token, line_no, bufout, INSERT);
             if (lch = get_color_by_num(token, line_no+1, COLOR_CURRENT, &offset)){
                 cdgc_split(lch, 0, len, len, CDGC_SPLIT_BREAK, NULL, tail_color);
                 if (*tail_color) put_color_by_num(token, line_no, tail_color);
             }
             if (len > MAX_LINE) wrap_input(token, &line_no, &len);
         }

   }else{           /* get bufout ready for output */
         if (bufout[len-1] == '\n')
            bufout[len-1] = '\0';

#ifdef Encrypt
         if (ENCRYPT) encrypt_line(bufout);
#endif

         put_line_by_num(token, line_no, bufout, INSERT);
         if (len > MAX_LINE) wrap_input(token, &line_no, &len);
         line_no++;
  }
   
   ptr = bufin;
   bufin = bufout;
   bufout = ptr;
   
} /* while !eof */

return;

} /* put_block_by_num */

/************************************************************************

NAME:      save_file - walk the data structure and write each line out.

************************************************************************/
#define INIT_WRITE_REPORT  10000
#define WRITE_REPORT 50000

void save_file(DATA_TOKEN *token,       /* opaque */
               FILE       *fp)          /* input  */
{
int i=0, j, k;
int line_count = 0;

#ifdef Encrypt
char line[MAX_LINE+2];
#endif
char msg[64];
char *bptr;

header_struct *header_ptr;
block_struct *block_ptr;

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)){ fprintf(stderr, "Bad token passed to save_file\n"); kill(getpid(), SIGABRT);})
DEBUG3( fprintf(stderr, "@ save_file\n");)

while ((i < DATA_SIZE) && (token->data[i].header != NULL))
{

    j=0;
    DEBUG3( fprintf(stderr, "DATA[%d](%d,&0x%x)\n", i, token->data[i].lines, token->data[i].header);)
    header_ptr = token->data[i].header;

    while ((j < HEADER_SIZE) && (header_ptr[j].block != NULL)){

        k=0;
        DEBUG3( fprintf(stderr, "\tHEADER[%d](%d,&0x%x)\n", j, header_ptr[j].lines,header_ptr[j].block);)
        block_ptr = header_ptr[j].block;

        while (k < header_ptr[j].lines){
            DEBUG3( 
               fprintf(stderr,"\t\tLine[%d](%2.2d):",line_count, block_ptr[k].size);
               if (fp != stderr) fprintf(stderr,"! %s\n", block_ptr[k].text);
            )
#ifdef Encrypt
            if (ENCRYPT){
                strcpy(line, block_ptr[k].text);
                encrypt_line(line);
                bptr = line;
            }else
#endif
                bptr =  block_ptr[k].text;
            if (bptr)
               {
                  if (fputs(bptr, fp) == EOF){ /* RES 2/9/1999 add test for eof and ferror code */
                     if (ferror(fp)){
                        snprintf(msg, sizeof(msg), "Error writing out file on line %d (%s)", line_count+1, strerror(errno));
                        dm_error(msg, DM_ERROR_LOG);
                        return; 
                     }
                  }
               }
            else{
               snprintf(msg, sizeof(msg), "Internal error in save_file, NULL pointer encountered, line %d lost.", line_count+1);
               dm_error(msg, DM_ERROR_LOG);
            }
            putc('\n', fp);
            if ((line_count == INIT_WRITE_REPORT) || (!(line_count % WRITE_REPORT) && (line_count >= INIT_WRITE_REPORT))){ 
                 snprintf(msg, sizeof(msg), "Written %d lines.", line_count);
                 dm_error(msg, DM_ERROR_MSG);
            }

            if (++line_count >= total_lines(token)){
                 dirty_bit(token) = 0;
                 if (line_count >= INIT_WRITE_REPORT) dm_error("File written.", DM_ERROR_MSG);
                 event_do(token, PW_EVENT, -1, 0, 0, NULL); 
                 return; 
            }
            k++;
        }
        j++;
    }
    i++;
}

if (line_count >= INIT_WRITE_REPORT) dm_error("File written", DM_ERROR_MSG);

event_do(token, PW_EVENT, -1, 0, 0, NULL); 
dirty_bit(token) = 0;

} /* save_file */

/************************************************************************

NAME:      hh_idx - given a line number, return  the three indexes that will
                    point us at it.

************************************************************************/

static void     hh_idx(DATA_TOKEN *token,
                       int        *data_idx,
                       int        *header_idx,
                       int        *block_idx,
                       int         line_no)
{

int i, j;
int block_top;
int block_bottom;

int total = 0;

header_struct *h_ptr;

DEBUG3( fprintf(stderr," @hh_idx(%d): start\n", line_no);)
DEBUG0( if (!token || (token->marker != TOKEN_MARKER)) {fprintf(stderr, "Bad token passed to hh_idx\n"); kill(getpid(), SIGABRT);})

#ifdef blah
fprintf(stderr,"ENTER-> last_hh_block_idx:%d   TOKEN(%d)\n", token->last_hh_block_idx, token);
if (token->last_hh_block_idx == 25)
            ob=0;
#endif

if (token->current_block_idx != -1){  /* optomization! (if same block as last call) */
    DEBUG3( fprintf(stderr," Optimizing DB search!\n");)
    block_top = token->last_hh_line - token->last_hh_block_idx;  
    block_bottom = block_top + token->data[token->last_hh_data_idx].header[token->last_hh_header_idx].lines -1;  
    if ((line_no <= block_bottom) && (line_no >= block_top)){  /* we can optomize */
       token->last_hh_block_idx += line_no - token->last_hh_line;
       *block_idx = token->last_hh_block_idx;
       *header_idx =  token->last_hh_header_idx;
       *data_idx = token->last_hh_data_idx;
       token->last_hh_line = line_no;  /* optomizes hh_idx */
       DEBUG3( fprintf(stderr," optomizer:[D:%d, H:%d, B:%d]: stop\n", *data_idx, *header_idx, *block_idx);)
       return; 
    }
}

if (line_no >= total_lines(token)) line_no = total_lines(token); 

if (line_no <= 0){  /* very first line is different because load a block may not have run */
    *block_idx = 0;                                
    *data_idx = 0;
    *header_idx = 0;
    token->last_hh_line = 0;  /* optomizes hh_idx */
    token->last_hh_block_idx = 0;
    token->last_hh_data_idx = 0;
    token->last_hh_header_idx = 0; 
    return;
}

/*get the data index */
for (i=0; i<DATA_SIZE; i++){
   total += token->data[i].lines; 

   if ((total > line_no) || (total >= total_lines(token))){ 
         total -= token->data[i].lines; 
         break; 
   }
}

/* get the header index */
if (i == DATA_SIZE){
      DEBUG( fprintf(stderr,"Line addressed out of range!: %d\n", line_no);)
      dm_error("Line addressed out of range! (Internal Error)", DM_ERROR_LOG);
      fprintf(stderr, "Line addressed out of range! (Internal Error)");
      create_crash_file();
      exit(1); 
}

h_ptr = token->data[i].header;

for (j=0; j<HEADER_SIZE; j++){
   if (h_ptr[j].block)
      total += h_ptr[j].lines;
   if ((total > line_no) || (total >= total_lines(token))){ 
         total -= h_ptr[j].lines;
         break; 
   }                                                                        
}                                                                        

*block_idx = line_no - total;
*data_idx = i;
*header_idx = j;

token->last_hh_line = line_no;  /* optomizes hh_idx */
token->last_hh_block_idx = *block_idx;
token->last_hh_data_idx = *data_idx;
token->last_hh_header_idx = *header_idx;

DEBUG3( fprintf(stderr," hh_idx[D:%d, H:%d, B:%d]: stop\n\n", *data_idx, *header_idx, *block_idx);)

}  /* hh_idx */

/************************************************************************

NAME:    remove_block - squeeze  a block out by removing it and
                pulling all following blocks up.

************************************************************************/

static void     remove_block(DATA_TOKEN *token,
                             int         data_idx,
                             int         header_idx)
{

header_struct *header_ptr;

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)) {fprintf(stderr, "Bad token passed to remove_block\n"); kill(getpid(), SIGABRT);})
header_ptr = token->data[data_idx].header;

free((char *)header_ptr[header_idx].block);
shift_left(token->data[data_idx].color_bits, HEADER_SIZE, header_idx);             /* color bits need to be removed */

while((header_idx < (HEADER_SIZE-1)) && (header_ptr[header_idx].block != NULL)){
   header_ptr[header_idx] = header_ptr[header_idx+1];
#ifdef blahbob
   header_ptr[header_idx].block = header_ptr[header_idx+1].block;
   header_ptr[header_idx].lines = header_ptr[header_idx+1].lines;
   copy_color_block(header_ptr[header_idx].color_data, header_ptr[header_idx+1].color_data);
#endif
   header_idx++;
}

if (-1 == prev_bit(token->data[data_idx].color_bits, HEADER_SIZE)){
   clear_color_bit(token->color_bits, data_idx);                  /* color bits need to be removed */

   if (-1 == prev_bit(token->color_bits, DATA_SIZE))
      token->colored = False;
}

header_ptr[HEADER_SIZE-1].block = NULL;
header_ptr[HEADER_SIZE-1].lines  = 0;
clear_color_block(header_ptr[HEADER_SIZE-1].color_bits);

}  /* remove_block  */

/************************************************************************

NAME:    remove_header - squeeze a header out by removing it and
                pulling all following blocks up.

************************************************************************/

static int      remove_header(DATA_TOKEN *token,
                              int         data_idx)
{

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)) {fprintf(stderr, "Bad token passed to remove_header\n"); kill(getpid(), SIGABRT);})
free((char *)token->data[data_idx].header);
shift_left(token->color_bits, DATA_SIZE, data_idx);             /* color bits need to be removed */

while((data_idx < (DATA_SIZE-1)) && (token->data[data_idx].header != NULL)){
   token->data[data_idx] = token->data[data_idx+1];
   data_idx++;
}

if (-1 == prev_bit(token->color_bits, DATA_SIZE))
   token->colored = False;

token->data[DATA_SIZE-1].header = NULL;
token->data[DATA_SIZE-1].lines  = 0;
clear_color_hdr(token->data[DATA_SIZE-1].color_bits);

return(0);

}  /* remove_header */

/************************************************************************

NAME:    split_header  - split a header into two roughly equal halves. 

************************************************************************/

static int  split_header(DATA_TOKEN *token, int data_idx)
{

int i, im1;
int split_point = HSP;

header_struct *header_ptr, *new_header;

DEBUG3( fprintf(stderr, " @split_header(%d) \n", data_idx);)
DEBUG0( if (!token || (token->marker != TOKEN_MARKER)) {fprintf(stderr, "Bad token passed to split_header\n"); kill(getpid(), SIGABRT);})

if (token->seq_insert_strategy)
   split_point = HEADER_SIZE -1;

i = data_idx;

header_ptr = token->data[i].header;

while (token->data[i].header != NULL)
  i++;

if (i >= DATA_SIZE)
{
       dm_error("File has become too big!", DM_ERROR_LOG);
       return(-1);
}

data_idx++;
while (i > data_idx)
{
    im1 = i - 1;
    token->data[i] = token->data[im1]; /* RES 1/7/2002 move elements down one to create a space, this is a structure copy */
    /*token->data[i].header = token->data[im1].header;  */
    /*token->data[i].lines = token->data[im1].lines;    */
    memset(&(token->data[im1]), 0, sizeof(data_struct)); /* RES 1/7/2002 clear the now empty structure */
    /*token->data[im1].header = NULL;   */
    /*clear_color_hdr(token->data[im1].color_bits);  */
    i--;
}
data_idx--;

new_header = (header_struct *) CE_MALLOC(HEADER_SIZE * sizeof(header_struct));
memset((char *) new_header, 0, HEADER_SIZE * sizeof(header_struct));
token->data[data_idx + 1].header = new_header;

if (new_header == NULL){
 /*  dm_error("Cannot malloc header.", DM_ERROR_LOG);  */
   return(-1);
}

for (i = split_point; i < HEADER_SIZE; i++){
   new_header[i - split_point] = header_ptr[i]; /* structure copy of header_struct */
   header_ptr[i].block = NULL;
   header_ptr[i].lines = 0;
   clear_color_block(header_ptr[i].color_bits);
}

new_header[i - split_point].block = NULL;
new_header[i - split_point].lines = 0;
clear_color_block(new_header[i - split_point].color_bits);

token->data[data_idx].lines = sum_header(token->data[data_idx].header);
token->data[data_idx+1].lines = sum_header(token->data[data_idx+1].header);

/* RES 01/07/2002 split data data color so we can calculate the token color bits correctly */
split_color(token->data[data_idx].color_bits,
            token->data[data_idx+1].color_bits,
            HEADER_SIZE,
            split_point);

/***************************************************************
* RES 02/07/2003 Add shift right call to reposition color bits after the block split .
*  
* First: Shift the color bits out assuming the color goes with the new header (will be shifted) 
* Then:  Test the old block (header_idx) and the new block (header_idx+blocks_needed) and set the bits
*        accordingly.
***************************************************************/
shift_right(token->color_bits, DATA_SIZE, data_idx);

if (-1 != prev_bit(token->data[data_idx].color_bits, HEADER_SIZE-1))
   set_color_bit(token->color_bits, data_idx);
else
   clear_color_bit(token->color_bits, data_idx);
if (-1 != prev_bit(token->data[data_idx+1].color_bits, HEADER_SIZE-1))
   set_color_bit(token->color_bits, data_idx+1);
else
   clear_color_bit(token->color_bits, data_idx+1);

return(0);

} /* split_header */

/************************************************************************

NAME:    split_block 

************************************************************************/

static header_struct   *split_block(DATA_TOKEN        *token,
                                    int                absolute_split_line,
                                    int                xtra_block)
{

int            i, j;
int            blocks_needed;
block_struct  *new_block;
int            data_idx;
int            header_idx;
int            last_header;
int            new_block_lines;
int            block_idx;    /* not used */
int            enough_room = 0;
int            done_split = 0;
int            first_block_to_move;
char           msg[256];


header_struct *header_ptr;
block_struct  *block_ptr;


DEBUG3( fprintf(stderr, " @split_block(%d,%d)\n", absolute_split_line, xtra_block);)
DEBUG0( if (!token || (token->marker != TOKEN_MARKER)) {fprintf(stderr, "Bad token passed to split_block\n"); kill(getpid(), SIGABRT);})

if (xtra_block)
   blocks_needed = 2;
else
   blocks_needed = 1;

token->current_block_idx = -1;

 /*
  *  See if there is room for the required number of new blocks.
  *  If not, do a header split and try again.  There should be
  *  enough room then.
  */

while(!enough_room){
   hh_idx(token, &data_idx, &header_idx, &block_idx, absolute_split_line); 

   header_ptr = token->data[data_idx].header;
   last_header = header_idx;      /* find the last header */
   while ((last_header < HEADER_SIZE) && (header_ptr[last_header].block != NULL) && (last_header < HEADER_SIZE)) 
       last_header++;
   if (last_header + blocks_needed > HEADER_SIZE){
        if (!done_split){
              if (split_header(token, data_idx))
                  return(NULL); /* could not split */
              done_split = 1;
        }else{
              DEBUG3( fprintf(stderr, "split_block:  header split failed to produce more room (Internal Error)!\n");)
              return((header_struct *) 0);
        }
   }else
        enough_room = 1;
}

block_ptr = header_ptr[header_idx].block;
DEBUG3( fprintf(stderr, " split_block at [D:%d, H:%d, B:%d] \n", data_idx, header_idx, block_idx);)

 /*
  *  Get the new line block
  */

new_block = (block_struct *) CE_MALLOC(LINES_PER_BLOCK * sizeof(block_struct));
                      /* get a new block */
if (!new_block){
  /*    dm_error("Out of Memory! (Memdata/SB)", DM_ERROR_LOG);  */
      return((header_struct *) 0);
}

 /*
  *  Make sure we are splitting at a valid place
  */

if (block_idx < header_ptr[header_idx].lines && block_idx >= 0)
   {
      /*
       *  Copy everything from <block_idx> forward into the new block .
       */

      for (i = block_idx+1, j = 0;
           i < header_ptr[header_idx].lines;
           i++, j++)
      {
          new_block[j].text = block_ptr[i].text; 
          new_block[j].size = block_ptr[i].size; 
          new_block[j].color_data = block_ptr[i].color_data; 
      }

      new_block_lines = j;
      new_block[j+1].text = NULL;     /* terminate the blocks */
      new_block[j+1].color_data = NULL; 
      block_ptr[block_idx+1].text = NULL;
      block_ptr[block_idx+1].color_data = NULL;
      header_ptr[header_idx].lines = block_idx+1;   /* set new size in old block */
   }
else
   {
      snprintf(msg, sizeof(msg), "split_block:  Attempted split at invalid place %d, (%d lines in block)", block_idx, header_ptr[header_idx].lines);
      dm_error(msg, DM_ERROR_LOG);
      return((header_struct *) 0);
   }

 /*
  *  Shuffle the header pointers forward to get to make room.
  *  Move the NULL header pointer unless we are at the last header.
  */

if (last_header < HEADER_SIZE-1)
  first_block_to_move = last_header;
else
  first_block_to_move = last_header - 1;

for (i = first_block_to_move, j = i + blocks_needed;
     i > header_idx;
     i--, j--)
{
    header_ptr[j] = header_ptr[i];
}

/* clear the one or two new spaces */
while(j > header_idx){
   clear_color_block(header_ptr[j].color_bits);
   header_ptr[j--].block = NULL;
}

 /*
  *  Put the new line block in place in the header.
  */

header_ptr[header_idx+blocks_needed].lines = new_block_lines;
header_ptr[header_idx+blocks_needed].block = new_block;
split_color(header_ptr[header_idx].color_bits,
            header_ptr[header_idx+blocks_needed].color_bits,
            LINES_PER_BLOCK,
            block_idx+1);

/***************************************************************
* RES 02/07/2003 Add shift right call to reposition color bits in the header after the block split .
*  
* First: Shift the color bits out assuming the color goes with the new block (will be shifted) 
* Then:  Test the old block (header_idx) and the new block (header_idx+blocks_needed) and set the bits
*        accordingly.
***************************************************************/
shift_right(token->data[data_idx].color_bits, HEADER_SIZE, header_idx);
if (blocks_needed > 1) /* must be 2 */
   shift_right(token->data[data_idx].color_bits, HEADER_SIZE, header_idx);

if (-1 != prev_bit(header_ptr[header_idx].color_bits, LINES_PER_BLOCK-1))
   set_color_bit(token->data[data_idx].color_bits, header_idx);
else
   clear_color_bit(token->data[data_idx].color_bits, header_idx);
if (-1 != prev_bit(header_ptr[header_idx+blocks_needed].color_bits, LINES_PER_BLOCK-1))
   set_color_bit(token->data[data_idx].color_bits, header_idx+blocks_needed);
else
   clear_color_bit(token->data[data_idx].color_bits, header_idx+blocks_needed);

 /*
  *  Return either 1 or the address of the empty header
  *  to let the caller know of our success
  */


if (blocks_needed == 1)
   return((header_struct *)1);
else
   return(&header_ptr[header_idx+1]);


} /* split_block */

/************************************************************************

NAME:    split_line - split a line at a given point (the file may grow by
                      one line in length) if new_line then grow.  
                                          1=split, 0=truncate 

************************************************************************/

void split_line(DATA_TOKEN *token,
                int         line,
                int         column,
                int         new_line)
{
int len;
int offset;  /* dummy */
char *text;
char buf[MAX_LINE+2];
char *color;
char head_color[MAX_LINE+1], tail_color[MAX_LINE+1];

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)) {fprintf(stderr, "Bad token passed to split_line\n"); kill(getpid(), SIGABRT);})
DEBUG3( fprintf(stderr, " split_line at [line:%d, column:%d, flag:%d] \n", line, column, new_line);)

if ((line > total_lines(token)) || (line < 0)){
      dm_error("No such line in file.", DM_ERROR_BEEP);
      return;
}

if (column < 0)
    column = 0;

text = get_line_by_num(token, line);
color = get_color_by_num(token, line, COLOR_CURRENT, &offset);

DEBUG3( fprintf (stderr, "Spliting line[%d]:'%s'\n", line, text);)

len =  strlen(text);
cdgc_split(color, column, 0, len, CDGC_SPLIT_BREAK, head_color, tail_color);

if (new_line)
    if (len <= column){
       put_line_by_num(token, line, "", INSERT);
    }else{
       put_line_by_num(token, line, text + column, INSERT);
       put_color_by_num(token, line+1, tail_color);
    }

if (len >= column){
    /* if (!undo_semafor) event_do(token, SL_EVENT, line, column, new_line, text + column);  deleted 2/15/93 */
     strncpy(buf, text, column);
     *(buf + column) = '\0';
     /* *(text + column) = NULL; old */
     put_line_by_num(token, line, buf, OVERWRITE);  /* added 2/15/93 */
     put_color_by_num(token, line, head_color);
}else{
   strcpy(buf, text);
   while (len < column) buf[len++] = ' ';
   buf[len] = '\0';
   put_line_by_num(token, line, buf, OVERWRITE);
   put_color_by_num(token, line, head_color);
}    

if (new_line && (line <= token->last_line_no))  /* invalidate */
   token->last_line_no = -1;

} /* split_line  */

/************************************************************************

NAME:    sum_header - total the number of lines in a data[?].header 

************************************************************************/

static int sum_header(header_struct *header)
{

int i;
int sum = 0;

for (i= 0; (i < HEADER_SIZE) && header[i].block; i++)
     sum +=  header[i].lines;

return(sum);

}  /* sum_header */

/************************************************************************

NAME:    sum_size - total the malloced size of a range of code 

************************************************************************/

int sum_size(DATA_TOKEN *token, int from_line, int to_line)
{

int sum = 0;

int data_idx, header_idx, block_idx;

header_struct *header_ptr;
block_struct *block_ptr;

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)){ fprintf(stderr, "Bad token passed to sum_size"); kill(getpid(), SIGABRT);})
DEBUG3( fprintf (stderr, " @Sum_size(%d-%d)\n", from_line, to_line);)

if (to_line > total_lines(token))
      to_line = total_lines(token);

hh_idx(token, &data_idx, &header_idx, &block_idx, from_line);

while (token->data[data_idx].header != NULL){
    header_ptr = token->data[data_idx].header;
    DEBUG3(fprintf(stderr, " Header [%d] lines = %d\n", data_idx,  token->data[data_idx].lines);)
    while ((header_idx < HEADER_SIZE) && (header_ptr[header_idx].block != NULL)){
        block_ptr = header_ptr[header_idx].block;
        DEBUG3(fprintf(stderr, " Block [%d,%d] lines = %d\n", data_idx, header_idx, header_ptr[header_idx].lines);)
        while (block_idx < header_ptr[header_idx].lines){
            if (from_line++ > to_line)
                return(sum); 
            sum +=  block_ptr[block_idx].size;
            DEBUG3( fprintf (stderr, " Line %d = %d [sum(%d)]\n", from_line-1, block_ptr[block_idx].size, sum);)
            block_idx++;
        }
        header_idx++;
        block_idx=0;
    }
    data_idx++;
    header_idx=0;
}

return(sum);

}  /* sum_size */

#ifdef Encrypt

/****************************************************************************
*
*	encrypt_init - Initialize the rotor arrarys for the encryption
*
****************************************************************************/

#define ROTORSZ 256
#define MASK 0377

char	t1[ROTORSZ];
char	t2[ROTORSZ];
char	t3[ROTORSZ];

char	ptr[] = "%2.2d";

void encrypt_init(pw)
char *pw;
{
	int ic, i, k, temp;
	unsigned random;
   char   buf[13];
	long seed;

	strncpy(buf, pw, 8);

	while (*pw)
		*pw++ = '\0';

	seed = 666;
	for (i=0; i<13; i++)
		seed = seed*buf[i] + i;

	for(i=0;i<ROTORSZ;i++) {
		t1[i] = i;
		t3[i] = 0;
	}

	for(i=0;i<ROTORSZ;i++) {
		seed = 5*seed + buf[i%13];
		random = seed % 65521;
		k = ROTORSZ-1 - i;
		ic = (random&MASK)%(k+1);
		random >>= 8;
		temp = t1[k];
		t1[k] = t1[ic];
		t1[ic] = temp;
		if(t3[k]!=0) continue;
		ic = (random&MASK) % k;
		while(t3[ic]!=0) ic = (ic+1) % k;
		t3[k] = ic;
		t3[ic] = k;
	}

	for(i=0;i<ROTORSZ;i++)
		t2[t1[i]&MASK] = i;

}  /* encrypt_init */

/****************************************************************************
*
*	encrypt_line -  A one-rotor machine designed along the lines of Enigma
*	but considerably trivialized.
*
****************************************************************************/

/* static int n1 = 0, n2 = 0 */

void encrypt_line(char *ptr)
{
	int n1 = 0, n2 = 0;
   
#ifdef DebuG
char *dptr = ptr;
#endif

DEBUG3( fprintf(stderr, " @Encrypt('%s'[%d]->", ptr, ptr);)

	while(*ptr) {
		*ptr = t2[(t3[(t1[(*ptr+n1)&MASK]+n2)&MASK]-n2)&MASK]-n1;
		n1++;
		if(n1==ROTORSZ) {
			n1 = 0;
			n2++;
			if(n2==ROTORSZ) n2 = 0;
		}
       ptr++;
	}

DEBUG3( fprintf(stderr, "'%s'[%d]\n", dptr, ptr);)

}   /* encrypt_line */

#endif

/************************************************************************

NAME:      join_line  - Join two adjacent lines

PURPOSE:    This routine deletes the data between two points in the memory map
            line numbers and columns are taken into account.

PARAMETERS:

   1.  token      -  pointer to DATA_TOKEN (INPUT / OUTPUT)
                     This is the token for the memdata structure being operated on.

   2.  line_no    -  int  (INPUT)
                     This is the line number of first line in the set of two lines
                     to be joined.  That is, line_no + 1 is appended onto the end of
                     this line.


FUNCTIONS :

   1.   Position the file to read the first line and get it.  If it does not
        exist, quit - there is nothing to do.

   2.   Read sequentially to get the next line.  If it does not exist (eof), 
        quit - there is nothing to do.

   3.   Attach the first and second lines in a work buffer and check for overflow.
        Truncate on overflow with a message.

   4.   Replace the first line with the composite line.

   5.   Delete the second line.


*************************************************************************/

void    join_line(DATA_TOKEN      *token,
                  int              line_no)
{               
char   *first_line;
char   *second_line;
char    buff[(MAX_LINE * 2)+1];

if ((line_no+1) >= total_lines(token))
   return;

if (cc_ce)
   cc_joinln(token, line_no);

first_line  = get_line_by_num(token, line_no);
if (first_line == NULL)
   return;

second_line  = get_line_by_num(token, line_no+1);
if (second_line == NULL)
   return;

strcpy(buff, first_line);
strcat(buff, second_line);

if ((int)strlen(buff) > MAX_LINE)
   {
      dm_error("Line too long", DM_ERROR_BEEP);
      buff[MAX_LINE] = '\0';
   }

put_line_by_num(token, line_no, buff, OVERWRITE);
delete_line_by_num(token, line_no + 1, 1);

}  /* end of join_line */  

#ifdef DebuG
/************************************************************************

NAME:      print_file - walk the data structure and print each line out.

************************************************************************/

void print_file(DATA_TOKEN *token)       /* input  */
{
int i=0, j, k;
int line_count = 0;
/*char *bptr;*/

header_struct *header_ptr;
block_struct *block_ptr;

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)){ fprintf(stderr, "Bad token passed to print_file"); kill(getpid(), SIGABRT);})

while (token->data[i].header != NULL)
{

    j=0;
    fprintf(stderr, "DATA[%d](%d,&0x%x)\n", i, token->data[i].lines, token->data[i].header);
    header_ptr = token->data[i].header;

    while ((j < HEADER_SIZE) && (header_ptr[j].block != NULL)){

        k=0;
        fprintf(stderr, "\tHEADER[%d](%d,&0x%x)\n", j, header_ptr[j].lines,header_ptr[j].block);
        block_ptr = header_ptr[j].block;

        while (k < header_ptr[j].lines){
            fprintf(stderr,"\t\tLine[%d](%2.2d):",line_count, block_ptr[k].size);
            fprintf(stderr,"! %s\n", block_ptr[k].text);
            /*bptr =  block_ptr[k].text;*/
            if (++line_count >= total_lines(token)){
                 return; 
            }
            k++;
        }
        j++;
    }
    i++;
}

} /* print_file */


/************************************************************************

NAME:      print_color_bits - Dump the color bits information.

************************************************************************/

void print_color_bits(DATA_TOKEN *token)       /* input  */
{
int i=0, j, k;
int line_count = 0;
int any_in_block, any_in_header;

header_struct *header_ptr;
block_struct *block_ptr;

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)){ fprintf(stderr, "Bad token passed to print_color"); kill(getpid(), SIGABRT);})

while (token->data[i].header != NULL)
{

    j=0;
    if ((i % WORD_BIT) == 0)
       fprintf(stderr, "data color bits[%d] = 0X%08X   first line = %d\n", i/WORD_BIT, token->color_bits[i/WORD_BIT], line_count);
    else
       fprintf(stderr, "\n");
    header_ptr = token->data[i].header;

    while ((j < HEADER_SIZE) && (header_ptr[j].block != NULL)){

        k=0;
        block_ptr = header_ptr[j].block;
        if ((j % WORD_BIT) == 0)
           fprintf(stderr, "    header color bits[%d] = 0X%08X   first line = %d\n", j/WORD_BIT, token->data[i].color_bits[j/WORD_BIT], line_count);
       else
          fprintf(stderr, "\n");

        while (k < header_ptr[j].lines){
           if ((k % WORD_BIT) == 0)
              fprintf(stderr, "        block color bits[%d] = 0X%08X   first line = %d\n", k/WORD_BIT, header_ptr[j].color_bits[k/WORD_BIT], line_count);
            if (++line_count >= total_lines(token)){
                 break; 
            }
            k++;
        }
        j++;
    }
    i++;
}

print_color(token);

} /* print_color_bits */

/************************************************************************

NAME:      print_color - walk the data structure and print each color line.

************************************************************************/

void print_color(DATA_TOKEN *token)       /* input  */
{
int i=0, j, k;
int line_count = 0;
int any_in_block, any_in_header/*, any_in_data*/;
/*char *bptr;*/

header_struct *header_ptr;
block_struct *block_ptr;

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)){ fprintf(stderr, "Bad token passed to print_color"); kill(getpid(), SIGABRT);})

/*any_in_data = False;*/
while (token->data[i].header != NULL)
{

    j=0;
    fprintf(stderr, "DATA[%d](%d,&0x%x)\n", i, token->data[i].lines, token->data[i].header);
    header_ptr = token->data[i].header;
    any_in_header = False;

    while ((j < HEADER_SIZE) && (header_ptr[j].block != NULL)){

        k=0;
        /* fprintf(stderr, "\tHEADER[%d](%d,&0x%x)\n", j, header_ptr[j].lines,header_ptr[j].block); */
        block_ptr = header_ptr[j].block;
        any_in_block = False;

        while (k < header_ptr[j].lines){
            if (block_ptr[k].color_data)
               {
                  fprintf(stderr,"%6d:\"%s\"\n", line_count, block_ptr[k].color_data);
                  any_in_block = True;
                  if (prev_bit(header_ptr[j].color_bits, k) != k)
                     fprintf(stderr,"ERROR: Line %6d, block level bit is NOT ON when it should be (%d,%d,%d)\n", line_count, i, j, k);
               }
            else
               if (prev_bit(header_ptr[j].color_bits, k) == k)
                  fprintf(stderr,"ERROR: Line %6d, block level bit is on when it should NOT be (%d,%d,%d)\n", line_count, i, j, k);
            if (++line_count >= total_lines(token)){
                 break; 
            }
            k++;
        }
        if (any_in_block)
           {
              any_in_header = True;
              if (prev_bit(token->data[i].color_bits, j) != j)
                 fprintf(stderr,"ERROR: Line %6d, header level bit is NOT ON when it should be (%d, should be %d, prevbit(%d))\n", line_count, i, j, prev_bit(token->data[i].color_bits, j));
           }
        else
           {
              if (prev_bit(token->data[i].color_bits, j) == j)
                 fprintf(stderr,"ERROR: Line %6d, header level bit is on when it should NOT be (%d, should be %d, prevbit(%d))\n", line_count, i, j, prev_bit(token->data[i].color_bits, j));
           }
        j++;
    }
    if (any_in_header)
       {
          /*any_in_data = True;*/
          if (prev_bit(token->color_bits, i) != i)
             fprintf(stderr,"ERROR: Line %6d, data level bit is NOT ON when it should be (should be %d, prevbit(%d))\n", line_count, i, prev_bit(token->color_bits, i));
       }
    else
       {
          if (prev_bit(token->color_bits, i) == i)
             fprintf(stderr,"ERROR: Line %6d, data level bit is on when it should NOT be (should be %d, prevbit(%d))\n", line_count, i, prev_bit(token->color_bits, i));
       }
    i++;
}

} /* print_color */


/************************************************************************

NAME:    dump_ds - same as save_file, but no printing

************************************************************************/

void dump_ds(DATA_TOKEN *token)       /* opaque */
{
int i=0, j;
int total = 0;
int header_total;

header_struct *header_ptr;

DEBUG0( if (token->marker != TOKEN_MARKER){ fprintf(stderr, "Bad token passed to dump_ds\n"); kill(getpid(), SIGABRT);})

fprintf(stderr, "TOKEN          = 0x%08X\n",   token);


while (token->data[i].header != NULL)
{

    fprintf(stderr, "DATA[%2d] CNT = %d\n",   i, token->data[i].lines);
    fprintf(stderr, "DATA[%2d] ptr = 0x%X\n", i, token->data[i].header);
    j=0;
    header_total = 0;
    header_ptr = token->data[i].header;

    while ((j < HEADER_SIZE) && (header_ptr[j].block != NULL))
    {

        fprintf(stderr, "               |  HEAD[%3d] CNT = %d,  PTR = 0x%X\n",   j, header_ptr[j].lines, header_ptr[j].block);
        total += header_ptr[j].lines;
        header_total += header_ptr[j].lines;
        j++;
   }
   fprintf(stderr, " Header TOTAL: %d\n", header_total);
   i++;
}

fprintf(stderr, " TOTAL: %d\n", total);

} /* dump_ds */

#endif

/************************************************************************

NAME:    wrap_input - wrap input to put_block_by_num

************************************************************************/

void wrap_input(DATA_TOKEN *token, int *line_no, int *len) 
{

while (*len > MAX_LINE){
  split_line(token, *line_no, MAX_LINE, 1);
  *line_no += 1;
  *len -= MAX_LINE;
}

} /* wrap_input() */

/************************************************************************

NAME:    get_line - get a line of data from the input fd/fp
                    function just like fgets();

************************************************************************/

#ifdef MD_NULL

void get_line(char *buff, int size, int fd, int *len) 
{

read();


} /* wrap_input() */

#endif

/************************************************************************

NAME:      vt100_eat  -   Eat vt100 control sequences when in cv -man mode

PURPOSE:    This routine handles the data conversion in cv -man mode.  As
            data is loaded vt100 escape sequences are filtered out.
PARAMETERS:

   1.  target     -  pointer to char (OUTPUT)
                     This is where the processed data goes.

   2.  line       -  pointer to char (INPUT)
                     This is the line to be processed.

FUNCTIONS :

   1.   Look at each character an process it as a simple character or as a command sequence.

NOTES:
   1)   Target and line may be equal!
   2)   target=NULL means do the processing in place

*************************************************************************/

/*
 *   VT100 tokens
 */

#define VT_BL    007
#define VT_BS    010
#define VT_CN    030
#define VT_CR    015
#define VT_FF    014
#define VT_HT    011
#define VT_LF    012
#define VT_SI    017
#define VT_SO    016
#define VT_SP    040
#define VT_VT    013
#define VT__    0137

#define VT_ESC   033


void vt100_eat(char        *target, char        *line)
{
char          *cmd_trailer;

if (!target) target = line; /* make safe */

while(*line){
   switch(*line){
    case VT_BL :   /* Bell Character */
               line++;
               break;

    case VT_BS :   /* Backspace Character */
               target--;
               line++;
               break;

    case VT_CN :   /* Cancel Character */
               line++;
               break;

    case VT_CR :   /* Carriage Return Character */
               line++;
               break;

    case VT_LF:   /* Linefeed Character */
               *target++ = *line++;
               break;

    case VT_SI :   /* Shift In Character, Invoke the current G0 character set. */
               line++;
               break;

    case VT_SO :   /* Shift Out Character, Invoke the current G1 character set.  */
               line++;
               break;

    case VT_VT :   /* Vertical Tab Character */
               line++;
               break;

    case VT_ESC :
               line++;
               switch(*line)
               {

               case 'A' :   /* VT52 Cursor Up   */
               case 'B' :   /* VT52 Cursor Down */
               case 'b' :   /* Enable Manual Input */
               case 'C' :   /* VT52 Cursor Right */
               case 'c' : /* Reset to Initial State */
               case 'D' :   /* Index  or VT52 Cursor Left */
               case 'E' :   /* Next Line */
               case 'F' :   /* VT52 Enter Graphics Mode */
               case 'G' :   /* VT52 Exit Graphics Mode */
               case 'H' :   /* Horizontal Tab Set or  VT52 Cursor to Home*/
               case 'I' :   /* VT52 Reverse Linefeed */
               case 'J' :   /* VT52 Erase to End of Screen */
               case 'K' :   /* VT52  Erase to End of Line */
               case 'M' :   /* Reverse Index */
               case 'Z' :   /* Identify Terminal - Same as ansi device attributes or VT52 Identify */
               case '7' :   /* Save Cursor   */
               case '8' :   /* Restore Cursor */
               case '9' :   /* Restore Cursor */
               case '=' :   /* Keypad Application Mode or VT52 Alternate Keypad Mode */
               case '>' :   /* Keypad Numeric Mode or VT52 Exit Alternate Keypad Mode */
               case '<' :   /* VT52 Enter Ansi Mode */
               case '\'' :   /* Disable Manual Input */
                        line++;
                        break;

               case 'Y' : /* VT52 Direct Cursor Address */
                        line += 3;
                        break;
                         
               case '%' : /* SELECT CODE */
               case '(' :   /* Select G0 character set */
               case ')' :   /* Select G1 character set */
                        line += 2;
                        break;

               case '#' :   /* Report Syntax Mode */
                        line++;
                        if (*line == '!')
                            line++;
                        line++;
                        break;

               case '[' :
                        line++;
                        cmd_trailer = strpbrk(line, " ABCcDfgHhIJKLlMmnPRrSTXZ@");
                        if (cmd_trailer == NULL)
                           {
                              DEBUG23(fprintf(stderr, "Incomplete vt100 command sequence\n");)
                              line = &line[strlen(line)];  /* kill rest of command */
                              break;
                           }

                        line = cmd_trailer + 1;
                        break;  /* end of case [ */
              default:
                  break;
              } /* switch in case VT_ESC */
              break;

    case VT_FF :   /* Formfeed Character */
    case VT_HT :   /* Horizontal Tab Character */
     default:
              *target++ = *line++;
              break;
    }  /* switch(input)  *line  */
} /* while input */

*target = '\0';

}  /*  vt100_eat() */

/************************************************************************

NAME:      get_color_by_num  -   Return the color data entry that refers to 
                                 the desired line.

PARAMETERS:

   1.  line_no     -  Line on or next above to get color data from

   2.  direction   - 1 = search UP include current line. 
                     0 = this line only
                    -1 = search down starting at passed line +1

   3. color_line_no - The actual line where the data was found.
                      Zero if no color data is found.

   Returned Value:  Empty string on not found.
                    Otherwise pointer to the color data.

*************************************************************************/

char *get_color_by_num(DATA_TOKEN *token,           /* opaque */
                       int         line_no,         /* input  */
                       int         direction,       /* direction to search for color info */
                       int        *color_line_no){  /* output */

int  i, bloc, dloc, hloc;
int  data_idx;
int  header_idx;
int  block_idx;

block_struct      *block_ptr;
header_struct     *header_ptr;

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)){ fprintf(stderr, "Bad token passed to get_color_by_num\n"); kill(getpid(), SIGABRT);})
DEBUG3( fprintf(stderr, " @get_color_by_number(token=0x%x,line=%d)\n", token, line_no);)

*color_line_no = 0;

if (!token->colored) return(""); /* memdata optimization (never been colored) */

if ((line_no >= total_lines(token)) || (line_no < 0))
     return("");

/*
 *  Find the current line line
 */

hh_idx(token, &data_idx, &header_idx, &block_idx, line_no);

header_ptr = token->data[data_idx].header;
if (!header_ptr) return("");    /* is this right when direction is specified ? */
block_ptr = header_ptr[header_idx].block;

if ((direction == COLOR_CURRENT) || (direction == COLOR_UP)){
   if (block_ptr[block_idx].color_data){
         *color_line_no = line_no;
         return(block_ptr[block_idx].color_data);
   }else
       if (direction == COLOR_CURRENT)
          return("");
}

if (direction == COLOR_UP){

    /*
     * Search up for  color data
     */
    
    /* check previous lines in the current block first */
    bloc = prev_bit(header_ptr[header_idx].color_bits, block_idx);
    if (bloc != -1){    /* we have color data in the same block */
        hloc = header_idx;
        dloc = data_idx;
    }else{     /* look in previous blocks in the current header */
        hloc = prev_bit(token->data[data_idx].color_bits, header_idx-1);
        if (hloc != -1){    /* we have color data in the same header */
            bloc = prev_bit(header_ptr[hloc].color_bits, LINES_PER_BLOCK);
            dloc = data_idx;
        }else{     /* look in previous headers */
            dloc = prev_bit(token->color_bits, data_idx-1);
            if (dloc == -1) return("");  
            header_ptr = token->data[dloc].header;
            hloc = prev_bit(token->data[dloc].color_bits, HEADER_SIZE);
            bloc = prev_bit(header_ptr[hloc].color_bits, LINES_PER_BLOCK);
        }
    }
    
}else{   /* direction = -1 DOWN */
    
    /*
     * Search down for color data
     */
    
    /* check later lines in the current block first */
    bloc = next_bit(header_ptr[header_idx].color_bits, LINES_PER_BLOCK, block_idx);
    if (bloc != -1){    /* we have color data in the same block */
        hloc = header_idx;
        dloc = data_idx;
    }else{     /* look in later blocks in the same header */
        hloc = next_bit(token->data[data_idx].color_bits, HEADER_SIZE, header_idx);
        if (hloc != -1){    /* we have color data in the same header */
            bloc = next_bit(header_ptr[hloc].color_bits, LINES_PER_BLOCK, -1);
            dloc = data_idx;
        }else{     /* look in later headers */
            dloc = next_bit(token->color_bits, DATA_SIZE, data_idx);
            if (dloc == -1) return("");  
            header_ptr = token->data[dloc].header;
            hloc = next_bit(token->data[dloc].color_bits, HEADER_SIZE, -1);
            bloc = next_bit(header_ptr[hloc].color_bits, LINES_PER_BLOCK, -1);
        }
    }
    
} /* ? direction */

/* token->last_line    = block_ptr[block_idx].text; optomization of future (RES 5/29/97 - probably don't want to do this, )*/

/*
 *  Didn't find anything, return a zero length string
 */
if (bloc < 0) /* added 5/29/97 RES, fixed memfault problem after block split */
    return("");

if (header_ptr[hloc].block == block_ptr) /* same block so we can optimize the calc */
     *color_line_no =  line_no + (bloc - block_idx);
else{
    for (i=0; i<(dloc); i++)
       *color_line_no += token->data[i].lines; 
    
    for (i=0; i<(hloc); i++)
       *color_line_no += token->data[dloc].header[i].lines; 
    
    *color_line_no += bloc;
    block_ptr = header_ptr[hloc].block;
    
}

DEBUG3( fprintf(stderr, " get_color(%s[line=%d])\n", block_ptr[bloc].color_data, *color_line_no);)
    
return(block_ptr[bloc].color_data);
    
} /* get_color_by_num() */
    
/************************************************************************

NAME:      put_color_by_num  -   Add/replace color data for a line 

PARAMETERS:

   1.  line_no     -  Line to add/modify color data


   3. color_data   -  Color data to addto line, can be NULL

*************************************************************************/

int put_color_by_num(DATA_TOKEN *token,           /* opaque */
                 int             line_no,         /* input  */
                 char           *color_data){


int  data_idx;
int  header_idx;
int  block_idx;

block_struct      *block_ptr;
header_struct     *header_ptr;
static char zeros[MAX_COLOR_BIT_BYTES];

DEBUG0( if (!token || (token->marker != TOKEN_MARKER)){ fprintf(stderr, "Bad token passed to put_color_by_num\n"); kill(getpid(), SIGABRT);})
DEBUG3( fprintf(stderr, " @put_color_by_num(token=0x%x,line=%d)\n", token, line_no);)


/* took off the if since, sparc's don't like branches. RES 4/15/96 */
COLORED(token) = True;

if ((line_no >= total_lines(token)) || (line_no < 0))
     return(-1); 

/*
 *  Find the current line line
 */

hh_idx(token, &data_idx, &header_idx, &block_idx, line_no);

header_ptr = token->data[data_idx].header;
block_ptr = header_ptr[header_idx].block;

if (!undo_semafor) event_do(token, PC_EVENT, line_no, 0, 0, block_ptr[block_idx].color_data);

if (block_ptr[block_idx].color_data)
    free(block_ptr[block_idx].color_data); /* don't NULL out pointer yet */

if (!color_data || !*color_data){  /* clear the color data */
    block_ptr[block_idx].color_data = NULL;
    clear_color_bit(header_ptr[header_idx].color_bits, block_idx);

    if (memcmp((char *)zeros, (char *)header_ptr[header_idx].color_bits, LINES_PER_BLOCK/CHAR_BIT) == 0){
       clear_color_bit(token->data[data_idx].color_bits, header_idx);

       if (memcmp((char *)zeros, (char *)token->data[data_idx].color_bits, HEADER_SIZE/CHAR_BIT) == 0){
          clear_color_bit(token->color_bits, data_idx);

          if (memcmp((char *)zeros, (char *)token->color_bits, DATA_SIZE/CHAR_BIT) == 0)
             COLORED(token) = False;
       }
    }
}else{
    if (block_ptr[block_idx].color_data == NULL){ /* was no color data here before */
       set_color_bit(header_ptr[header_idx].color_bits, block_idx);
       set_color_bit(token->data[data_idx].color_bits, header_idx);
       set_color_bit(token->color_bits, data_idx);
    }
    block_ptr[block_idx].color_data = malloc_copy(color_data);
}

return(0);

} /* put_color_by_num() */

#include "masktbl.h"

/****************************************************************

Name: prev_bit - In an array of int find the location of the right
                 most bit left(inclusive) of the location specified 
                 by bit.
Returns:   -1 on failure to find anything
           returns the index of the bit that was on.

*****************************************************************/

static int prev_bit(uint32_t set[], int bit){

int indx, offset, mof;
uint32_t masked;

/* handle searching off the top of memdata */
if (bit < 0) return(-1);

indx = bit / WORD_BIT;
offset = bit % WORD_BIT;

#ifdef blah
DEBUG3( 
         fprintf(stderr, " @prev_bit(bit=",bit);
         for (i=0; i <= indx; i++)
            fprintf(stderr, "set[%d]=%X)", i, set[i]);
         fprintf(stderr, "\n");
)
#endif

#ifdef blah
if (set[indx] & ((uint32_t)1 << ((WORD_BIT-1)-offset))) return(bit); /* bit its self is set (inclusive) */

if (offset == 0)
   masked = 0; /* shift of WORD_BIT is a no-op */
else
#endif
masked = ((uint32_t)~0 << ((WORD_BIT-1)-offset)) & set[indx]; /* mask of any right bits not needed */

if (!masked){
  while((--indx >= 0) && !set[indx]) /* NOP */;
  if (indx < 0) return(-1); /* no color info found above this point */
  masked = set[indx];
}

DEBUG3(fprintf(stderr, "bit=%d,offset=%d,mask=%X,set[%d]=%08X\n", bit, offset, masked, indx, set[indx]);)

/* we have the right int, lets get the right bit */

mof = 0;
#if WORD_BIT == 64
mof = last_mask_table[mof].next[!(!(last_mask_table[mof].mask & masked))];
#endif
mof = last_mask_table[mof].next[!(!(last_mask_table[mof].mask & masked))];
mof = last_mask_table[mof].next[!(!(last_mask_table[mof].mask & masked))];
mof = last_mask_table[mof].next[!(!(last_mask_table[mof].mask & masked))];
mof = last_mask_table[mof].next[!(!(last_mask_table[mof].mask & masked))];
mof = last_mask_table[mof].next[!(!(last_mask_table[mof].mask & masked))];

DEBUG3(fprintf(stdout, "last bit in word = %d, 0x%08X, last bit %d\n", mof, (1 << ((WORD_BIT-1)-mof)), indx*WORD_BIT + mof);)

return(indx*WORD_BIT + mof);

} /* prev_bit() */

/****************************************************************

Name: next_bit - find the next bit to the right of the input
                 (bit) location in the integer array (set).
Returns:   -1 on failure to find anything
           returns the index of the bit that was on.

****************************************************************/
static int next_bit(uint32_t set[], int size, int bit){

int indx, offset, mof, words;
uint32_t  masked;


bit++; /* start searching in bit after one passed */
if (bit >= size) return(-1);

indx = bit / WORD_BIT;
offset = bit % WORD_BIT;
words = size/WORD_BIT;

masked = ((uint32_t)~0 >> offset) & set[indx]; /* mask off any left bits not needed */

if (!masked){
  while( !set[++indx] && (indx < words)) /* NOP */;
  if (indx == words) return(-1); /* no color info found below this point */
  masked = set[indx];
}

/* we have the right int, lets get the right char */

mof = 0;
#if WORD_BIT == 64
mof = first_mask_table[mof].next[!(!(first_mask_table[mof].mask & masked))];
#endif
mof = first_mask_table[mof].next[!(!(first_mask_table[mof].mask & masked))];
DEBUG3(fprintf(stdout, "mofs = {%d, ", mof);)

mof = first_mask_table[mof].next[!(!(first_mask_table[mof].mask & masked))];
DEBUG3(fprintf(stdout, "%d, ", mof);)

mof = first_mask_table[mof].next[!(!(first_mask_table[mof].mask & masked))];
DEBUG3(fprintf(stdout, "%d, ", mof);)

mof = first_mask_table[mof].next[!(!(first_mask_table[mof].mask & masked))];
DEBUG3(fprintf(stdout, "%d, ", mof);)

mof = first_mask_table[mof].next[!(!(first_mask_table[mof].mask & masked))];
DEBUG3(fprintf(stdout, "%d}\n", mof);)

DEBUG3(fprintf(stdout, "first bit in word = %d, 0x%08X, first bit %d\n", mof, (1 << ((WORD_BIT-1)-mof)), indx*WORD_BIT + mof);)

if (((indx*WORD_BIT + mof) > 128) && (size == bit) && (size == HEADER_SIZE) )
 size = bit;  /* remove lint error 10/13/97, hope it still works, was == instead of - */

return(indx*WORD_BIT + mof);

} /* next_bit() */

#ifdef DebuG  
static char  *bits(uint32_t val)
{
static char  bc[WORD_BIT+1];
int   i;

for (i = 0; i < WORD_BIT; i++)
   if ((1 << ((WORD_BIT-1)-i)) & val)
      bc[i] = '1';
   else
      bc[i] = '0';

bc[WORD_BIT] = '\0';
return(bc);
}
#endif

/****************************************************************

Name: shift_right - Shift all the bits in a color array one bit
                    right starting at a given location.
                    NOTE: we should never shift off the last
                    bit.  This would have caused a block split.

****************************************************************/
static void shift_right(uint32_t set[], int size, int bit){

int indx, offset, i, words;


if ((bit >=(size-1))  || (bit < 0)) return;



indx = bit / WORD_BIT;
offset = bit % WORD_BIT;
words = size/WORD_BIT;

DEBUG3(
fprintf(stdout, "Right\n");
fprintf(stdout, "indx   %d\n", indx);
fprintf(stdout, "offset %d\n", offset);
fprintf(stdout, "words  %d\n", words);
)


DEBUG(
   if (set[words-1] & 1)
       fprintf(stderr, "Color bit would shift off end of array, 0x08X\n", set[words-1]);
)


/* get all the words up to the one with the bit we are inserting
   before */
for (i = words-1; i > indx; i--)
{
   set[i] = set[i] >> 1;
   if (set[i-1] & 1)
      set[i] |= ((uint32_t)1 << (WORD_BIT-1));
}

/*
   Shift the part past bit to the right by 1, leave the part to the
   left of bit alone.  We do the second shift in 2 parts because a shift
   of WORD_BIT bits, which should result in zero, is a no-op and
   results in a word full of ones. 
*/

DEBUG3(fprintf(stdout, "[%3d]: before %s\n", indx, bits(set[indx]));)

set[indx] = ((((uint32_t)~0 >> offset) & set[indx]) >> 1) |
             ((((uint32_t)~0 << ((WORD_BIT-1)-offset)) << 1) & set[indx]);

DEBUG3(fprintf(stdout, "[%3d]: after  %s\n", indx, bits(set[indx]));)

} /* shift_right() */


/****************************************************************

Name: shift_left  - Shift bits in a color array one bit
                    left starting at a given location.
                    The bit pos passed is the bit removed.

****************************************************************/
static void shift_left(uint32_t set[], int size, int bit){

int indx, offset, i, words;


if ((bit >=size)  || (bit < 0)) return;



indx = bit / WORD_BIT;
offset = bit % WORD_BIT;
words = size/WORD_BIT;

DEBUG3(
fprintf(stdout, "Left\n");
fprintf(stdout, "indx   %d\n", indx);
fprintf(stdout, "offset %d\n", offset);
fprintf(stdout, "words  %d\n", words);
)

/*
   Shift the part past bit to the left by 1, leave the part to the
   left of bit alone.  We do the shift in 2 parts because a shift
   of WORD_BIT bits, which should result in zero, is a no-op and
   results in a word full of ones. 
*/

DEBUG3(
fprintf(stdout, "[%3d]: before %s\n", indx, bits(set[indx]));
)

set[indx] = ((((uint32_t)~0 >> offset) & (set[indx]) << 1)) |
             ((((uint32_t)~0 << ((WORD_BIT-1)-offset)) << 1) & set[indx]);

if ((indx < (words-1)) && (set[indx+1] & ((uint32_t)1 << (WORD_BIT-1))))
   set[indx] |= 1;

DEBUG3(
fprintf(stdout, "[%3d]: after  %s\n", indx, bits(set[indx]));
)

/* shift all the bits past indx one . */
for (i = indx+1; i < words; i++)
{
   set[i] = set[i] << 1;
   if ((i < (words-1)) && (set[i+1] & ((uint32_t)1 << (WORD_BIT-1))))
      set[i] |= 1;
}

} /* shift_left() */


/****************************************************************

Name: split_color - Split a color array to compensate for a 
                    block or headers split.
                    Splits just before bit.

****************************************************************/
static void split_color(uint32_t set[], uint32_t nset[], int size, int bit){

int indx, offset, rem, i, j, words, words_1;


memset((char *)nset, 0, size/CHAR_BIT);

if ((bit >=size)  || (bit < 0)) return;



indx = bit / WORD_BIT;
offset = bit % WORD_BIT;
words = size/WORD_BIT;
words_1 = words - 1;
rem   = WORD_BIT - offset;

DEBUG3(
fprintf(stdout, "Split\n");
fprintf(stdout, "indx   %d\n", indx);
fprintf(stdout, "offset %d\n", offset);
fprintf(stdout, "words  %d\n", words);
)

/* if offset is on a word boundary, just do assignments */
if (offset == 0)
{
	for (i = indx, j = 0; i < words; i++, j++) {
		nset[j] = set[i];
		set[i]  = 0;
	}
} else
{
	for (i = indx, j = 0; i < words; i++, j++) {
		nset[j] = (set[i] << offset) | ((i < words_1) ? (set[i+1] >> rem) : 0);
		if (i != indx)
			set[i] = 0;
		else
			set[i] = set[i] & ((uint32_t)~0 << (WORD_BIT - offset));
	}
}

} /* split_color() */
