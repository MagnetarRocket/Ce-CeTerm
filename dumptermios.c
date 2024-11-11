#ifdef DebuG
#include <stdio.h>

#include "dumptermios.h"

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

void dump_termios(struct termios    *termios,
                  FILE              *stream)
{

fprintf(stream, "c_iflag = 0x%08x\n", termios->c_iflag);
/* c_iflag bits */
if (termios->c_iflag & IGNBRK) fprintf(stream, "IGNBRK,");
if (termios->c_iflag & BRKINT)  fprintf(stream, "BRKINT,"); 
if (termios->c_iflag & IGNPAR)  fprintf(stream, "IGNPAR,"); 
if (termios->c_iflag & PARMRK)  fprintf(stream, "PARMRK,"); 
if (termios->c_iflag & INPCK)   fprintf(stream, "INPCK,");  
#ifdef ISTRIP
if (termios->c_iflag & ISTRIP)   fprintf(stream, "ISTRIP,");  
#endif
#ifdef ISTRI
if (termios->c_iflag & ISTRI)   fprintf(stream, "ISTRI,");  
#endif
if (termios->c_iflag & INLCR)   fprintf(stream, "INLCR,");  
if (termios->c_iflag & IGNCR)   fprintf(stream, "IGNCR,");  
if (termios->c_iflag & ICRNL)   fprintf(stream, "ICRNL,");  
#ifdef IUCLC
if (termios->c_iflag & IUCLC)   fprintf(stream, "IUCLC,");  
#endif
if (termios->c_iflag & IXON)    fprintf(stream, "IXON,")   ;
if (termios->c_iflag & IXANY)   fprintf(stream, "IXANY,");  
if (termios->c_iflag & IXOFF)   fprintf(stream, "IXOFF,");  
#ifdef IMAXBEL
if (termios->c_iflag & IMAXBEL) fprintf(stream, "IMAXBEL,");
#endif
fputc('\n', stream);

fprintf(stream, "c_oflag = 0x%08x\n", termios->c_oflag);
/* c_oflag bits */
if (termios->c_oflag & OPOST)  fprintf(stream, "OPOST,"); 
#ifdef OLCUC
if (termios->c_oflag & OLCUC)  fprintf(stream, "OLCUC,"); 
#endif
if (termios->c_oflag & ONLCR)  fprintf(stream, "ONLCR,"); 
#ifdef OXTABS
if (termios->c_oflag & OXTABS)  fprintf(stream, "OXTABS,"); 
#endif
#ifdef ONOEOT
if (termios->c_oflag & ONOEOT)  fprintf(stream, "ONOEOT,"); 
#endif
#ifdef OCRNL
if (termios->c_oflag & OCRNL)  fprintf(stream, "OCRNL,"); 
#endif
#ifdef ONOCR
if (termios->c_oflag & ONOCR)  fprintf(stream, "ONOCR,"); 
#endif
#ifdef ONLRET
if (termios->c_oflag & ONLRET) fprintf(stream,"ONLRET,");
#endif
#ifdef OFILL
if (termios->c_oflag & OFILL)  fprintf(stream, "OFILL,"); 
#endif
#ifdef OFDEL
if (termios->c_oflag & OFDEL)  fprintf(stream, "OFDEL,"); 
#endif
#if defined(__USE_MISC) || defined(__USE_XOPEN)
if (termios->c_oflag & NLDLY)  fprintf(stream, "NLDLY,"); 
if (termios->c_oflag & NL0)    fprintf(stream, "NL0,");   
if (termios->c_oflag & NL1)    fprintf(stream, "NL1,");   
if (termios->c_oflag & CRDLY)  fprintf(stream, "CRDLY,"); 
if (termios->c_oflag & CR0)    fprintf(stream, "CR0,");   
if (termios->c_oflag & CR1)    fprintf(stream, "CR1,");   
if (termios->c_oflag & CR2)    fprintf(stream, "CR2,");   
if (termios->c_oflag & CR3)    fprintf(stream, "CR3,");   
if (termios->c_oflag & TABDLY) fprintf(stream, "TABDLY,");
if (termios->c_oflag & TAB0)   fprintf(stream, "TAB0,");  
if (termios->c_oflag & TAB1)   fprintf(stream, "TAB1,");  
if (termios->c_oflag & TAB2)   fprintf(stream, "TAB2,");  
if (termios->c_oflag & TAB3)   fprintf(stream, "TAB3,");  
if (termios->c_oflag & BSDLY)  fprintf(stream, "BSDLY,"); 
if (termios->c_oflag & BS0)    fprintf(stream, "BS0,");   
if (termios->c_oflag & BS1)    fprintf(stream, "BS1,");   
if (termios->c_oflag & FFDLY)  fprintf(stream, "FFDLY,"); 
if (termios->c_oflag & FF0)    fprintf(stream, "FF0,");   
if (termios->c_oflag & FF1)    fprintf(stream, "FF1,");   
#endif

#ifdef VTDLY
if (termios->c_oflag & VTDLY)  fprintf(stream, "VTDLY,");
#endif
#ifdef VT0
if (termios->c_oflag &   VT0)  fprintf(stream, "  VT0,");
#endif
#ifdef VT1
if (termios->c_oflag &   VT1)  fprintf(stream, "  VT1,");
#endif


fputc('\n', stream);

fprintf(stream, "c_cflag = 0x%08x\n", termios->c_cflag);
/* c_cflag bit meaning */
if (termios->c_cflag & CSIZE)   fprintf(stream, "CSIZE,"); 
if (termios->c_cflag &   CS5)   fprintf(stream, "  CS5,"); 
if (termios->c_cflag &   CS6)   fprintf(stream, "  CS6,"); 
if (termios->c_cflag &   CS7)   fprintf(stream, "  CS7,"); 
if (termios->c_cflag &   CS8)   fprintf(stream, "  CS8,"); 
if (termios->c_cflag & CSTOPB)  fprintf(stream, "CSTOPB,");
if (termios->c_cflag & CREAD)   fprintf(stream, "CREAD,"); 
if (termios->c_cflag & PARENB)  fprintf(stream, "PARENB,");
if (termios->c_cflag & PARODD)  fprintf(stream, "PARODD,");
if (termios->c_cflag & HUPCL)   fprintf(stream, "HUPCL,"); 
if (termios->c_cflag & CLOCAL)  fprintf(stream, "CLOCAL,");
#ifdef CCTS_OFLOW
if (termios->c_cflag & CCTS_OFLOW)  fprintf(stream, "CCTS_OFLOW,");
#endif
#ifdef CRTS_IFLOW
if (termios->c_cflag & CRTS_IFLOW)  fprintf(stream, "CRTS_IFLOW,");
#endif
#ifdef CDTR_IFLOW
if (termios->c_cflag & CDTR_IFLOW)  fprintf(stream, "CDTR_IFLOW,");
#endif
#ifdef CDSR_OFLOW
if (termios->c_cflag & CDSR_OFLOW)  fprintf(stream, "CDSR_OFLOW,");
#endif
#ifdef CCAR_OFLOW
if (termios->c_cflag & CCAR_OFLOW)  fprintf(stream, "CCAR_OFLOW,");
#endif
fputc('\n', stream);

fprintf(stream, "c_lflag = 0x%08x\n", termios->c_lflag);
/* c_lflag bits */
if (termios->c_lflag & ISIG)    fprintf(stream, "ISIG,");
if (termios->c_lflag & ICANON)  fprintf(stream, "ICANON,");
#if defined __USE_MISC || defined __USE_XOPEN
if (termios->c_lflag & XCASE)   fprintf(stream, "XCASE,");
#endif                         
if (termios->c_lflag & ECHO)    fprintf(stream, "ECHO,");  
if (termios->c_lflag & ECHOE)   fprintf(stream, "ECHOE,"); 
if (termios->c_lflag & ECHOK)   fprintf(stream, "ECHOK,"); 
if (termios->c_lflag & ECHONL)  fprintf(stream, "ECHONL,");
if (termios->c_lflag & NOFLSH)  fprintf(stream, "NOFLSH,");
if (termios->c_lflag & TOSTOP)  fprintf(stream, "TOSTOP,");
#ifdef __USE_MISC
if (termios->c_lflag & ECHOCTL)  fprintf(stream, "ECHOCTL,");
if (termios->c_lflag & ECHOPRT)  fprintf(stream, "ECHOPRT,");
if (termios->c_lflag & ECHOKE)  fprintf(stream, " ECHOKE,"); 
if (termios->c_lflag & FLUSHO)  fprintf(stream, " FLUSHO,"); 
if (termios->c_lflag & PENDIN)  fprintf(stream, " PENDIN,"); 
#endif                                                    
if (termios->c_lflag & IEXTEN)  fprintf(stream, " IEXTEN,"); 
#ifdef NOKERNINFO
if (termios->c_lflag & NOKERNINFO)    fprintf(stream, "NOKERNINFO,");  
#endif
#ifdef EXTPROC
if (termios->c_lflag & EXTPROC)    fprintf(stream, "EXTPROC,");  
#endif
#ifdef ALTWERASE
if (termios->c_lflag & ALTWERASE)    fprintf(stream, "ALTWERASE,");  
#endif
fputc('\n', stream);
fputc('\n', stream);

} /* end of dump_termios */

#endif            
