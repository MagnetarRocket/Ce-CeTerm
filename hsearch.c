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
*  module hsearch.c
*
*  This module provides the standard Unix hsearch functionality
*  with the exception that multiple existing tables are supported.
*  hcreate returns a pointer to the table which is passed as an
*  additional argument to the other functions.
*
*  This module works as described in the hsearch and hcreate manual
*  pages except that hcreate returns a pointer to the table and
*  hsearch and hdestroy take this value as a parameter.  This allows
*  support of multiple concurrent tables.
*
*  This code has been modified to integrate with the Ce environment.
*  This includes using the CE_MALLOC macro and the DEBUG macro.
*
*  This needs to be compiled with compile switches MULT and OPEN.
*  No testing was done with DIV (which probably works) and
*  CHAINED which will not work.  Also BRENT's modification causes
*  a memory fault sometimes, so don't put it in.
*
*  I also tweeked the crunch routine to get a better distribution
*  for the keys we are using.  This keeps the number of probes low. (1 or 2).
*
*  Routines:
*         hcreate          - Create a new hash table.
*         hsearch          - Search and/or add to the hash table.
*         hdestroy         - Delete a hash table.
*
*  Internal routines
*
***************************************************************/

#define MULT 1
#define OPEN 1

/*LINTLIBRARY*/
/* Compile time switches:

   MULT - use a multiplicative hashing function.
   DIV - use the remainder mod table size as a hashing function.
   CHAINED - use a linked list to resolve collisions.
   OPEN - use open addressing to resolve collisions.
   BRENT - use Brent's modification to improve the OPEN algorithm.
   SORTUP - CHAINED list is sorted in increasing order.
   SORTDOWN - CHAINED list is sorted in decreasing order.
   START - CHAINED list with entries appended at front.
   DRIVER - compile in a main program to drive the tests.
   DEBUG - compile some debugging printout statements.
   USCR - user supplied comparison routine.
*/

#include <stdio.h>
#include <string.h>

#if !defined(DRIVER) && defined(ARPUS_CE)
/*#ifndef DRIVER*/
#include "debug.h"
#include "emalloc.h"
#include "parsedm.h"
#else
#include "malloc.h"
#define CE_MALLOC(size) malloc(size)
#define dm_error(m,j) fprintf(stderr, "%s\n", m)
#define DEBUG26(m) {}
#define DEBUG0(m) {}
#endif
#include "hsearch.h"

#define SUCCEED        0
#define FAIL       1
#define TRUE       1
#define FALSE      0
#define repeat     for(;;)
#define until(A)   if(A) break;

typedef char *POINTER;


#ifdef OPEN
#    undef CHAINED
#else
#ifndef CHAINED
#    define OPEN
#endif
#endif

#ifdef MULT
#    undef DIV
#else
#ifndef DIV
#    define MULT
#endif
#endif

#ifdef START
#    undef SORTUP
#    undef SORTDOWN
#else
#ifdef SORTUP
#    undef SORTDOWN
#endif
#endif

#ifdef USCR
#    define COMPARE(A, B) (* hcompar)((A), (B))
     extern int (* hcompar)();
#else
#    define COMPARE(A, B) strcmp((A), (B))
#endif

#ifdef MULT
#    define SHIFT ((bitsper * sizeof(int)) - m) /* Shift factor */
#    define FACTOR 035761254233    /* Magic multiplication factor */
#    define HASH hashm     /* Multiplicative hash function */
#    define HASH2 hash2m   /* Secondary hash function */
static unsigned int bitsper;   /* Bits per byte */
static unsigned int hashm(POINTER key, unsigned int m);     /* Multiplication hashing scheme */
static unsigned int hash2m(POINTER key, unsigned int m);        /* Secondary hashing routine */
#else
#ifdef DIV
#    define HASH hashd     /* Division hashing routine */
#    define HASH2(A) 1     /* Secondary hash function */
static unsigned int hashd();
#endif
#endif

#ifdef CHAINED
typedef struct node {  /* Part of the linked list of entries */
   ENTRY item;
   struct node *next;
} NODE;
typedef NODE *TABELEM;
/*static NODE **table;*/   /* The address of the hash table */
static ENTRY *build();
#else
#ifdef OPEN

typedef struct {
   unsigned int       max_entries;             /* Size of the hash table */
   unsigned int       m;                       /* Log base 2 of length */
   unsigned int       prcnt;                   /* Number of probes this item */
   unsigned int       count;                   /* Number of entries in hash table */
   ENTRY             *entries;                 /* contents of the table                */
   ENTRY             *link_head;               /* linked list of entries in table */
   ENTRY             *link_tail;               /* linked list of entries in table */

} TABELEM;

#endif
#endif


static unsigned int crunch();

#ifdef DRIVER
static void hdump(TABELEM *base_table);            /* Dumps loc, data, probe count, key */
#define CE_MALLOC(size) malloc(size)
#define dm_error(m,j) fprintf(stderr, "%s\n", m)
#define DEBUG26(x) { }
#define DEBUG0(x) { }

main()
{
char       line[80];   /* Room for the input line */
int        i = 0;      /* Data generator */
ENTRY     *res;        /* Result of hsearch */
ENTRY     *new;        /* Test entry */
TABELEM   *base_table;

if (base_table = hcreate(5))
   fprintf(stdout, "max_entries = %u, m = %u\n", base_table->max_entries, base_table->m);
else
   {
      fprintf(stderr, "Out of core\n");
      exit(1);
   }

repeat
{
   hdump(base_table);
   fprintf(stderr, "Enter a probe: ");
   until (EOF == scanf("%s", line));
#ifdef DEBUG
   fprintf(stdout, "%s, ", line);
#ifdef DIV
   fprintf(stdout, "division: %d, ", hashd(line));
#endif
   fprintf(stdout, "multiplication: %d\n", hashm(line, base_table->m));
#endif
   new = (ENTRY *) malloc(sizeof(ENTRY));
   if(new == NULL)
      {
         fprintf(stderr, "Out of core \n");
         exit(2);
      }
   else
      {
         new->key = malloc((unsigned) strlen(line) + 1);
         if (new->key == NULL)
            {
               fprintf(stderr, "Out of core \n");
               exit(3);
            }
         strcpy(new->key, line);
         new->data = malloc(sizeof(int));
         if (new->data == NULL)
            {
               fprintf(stderr, "Out of core \n");
               exit(FAIL);
            }
         *new->data = i++;
      }
   res = hsearch(base_table, *new, ENTER);
   fprintf(stdout, "The number of probes required was %d\n", base_table->prcnt);
   if (res == (ENTRY *) 0)
       fprintf(stdout, "Table is full\n");
   else
      {
         fprintf(stdout, "Success:  Key = %s, Value = %d\n", res->key, *res->data);
      }
}  /* end of repeat group */

exit(SUCCEED);
} /* end of main */
#endif

/***************************************************************
*  
*  hcreate - create a hash table and return the pointer to it.
*
*  Takes the minimum number of entries in the hash table
*  Returns the pointer to the table or NULL on out of memory.
*
***************************************************************/

void  *hcreate(unsigned int   size)        /* Create a hash table no smaller than size */
{
unsigned int  unsize;   /* Holds the shifted size */
TABELEM      *table;
unsigned int  max_entries;
int           length;
unsigned int  m;

if ((int)size <= 0)
   return(NULL);

unsize = size;      /* +1 for empty table slot; -1 for ceiling */
max_entries = 1;    /* Maximum entries in table */
m = 0;              /* Log2 max_entries */
while (unsize)
{
   unsize >>= 1;
   max_entries <<= 1;
   m++;
}

length = (max_entries * sizeof(ENTRY)) + sizeof(TABELEM);
table = (TABELEM *) CE_MALLOC(length);
memset((char *)table, 0, length);
table->entries = (ENTRY *)((char *)table + sizeof(TABELEM));  /* table entrys follow header information */
table->max_entries  = max_entries; /* count of entries in table */
table->m       = m;

DEBUG26(fprintf(stderr, "hcreate:  HASH table at 0x%X create, size = %d\n", table, table->max_entries);)
return((void *)table);

} /* end of hcreate */


/***************************************************************
*  
*  Get rid of a table created with hcreate
*
*  Takes as an argument the pointer returned with the call to hcreate.
*  
***************************************************************/

void hdestroy(void *table_p)    /* Reset the module to its initial state */
{
#if !defined(DRIVER) && defined(ARPUS_CE)
unsigned int  i;
#endif
TABELEM      *base_table = (TABELEM *)table_p;
ENTRY        *table      = ((TABELEM *)table_p)->entries;

#if !defined(DRIVER) && defined(ARPUS_CE)
/*ifndef DRIVER*/
/***************************************************************
*  Ce code, free the table entries.  We know what they are.
***************************************************************/
for (i = 0; i < base_table->max_entries; i++)
{
   if (table[i].key)
      free(table[i].key);
   if (table[i].data)
      free_dmc((DMC *)table[i].data);
}
#endif

free((POINTER) table_p);
DEBUG26(fprintf(stderr, "hdestroy:  destroyed HASH table at 0x%X\n", table);)

}

#ifdef OPEN
/* Hash search of a fixed-capacity table.  Open addressing used to
   resolve collisions.  Algorithm modified from Knuth, Volume 3,
   section 6.4, algorithm D.  Labels flag corresponding actions.
*/

/***************************************************************
*  
*  Find or insert something in the table.
*
*  Arguments:
*  1. pointer to table created with hcreate
*  2. Item to find or create
*  3. Find or Insert flag
*  
***************************************************************/

ENTRY *hsearch(void *table_p, ENTRY item, ACTION action) /* Find or insert the item into the table */
{
unsigned int  i;    /* Insertion index */
unsigned int  c;    /* Secondary probe displacement */
TABELEM      *base_table = (TABELEM *)table_p;
ENTRY        *table      = ((TABELEM *)table_p)->entries;

base_table->prcnt = 1;

/* D1: */ 
    i = HASH(item.key, base_table->m);    /* Primary hash on key */
    /*DEBUG26(if (action == ENTER) fprintf(stderr, "hash = %o\n", i);)*/

/* D2: */
    if (table[i].key == NULL)   /* Empty slot? */
       goto D6;
    else
       if (COMPARE(table[i].key, item.key) == 0)  /* Match? */
          {
             DEBUG26(fprintf(stderr, "Number of hash table probes:  %d\n", base_table->prcnt);)
             return(&table[i]);
          }

/* D3: */
    c = HASH2(item.key, base_table->m);   /* No match => compute secondary hash */
    /*DEBUG26(if (action == ENTER) fprintf(stderr, "hash2 = %o\n", c);)*/

D4: 
    i = (i + c) % base_table->max_entries;  /* Advance to next slot */
    base_table->prcnt++;

/* D5: */
    if (table[i].key == NULL)   /* Empty slot? */
       goto D6;
    else
       if (COMPARE(table[i].key, item.key) == 0)  /* Match? */
          {
             DEBUG26(fprintf(stderr, "Number of hash table probes:  %d\n", base_table->prcnt);)
             return(&table[i]);
          }
       else
          goto D4;

D6:  /* could not find entry in the table */
   if (action == FIND)     /* Insert if requested */
      {
         DEBUG26(fprintf(stderr, "Number of hash table probes:  %d\n", base_table->prcnt);)
         return((ENTRY *) NULL);
      }

   if (base_table->count == (base_table->max_entries - 1))  /* Table full? */
      {
         DEBUG0(fprintf(stderr, "Key def hash table is full\n");)
         return((ENTRY *) NULL);
      }

#ifdef BRENT
/*
   Brent's variation of the open addressing algorithm.  Do extra
   work during insertion to speed retrieval.  May require switching
   of previously placed items.  Adapted from Knuth, Volume 3, section
   4.6 and Brent's article in CACM, volume 10, #2, February 1973.

   Brent's variation also causes a memory fault in the line cj = HASH2(table[pj]...
   when table[pj] ends up NULL.
*/

   {
   unsigned int p0 = HASH(item.key, base_table->m);   /* First probe index */
   unsigned int c0 = HASH2(item.key, base_table->m);  /* Main branch increment */
   unsigned int r = base_table->prcnt - 1; /* Current minimum distance */
   unsigned int j;         /* Counts along main branch */
   unsigned int k;         /* Counts along secondary branch */
   unsigned int curj;      /* Current best main branch site */
   unsigned int curpos;    /* Current best table index */
   unsigned int pj;        /* Main branch indices */
   unsigned int cj;        /* Secondary branch increment distance*/
   unsigned int pjk;       /* Secondary branch probe indices */

   if (base_table->prcnt >= 3)
      {
      for(j = 0; j < base_table->prcnt; j++)
      {  /* Count along main branch */
         pj = (p0 + j * c0) % base_table->max_entries; /* New main branch index */
         cj = HASH2(table[pj].key, base_table->m); /* Secondary branch incr. */
         for(k=1; j+k <= r; k++) 
         {   /* Count on secondary branch*/
             pjk = (pj + k * cj) % base_table->max_entries; /* Secondary probe */
             if(table[pjk].key == NULL) { /* Improvement found */
               r = j + k;  /* Decrement upper bound */
               curj = pj;  /* Save main probe index */
               curpos = pjk;   /* Save secondeary index */
           }
       }
       }
       if(r != base_table->prcnt - 1) {       /* If an improvement occurred */
       table[curpos] = table[curj]; /* Old key to new site */
       DEBUG26(fprintf(stderr, "Switch curpos = %o, curj = %o, oldi = %o\n", curj, curpos, i);)
       i = curj;
       }
   }
    }
#endif /* BRENT */

   /* add new entry to the table */
   base_table->count++;   /* Increment table occupancy count */
   table[i].key  = item.key;       /* Save item */
   table[i].data = item.data;
   /*  Put the new linked list entry at the end of the linked list. */
   if (base_table->link_head == NULL)
      base_table->link_head      = &table[i];
   else
      base_table->link_tail->link = (struct ENTRY *)&table[i];
   base_table->link_tail = &table[i];
   DEBUG26(fprintf(stderr, "Number of hash table probes:  %d\n", base_table->prcnt);)
   return(&table[i]);     /* Address of item is returned */
} /* end of hsearch */


/***************************************************************
*  
*  Get the head of the linked list of entries for sequential processing
*
*  Arguments:
*  1. pointer to table created with hcreate
*  
***************************************************************/

ENTRY *hlinkhead(void *table_p)
{
return(((TABELEM *)table_p)->link_head);
} /* end of hlinkhead */

#endif

#ifdef USCR
#ifdef DRIVER
static int
compare(a, b)
POINTER a;
POINTER b;
{
    return(strcmp(a, b));
}

int (* hcompar)() = compare;
#endif  /* ifdef driver */
#endif  /* ifdef USCR   */

#ifdef CHAINED
#    ifdef SORTUP
#        define STRCMP(A, B) (COMPARE((A), (B)) > 0)
#    else
#    ifdef SORTDOWN
#        define STRCMP(A, B) (COMPARE((A), (B)) < 0)
#    else
#        define STRCMP(A, B) (COMPARE((A), (B)) != 0)
#    endif
#    endif

ENTRY
*hsearch(item, action) /* Chained search with sorted lists */
ENTRY item;        /* Item to be inserted or found */
ACTION action;     /* FIND or ENTER */
{
    NODE *p;       /* Searches through the linked list */
    NODE **q;      /* Where to store the pointer to a new NODE */
    unsigned int i;    /* Result of hash */
    int res;       /* Result of string comparison */

    base_table->prcnt = 1;

    i = HASH(item.key, base_table->m);    /* Table[i] contains list head */

    if(table[i] == (NODE*)NULL) { /* List has not yet been begun */
   if(action == FIND)
       return((ENTRY *) NULL);
   else
       return(build(&table[i], (NODE *) NULL, item));
    }
    else {         /* List is not empty */
   q = &table[i];
   p = table[i];
   while(p != NULL && (res = STRCMP(item.key, p->item.key))) {
       base_table->prcnt++;
       q = &(p->next);
       p = p->next;
   }

   if(p != NULL && res == 0)   /* Item has been found */
       return(&(p->item));
   else {          /* Item is not yet on list */
       if(action == FIND)
       return((ENTRY *) NULL);
       else
#ifdef START
       return(build(&table[i], table[i], item));
#else
       return(build(q, p, item));
#endif
   }
    }
}

static ENTRY
*build(last, next, item)
NODE **last;       /* Where to store in last list item */
NODE *next;        /* Link to next list item */
ENTRY item;        /* Item to be kept in node */
{
    NODE *p = (NODE *) malloc(sizeof(NODE));

    if(p != NULL) {
   p->item = item;
   *last = p;
   p->next = next;
   return(&(p->item));
    }
    else
   return(NULL);
}
#endif

#ifdef DIV
static unsigned int
hashd(key)     /* Division hashing scheme */
POINTER key;       /* Key to be hashed */
{
    return(crunch(key) % base_table->max_entries);
}
#else
#ifdef MULT
/*
    NOTE: The following algorithm only works on machines where
    the results of multiplying two integers is the least
    significant part of the double word integer required to hold
    the result.  It is adapted from Knuth, Volume 3, section 6.4.
*/

static unsigned int hashm(POINTER key, unsigned int m)     /* Multiplication hashing scheme */
{
static int first = TRUE;   /* TRUE on the first call only */

if(first)
   {        /* Compute the number of bits in a byte */
      unsigned char c = (unsigned char)~0;   /* A byte full of 1's */
      bitsper = 0;
      while(c)
      {
         c >>= 1;      /* Shift until no more 1's */
         bitsper++;    /* Count number of shifts */
      }
      first = FALSE;
  }
return((int) (((unsigned) (crunch(key) * FACTOR)) >> SHIFT));
}

/*
 * Secondary hashing, for use with multiplicitive hashing scheme.
 * Adapted from Knuth, Volume 3, section 6.4.
 */

static unsigned int hash2m(POINTER key, unsigned int m)        /* Secondary hashing routine */
{
    return((int) (((unsigned) ((crunch(key) * FACTOR) << m) >> SHIFT) | 1));
}
#endif
#endif

static unsigned int crunch(key)        /* Convert multicharacter key to unsigned int */
POINTER key;
{
    unsigned int sum = 0;  /* Results */
    int s;         /* Length of the key */
    /*unsigned short int  alignment;*/

    for(s = 0; *key; s++)  /* Add up the bytes two at a time,  shift each short before adding it in. */
    {
       sum += ((((int)(*key))<<8)+*(key+1)) << (s-1);
       /*sum += ((unsigned int)*((short int *)key)) << (s-1);*/
       key++;
    }
    /*   sum += *key++; original crunch, Simply add up the bytes  */

    return(sum + s);
}

#ifdef DRIVER
static void hdump(TABELEM *base_table)            /* Dumps loc, data, probe count, key */
{
unsigned int i;    /* Counts table slots */
ENTRY        *table      = base_table->entries;
#ifdef OPEN
    unsigned int sum = 0;  /* Counts probes */
#else
#ifdef CHAINED
    NODE *a;       /* Current Node on list */
#endif
#endif

    for(i = 0; i < base_table->max_entries; i++)
#ifdef OPEN
   if (table[i].key == NULL)
      fprintf(stdout, "%x.\t-,\t-,\t(NULL)\n", i);
   else
      {
         unsigned int oldpr = base_table->prcnt; /* Save current probe count */
         hsearch(base_table, table[i], FIND);
         sum += base_table->prcnt;
         fprintf(stdout, "%x.\t%d,\t%d,\t%s\n", i,
         *table[i].data, base_table->prcnt, table[i].key);
         base_table->prcnt = oldpr;
      }
   fprintf(stdout, "Total probes = %d\n", sum);
#else
#ifdef CHAINED
   if (table[i] == NULL)
       fprintf(stdout, "%x.\t-,\t-,\t(NULL)\n", i);
   else
      {
         fprintf(stdout, "%x.", i);
         for (a = table[i]; a != NULL; a = a->next)
            fprintf(stdout, "\t%d,\t%#0.4x,\t%s\n", *a->item.data, a, a->item.key);
      }
#endif /* CHAINED */
#endif /* ELSE NOT OPEN */
} /* end of hdump */
#endif /* DRIVER */
