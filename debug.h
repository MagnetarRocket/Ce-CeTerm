#ifndef _DEBUG_H_INCLUDED
#define _DEBUG_H_INCLUDED

/* static char *debug_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  debug.h
*
*  Defintion of the DEBUG macro.  This macro will conditionally
*  compile in debugging statements or exclude them.
*  
***************************************************************/


#ifdef _MAIN_
unsigned int         debug = 0;
#else
extern unsigned int  debug;
#endif

#ifdef __STDC__
void Debug(char *env_name);
#else
void Debug();
#endif


/***************************************************************
*  
*  Masks for all the bits numbered 1 though 32
*  
***************************************************************/

#define  D_BIT1     0x00000001
#define  D_BIT2     0x00000002
#define  D_BIT3     0x00000004
#define  D_BIT4     0x00000008
#define  D_BIT5     0x00000010
#define  D_BIT6     0x00000020
#define  D_BIT7     0x00000040
#define  D_BIT8     0x00000080
#define  D_BIT9     0x00000100
#define  D_BIT10    0x00000200
#define  D_BIT11    0x00000400
#define  D_BIT12    0x00000800
#define  D_BIT13    0x00001000
#define  D_BIT14    0x00002000
#define  D_BIT15    0x00004000
#define  D_BIT16    0x00008000
#define  D_BIT17    0x00010000
#define  D_BIT18    0x00020000
#define  D_BIT19    0x00040000
#define  D_BIT20    0x00080000
#define  D_BIT21    0x00100000
#define  D_BIT22    0x00200000
#define  D_BIT23    0x00400000
#define  D_BIT24    0x00800000
#define  D_BIT25    0x01000000
#define  D_BIT26    0x02000000
#define  D_BIT27    0x04000000
#define  D_BIT28    0x08000000
#define  D_BIT29    0x10000000
#define  D_BIT30    0x20000000
#define  D_BIT31    0x40000000
#define  D_BIT32    0x80000000

/***************************************************************
*  
*  If DebuG (compile flag -DDebuG ) is not on, compile
*  a debug call for each bit if DebuG is defined.
*  
***************************************************************/

#ifdef  DebuG

#define DEBUG0(x)   {x}
#define DEBUG(x)    {if (debug) {x}}
#define DEBUG1(x)   {if (debug & D_BIT1 ) {x}}
#define DEBUG2(x)   {if (debug & D_BIT2 ) {x}}
#define DEBUG3(x)   {if (debug & D_BIT3 ) {x}}
#define DEBUG4(x)   {if (debug & D_BIT4 ) {x}}
#define DEBUG5(x)   {if (debug & D_BIT5 ) {x}}
#define DEBUG6(x)   {if (debug & D_BIT6 ) {x}}
#define DEBUG7(x)   {if (debug & D_BIT7 ) {x}}
#define DEBUG8(x)   {if (debug & D_BIT8 ) {x}}
#define DEBUG9(x)   {if (debug & D_BIT9 ) {x}}
#define DEBUG10(x)  {if (debug & D_BIT10) {x}}
#define DEBUG11(x)  {if (debug & D_BIT11) {x}}
#define DEBUG12(x)  {if (debug & D_BIT12) {x}}
#define DEBUG13(x)  {if (debug & D_BIT13) {x}}
#define DEBUG14(x)  {if (debug & D_BIT14) {x}}
#define DEBUG15(x)  {if (debug & D_BIT15) {x}}
#define DEBUG16(x)  {if (debug & D_BIT16) {x}}
#define DEBUG17(x)  {if (debug & D_BIT17) {x}}
#define DEBUG18(x)  {if (debug & D_BIT18) {x}}
#define DEBUG19(x)  {if (debug & D_BIT19) {x}}
#define DEBUG20(x)  {if (debug & D_BIT20) {x}}
#define DEBUG21(x)  {if (debug & D_BIT21) {x}}
#define DEBUG22(x)  {if (debug & D_BIT22) {x}}
#define DEBUG23(x)  {if (debug & D_BIT23) {x}}
#define DEBUG24(x)  {if (debug & D_BIT24) {x}}
#define DEBUG25(x)  {if (debug & D_BIT25) {x}}
#define DEBUG26(x)  {if (debug & D_BIT26) {x}}
#define DEBUG27(x)  {if (debug & D_BIT27) {x}}
#define DEBUG28(x)  {if (debug & D_BIT28) {x}}
#define DEBUG29(x)  {if (debug & D_BIT29) {x}}
#define DEBUG30(x)  {if (debug & D_BIT30) {x}}
#define DEBUG31(x)  {if (debug & D_BIT31) {x}}
#define DEBUG32(x)  {if (debug & D_BIT32) {x}}

#else

/***************************************************************
*  
*  If DebuG (compile flag -DDebuG ) is not on, leave out the
*  debug code.
*  
***************************************************************/

#define DEBUG(x)    {}
#define DEBUG0(x)   {}
#define DEBUG1(x)   {}
#define DEBUG2(x)   {}
#define DEBUG3(x)   {}
#define DEBUG4(x)   {}
#define DEBUG5(x)   {}
#define DEBUG6(x)   {}
#define DEBUG7(x)   {}
#define DEBUG8(x)   {}
#define DEBUG9(x)   {}
#define DEBUG10(x)  {}
#define DEBUG11(x)  {}
#define DEBUG12(x)  {}
#define DEBUG13(x)  {}
#define DEBUG14(x)  {}
#define DEBUG15(x)  {}
#define DEBUG16(x)  {}
#define DEBUG17(x)  {}
#define DEBUG18(x)  {}
#define DEBUG19(x)  {}
#define DEBUG20(x)  {}
#define DEBUG21(x)  {}
#define DEBUG22(x)  {}
#define DEBUG23(x)  {}
#define DEBUG24(x)  {}
#define DEBUG25(x)  {}
#define DEBUG26(x)  {}
#define DEBUG27(x)  {}
#define DEBUG28(x)  {}
#define DEBUG29(x)  {}
#define DEBUG30(x)  {}
#define DEBUG31(x)  {}
#define DEBUG32(x)  {}
#endif

#endif

