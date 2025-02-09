#ifndef _MEMDATA_INCLUDED
#define _MEMDATA_INCLUDED

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
*  Routines in memdata.c
*     mem_init              -  Allocate and initialize a memdata structure and return a pointer to it.
*     load_a_block          -  Read a block of data from a file
*     position_file_pointer -  Position for next_line and prev_line
*     next_line             -  Read sequentially forward
*     prev_line             -  Read sequentially backward
*     dirty                 -  Is the file dirty (save needed)?
*     get_line_by_num       -  Read a line (position independent)
*     delete_line_by_num    -  Delete a line
*     split_line            -  Split a line into two adjacent lines
*     put_line_by_num       -  Replace or insert a line
*     put_block_by_num      -  Insert multiple lines from a file
*     save_file             -  Write the file out.
*     delayed_delete        - Mark a line to be deleted later
*     sum_size              - Sum the malloced sizes of a range of lines
*     encrypt_init          -  Initialize an encryption
*     encrypt_line          -  Encrypt a line of data
*     join_line             -   join one line to the next one after it.
*     mem_kill              - kill a memdata structure.
*     vt100_eat             - Eat vt100 control sequences when in cv -man mode
*     print_color_bits      - Dump the color lines and the color bit patterns
*     print_color           - walk the data structure and print each color line
*     get_color_by_num      - Return forwrd/backward next color info
*     put_color_by_num      - Add/replace color data for a line
*     print_file            - same as save_file, but printing (DEBUGGING)
*     dump_ds               - same as print_file, but structure only (DEBUGGING)
*
***************************************************************/

#include <limits.h>
#ifndef WORD_BIT
#define WORD_BIT 32 /* # of bits in an "int" */
#endif
#if defined(linux) || defined(HAVE_STDINT_H)
#include <stdint.h>         /* /usr/include/stdint.h uint32_t    */
#else
#define uint32_t unsigned int
#endif

/***************************************************************
*  
*  Internal structure declarations for undo.h (Do not use externally!)
*  
***************************************************************/

struct event_struct;  /* forward reference */

struct event_struct
{
short int malloc_size;
short int malloc_old_size;
int line;                    /* line it occured on */
int column;                  /* column it occured on */
int type;                    /* type of database mnodifying event {PL, DL, ...} */
int paste_type;              /* if paste, was it OVERWRITE or INSERT */
char *data;                  /* data being deleted ... */
char *old_data;              /* data being replaced in a OVERWRITE */
struct event_struct *next;   /* linked list pointers */
struct event_struct *last;       /* same */
};

typedef struct event_struct event_struct;

/***************************************************************
*  
*  Internal structure declarations (Do not use externally!)
*  
***************************************************************/

#define  DATA_SIZE           1024    /* these three must be mupltiples of sizeof(int)=32 */
#define  HEADER_SIZE          512
#define  LINES_PER_BLOCK      256

/* must be largest of DATA_SIZE, HEADER_SIZE, LINES_PER_BLOCK */
#define  MAX_COLOR_BIT_BYTES   DATA_SIZE/CHAR_BIT

#define  COLOR_UP      1
#define  COLOR_DOWN   -1
#define  COLOR_CURRENT 0

typedef struct block_struct{
                  char   *text;                 /* pointer to a line of text.                   */
                  char   *color_data;           /* per line color data.                   */
                  int     size;                 /* length of the storage allocated for the line */
} block_struct;

typedef struct header_struct{
                  uint32_t             color_bits[LINES_PER_BLOCK/WORD_BIT]; /* one bit for each block_struct (line) in the array pointed to by the following pointer */
                  struct block_struct *block;   /* pointer to a block struct                  */
                  int   lines;                  /* count of lines in block struct pointed to  */
} header_struct;

typedef struct data_struct{
                  uint32_t              color_bits[HEADER_SIZE/WORD_BIT]; /* one bit for each header_struct in the array pointed to by the following pointer */
                  struct header_struct *header; /* pointer to a header struct                 */
                  int   lines;                  /* count of all the  lines in all the blocks in the header */
} data_struct;

#define TOKEN_MARKER (unsigned long int)0xBEEFFEED

typedef struct DATA_TOKEN
{
   unsigned long int   marker;
                                    /* position variables for next_line and prev_line */
   int                 writable;
   int                 current_line_number;
   block_struct       *current_block_ptr;
   int                 current_block_idx;
   header_struct      *current_header_ptr;
   int                 current_header_idx;
   int                 current_data_idx;
   int                 total_lines_infile;
   int                 last_line_no;
   char               *last_line;
   int                 dirty_file;
   int                 line_load_level;
   event_struct       *event_head; 
   int                 last_hh_block_idx;   /* optimization code */
   int                 last_hh_header_idx;
   int                 last_hh_data_idx;
   int                 last_hh_line;
   int                 seq_insert_strategy; /* RES 01/07/2003, pad mode, split blocks differently */
   int                 colored; /* has this memdata ever been colored on? */
   uint32_t            color_bits[DATA_SIZE/WORD_BIT];   /* one bit for each data_struct in the following array */
   data_struct         data[DATA_SIZE];  /* the body of the header */

} DATA_TOKEN;

#ifdef blah
/* Macros changed to use sizeof 1/10/96  Seems to fix data corruption */
#define clear_color_data(cb)  memset((char *)(cb), 0, DATA_SIZE/sizeof(int))
#define clear_color_hdr(cb)   memset((char *)(cb), 0, HEADER_SIZE/sizeof(int))
#define clear_color_block(cb) memset((char *)(cb), 0, LINES_PER_BLOCK/sizeof(int))  /* should have been bits per char instead of sizeof(int) */
#endif
#define clear_color_data(cb)  memset((char *)(cb), 0, sizeof(cb))
#define clear_color_hdr(cb)   memset((char *)(cb), 0, sizeof(cb))
#define clear_color_block(cb) memset((char *)(cb), 0, sizeof(cb))

/***************************************************************
*  RES  3/31/1999 - Fixes problem on Dec Alpha (64 bit machine)
*  Set and clear color bit macros use a shift to divide by the number
*  of bits in a long.  We conditionally set this with if's as
*  what we want is the log2 of the number of bits in a word and
*  we want it processed by the preprocessor.
***************************************************************/
#if WORD_BIT == 64
#define SHIFT_BITS 6
#endif
#if WORD_BIT == 32
#define SHIFT_BITS 5
#endif

#define set_color_bit(cb, bit)   ((cb)[(bit) >> SHIFT_BITS] |=  (1 << ((WORD_BIT-1)-((bit) & (WORD_BIT-1)))))
#define clear_color_bit(cb, bit) ((cb)[(bit) >> SHIFT_BITS] &= ~(1 << ((WORD_BIT-1)-((bit) & (WORD_BIT-1)))))
/*#define set_color_bit(cb, bit)   ((cb)[(bit) >> SHIFT_BITS] |=  (1 << ((LONG_BIT-1)-((bit) & (LONG_BIT-1))))) RES, Changed 4/14/2005 */
/*#define clear_color_bit(cb, bit) ((cb)[(bit) >> SHIFT_BITS] &= ~(1 << ((LONG_BIT-1)-((bit) & (LONG_BIT-1))))) RES, Changed 4/14/2005 *
/*#define set_color_bit(cb, bit)   ((cb)[(bit) >> 5] |=  (1 << ((LONG_BIT-1)-((bit) & (LONG_BIT-1))))) RES, Changed 3/31/1999 */
/*#define clear_color_bit(cb, bit) ((cb)[(bit) >> 5] &= ~(1 << ((LONG_BIT-1)-((bit) & (LONG_BIT-1))))) RES, Changed 3/31/1999 */
/* algorthm:                     ((cb)[(bit) / 32] &= ~(1 << ((bit) % 32))) historic, not right*/
/* bit zero is the sign bit */

#define WARN {int warni; char *warnp; warnp = warni;} /* generates a warning message */

/***************************************************************
*  
*  Global variables accessable from the outside
*  
***************************************************************/

#ifdef _MEMDATA_
extern char *cmdname;
#else
extern int line_load_level;
#endif

/***************************************************************
*  
*  Literals used in calls and declarations 
*  
***************************************************************/

#define INSERT    1    /* put_line_by_num() */
#define OVERWRITE 0

#define NOW   0         /* delayed_delete() */
#define LATER 1

#define MAX_LINE 16384 /* was 4096 */

#include "debug.h"

/***************************************************************
*   Macros
***************************************************************/

#define dirty_bit(token)   (token->dirty_file)
#define total_lines(token) (token->total_lines_infile)
#define WRITABLE(token)    (token->writable)
#define VALID_LINE(token)  ((token->current_block_ptr[token->current_block_idx].size <= 0) ? 0 : 1)
#define COLORED(token)     (token->colored)

#ifndef MIN
#define MIN(a,b)  ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b)  ((a) > (b) ? (a) : (b))
#endif

#ifndef ABSVALUE
#define ABSVALUE(a)    ((a) > 0 ? (a) : -(a))
#endif

#ifndef ABS2
#define ABS2(a,b) (((a) > (b)) ? ((a) - (b)) : ((b) - (a)))
#endif

#define TOGLE(a) a = !a

/***************************************************************
*  
*  Prototypes
*  
***************************************************************/

DATA_TOKEN  *mem_init(int percent_full, int sequential_insert_strategy);


void   load_a_block(DATA_TOKEN *token,        /* opaque */
                    FILE       *stream,
                    int         eat_vt100,    /* input */
                    int        *eof);         /* input / output */

void position_file_pointer(DATA_TOKEN *token,       /* opaque */
                           int         line_no);    /* input  */

char    *next_line(DATA_TOKEN *token); 
char    *prev_line(DATA_TOKEN *token);

char    *get_line_by_num(DATA_TOKEN *token,       /* opaque */
                         int         line_no);    /* input  */

int      delete_line_by_num(DATA_TOKEN *token,       /* opaque */
                            int         line_no,     /* input  */
                            int         count);      /* output */ 

int      delayed_delete(DATA_TOKEN *token,       /* opaque */
                        int         line_no,     /* input  */
                        int         delay,       /* input  */
                        int         *column,     /* input/output */
                        int         join);       /* input  */

void     split_line(DATA_TOKEN *token,       /* opaque */
                    int         line_no,     /* input  */
                    int         column_no,   /* input  */
                    int         new_line);   /* input  */


int      put_line_by_num(DATA_TOKEN *token,       /* opaque */
                         int         line_no,     /* input */
                         char       *line,        /* input */ 
                         int         flag);       /* input;   0=overwrite 1=insert after */

void     put_block_by_num(DATA_TOKEN *token,       /* opaque */
                          int         line_no,     /* input  */
                          int         lines,       /* input  */
                          int         column_no,   /* input  */
                          FILE       *stream);     /* input  */

void     print_file(DATA_TOKEN *token);   /* input  */

void     save_file(DATA_TOKEN *token,       /* opaque */
                   FILE       *fp);         /* input  */

char *get_color_by_num(DATA_TOKEN *token,       /* opaque */
                       int         line_no,     /* input  */
                       int         direction,       /* direction to search for color info */
                       int        *color_line_no);  /* output */

int put_color_by_num(DATA_TOKEN *token,           /* opaque */
                 int         line_no,             /* input  */
                 char        *color_data);        /* input  */

#ifdef DebuG
void     dump_ds(DATA_TOKEN *token); /* input */

void print_color_bits(DATA_TOKEN *token); /* input  */

void print_color(DATA_TOKEN *token); /* input  */

#endif

int      mem_kill(DATA_TOKEN *token); /* input */

void     encrypt_line(char *line);    /* input */

void     encrypt_init(char *passwd);  /* input */

int      sum_size(DATA_TOKEN *token, int from_line, int to_line); /* inputs */

void     join_line(DATA_TOKEN      *token,      /* input */
                   int              line_no);   /* input */

void vt100_eat(char        *target,
               char        *line);

#endif

