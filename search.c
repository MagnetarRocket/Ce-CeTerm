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

/***************************************************************

1) nead to change references to sd->pat to local_pattern
   so that /one/   s//two/   ^r  will work on 'One One One'

       
*
*  module search.c
*
*  Routines:
*     search       -   Find a string in the file either forward or backward.
*     process_inserted_count_change - after a subs, if count_change were added, update memdata
*
*  Internal:
*     lower_case     -  lower case a string
*     step_back      -  search in reverse for a string
*     subs           -  substitute an expression in a line  
*     getchr         -  return the next character of the pattern, called from compile
*     init_bra       -  initialize the bracket array
*     compile_error  -  issues an error message, called from compile via ERROR()
*     place          -  insert a string in another string
*     newline_step   -  find/replace with newlines in the expression
*     print_pat_expr -  prints the hex expression in readable format (DIAGNOSTIC)
*     process_inserted_newlines - find all the newlines and process them
*     newline_precomp -  @n<pat>@n -> ^<pat>$ ...
*     newlines_filter -  changes '\n' to \010, ends in '$' return(1);
*     nbstep          -  try a bracketed newline in two steps()
*     newline_bracket -  is there a newline in a bracket
*     eget            -  get a character out of an expression (strchr like)
*
*  External(Too Me):
*     compile        -  compile a Regular expression (provided by OS)
*     step           -  execute the expression on a string (provided by OS)
*
*
*  Notes:
*
*    compiles parms:  
*                     flag, notused
*                     begin address for compiled expression storage
*                     end address for compiled expression storage
*                     string termination character
*    step:
*                     line to search in
*                     expression compiled above
*
*   loc1 - is an external (char *)  that is set to the begining of
*          the string found by looking for a RE in step()
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <errno.h>          /* /usr/include/errno.h      */
#include <limits.h>         /* /usr/include/limits.h     */
#include <string.h>         /* /usr/include/string.h     */
#include <stdlib.h>         /* needed on linux for exit */
#include <ctype.h>          /* /usr/include/ctype.h      */

#define FAST_SUBS 1         /* turn on certain compile time features */
#define NEWLINE_BRACKET 1 
/* #define sdr */              /* static data brackets */


#include "dmwin.h"
#include "debug.h"
#include "memdata.h"
#include "search.h"
#include "emalloc.h"
#include "cd.h"

#define SEARCH_C  1
#define NEWLINE   10
#define LARGE_NUM  (MAX_LINE*10)
#define strend(x) (x + strlen(x))   /* points to the NULL */

int getchr(void);   /* called by compile() */

int step(char *p1, char *p2); /* in regexp.h */
int advance(char *lp, char *ep);
char *compile(char *instring, char *ep, char *endbuf, int seof); 


/*
 *  The following is needed by regex.h 
 */

#define GETC()     getchr()
#define INIT		extern int peekc;
#define PEEKC()	(peekc = getchr())

/* #define UNGETC(c)	(peekc = c) */ 
/* #define UNGETC(c)	((c == '\n') ? c : peekc = c)  */
#define UNGETC	{if (c == '\n') goto defchar; else peekc = c;} 

/* #define RETURN(c)	{if (*(c-1) == 22){*(c-1) = '\n';goto defchar;}else return;}  */
/*define RETURN(c) return(c)  old value prior to 2/20/2020 */
#define RETURN(c) return(c)  

/* #define ERROR(c)	error1(c)  */
#define ERROR(c)	{compile_error(c);return((char *)1);}

int	lastc;
int	peekc;

extern int circf;

extern char *loc1, *loc2, *locs; /* return of column found */

#define	NBRA	9

struct spattern {  /* static structure per pattern */
     int ends_in_dollar; /* the input pattern */
     /* (   V unsigned added 4/29/93 for getchr bug for 8 bit characters) */
     unsigned char *pat_ptr;  /* pattern_pointer for getchr() */
     char pat[EBUF_MAX];  /* pattern passed in and trashed */
     char saved_subs[EBUF_MAX];   /* for '%' replacement option */
     int join;   /* only set when pattern is '@n' by its self */
     int begin_line;
     int newlines; 
     int scase;           /* 10/17/97 s/ABC/DEF/  ^r */
     int compile_failed; 
     int dont_match_line_zero; 
     int ends_in_newline;
     char expression[EBUF_MAX];
     int expression_compiled;  /* prevent someone from doing a '//' in the editor */
                               /* without ever having set an expression */ 
     char sbuff[MAX_LINE+2];   /* #ifdef FAST_SUBS memdata stuff directly! */
     void *color_sd;    /* static data storage for color changes */

/* from  regexp.h ! */
#ifdef sdr
     char  brablist[NBRA][256];
     char	*braslist[NBRA];
     char	*braelist[NBRA];
#endif
     int	nodelim;
     int	low;
     int	size;
};

struct spattern *sd;

  /* return macro from search() */
#define SRETURN { if (*to_col < 0) *to_col=0; \
                  if (*found_col < 0) *found_col = 0;\
                  if (sd->color_sd) cd_add_remove(token, &(sd->color_sd), -1, 0, 0); \
                  return; }

void   lower_case(char *string);
static int  step_back(char *buff, char *expression, int end);
static int  newline_step(DATA_TOKEN *token, char *buf, char *exp, int line, int *change, char *dflag, int end_line, int end_col);
static void newline_precomp(char *pat);
static void init_bra(void);
static void subs(DATA_TOKEN *token, void **color_sd, int lineno, char *substitute, char *line, int *count_change, int *size);
static char *place(char *genbuf, char *sp, char *l1, char *l2);
static void  print_pat_expr(char *pat, char *expr); /* diagnostic */
#ifdef UNIX_NEWLINE
static int newlines_filter(char *pat);
#endif
static int nbstep(char *buf, char *expr);
static int newline_bracket(char *expr, int *nstar);
static char *eget(char *expr, char token); 

/******************  REGEXP.H  ****************************/
/* #include "regexp.h" */          /* /usr/include/regexp.h    */
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

static void getrnge(char *str);

#define	CBRA	2
#define	CCHR	4
#define	CDOT	8
#define	CCL	    12
#define	CXCL	16
#define	CDOL	20
#define	CCEOF	22
#define	CKET	24
#define	CBACK	36
#define    NCCL	40

#define	STAR	01
#define    RNGE	03

/* #define	NBRA	9 moved to search.h KBP 12/5/94 */

#define PLACE(c)	ep[c >> 3] |= bittab[c & 07]
#define ISTHERE(c)	(ep[c >> 3] & bittab[c & 07])
#define ecmp(s1, s2, n)	(!strncmp(s1, s2, n))

static unsigned char bittab[] = { 1, 2, 4, 8, 16, 32, 64, 128 };

int	sed, nbra;
char	*loc1, *loc2, *locs;

int	circf;
#ifndef sdr
     char	*braslist[NBRA];
     char	*braelist[NBRA];
#endif

char *
compile(instring, ep, endbuf, seof)
int seof;                              /* added by Plylerk 8/31/92 */
register char *ep;
char *instring, *endbuf;
{
	INIT	/* Dependent declarations and initializations */
	register char c;
	register char eof = seof;
	char *lastep /* = instring KBP */;
	int cclcnt;
	char bracket[NBRA], *bracketp;
	int closed;
	int neg;
	int lc;
	int i, cflg;
	int iflag; /* used for non-ascii characters in brackets */

	lastep = 0;
	bracketp = bracket; /* moved to here by KBP */
	if((c = GETC()) == eof || c == '\n') {
		if(c == '\n') {
			UNGETC(c);
			sd->nodelim = 1;
		}
		if(*ep == 0 && !sed)
			ERROR(41);
		RETURN(ep);
	}
	/* bracketp = bracket;  moved from here by KBP */
	circf = closed = nbra = 0;
	if(c == '^')
		circf++;
	else
		UNGETC(c);
	while(1) {
		if(ep >= endbuf)
			ERROR(50);
		c = GETC();
		if(c != '*' && ((c != '\\') || (PEEKC() != '{')))
			lastep = ep;
		if(c == eof) {
			*ep++ = CCEOF;
			if (bracketp != bracket)
				ERROR(42);
			RETURN(ep);
		}
		switch(c) {

		case '.':
			*ep++ = CDOT;
			continue;

		case '\n':
			if(!sed) {
				UNGETC(c);
				*ep++ = CCEOF;
				sd->nodelim = 1;
				if(bracketp != bracket)
					ERROR(42);
				RETURN(ep);
			}
			else ERROR(36);
		case '*':
			if(lastep == 0 || *lastep == CBRA || *lastep == CKET)
				goto defchar;
			*lastep |= STAR;
			continue;

		case '$':
			if(PEEKC() != eof && PEEKC() != '\n')
				goto defchar;
			*ep++ = CDOL;
			continue;

		case '[':
			if(&ep[17] >= endbuf)
				ERROR(50);

			*ep++ = CCL;
			lc = 0;
			for(i = 0; i < 16; i++)
				ep[i] = 0;

			neg = 0;
			if((c = GETC()) == '^') {
				neg = 1;
				c = GETC();
			}
			iflag = 1;
			do {
				c &= 0377; /* not KBP 2/22/94 8-bit Eurpean */
				if(c == '\0' /* || c == '\n' KBP 2/22/94 */)
					ERROR(49);

			/*	if((c & 0200) && iflag) {  fixes  /[@n]/ */

				if(((c & 0200) || (c == '\n')) && iflag) {
					iflag = 0;
					if(&ep[32] >= endbuf)
						ERROR(50);
					ep[-1] = CXCL;
					for(i = 16; i < 32; i++)
						ep[i] = 0;
				}
				if(c == '-' && lc != 0) {
					if((c = GETC()) == ']') {
						PLACE('-');
						break;
					}
					if((c & 0200) && iflag) {
						iflag = 0;
						if(&ep[32] >= endbuf)
							ERROR(50);
						ep[-1] = CXCL;
						for(i = 16; i < 32; i++)
							ep[i] = 0;
					}
					while(lc < c ) {
						PLACE(lc);
						lc++;
					}
				}
				lc = c;
				PLACE(c);
			} while((c = GETC()) != ']');
			
			if(iflag)
				iflag = 16;
			else
				iflag = 32;
			
			if(neg) {
				if(iflag == 32) {
					for(cclcnt = 0; cclcnt < iflag; cclcnt++)
						ep[cclcnt] ^= 0377;
					ep[0] &= 0376;
				} else {
					ep[-1] = NCCL;
					/* make nulls match so test fails */
					ep[0] |= 01;
				}
			}

			ep += iflag;

			continue;

		case '\\':
			switch(c = GETC()) {

			case '(':
				if(nbra >= NBRA)
					ERROR(43);
				*bracketp++ = nbra;
				*ep++ = CBRA;
				*ep++ = nbra++;
				continue;

			case ')':
				if(bracketp <= bracket) 
					ERROR(42);
				*ep++ = CKET;
				*ep++ = *--bracketp;
				closed++;
				continue;

			case '{':
				if(lastep == (char *) 0)
					goto defchar;
				*lastep |= RNGE;
				cflg = 0;
			nlim:
				c = GETC();
				i = 0;
				do {
					if('0' <= c && c <= '9')
						i = 10 * i + c - '0';
					else
						ERROR(16);
				} while(((c = GETC()) != '\\') && (c != ','));
				if(i > 255)
					ERROR(11);
				*ep++ = i;
				if(c == ',') {
					if(cflg++)
						ERROR(44);
					if((c = GETC()) == '\\')
						*ep++ = 255;
					else {
						UNGETC(c);
						goto nlim;
						/* get 2'nd number */
					}
				}
				if(GETC() != '}')
					ERROR(45);
				if(!cflg)	/* one number */
					*ep++ = i;
				else if((ep[-1] & 0377) < (ep[-2] & 0377))
					ERROR(46);
				continue;

			case '\n':
				ERROR(36);

			case 'n':
				c = '\n';
				goto defchar;

			default:
				if(c >= '1' && c <= '9') {
					if((c -= '1') >= closed)
						ERROR(25);
					*ep++ = CBACK;
					*ep++ = c;
					continue;
				}
			}
	/* Drop through to default to use \ to turn off special chars */

		defchar:
		default:
			lastep = ep;
			*ep++ = CCHR;
			*ep++ = c;
		}
	}
}

int step(p1, p2)
register char *p1, *p2; 
{
	register char c;


	if(circf) {
		loc1 = p1;
		return(advance(p1, p2));
	}
	/* fast check for first character */
	if(*p2 == CCHR) {
		c = p2[1];
		do {
			if(*p1 != c)
				continue;
			if(advance(p1, p2)) {
				loc1 = p1;
				return(1);
			}
		} while(*p1++);
		return(0);
	}
		/* regular algorithm */
	do {
		if(advance(p1, p2)) {
			loc1 = p1;
			return(1);
		}
	} while(*p1++);
	return(0);
}
char *olp;
int braset = 0;
int advance(lp, ep)
register char *lp, *ep;
{
	register char *curlp;
	int c;
	char *bbeg; 
	register char neg;
	int ct;

	while(1) {
		neg = 0;
		switch(*ep++) {

		case CCHR:
			if(*ep++ == *lp++)
				continue;
			return(0);
	
		case CDOT:
			if(*lp++)
				continue;
			return(0);
	
		case CDOL:
			if(*lp == 0)
				continue;
			return(0);
	
		case CCEOF:
			loc2 = lp;
			return(1);
	
		case CXCL: 
			c = (unsigned char)*lp++;
			if(ISTHERE(c)) {
				ep += 32;
				continue;
			}
			return(0);
		
		case NCCL:	
			neg = 1;

		case CCL: 
			c = *lp++;
			if(((c & 0200) == 0 && ISTHERE(c)) ^ neg) {
				ep += 16;
				continue;
			}
			return(0);
		
		case CBRA:
#ifdef sdr
			sd->braslist[*ep++] = lp;
           olp = lp;    /* #3415 added to fix s/@[{?*}@]/ PRED(@1)]/ */
           braset = 1;     /* 3415 */
#else
			braslist[*ep++] = lp;
#endif
			continue;
	
		case CKET:
#ifdef sdr
           { int blen;
           blen = lp - olp /* sd->braslist[*ep] */;
 /*        if (sd->brablist[*ep]) free(sd->brablist[*ep]); sd->brablist[*ep] = (char *)malloc(blen+1); */
           strncpy(sd->brablist[*ep], sd->braslist[*ep], blen);
           if (braset)  *(sd->brablist[*ep] + blen -1 /*+1*/) = NULL;
		 	sd->braslist[*ep] = sd->brablist[*ep];
		 	sd->braelist[*ep] = sd->braslist[*ep] + blen -1; /* -1 added  fixed 10/15/97 for s/{?*}/xxx"@1"/ */
           lp = sd->braslist[*ep] + blen;
           braset = 0;
           *ep++;
           }
#else
			braelist[*ep++] = lp;
#endif
			continue;
	
		case CCHR | RNGE:
			c = *ep++;
			getrnge(ep);
			while(sd->low--)
				if(*lp++ != c)
					return(0);
			curlp = lp;
			while(sd->size--) 
				if(*lp++ != c)
					break;
			if(sd->size < 0)
				lp++;
			ep += 2;
			goto star;
	
		case CDOT | RNGE:
			getrnge(ep);
			while(sd->low--)
				if(*lp++ == '\0')
					return(0);
			curlp = lp;
			while(sd->size--)
				if(*lp++ == '\0')
					break;
			if(sd->size < 0)
				lp++;
			ep += 2;
			goto star;
	
		case CXCL | RNGE:
			getrnge(ep + 32);
			while(sd->low--) {
				c = (unsigned char)*lp++;
				if(!ISTHERE(c))
					return(0);
			}
			curlp = lp;
			while(sd->size--) {
				c = (unsigned char)*lp++;
				if(!ISTHERE(c))
					break;
			}
			if(sd->size < 0)
				lp++;
			ep += 34;		/* 32 + 2 */
			goto star;
		
		case NCCL | RNGE:
			neg = 1;
		
		case CCL | RNGE:
			getrnge(ep + 16);
			while(sd->low--) {
				c = *lp++;
				if(((c & 0200) || !ISTHERE(c)) ^ neg)
					return(0);
			}
			curlp = lp;
			while(sd->size--) {
				c = *lp++;
				if(((c & 0200) || !ISTHERE(c)) ^ neg)
					break;
			}
			if(sd->size < 0)
				lp++;
			ep += 18; 		/* 16 + 2 */
			goto star;
	
		case CBACK:
#ifdef sdr
			bbeg = sd->braslist[*ep];
			ct = sd->braelist[*ep++] - bbeg;
#else	
			bbeg = braslist[*ep];
			ct = braelist[*ep++] - bbeg;
#endif
			if(ecmp(bbeg, lp, ct)) {
				lp += ct;
				continue;
			}
			return(0);
	
		case CBACK | STAR:
#ifdef sdr
			bbeg = sd->braslist[*ep];
			ct = sd->braelist[*ep++] - bbeg;
#else	
			bbeg = braslist[*ep];
			ct = braelist[*ep++] - bbeg;
#endif
			curlp = lp;
			while(ecmp(bbeg, lp, ct))
				lp += ct;
	
			while(lp >= curlp) {
				if(advance(lp, ep))	return(1);
				lp -= ct;
			}
			return(0);
	
	
		case CDOT | STAR:
			curlp = lp;
			while(*lp++);
			goto star;
	
		case CCHR | STAR:
			curlp = lp;
			while(*lp++ == *ep);
			ep++;
			goto star;
	
		case CXCL | STAR:
			curlp = lp;
			do {
				c = (unsigned char)*lp++;
			} while(ISTHERE(c));
			ep += 32;
			goto star;
		
		case NCCL | STAR:
			neg = 1;

		case CCL | STAR:
			curlp = lp;
			do {
				c = *lp++;
			} while(((c & 0200) == 0 && ISTHERE(c)) ^ neg);
			ep += 16;
			goto star;
	
		star:
			do {
				if(--lp == locs)
					break;
				if(advance(lp, ep))
					return(1);
			} while(lp > curlp);
			return(0);

		}
	}
}

static void getrnge(str)
register char *str;
{
	sd->low = *str++ & 0377;  /*  V may not be ANSI */
	sd->size = ((unsigned char)*str == 255) ? 20000: (*str &0377) - sd->low;
}


/******************  REGEXP.H  ****************************/


/************************************************************************

NAME:      search  -  Find a string in a range

PURPOSE:    This routine will accept a text range, pattern, and 
            direction and locate the pattern in the file.  If the 
            pattern is found, the row and col in the file are returned.
            Otherwise, they are set to -1. -2 means the regular expression
            had a syntax error.

PARAMETERS:

   1.  from_line   - int (INPUT)
               This is the row number of the top of the
               search area or the start of the area if the to_line is zero.
               For a forward search, this is the start line.  For a 
               backwards search, this is the end line unless the to line
               is zero in which case a search is done to top of file.

   2.  from_col    - int (INPUT)
              This is the column number (zero based) corresponding to a postion
              on from line.  See the from line parm to see what this means.

   3.  to_line     - int (INPUT)
              This is the row number of the bottom of the seach area or zero if search
              to end of file is requested.  For a forward search, this is the end line.
              For a reverse search, this is the start line unless it is zero in 
              which case a search to top of file is done.

   4.  to_col        - int (INPUT)
              This is the column number (zero based) corresponding to a postion
              on to line.  See the to line parm to see what this means.

   5.  reverse_find  - int (INPUT)
              This flag indicates a reverse direction search.
              Values:
                       0  -  Search Forward
                       1  -  Search Reverse

   6.  pattern       - pointer to char (INPUT)
               This is a pointer to the pattern to be searched for.

   7.  case_s        - int (INPUT)
               This is a flag to determine if case sensitivity will be used.
               Values:
                       0  -  Case insensitive
                       1  -  Case sensitive

   8.  found_line    - pointer to int (OUTPUT)
               If found, the line in the file is returned in this parm.
               Otherwise -1 is returned. -2 is syntax error.

   9.  found_col     - pointer to int (OUTPUT)
               If found, the col in the file is returned in this parm.

   10. count_change  -   tells the caller the number of line in the file changed

   11. rectangular   -   rectangular cut/copy flag (means rectangular search/subs

   12. rec_start_col -   Start column of all rectangular searches/subs (could be 0!)

   13. rec_end_col -   End column of all rectangular searches/subs (could be 0!)

   14. so -  End column of all rectangular searches/subs (could be 0!)


FUNCTIONS :

   1.   Determine whether this is a forward or backwards search and
        establish a start and end postion.

   2.   For a forward search subs, starting at the top to the 
        bottom.  Reverse searches go the other way.

   3.   Set the return code and values based on whether the
        data is found.


*************************************************************************/

void search(    DATA_TOKEN *token,            /* input  */
                int         from_line,        /* input  */
                int         from_col,         /* input  */
                int         to_line,          /* input  */
                int        *to_col,           /* input  */
                int         reverse_find,     /* input  */
                char       *pattern,          /* input  */
                char       *substitute,       /* input  */
                int         case_insensitive, /* input  */
                int        *found_line,       /* output */
                int        *found_col,        /* output */
                int        *count_change,     /* input/output */
                int        rectangular,       /* input  */
                int        rec_start_col,     /* input  */
                int        rec_end_col,       /* input  */
                int        *substitutes_made, /* input  */
                int        so,                /* input  */
                char       escape_char,       /* input  */
                void       **sdata)           /* input/output  */
{

int len;
int size;
int end_col;
int end_line;
int start_col;                                                 
int start_line;

int dummy = 0;

#ifdef FAST_SUBS
char *obuff;
int dirty_line = -1; /* -1=invalid, 0=clean, 1=dirty */
int really_found_line = DM_FIND_NOT_FOUND;
int really_found_col  = DM_FIND_NOT_FOUND;
int saved_to_col;
#endif


#ifdef UNIX_NEWLINE
char nfpat[EBUF_MAX];
#endif
 
int dollar_only;
int null_offset = 0;

int newline_change = 0;  /* determines how many \n's went away */

char *buff; 

char tbuff[MAX_LINE+2];   /* subs changes the buff and we don't want it playing with */

DEBUG0( if (token->marker != TOKEN_MARKER) {fprintf(stderr, "Bad token passed to search\n"); exit(8);})

DEBUG13( fprintf(stderr, " @search(from(%d,%d) to(%d,%d) dir(%d) pat:'%s' case(%d))\n", 
        from_line, from_col, to_line, (*to_col), reverse_find,  pattern ? pattern : "<NULL>", case_insensitive);)

DEBUG13( fprintf(stderr, "        (rec(%d,[%d,%d]) subs(%s))\n", rectangular, rec_start_col, rec_end_col,
                        (substitute == REPEAT_SUBS) ? "REPEAT_SUBS" : (substitute ? substitute : "NULL"));)

DEBUG27( fprintf(stderr, "SEARCH(Total lines in file: %d)\n",token->total_lines_infile);)

*found_line = DM_FIND_NOT_FOUND;  /* pessimistic view */
*found_col = DM_FIND_NOT_FOUND;   /* pessimistic view */

if (!*sdata){
  DEBUG13(fprintf(stderr, "Initializing static pattern buffer\n");)
  if (!(*sdata = (void *)CE_MALLOC(sizeof(struct spattern)))){ /* static data structure initialized */
      dm_error("malloc failed!", DM_ERROR_BEEP);
      SRETURN;
  }
  sd = (struct spattern *)*sdata;
  sd->ends_in_newline = 0;
  sd->expression[0] = '\0';
  sd->expression_compiled = 0;
  sd->color_sd = NULL;
  sd->scase = True;
}else
  sd = (struct spattern *)*sdata;

locs = loc1 = loc2 = "";  /* for compile */

sd->compile_failed = 0;

if (substitute && (substitute != REPEAT_SUBS)){
     if (strcmp(substitute, "%")){  /* implies "use last replace expression" */
         strcpy(sd->saved_subs, substitute);
         buff = sd->saved_subs; /* fixes IBM/RS6000 cc -O bug  */
         if (escape_char == '@'){ 
             a_to_re(buff, 1);
         }
     }
}

if (substitute) init_bra();

if ((pattern != NULL) && (pattern[0] != '\0')){  /* not repeat find same string */
   sd->join = 0; /* 9/15/93 to fix <below> 987658 */
   sd->expression_compiled = 0; /* reset succsessfull compilation flag */
   sd->ends_in_newline = 0; /* reset this! */
   lastc =0;peekc=0;   /* init for regexp.h (compile) */

   if (substitute)
        sd->scase = True;
   else
        sd->scase = False;

   strcpy(sd->pat, pattern);   /* place in safe place */
   sd->pat_ptr = (unsigned char *)sd->pat;
   if (case_insensitive)
        lower_case(sd->pat);   /* change 11/11/99 */

   len = strlen(sd->pat) -1;
   sd->ends_in_dollar = 0;
   if (escape_char == '@'){  /* aegis style regular expression */
       sd->ends_in_dollar = a_to_re(sd->pat, 0);
   }else{
#ifdef UNIX_NEWLINE
       strcpy(nfpat, pat);
       ends_in_dollar = newlines_filter(pat); /* would fix \n capability in unix mode */
       if (strcmp(pat, nfpat))
          DEBUG0(fprintf(stderr, "UNIX_NEWLINE changed '%s' -> '%s'\n", nfpat, pat);)
#else
       if (sd->pat[len] == '$') sd->ends_in_dollar++; /* temporary code */
#endif
   }

#ifdef blah /* pulled  3/11/94: 1,$s/%?*@n// */
   if (pat[len] == NEWLINE){
       pat[len] = '$';
       sd->ends_in_dollar++;
   }
#endif

   /* sd->join = 0;  pulled 9/15/93 fixed <above> 987658 */
   if ((*sd->pat_ptr == NEWLINE) && (*(sd->pat_ptr + 1) == '\0')){
          if (substitute) sd->join++; 
          *sd->pat_ptr = '$';  /* finds end of line */
   }

/* 9/14/93 fixes :22222' s/'@n/@n/ down to->(4 lines) */
   len = strlen(sd->pat);
   if ((len>1) && (sd->pat_ptr[len-1] == NEWLINE) && (sd->pat_ptr[len] == '\0')){
       sd->ends_in_newline++;
   }
/* here */

   if (sd->pat[0] == '^')
      sd->begin_line++; /* looking for begining of line */
   else
      sd->begin_line = 0;

   sd->dont_match_line_zero = 0;
   if ((strchr(sd->pat, NEWLINE) || strchr(sd->pat, NEWLINE | 0200)) && !sd->join){
       newline_precomp(sd->pat); /* preprocesses @n's */
       sd->newlines++; 
   }else
       sd->newlines = 0;

   if (!sd->compile_failed) /* 11/9/93 incase re() failed*/
       compile((char *) 0, sd->expression, sd->expression + EBUF_MAX, 0);
   if (sd->compile_failed){
        *found_line = DM_FIND_ERROR;  /* pessimistic view */
        SRETURN;  /* compile error */
   }
   sd->expression_compiled++;
   DEBUG13(print_pat_expr(sd->pat, sd->expression);) 

}   /* !repeat find */

/*
 *  sanity checks
 */

if (from_col < -1) from_col = -1;

if (from_line < 0){
    from_line = 0;
    from_col = -1;
}

if (to_line >= total_lines(token)){
    to_line = total_lines(token) -1;
    (*to_col) = MAX_LINE +1;
}

if (!sd->expression_compiled){
     compile_error(41);
     *found_line = DM_FIND_ERROR;  
     SRETURN;  /* no expression ever compiled */
}

/*
 *  find/subs forward, or find reverse?
 */

if (((sd->pat[0] == '$') && (sd->pat[1] == '\0')) || !sd->ends_in_dollar) /*12/6/92 eid added to fix /[~ ]/ aka click on file/edit when file is last identifier on a line */
     dollar_only = 1;
else 
     dollar_only = 0;

if (!reverse_find){

    end_col = (*to_col);
    if (end_col < 1){    /* no reason to look at a line of zero length */
        end_col = LARGE_NUM -1;
        to_line--;
    }

#ifdef BLAH
    if (sd->begin_line && (from_col >= 0)){ /* 3/22/95 s/%  // '        kevin' */
        from_line++;
        from_col=-1;
    }
#endif

    start_line = from_line;
    start_col  = from_col +1;
    end_line = to_line;

    end_col--;   /* for offset 1 -> 0 */

 /* if (rectangular && (start_col < rec_start_col)) start_col = rec_start_col;  moved down 3 lines 4/6/93 */

#ifdef FAST_SUBS
    saved_to_col = *to_col;
    if (rectangular && (start_line == end_line) && ((start_col+1) == *to_col)) SRETURN; /* bob some times calls me this way erroniously */
#endif

    position_file_pointer(token, start_line);
    for (buff = next_line(token); (buff != NULL) && (start_line <= end_line); buff = next_line(token)){

#ifdef FAST_SUBS
       *to_col = saved_to_col;
       if (dirty_line == -1) dirty_line = 0; /* validate */
       if (dirty_line){
           DEBUG13(fprintf(stderr, "Changing line(1)[%d]:'%s'\n", start_line-1, obuff);)
           put_line_by_num(token, start_line-1, obuff, OVERWRITE);
           dirty_line = 0;
       }

begin: /* Yes we have goto's! */

       DEBUG27(fprintf(stderr, "###### FILE Before next iteration:\n"););
       DEBUG27(print_file(token););

       if (*found_line > -1) really_found_line =  *found_line;  /* 9/30/93 (0)->(-1) fixes 1,$s/X/Y/ on a 1 line file*/
       if (*found_col > -1)  really_found_col  =  *found_col;   /* 9/30/93 (0)->(-1) */

       *found_line = DM_FIND_NOT_FOUND; 
       *found_col = DM_FIND_NOT_FOUND; 
#endif

       if (rectangular && (start_col < rec_start_col)) start_col = rec_start_col;

       len = size = strlen(buff);
       if ((sd->begin_line && (start_col > 0)) ||
           ((start_col >= (len + (dollar_only /*12/6/92*/))) && len)){
           start_col = 0;     /* can't find begining of line in this region */
           start_line++;
           if (rectangular){
                end_col = rec_end_col - 1;
                (*to_col) = rec_end_col;
           }
           continue;
       }

       if (buff != tbuff)  /* 5/19/93 optomizes */
           strcpy(tbuff, buff);  
       buff = tbuff;      /* sub needs its own copy */

       if (case_insensitive && !substitute && !sd->scase) lower_case(buff);
       DEBUG13( fprintf(stderr, " scanningF(%s)[start(%d,%d),end(%d,%d)]\n", buff + start_col, start_line, start_col, end_line, end_col);) 

       if (sd->newlines){   /* do we have one or more sd->newlines in the pattern? */
            if (((start_col < len) || ((start_col == len) && !*buff)) &&
                 newline_step(token, buff + start_col, sd->expression, start_line, &newline_change, substitute, end_line, end_col)){   /* substitute -> sd->saved_subs 1/8/93 */
                if (loc1==loc2) null_offset = 1;          /* (len<sc) s/{?*}/x@1/ 2/17/94 */
                loc1 = buff + start_col + (long)loc1;     /* RES 1/5/2016, change int to long for CentOS 7 */
                loc2 = buff + start_col + (long)loc2;     /* RES 1/5/2016, change int to long for CentOS 7 */
                *found_col = loc1 - buff;
                *count_change += newline_change;
                DEBUG13( fprintf(stderr, " nfound-buff(%d),loc1(%d){%d},loc2(%d){%d},start_line[%d]\n",buff, loc1, loc1 - buff, loc2, loc2 - buff, start_line);) 
            }
            if (sd->compile_failed){  
                  *found_line = DM_FIND_ERROR;  /* pessimistic view */
                  DEBUG13( fprintf(stderr, "Return[%d,%d]\n", *found_line, *found_col);) 
                  SRETURN;  /* compile error [@n]* or [@n]@n* */
            }
            position_file_pointer(token, start_line+1); /* 2/9/94 since newline step does position/next we have to reposition */
       }else{
            if (!sd->ends_in_dollar) strcat(buff, "\n");   /*(buff + start_col) would be faster */
            if (start_col > len){ /* kludge for /$/ */
                start_col = 0;
                start_line++;
                if (rectangular){
                    end_col = rec_end_col - 1;
                    (*to_col) = rec_end_col;
                }
#ifdef FAST_SUBS
                if (dirty_line > 0) 
                    put_line_by_num(token, start_line-1, buff, OVERWRITE);
                dirty_line = 0;
#endif
                continue;
            } 
#ifdef blah
            /*   ((start_col == len) && ((((pat[0] == '$') && !pat[1]) || sd->ends_in_dollar) (* || (buff[len] == NEWLINE) *) ))) && /* breaks [^]a-za-z._$0-9/\[~-] -> click on edit of last char on a line */
            /*   ((start_col == len) && (((pat[0] == '$') && !pat[1]) || (buff[len] == NEWLINE)))) &&     [[[ fixes: /%[ @t]*$/ does not find NULL line ]]] */ /* also try s/@n/a@nb/ on a range that ends in a blank line */
#endif
            if (((start_col < len) || 
                  ((start_col == len) && 
                 (((sd->pat[0] == '$') && !sd->pat[1]) || 
                  ((sd->pat[0] == '^') && !start_col)  ||    /* this line added 6/29/95 to fix 1,$s/%/>/ not work on empty lines */
                  (sd->ends_in_dollar) || 
                  (sd->pat[strlen(sd->pat)-1] == ']') ))) && 
/*#define ss_problems * commented in 9/13/99 to fix 1,$s/[~ ]/r/ infinite loops, commented out 2/6/2001 to fix click on last character of a file name finding first token on next line */
#ifdef ss_problems
                 !((start_col == len) && (sd->ends_in_dollar || (sd->pat[0] != '^') && (buff[len] == NEWLINE) )) &&
                 /* commented in for s/[~x]/x/ ; [[[ this could cause trouble 7/27/94 ]]]; buff[null] commented out to prevent infinit loop s/[ ]*_/12/ */
                 /* this still does not fix /%[ @t]$/ finding blank lines ; try m1 on last letter of file name (m1=ls -la)*/
#endif
                step(buff + start_col, sd->expression)){ /* len<sc s/{?*}/x@1/ 2/17/94 */
                     if (loc1==loc2) null_offset = 1;
                     if (!sd->ends_in_dollar){
                         if (loc1 == (buff + len +1)) loc1--;  /* can't point a '\n' */
                         if (loc2 == (buff + len +1)) loc2--;
                     }
                     *found_col = loc1 - buff;
                     DEBUG13( fprintf(stderr, " found-buff(%d),loc1(%d){%d},loc2(%d){%d},start_line[%d]\n",buff, loc1, loc1 - buff, loc2, loc2 - buff, start_line);) 
            }
            if (!sd->ends_in_dollar) buff[len] = '\0'; /* kill the newline */
       }  /* sd->newlines */

       if ((*found_col != DM_FIND_NOT_FOUND) || sd->join){
           if (rectangular && ((*found_col > end_col) || (*found_col < start_col))){
               if (*found_col > end_col){
                   start_col = rec_start_col;
                   start_line++;
                   if (rectangular){
                      end_col = rec_end_col -1;
                      (*to_col) = rec_end_col;
                   }
               }
               *found_col =  DM_FIND_NOT_FOUND; /* just missed */
#ifdef FAST_SUBS
               if (dirty_line > 0) 
                   put_line_by_num(token, start_line-1, buff, OVERWRITE);
               dirty_line = 0;
#endif
               continue;
           }
           if ((start_line == end_line) && (*found_col > end_col) ||
              ((start_line == from_line) && (*found_col < start_col))){

               *found_col  = DM_FIND_NOT_FOUND; /* just missed */

           }else{
               *found_line = start_line;
               if (substitute){
                   strcpy(sd->sbuff, sd->saved_subs);
                   
                   if (newline_change) strcpy(buff, get_line_by_num(token, start_line));

                   if (sd->join){
                        loc1 = loc2 = buff + len;
                        delayed_delete(token, start_line, LATER, &dummy, sd->join);
                        (*count_change)++;
                   }

                   subs(token, &(sd->color_sd), start_line, sd->sbuff, buff, count_change, &size);

                   if (rectangular || (*found_line == end_line)){ /* 5/7/93 */
#ifdef FAST_SUBS
                       end_col += size - len;
#endif
                       (*to_col) += size - len; 
                       DEBUG13(fprintf(stderr," 'line end' expanded by '%d' characters size=%d,len=%d\n", size - len, size, len);)
                   }
                   *found_col = null_offset + loc2 - buff -1; /* point to end of replacement */
#ifdef FAST_SUBS
                   (*substitutes_made)++;  /* 9 16/93 */
                   if (sd->newlines){ /* 9/15/93 fixes abc\nabc\nabc s/c@na/a/ */
                       put_line_by_num(token, start_line, buff, OVERWRITE);
                       DEBUG13( fprintf(stderr, "Return[%d,%d]\n", *found_line, *found_col);) 
                       SRETURN;  /* 9/15/93 */
                   }

                   dirty_line++;
                   start_col = *found_col + 1;
                 /*  (*substitutes_made)++;   9/16/93 */
                   obuff = buff;
                   if (so || sd->begin_line /* 8/6/93 */){
                       start_line++;
                       start_col = 0;
                       really_found_line =  *found_line;
                       really_found_col  =  *found_col;
                       if (rectangular){
                           end_col = rec_end_col -1;
                           (*to_col) = rec_end_col;   
                       }
                       continue; /* so means only one change per line */
                   }
                   /* sd->begin_line = 0; 8/6/93 */ /* 7-22-93 s/%[ ]* / / bug fixed */
                   goto begin;
#else
                   put_line_by_num(token, start_line, buff, OVERWRITE);
#endif
               }   /* subs */
           } /* if(==&&==||==&&==) else */

           if (*found_col < 0) *found_col = 0;
           DEBUG13( fprintf(stderr, "FOUND(line:%d,col:%d)\n", *found_line, *found_col);)
           if ((*found_col == 0) && (from_col == -1) && (size != len))
                   if (debug) fprintf(stderr," Obsolete code used! (Report to ETG imediatly!!!)\n");
              /*   *found_col = -1; 5/7/93 */ /* 12/6/92 */
#ifdef FAST_SUBS
           if (substitute){
               if (dirty_line > 0){
                   put_line_by_num(token, start_line, obuff, OVERWRITE);
                   DEBUG13(fprintf(stderr, "Changing line(2)[%d]:'%s'\n",start_line, obuff);)
               }
               *found_col = really_found_col;
               *found_line = really_found_line;
               DEBUG13( fprintf(stderr, "REALLY_FOUND(line:%d,col:%d)\n", *found_line, *found_col);)
           }
#endif     
           DEBUG13( fprintf(stderr, "Return[%d,%d]\n", *found_line, *found_col);) 
           SRETURN;
       }else{
           if (rectangular){
               start_col = rec_start_col;
               end_col = rec_end_col -1;
               (*to_col) = rec_end_col;
           }
           start_line++;
       }  

       if ((start_col != 0) && !rectangular)
            start_col = 0;

    }  /* end of for loop walking through lines */

#ifdef FAST_SUBS
    if (dirty_line > 0){
        put_line_by_num(token, start_line-1, obuff, OVERWRITE);
        DEBUG13(fprintf(stderr, "Changing line(3)[%d]:'%s'\n",start_line-1, obuff);)
    }
    *found_line = really_found_line;
    *found_col  = really_found_col;
    DEBUG13( fprintf(stderr, "REALLY_FOUND(line:%d,col:%d)\n", *found_line, *found_col);)
#endif

}else{  /* end of forward find */   /* reverse find */

    start_line = to_line +1; 
    start_col  = (*to_col) -1;
    end_line   = from_line;
    end_col    = from_col;

 /* no reason to look at a line of zero length */
    if (start_col < 0){    /* was 1, fixes column one reverse search of 'a' */
        start_col = LARGE_NUM -1;
        start_line--;
    }

    position_file_pointer(token, start_line);
    for (buff = prev_line(token); (buff != NULL) && (start_line >= end_line); buff = prev_line(token)){

       if (sd->begin_line && (start_line == end_line) && (start_col > 0)){
           start_line--;  /* can't find begining off line in this region */
           continue;
       }

       strcpy(tbuff, buff);  
       buff = tbuff;        /* sub needs its own copy */

       start_line--;
       len = strlen(buff);

       if (case_insensitive && !sd->scase) lower_case(buff);
       DEBUG13( fprintf(stderr, " scanningR(%s)[start_line:%d, end_line:%d]\n", buff, start_line, end_line);) 
       if (sd->newlines){
           if (newline_step(token, buff, sd->expression, start_line, &newline_change, substitute, to_line+1, (*to_col) -1)){
               loc1 = buff + (long)loc1;   /* RES 1/5/2016, change int to long for CentOS 7 */
               loc2 = buff + (long)loc2;   /* RES 1/5/2016, change int to long for CentOS 7 */
               *found_col = loc1 - buff;
               DEBUG13( fprintf(stderr, " nrfound-buff(%d),loc1(%d){%d},loc2(%d){%d},start_line[%d]\n",buff, loc1, loc1 - buff, loc2, loc2 - buff, start_line);) 
           }
           position_file_pointer(token, start_line); /* 2/9/94 since newline step does position/next we have to reposition */
       }else{
           if (!sd->ends_in_dollar) strcat(buff, "\n");
           if (sd->ends_in_dollar && (start_col > 0)) start_col--; /* dont find NULL */
           if (step_back(buff, sd->expression, start_col)){
                if (!sd->ends_in_dollar){
                   if (loc1 == (buff + len +1)) loc1--;  /* can't point a '\n' */
                   if (loc2 == (buff + len +1)) loc2--;
                }
               *found_col = loc1 - buff;
               DEBUG13( fprintf(stderr, " rfound-buff(%d),loc1(%d){%d},loc2(%d){%d},start_line[%d]\n",buff, loc1, loc1 - buff, loc2, loc2 - buff, start_line);) 
           }
           if (!sd->ends_in_dollar) buff[len] = '\0'; /* kill the newline */
       }

       if (*found_col != DM_FIND_NOT_FOUND){
           if (((start_line == to_line) && (*found_col > start_col)) ||
               ((start_line == from_line) && (*found_col < end_col))){
                *found_col  = DM_FIND_NOT_FOUND; /* just missed */
           }else{
                *found_line = start_line;
                DEBUG13( fprintf(stderr, "FOUND(line:%d,col:%d)\n", *found_line, *found_col);)
                DEBUG13( fprintf(stderr, "Return[%d,%d]\n", *found_line, *found_col);) 
                SRETURN;
           }
       }

       if (start_col != -1) /* -1 is a flag to step_back that col does not count */
            start_col = -1;

    }  /* end of for loop walking through lines */

} /* end of reverse find */

    DEBUG13( fprintf(stderr, "F-Return[%d,%d]\n", *found_line, *found_col);) 
    SRETURN;
} /* end of search */

/***********************************************************************
*
*     lower_case -- Convert a string to lower case.
*
*  INPUTS (TO FUNCTION):
*
*     string -- A string of characters.
*
*  FUNCTIONS:
*
*     1. Set the sixth bit of each character that is in: A..Z to 1.
*
***********************************************************************/

void lower_case(string)

char *string;

{
register char *ptr = string;

while (*ptr){
   if (isupper((unsigned char)*ptr))
       *ptr |= 040;            /* sets sixth bit to 1 to lower case it */
   ptr++;
   }
}

/***********************************************************************
*
*  getchr - get the next character from the pattern buffer
*
***********************************************************************/

int getchr()
{
if (lastc=peekc) {
    peekc = 0;
    return(lastc);
}

if (*sd->pat_ptr <= 0)
    return(lastc = '\0');

/* before 8 bit 4/29/93 lastc = (*sd->pat_ptr) & 0177;  */
lastc = *sd->pat_ptr;

sd->pat_ptr++;
return(lastc);

}   /* getchr() */

/***********************************************************************
*
*  step_back - step backwards looking for an RE in reverse
*              step() find the next occurance of expression in buff
*              so I keep looking for expression in buff until
*              no more are found or we have exceeded end. 
*              I then return the last occurance of the expression found.
*
***********************************************************************/

static int step_back(char *buff, char *expression, int end)
{

int rc;
int found = 0;

char *ptr;
char *last;
char *end_ptr;
char *loc1_save, *loc2_save;

if (sd->begin_line)  /* if the search is nailed to the begining of line only one call */
    return(step(buff, expression));  /* is needed */

if (end == -1)  /* if end=-1 then we are not on the first line so any column is ok */
    end = MAX_LINE +1; 

end_ptr = MIN(buff + end +1, strend(buff));
/* ptr = strend(buff);  debug */

buff[MAX_LINE] = '\0'; /* just in case! */

ptr = buff-1;
last = NULL;

while  ((rc = step(ptr+1, expression)) && (loc2 <= end_ptr)){ 

          found += rc;
          if (last)
              last = loc2;
          else  
              last = ptr;

          ptr = loc1;
          loc1_save = loc1;
          loc2_save = loc2;
}

if  (found && (loc2 > end_ptr)){
    loc1 = loc1_save;
    loc2 = loc2_save;
    if (last > end_ptr){
        found = 0; /* we found one on the line, but it was past to_col */
    }
}

return(found);

} /* step_back() */

/***********************************************************************
*
*  init_bra - initialize the bras/brae list for: s/\(xxx\)/\1/
*             'bra' stands for 'brackets'.  These arrays store
*             the saved patterns that will be called back latter.
*
***********************************************************************/

static void init_bra()
{
int c;

for (c=0; c<NBRA; c++) {
#ifdef sdr
  *sd->brablist[c] = NULL;
  sd->braslist[c] = 0;
  sd->braelist[c] = 0;
#else
  braslist[c] = 0;
  braelist[c] = 0;
#endif
}

} /* init_bra() */

/***********************************************************************
*
*  compile_error - print various compiler errors
*
***********************************************************************/

void compile_error(int rc)
{

sd->compile_failed++;

switch(rc){
case 11: dm_error("Range endpoint too large.", DM_ERROR_BEEP);
         break;

case 16: dm_error("Bad number.", DM_ERROR_BEEP);
         break;

case 25: dm_error("'\\digit' out of range.", DM_ERROR_BEEP);
         break;
          
case 36: dm_error("Illegal or missing delimiter.", DM_ERROR_BEEP);
         break;

case 41: dm_error("No remembered search string.", DM_ERROR_BEEP);
         break;

case 42: dm_error("'\\( \\)' or '{ }' imbalanced.", DM_ERROR_BEEP);
         break;

case 43: dm_error("Too many '\\(' or '{'.", DM_ERROR_BEEP);
         break;

case 44: dm_error("More than 2 numbers given.", DM_ERROR_BEEP);
         break;

case 45: dm_error("'\\}' expected.", DM_ERROR_BEEP);
         break;

case 46: dm_error("First number exceeds second.", DM_ERROR_BEEP);
         break;

case 49: dm_error("'[ ]' imbalance.", DM_ERROR_BEEP);
         break;

case 50: dm_error("Regular expression overflow.", DM_ERROR_BEEP);
         break;

case 51: dm_error("Newline character not allowed inside [].", DM_ERROR_BEEP);
         break;

case 52: dm_error("Newline characters, Brackets, and Asterisk may not all be used together in a regular expression.", DM_ERROR_BEEP);
         break;

    default:
       dm_error("Unknown regular expression error(%d).",rc);
} /* switch */

}

/***********************************************************************
*
*  subs - given a found string, replace it with the substitute pattern
*
* NOTES:  
*
*          '%' is a special replace according to ed(1) manpage.
*
***********************************************************************/
 
static void subs(DATA_TOKEN *token, void **color_sd, int lineno, char *substitute, char *line, int *count_change, int *size)
{

int c;
int bs_flag;  /* back slash found flag */

char *lp, *gp, *rp;

char genbuf[MAX_LINE+2];  /* line after substitution is completed */

DEBUG13( fprintf(stderr, " @Subs(%s,%s)\n", substitute, line);) 

DEBUG13( if (substitute == sd->saved_subs) fprintf(stderr, " %%-found, using last replacement(%s)\n", sd->saved_subs);) 

/* 
 *  now we can walk the line and do the substitute 
 */

lp = line;
gp = genbuf;
rp = substitute;   /* replace pointer */

*gp = '\0'; /* unintialized before this */

while (lp < loc1)
	*gp++ = *lp++;

DEBUG13( fprintf(stderr, " phase-I:(%s)\n", (*gp='\0',genbuf));) 

while (c = *rp++) {

   bs_flag = 0;

   if (c == '\\'){
       bs_flag++;
       c = *rp++;  
   }

   if (!bs_flag && (c=='&')){  /* changed 6/9/93 */
       if (COLORED(token)){
           cd_add_remove(token, color_sd, lineno, gp - genbuf, loc2-loc1); /* colorization */
       }
       if (!(gp = place(genbuf, gp, loc1, loc2))) return;
       continue;
	}  

   if (bs_flag && (c >= '1' && c < nbra + '1')){
#ifdef sdr
       if (COLORED(token)) cd_add_remove(token, color_sd, lineno, gp - genbuf, sd->braelist[c-'1'] - sd->braslist[c-'1']); /* colorization */
       if (!(gp = place(genbuf, gp, sd->braslist[c-'1'], sd->braelist[c-'1']))) return;
       DEBUG13( fprintf(stderr, " inserting:([%s] until [%s])\n", sd->braslist[c-'1'], sd->braelist[c-'1'] );) 
#else
       if (COLORED(token)){
           cd_add_remove(token, color_sd, lineno, gp - genbuf, braelist[c-'1'] - braslist[c-'1']); /* colorization */
       }
       if (!(gp = place(genbuf, gp, braslist[c-'1'], braelist[c-'1']))) return;
       DEBUG13( fprintf(stderr, " inserting:([%s] until [%s])\n", braslist[c-'1'], braelist[c-'1'] );) 
#endif
       continue;
   }

   if (c == NEWLINE)     /* AEGIS CASE */
      (*count_change)++;

   if (bs_flag && (c == 'n')){  /* UNIX case */
       c = NEWLINE;
       (*count_change)++;
   }
                                    
   *gp++ = c;
   if (COLORED(token)){ 
      cd_add_remove(token, color_sd, lineno, gp - genbuf, 1); /* colorization */
   }
   if (gp > &genbuf[MAX_LINE]){
       dm_error("Line too long, truncated", DM_ERROR_BEEP);
       genbuf[MAX_LINE+1] = '\0'; return;
   }
}

DEBUG13( fprintf(stderr, " phase-II:(%s)\n", (*gp='\0',genbuf));) 

lp = loc2;
cd_add_remove(token, color_sd, lineno, gp - genbuf -2, loc1-loc2); /* colorization */
loc2 = gp - genbuf + line;
while (*gp++ = *lp++)
   if (gp > &genbuf[MAX_LINE]){
       dm_error("Line too long, truncated", DM_ERROR_BEEP);
       genbuf[MAX_LINE+1] = '\0'; return;
   }

DEBUG13( fprintf(stderr, " phase-III:(%s)\n", genbuf);) 
strcpy(line, genbuf); /* send the new line back to the caller */

*size = gp - genbuf - 1;

}   /* subs() */

/***********************************************************************
*
*  place - swiped from ed.c: put the string from l1 to l2 at the end of sp
*
***********************************************************************/

static char *place(char *genbuf, char *sp, char *l1, char *l2)
{

while ((l1 < l2) && *l1) {   /* *l1 because of the \n that was on the end */
   *sp++ = *l1++;
   if (sp > &genbuf[MAX_LINE]){
       dm_error("Line too long, truncated", DM_ERROR_BEEP);
       genbuf[MAX_LINE+1] = '\0'; return(sp);
   }                        /* sp added at LINT request 6/222/95 */
}

return(sp);

} /* place() */

/***********************************************************************
*
*  process_inserted_newlines - Find all the newlines in a region and 
*                              split them. Called after a search that
*                              created new lines
*
***********************************************************************/

void process_inserted_newlines(DATA_TOKEN* token, int from_line, int to_line, int *row, int *column)
{

char *ptr;
char *line = (char *)1; /* initialize the line pointer */

int len;
int pline; /* short for (ptr - line) */
int count = to_line;   /* count is zero based! */
int column_orig = *column;

int greatest_impacted_line = 0;

DEBUG13( fprintf(stderr, " @PIN(%d, %d)[row:%d, col:%d]\n", from_line, to_line, *row, *column);) 

/* position_file_pointer(token, to_line + 1); */
DEBUG27(print_file(token););

while ((line != NULL) && (count >= from_line)){

       if (!greatest_impacted_line) greatest_impacted_line = *row;
       if ((count <= total_lines(token)) &&  /* added '=' 9/16/93 for last line del */
           delayed_delete(token, count, NOW, column, 1 /*JOIN*/) && /* changes made 9/13/93 to fix s/@n/@n@n/ */
           (count < greatest_impacted_line)) /* was <= 11/30/93 to fix s/@n/5/ cursor placement */  
              greatest_impacted_line--;

       line = get_line_by_num(token, count); /* shorten our copy of line 9/13/93 */
       /* line = prev_line(token); 2/9/94 */
       if (!line) line = ""; /* 2/9/94 */
       DEBUG13( fprintf(stderr, "GIL:%d(fl:%d); column: %d\n", greatest_impacted_line, from_line, *column);)

       if (sd->ends_in_newline){ 
            len = MAX(((int)strlen(line) -1), 0);
            if (line[len] == NEWLINE)
                line[len] = '\0';
       }

       while (ptr = strrchr(line, NEWLINE)){ 
          greatest_impacted_line++;
          DEBUG13( fprintf(stderr, " PIN(%s[%d],count=%d)\n", line, ptr - line, count);) 
          pline = ptr - line;
          if (count == *row){
             *column -= pline + 1;  /* new column */
             DEBUG13( fprintf(stderr, "PIN changed effected column to:%d\n", *column);)
          }
          if (((count+1) >= total_lines(token)) && (ptr > line) && (*(ptr-1) == NEWLINE)){ 
              greatest_impacted_line++;
              *ptr = '\0';
              pline--; /* takes care of problem where sd->join_lines is called from delayed_delete on last_line */
          }                                     
          split_line(token, count, pline +1, 1);  
          split_line(token, count, pline, 0);
          line = get_line_by_num(token, count); /* shorten our copy of line 9/13/93 */
       }

       count--;
}  /* while count */

if (sd->join && ((*column)>0))
   *column = *column - column_orig - 1;  /* 12/1/93 fixes cursor :222' s/@n/5/ */

*row = greatest_impacted_line;

line = get_line_by_num(token, *row);   /* kludge up count since it should not be */
if (line)                              /*   passed end of line */
  count = strlen(line);                /* */
else 
  count = *column;
if (*column > count) *column = count;  /* 8/11/94 */

DEBUG27(print_file(token););

} /* place() */

/***********************************************************************
*
*  newline_step - Special case where the search has new lines in it
*
*                 The algorythm is to inch forward line-by-line 
*                 and match lines to \nexpression\n pairs
*                 If I get an exact match, then replace the first line with the
*                 matched second line and mark the others for delete
*
*                 dflag - means this is a substitute, so effect changes
*
*  1) Could be optomized by not having newline_bracket called in every loop 
*
***********************************************************************/

#define ESTAR   1
#define ECHR    4    /* these are my own, but should map into the same in regexp.h */
#define ECCL	 12
#define ECXCL	 16
#define EDOLLAR 20
#define ENULL   22

static int newline_step(DATA_TOKEN *token, char *buf, char *expression, int line, int *count_change, char *dflag, int end_line, int end_col)
{

char *hptr;   /* head, tail, and end pointer for the pattern substring being matched */
char *tptr;
char *eptr;

char *bptr;   /* buf ptr and end_of_buf pointer */
char *ebptr;

char *next;     /* points to next line to be checked */

char expr[EBUF_MAX];

char my_buf[MAX_LINE+2];
char new_line[MAX_LINE+2];

int i;
int rc = 0;
int dummy = 0;
int expr_len;
int orig_line;
long first_loc1;     /* RES 1/5/2016, change int to long for CentOS 7 */

int count = 0;       /* newline_step (DDE flag) */
long loc_sum = 0;    /* RES 1/5/2016, change int to long for CentOS 7 */
int last_pass = 0;
int first_pass = 1;
int nstar=0;  /* 0 added LINT 6/22/94 */ /* current expr is @n* 2/18/94 */
/* int nstar_first_pass;  fix 1132 */

DEBUG13( fprintf(stderr, " @NL_step(buf:%s, expr:%s, line: %d)\n", buf, expression, line);)

position_file_pointer(token, line);

if (!VALID_LINE(token)){
    DEBUG13(next = next_line(token);  /* 9/15/93 */
    fprintf(stderr, "Line not valid![%d]:%s\n", line, next);)
    return(0);
}

next = next_line(token);  /* 9/15/93 */

new_line[0] = '\0';  

strcpy(my_buf, buf);  /* save off the input we need to damage */
orig_line = line;

hptr = expression; 
eptr = eget(hptr, ENULL);

if (sd->dont_match_line_zero && !line){
        return(0);    
}
                   /* V added #236239                      V */
while (((hptr < eptr) || ((hptr == eptr) && sd->ends_in_newline)) && hptr && !last_pass){
  
    if (line > end_line) 
        if (rc && nstar)    /* #34582 added to fixe 1,$s!@n@n@n*!@n! on file that has @n@n@n last lines */
               break;       /* its actually ok */
            else
               return(0); /* can't look too far! 9/16/92 */

    if (!rc){
        nstar = 0; /* reset on failed push */
        /* nstar_first_pass = 1;  fix 1132 */
    }

    strcat(new_line, next);
    
    if (sd->ends_in_newline && (hptr == eptr)) break; /* #236239 3/14/94 fixes 222' -> s/'@n// */

    bptr = my_buf;           /* get the begin/end points */
    ebptr = strend(my_buf);
  
    if ((rc < 0) && !nstar){         /* nbstep() 2/28/94 */
          tptr = hptr;
          hptr = eget(hptr, ECCL) + 16 + 1;
          if (hptr == (char *)17) hptr = eget(tptr, ECCL | 01) + 32 + 1;
          if (hptr == (char *)17) hptr = eget(tptr, ECXCL) + 32 + 1;
          if (hptr == (char *)33) hptr = eget(tptr, ECXCL | 01) + 32 + 1;
          if (hptr == (char *)33) hptr = eget(tptr, ENULL);  /* error case */
    }

    tptr = eget(hptr, EDOLLAR);   /* get the first/next pattern */
    if (!tptr){
        tptr = eget(hptr, ENULL);
        last_pass++;
    }else{
        if ((*(tptr+1) == ENULL) && !sd->ends_in_newline) /* added !ends 1130/93 fixes :2222' 22,25s/'@n/5/ */
            last_pass++;
    }
  
    expr_len = tptr - hptr +2; /* 1 for'$' and 1 for ECHR */
    memcpy(expr, hptr, expr_len);

    if (expr[expr_len-3] == (ECHR | ESTAR)){   /* @n* 2/18/94 */
        expr[expr_len-3] = ECHR;  
        nstar++;
    }
     
    if ((expr[expr_len -2] == EDOLLAR) && (expr[expr_len-3] == ECHR)){
         expr[expr_len -3] = EDOLLAR;  /*  zap the \4 compile added */
         expr_len -= 2;
    }
    expr[expr_len] = ENULL;
    expr[expr_len +1] = '\0';     /* makes me feel better :-) (needed in nbstep()) */

    DEBUG13(print_pat_expr("???", expr);)
    if (!first_pass) circf++; /* tells advance in regexp.h the expr is anchored */
#ifdef NEWLINE_BRACKET
    if (newline_bracket(expr, &nstar)){
        if (nstar){
           compile_error(52);
           return(0);
        }
        last_pass = 0;
        rc = nbstep(bptr, expr);
    }else
#endif
        rc = step(bptr, expr); 
    if (!first_pass) circf = 0;

    if (rc && nstar)  /* @n* 2/18/94 */
         last_pass = 0;  /* reset last pass if we are looping stationarily */

    if ((end_line == line) && ((loc2-1) > (bptr + end_col)))
        return(0); /* can't look too far! 9/16/93 */
  
    if (!rc && (!nstar /* || nstar_first_pass fix 1132 */)){    /* no match */   /* && @n* */
        DEBUG13( fprintf(stderr, "Failed line: '%s'\n", next);)
        return(0);
    }else
        DEBUG13( fprintf(stderr, "Successful line: '%s'\n", next);)

   /* if (nstar) nstar_first_pass = 0;  fix 1132 */

    if (first_pass){
        first_loc1 = loc1 - bptr;
        loc_sum = loc2 - bptr;   /* ignore unmatched front */
    }

    if (nstar && (line == end_line)) /* #34582 */
        last_pass++;
      
    if (last_pass)
        loc_sum += loc2 - bptr;    /* ignore unused end */
  
    if (last_pass && first_pass)
        loc_sum = loc2 - bptr;     /* offset to loc2 */
  
    if (!last_pass && !first_pass)
        loc_sum += ebptr - bptr;   /* advance */
  
    DEBUG13( fprintf(stderr, "loc_sum: %d\n", loc_sum);)
  
    count++;
    line++; 
    while (!VALID_LINE(token) && next){
       count++;
       line++; 
       next = next_line(token);
    }
    next = next_line(token);
  
    if (last_pass && !next){  /* 9/20/93 fixes 1,$s/a@n/@n/ does not match last line */
        next = ""; 
        count--;
        line--;
    }
  
    if (!next && ((tptr + 1) < eptr))
        return(0);   /* no next line */
  
    if (next && ((int)(strlen(new_line) + strlen(next)) > MAX_LINE)){   /* will it fit */
        dm_error("Substitute exceeds regular expression capacity!", DM_ERROR_BEEP);
        return(0);
    }
  
    if (next) strcpy(my_buf, next);
  
    if ((rc>0) && !nstar)  /* @n* 2/18/94  rc=-1 for [@n*] */
        hptr = tptr +1; 
  
    if (first_pass)
        first_pass = 0;
  
} /* while expression */ 

loc1 = (char *)first_loc1; /* loc1 and loc2 become relative addresses in this case */
loc2 = (char *)loc_sum;

if (dflag){
    if (sd->ends_in_newline) count++; /* #236239 */
    put_line_by_num(token, orig_line++, new_line, OVERWRITE);
    for (i = 1; i < count; i++)   /* mark the other lines for deletion */
         delayed_delete(token, orig_line++, LATER, &dummy, 0 /*NO JOIN*/);
    *count_change = count;
}

DEBUG13( fprintf(stderr, "NLstep might change %d lines\n", count);)

return(1);

}  /* newline_step() */

#ifdef NEWLINE_BRACKET
/***********************************************************************
*
*  nbstep - special step() for sd->newlines inside [data@n]
*           {rc = 0 failed, rc = + [] succeeded, rc = - @n succeeded}
***********************************************************************/

static int nbstep(char *buf, char *expr)
{
char *b;
int offset;
/* int x; 1=CXCL; 0=CCL */
int rc;
/* char *eptr; */
char nexpr[EBUF_MAX];

  b = eget(expr, ECCL);
  if (!b) b = eget(expr, ECCL | 01);
  if (!b){
      b = eget(expr, ECXCL);
      if (!b) b = eget(expr, ECXCL | 01);
     /* x = 1; */
  }

  if (!b) return(step(buf, expr));       /* try with x[data]y */

  rc = step(buf, expr);       /* try with x[data]y */

  offset = b - expr;
  if (!rc){
      memcpy(nexpr, expr, offset);
      nexpr[offset] = EDOLLAR;
      nexpr[offset+1] = ENULL;
      nexpr[offset+2] = '\0';
/*    eptr = eget(expr + offset, ENULL);
      memcpy(nexpr + offset, b + (x?32:16) +1, eptr - (b + (x?32:16))); */
      rc -= step(buf, nexpr);  /*  try with x@ny  */
  }

  return(rc);

}  /* nbstep() */

/***********************************************************************
*
*  newline_bracket - determine if there is a newline inside a bracket
*
***********************************************************************/

static int newline_bracket(char *expr, int *nstar)
{   
char *b;
char newline = NEWLINE;

  b = eget(expr, ECCL);
  if (!b) b = eget(expr, ECCL | 01);
  if (!b) b = eget(expr, ECXCL);
  if (!b) b = eget(expr, ECXCL | 01);

  if (!b) return(0);
 /*  if ((*b & 01)) *nstar = 1; ( fixes /[ @t]*@n/ reports error(52)) */ 
  if (*b == (ECXCL | 01)) *nstar = 1;

  return( ((*b == ECXCL) || (*b == (ECXCL | 01))) &&  /* /[@n]/ FIXED  this line commented in so /%[ @t]@n] works */
          (b[(newline >> 3) +1] & bittab[newline & 07]));
                              /* don't ask :-) , look at PLACE()/regexp.h */
   /*       (b[(NEWLINE|0200) >> 3] & bittab[(NEWLINE|0200) & 07]));   */
} /* newline_bracket() */
#endif

/***********************************************************************
*
*  eget - safe strchr for expressions
*
***********************************************************************/

static char *eget(char *expr, char token)
{   

while ((*expr != ENULL) && (*expr != token)) expr++;

if ((token != ENULL) && (*expr == ENULL))
    return((char *)0);

return(expr);

} /* eget() */

#ifdef DebuG
/***********************************************************************
*
*  print_pat_expr - Diagnostic printf of a 
*
***********************************************************************/

static void print_pat_expr(char *pat, char *expr)
{

fprintf(stderr, " Compile(%s)->'", pat); 

while (*expr != ENULL){
    if (*expr == ENULL)
        *(expr +1) = '\0'; /* CCEOF in regexp.h prints better with a NULL */
    fprintf(stderr, ((*expr <= ' ') || (*expr >= '~')) ? "\\%d" : "%c", *(expr++)); 
}

fprintf(stderr, "'\n");         

} /* print_pat_expr() */
#endif

/***********************************************************************
*
*  newline_precomp - takes a pattern and changes patterns bounded by @n's to:  
*                    ^pattern$  for newline_step
***********************************************************************/

static void newline_precomp(char *pat)
{

int idx;

int first_pass = 1;
int last_pass  = 0;

char *hptr, *tptr, *eptr;

char expr[EBUF_MAX];

DEBUG13( fprintf(stderr, " @newline_precomp(%s)->", pat);) 

strcpy(expr, pat);
pat[0] = '\0';

hptr = expr;
eptr = strend(hptr);

while ((hptr < eptr) && hptr && !last_pass){

      tptr = strchr(hptr, NEWLINE);   /* get the first pattern */
      if (!tptr){
          tptr = strend(hptr);
          last_pass++;
      }else{
          *tptr = '\0';
      }

      strcat(pat, hptr);

      if (!last_pass && (*tptr != '$')){
          idx = strlen(pat);
          pat[idx] = EDOLLAR;  /* add a \20 to the end token for '$' */
          pat[idx+1] = '\0';
      }

      if (first_pass)
          first_pass = 0;

      hptr = tptr +1;
}

#ifdef NEWLINE_BRACKET    
pat[strlen(pat)+1] = ENULL; /* safty net for eget() below 10/19/91 -> /%[ @t]@n]/ and /[@n]/ */
hptr = pat; /* post process [@n] out */
while (hptr = eget(hptr, NEWLINE | 0200))
        *hptr = NEWLINE;
#endif

DEBUG13( fprintf(stderr, "'%s'\n", pat);) 

} /* newline_precomp() */

/*#define  UNIX_NEWLINE /* take out if problems occur */
#ifdef UNIX_NEWLINE
/***********************************************************************
*
*  newlines_filter - given a string the following changes are made:
*
*                    change '\n' to a x10
                     change [\n] to error message
*                    if the pattern ends in a dollar return 1
***********************************************************************/

static int newlines_filter(char *pat)
{

int escape = 0;
char *ptr;

ptr = pat;

while (*pat){

  switch(*pat){
  
  case '\\': 
             escape = !escape;
             if (escape){
                 *ptr++ = *pat++;
                 continue;
             }
             break;

  case '[':
             while (*pat && (*pat != ']')){
                  if ((*pat == '\\') && (*pat == 'n')){ 
                  /*    ptr--;
                     *pat = NEWLINE;     V */
                      compile_error(51);return(1); /* temp patch */
                  }
                  *ptr++ = *pat++;
             }
             break;

  case 'n':
             if (escape){ 
                ptr--;
                *pat = NEWLINE;
             }
             break;

  case '$':  if (!*(ptr+1) && !escape){
                    *ptr = *pat;
                    *ptr = NULL;
                    return(1);
             }
             break;

} /* switch */

escape = 0;
*ptr++ = *pat++;

} /* while */

return(0);
 
}
#endif
