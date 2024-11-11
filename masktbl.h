#ifndef _MASKTBL_H_INCLUDED
#define _MASKTBL_H_INCLUDED

/*static char *masktbl_h_sccsid = "%Z% %M% %I% - %G% %U% ";*/

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
*  Include file masktbl.h
*
*  Tables used to find the first and last bits turned on in
*  a word.  There are 4 tables in this file.  Two are for
*  4 byte ints and two for 8 byte ints.  Two are for finding
*  the first bit on in the word, two are for the last.
*  
***************************************************************/

typedef struct {
   unsigned long  mask;
   int            next[2];  /* False value then True value */
} MASK_TBL;


#if WORD_BIT == 32

/***************************************************************
*  find the lowest order bit on in the word
*  this is the one with the greatest bit number
*  The sign bit is bit 0
***************************************************************/

static MASK_TBL   last_mask_table[] = {

0x0000ffff,   /*  0  */             1,   2,

0x00ff0000,   /*  1 ,  0 False */   3,   4,
0x000000ff,   /*  2 ,  0 True  */   5,   6,

0x0f000000,   /*  3 ,  1 False */   7,   8,
0x000f0000,   /*  4 ,  1 True  */   9,  10,
0x00000f00,   /*  5 ,  2 False */  11,  12,
0x0000000f,   /*  6 ,  2 True  */  13,  14, 

0x30000000,   /*  7 ,  3 False */  15,  16,
0x03000000,   /*  8 ,  3 True  */  17,  18,
0x00300000,   /*  9 ,  4 False */  19,  20, 
0x00030000,   /* 10 ,  4 True  */  21,  22,
0x00003000,   /* 11 ,  5 False */  23,  24,
0x00000300,   /* 12 ,  5 True  */  25,  26,
0x00000030,   /* 13 ,  6 False */  27,  28,
0x00000003,   /* 14 ,  6 True  */  29,  30,

0x40000000,   /* 15 ,  7 False */   0,   1,
0x10000000,   /* 16 ,  7 True  */   2,   3,
0x04000000,   /* 17 ,  8 False */   4,   5,
0x01000000,   /* 18 ,  8 True  */   6,   7,
0x00400000,   /* 19 ,  9 False */   8,   9,
0x00100000,   /* 20 ,  9 True  */  10,  11,
0x00040000,   /* 21 , 10 False */  12,  13,
0x00010000,   /* 22 , 10 True  */  14,  15,
0x00004000,   /* 23 , 11 False */  16,  17,
0x00001000,   /* 24 , 11 True  */  18,  19,
0x00000400,   /* 25 , 12 False */  20,  21,
0x00000100,   /* 26 , 12 True  */  22,  23,
0x00000040,   /* 27 , 13 False */  24,  25,
0x00000010,   /* 28 , 13 True  */  26,  27,
0x00000004,   /* 29 , 14 False */  28,  29,
0x00000001,   /* 30 , 14 True  */  30,  31
};


/***************************************************************
*  find the highest order bit on in the word
*  this is the one with the lowest bit number
*  The sign bit is bit 0
***************************************************************/

static MASK_TBL   first_mask_table[] = {

(unsigned long)0xffff0000,   /*  0  */             1,   2,

(unsigned long)0x0000ff00,   /*  1 ,  0 False */   3,   4,
(unsigned long)0xff000000,   /*  2 ,  0 True  */   5,   6,

(unsigned long)0x000000f0,   /*  3 ,  1 False */   7,   8,
(unsigned long)0x0000f000,   /*  4 ,  1 True  */   9,  10,
(unsigned long)0x00f00000,   /*  5 ,  2 False */  11,  12,
(unsigned long)0xf0000000,   /*  6 ,  2 True  */  13,  14, 

(unsigned long)0x0000000C,   /*  7 ,  3 False */  15,  16,
(unsigned long)0x000000C0,   /*  8 ,  3 True  */  17,  18,
(unsigned long)0x00000C00,   /*  9 ,  4 False */  19,  20, 
(unsigned long)0x0000C000,   /* 10 ,  4 True  */  21,  22,
(unsigned long)0x000C0000,   /* 11 ,  5 False */  23,  24,
(unsigned long)0x00C00000,   /* 12 ,  5 True  */  25,  26,
(unsigned long)0x0C000000,   /* 13 ,  6 False */  27,  28,
(unsigned long)0xC0000000,   /* 14 ,  6 True  */  29,  30,

(unsigned long)0x00000002,   /* 15 ,  7 False */  31,  30,
(unsigned long)0x00000008,   /* 16 ,  7 True  */  29,  28,
(unsigned long)0x00000020,   /* 17 ,  8 False */  27,  26,
(unsigned long)0x00000080,   /* 18 ,  8 True  */  25,  24,
(unsigned long)0x00000200,   /* 19 ,  9 False */  23,  22,
(unsigned long)0x00000800,   /* 20 ,  9 True  */  21,  20,
(unsigned long)0x00002000,   /* 21 , 10 False */  19,  18,
(unsigned long)0x00008000,   /* 22 , 10 True  */  17,  16,
(unsigned long)0x00020000,   /* 23 , 11 False */  15,  14,
(unsigned long)0x00080000,   /* 24 , 11 True  */  13,  12,
(unsigned long)0x00200000,   /* 25 , 12 False */  11,  10,
(unsigned long)0x00800000,   /* 26 , 12 True  */   9,   8,
(unsigned long)0x02000000,   /* 27 , 13 False */   7,   6,
(unsigned long)0x08000000,   /* 28 , 13 True  */   5,   4,
(unsigned long)0x20000000,   /* 29 , 14 False */   3,   2,
(unsigned long)0x80000000,   /* 30 , 14 True  */   1,   0
};

#elif WORD_BIT == 64

/***************************************************************
*  find the lowest order bit on in the word
*  this is the one with the greatest bit number
*  The sign bit is bit 0
***************************************************************/

static MASK_TBL   last_mask_table[] = {

(unsigned long)0x00000000ffffffff,   /*  0  */             1,   2,

(unsigned long)0x0000ffff00000000,   /*  1 ,  0 False */   3,   4,
(unsigned long)0x000000000000ffff,   /*  2 ,  0 True  */   5,   6,

(unsigned long)0x00ff000000000000,   /*  3 ,  1 False */   7,   8,
(unsigned long)0x000000ff00000000,   /*  4 ,  1 True  */   9,  10,
(unsigned long)0x0000000000ff0000,   /*  5 ,  2 False */  11,  12,
(unsigned long)0x00000000000000ff,   /*  6 ,  2 True  */  13,  14, 

(unsigned long)0x0f00000000000000,   /*  7 ,  3 False */  15,  16,
(unsigned long)0x000f000000000000,   /*  8 ,  3 True  */  17,  18,
(unsigned long)0x00000f0000000000,   /*  9 ,  4 False */  19,  20, 
(unsigned long)0x0000000f00000000,   /* 10 ,  4 True  */  21,  22,
(unsigned long)0x000000000f000000,   /* 11 ,  5 False */  23,  24,
(unsigned long)0x00000000000f0000,   /* 12 ,  5 True  */  25,  26,
(unsigned long)0x0000000000000f00,   /* 13 ,  6 False */  27,  28,
(unsigned long)0x000000000000000f,   /* 14 ,  6 True  */  29,  30,

(unsigned long)0x3000000000000000,   /* 15 ,  7 False */  31,  32,
(unsigned long)0x0300000000000000,   /* 16 ,  7 True  */  33,  34,
(unsigned long)0x0030000000000000,   /* 17 ,  8 False */  35,  36,
(unsigned long)0x0003000000000000,   /* 18 ,  8 True  */  37,  38,
(unsigned long)0x0000300000000000,   /* 19 ,  9 False */  39,  40,
(unsigned long)0x0000030000000000,   /* 20 ,  9 True  */  41,  42,
(unsigned long)0x0000003000000000,   /* 21 , 10 False */  43,  44,
(unsigned long)0x0000000300000000,   /* 22 , 10 True  */  45,  46,
(unsigned long)0x0000000030000000,   /* 23 , 11 False */  47,  48,
(unsigned long)0x0000000003000000,   /* 24 , 11 True  */  49,  50,
(unsigned long)0x0000000000300000,   /* 25 , 12 False */  51,  52,
(unsigned long)0x0000000000030000,   /* 26 , 12 True  */  53,  54,
(unsigned long)0x0000000000003000,   /* 27 , 13 False */  55,  56,
(unsigned long)0x0000000000000300,   /* 28 , 13 True  */  57,  58,
(unsigned long)0x0000000000000030,   /* 29 , 14 False */  59,  60,
(unsigned long)0x0000000000000003,   /* 30 , 14 True  */  61,  62,

(unsigned long)0x4000000000000000,   /* 31 , 15 False */   0,   1,
(unsigned long)0x1000000000000000,   /* 32 , 15 True  */   2,   2,
(unsigned long)0x0400000000000000,   /* 33 , 16 False */   4,   5,
(unsigned long)0x0100000000000000,   /* 34 , 16 True  */   6,   7,
(unsigned long)0x0040000000000000,   /* 35 , 17 False */   8,   9,
(unsigned long)0x0010000000000000,   /* 36 , 17 True  */  10,  11,
(unsigned long)0x0004000000000000,   /* 37 , 18 False */  12,  13,
(unsigned long)0x0001000000000000,   /* 38 , 18 True  */  14,  15,
(unsigned long)0x0000400000000000,   /* 39 , 19 False */  16,  17,
(unsigned long)0x0000100000000000,   /* 40 , 19 True  */  18,  19,
(unsigned long)0x0000040000000000,   /* 41 , 20 False */  20,  21,
(unsigned long)0x0000010000000000,   /* 42 , 20 True  */  22,  23,
(unsigned long)0x0000004000000000,   /* 43 , 21 False */  24,  25,
(unsigned long)0x0000001000000000,   /* 44 , 21 True  */  26,  27,
(unsigned long)0x0000000400000000,   /* 45 , 22 False */  28,  29,
(unsigned long)0x0000000100000000,   /* 46 , 22 True  */  30,  31,
(unsigned long)0x0000000040000000,   /* 47 , 23 False */  32,  33,
(unsigned long)0x0000000010000000,   /* 48 , 23 True  */  34,  35,
(unsigned long)0x0000000004000000,   /* 49 , 24 False */  36,  37,
(unsigned long)0x0000000001000000,   /* 50 , 24 True  */  38,  39,
(unsigned long)0x0000000000400000,   /* 51 , 25 False */  40,  41,
(unsigned long)0x0000000000100000,   /* 52 , 25 True  */  42,  43,
(unsigned long)0x0000000000040000,   /* 53 , 26 False */  44,  45,
(unsigned long)0x0000000000010000,   /* 54 , 26 True  */  46,  47,
(unsigned long)0x0000000000004000,   /* 55 , 27 False */  48,  49,
(unsigned long)0x0000000000001000,   /* 56 , 27 True  */  50,  51,
(unsigned long)0x0000000000000400,   /* 57 , 28 False */  52,  53,
(unsigned long)0x0000000000000100,   /* 58 , 28 True  */  54,  55,
(unsigned long)0x0000000000000040,   /* 59 , 29 False */  56,  57,
(unsigned long)0x0000000000000010,   /* 60 , 29 True  */  58,  59,
(unsigned long)0x0000000000000004,   /* 61 , 30 False */  60,  61,
(unsigned long)0x0000000000000001,   /* 62 , 30 True  */  62,  63,

};

/***************************************************************
*  find the highest order bit on in the word
*  this is the one with the lowest bit number
*  The sign bit is bit 0
***************************************************************/

static MASK_TBL   first_mask_table[] = {

(unsigned long)0xffffffff00000000,   /*  0  */             1,   2,

(unsigned long)0x00000000ffff0000,   /*  1 ,  0 False */   3,   4,
(unsigned long)0xffff000000000000,   /*  2 ,  0 True  */   5,   6,

(unsigned long)0x000000000000ff00,   /*  3 ,  1 False */   7,   8,
(unsigned long)0x00000000ff000000,   /*  4 ,  1 True  */   9,  10,
(unsigned long)0x0000ff0000000000,   /*  5 ,  2 False */  11,  12,
(unsigned long)0xff00000000000000,   /*  6 ,  2 True  */  13,  14, 

(unsigned long)0x00000000000000f0,   /*  7 ,  3 False */  15,  16,
(unsigned long)0x000000000000f000,   /*  8 ,  3 True  */  17,  18,
(unsigned long)0x0000000000f00000,   /*  9 ,  4 False */  19,  20, 
(unsigned long)0x00000000f0000000,   /* 10 ,  4 True  */  21,  22,
(unsigned long)0x000000f000000000,   /* 11 ,  5 False */  23,  24,
(unsigned long)0x0000f00000000000,   /* 12 ,  5 True  */  25,  26,
(unsigned long)0x00f0000000000000,   /* 13 ,  6 False */  27,  28,
(unsigned long)0xf000000000000000,   /* 14 ,  6 True  */  29,  30,

(unsigned long)0x000000000000000C,   /* 15 ,  7 False */   0,   0,
(unsigned long)0x00000000000000C0,   /* 16 ,  7 True  */   0,   0,
(unsigned long)0x0000000000000C00,   /* 17 ,  8 False */   0,   0,
(unsigned long)0x000000000000C000,   /* 18 ,  8 True  */   0,   0,
(unsigned long)0x00000000000C0000,   /* 19 ,  9 False */   0,   0,
(unsigned long)0x0000000000C00000,   /* 20 ,  9 True  */   0,   0,
(unsigned long)0x000000000C000000,   /* 21 , 10 False */   0,   0,
(unsigned long)0x00000000C0000000,   /* 22 , 10 True  */   0,   0,
(unsigned long)0x0000000C00000000,   /* 23 , 11 False */   0,   0,
(unsigned long)0x000000C000000000,   /* 24 , 11 True  */   0,   0,
(unsigned long)0x00000C0000000000,   /* 25 , 12 False */   0,   0,
(unsigned long)0x0000C00000000000,   /* 26 , 12 True  */   0,   0,
(unsigned long)0x000C000000000000,   /* 27 , 13 False */   0,   0,
(unsigned long)0x00C0000000000000,   /* 28 , 13 True  */   0,   0,
(unsigned long)0x0C00000000000000,   /* 29 , 14 False */   0,   0,
(unsigned long)0xC000000000000000,   /* 30 , 14 True  */   0,   0,

(unsigned long)0x0000000000000002,   /* 31 , 15 False */   0,   1,
(unsigned long)0x0000000000000008,   /* 32 , 15 True  */   2,   2,
(unsigned long)0x0000000000000020,   /* 33 , 16 False */   4,   5,
(unsigned long)0x0000000000000080,   /* 34 , 16 True  */   6,   7,
(unsigned long)0x0000000000000200,   /* 35 , 17 False */   8,   9,
(unsigned long)0x0000000000000800,   /* 36 , 17 True  */  10,  11,
(unsigned long)0x0000000000002000,   /* 37 , 18 False */  12,  13,
(unsigned long)0x0000000000008000,   /* 38 , 18 True  */  14,  15,
(unsigned long)0x0000000000020000,   /* 39 , 19 False */  16,  17,
(unsigned long)0x0000000000080000,   /* 40 , 19 True  */  18,  19,
(unsigned long)0x0000000000200000,   /* 41 , 20 False */  20,  21,
(unsigned long)0x0000000000800000,   /* 42 , 20 True  */  22,  23,
(unsigned long)0x0000000002000000,   /* 43 , 21 False */  24,  25,
(unsigned long)0x0000000008000000,   /* 44 , 21 True  */  26,  27,
(unsigned long)0x0000000020000000,   /* 45 , 22 False */  28,  29,
(unsigned long)0x0000000080000000,   /* 46 , 22 True  */  30,  31,
(unsigned long)0x0000000200000000,   /* 47 , 23 False */  32,  33,
(unsigned long)0x0000000800000000,   /* 48 , 23 True  */  34,  35,
(unsigned long)0x0000002000000000,   /* 49 , 24 False */  36,  37,
(unsigned long)0x0000008000000000,   /* 50 , 24 True  */  38,  39,
(unsigned long)0x0000020000000000,   /* 51 , 25 False */  40,  41,
(unsigned long)0x0000080000000000,   /* 52 , 25 True  */  42,  43,
(unsigned long)0x0000200000000000,   /* 53 , 26 False */  44,  45,
(unsigned long)0x0000800000000000,   /* 54 , 26 True  */  46,  47,
(unsigned long)0x0002000000000000,   /* 55 , 27 False */  48,  49,
(unsigned long)0x0008000000000000,   /* 56 , 27 True  */  50,  51,
(unsigned long)0x0020000000000000,   /* 57 , 28 False */  52,  53,
(unsigned long)0x0080000000000000,   /* 58 , 28 True  */  54,  55,
(unsigned long)0x0200000000000000,   /* 59 , 29 False */  56,  57,
(unsigned long)0x0800000000000000,   /* 60 , 29 True  */  58,  59,
(unsigned long)0x2000000000000000,   /* 61 , 30 False */  60,  61,
(unsigned long)0x8000000000000000,   /* 62 , 30 True  */  62,  63,

};


#else

#error Unsupported word bit size, WORD_BIT

#endif

#endif


