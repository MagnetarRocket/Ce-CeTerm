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

char *
compile(instring, ep, endbuf, seof)
int seof;                              /* added by Plylerk 8/31/92 */
register char *ep;
char *instring, *endbuf;
{
	INIT	/* Dependent declarations and initializations */
	register c;
	register eof = seof;
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
	register c;


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

advance(lp, ep)
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
			sd->braslist[*ep++] = lp;
			continue;
	
		case CKET:
           { int blen;
           blen = lp - (sd->braslist[*ep]);
           if (sd->brablist[*ep]) free(sd->brablist[*ep]);
           sd->brablist[*ep] = (char *)malloc(blen+1);
           strncpy(sd->brablist[*ep], sd->braslist[*ep], blen);
           *(sd->brablist[*ep] + blen +1) = NULL;
		 	sd->braslist[*ep] = sd->brablist[*ep];
		 	sd->braelist[*ep] = sd->braslist[*ep] + blen -1; /* fixed 10/15/97 for s/{?*}/xxx"@1"/ */
           *ep++;
           }
		/*	sd->braelist[*ep++] = lp;  */
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
			bbeg = sd->braslist[*ep];
			ct = sd->braelist[*ep++] - bbeg;
	
			if(ecmp(bbeg, lp, ct)) {
				lp += ct;
				continue;
			}
			return(0);
	
		case CBACK | STAR:
			bbeg = sd->braslist[*ep];
			ct = sd->braelist[*ep++] - bbeg;
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
	sd->low = *str++ & 0377;
	sd->size = (*str == 255)? 20000: (*str &0377) - sd->low;
}

