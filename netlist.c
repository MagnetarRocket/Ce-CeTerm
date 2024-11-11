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

/************************************************************************

MODULE NAME:  netlist.c

          This module contains routines:
          netlist             -  Get the list of internet addresses for this node
          socketfd2hostinet   -  Get the internet ip address of a socket file descriptor

*************************************************************************/


#include <stdio.h>        /* /usr/include/stdio.h        */
#include <string.h>       /* /usr/include/string.h       */
#include <sys/types.h>    /* /usr/include/sys/types.h    */
#include <fcntl.h>        /* /usr/include/fcntl.h        */
#ifdef OMVS
typedef  unsigned short ushort;
#endif
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>   /* /usr/include/sys/socket.h   */
#include <netinet/in.h>   /* /usr/include/netinet/in.h   */
#include <arpa/inet.h>    /* /usr/include/arpa/inet.h    */
#include <netdb.h>        /* /usr/include/netdb.h        */
#ifndef OMVS
#include <sys/param.h>    /* /usr/include/sys/param.h    */
#endif
#endif
#include <errno.h>        /* /usr/include/errno.h        */
#if defined(FREEBSD) || defined(OMVS)
#include <stdlib.h>       /* /usr/include/stdlib.h       */
#else
#include <malloc.h>       /* /usr/include/malloc.h       */
#endif

#ifndef WIN32
#include <sys/ioctl.h>    /* /usr/include/sys/ioctl.h    */
#ifdef apollo
#include <sys/net/if.h>   /* /usr/include/sys/net/if.h   */
#else
#include <sys/time.h>     /* /usr/include/sys/time.h     */
#include <net/if.h>       /* /usr/include/net/if.h       */
#endif
#ifndef SIOCGIFCONF
#include <sys/sockio.h>   /* /usr/include/sys/sockio.h   */
#endif
#define MAXIFS	256
#endif

#define DEFAULT_IP 0x7F000001
#define DEFAULT_MASK 0xFFFFFF00

#include "netlist.h"

#if defined(ARPUS_CE) && !defined(PGM_CEAPI)
#include "debug.h"
#include "dmwin.h" 
#else
#ifndef DEBUG
#define DEBUG(a) {}
#endif
#ifndef DEBUG1
#define DEBUG1(a) {}
#endif
#define dm_error(m,j) fprintf(stderr, "%s\n", m)
#endif

#ifdef DebuG
static char  *my_inet_ntoa(unsigned long   inet_addr);
#endif

#ifdef WIN32
static char *getWSAmsg(char *long_msg, int sizeof_long_msg);
#endif

/************************************************************************

NAME:      netlist - Get the list of internet addresses for this node

PURPOSE:   This routine extracts and returns internet data about this
           host.  The network type dr0 (domain ring 0), en0 (ethernet 0),
           is passed as an input parm.

PARAMETERS:
 
   1.   host_list  - pointer to unsigned long (OUTPUT)
                     This is the list of host internet address returned.
                     Each address occupies one int.

   2.   max_host   - int (INPUT)
                     This is the maximum number of host addresses which may
                     be put into the host_addr array.

   3.   netmask    - pointer to unsigned long (OUTPUT)
                     This is the netmask in binary form
                     <num.num.num.num>.  
                     It is an array which matches one for one with the returned
                     host_list.


FUNCTIONS :

   1.   get a socket to work with.
   2.   use ioctl to get the host address and the net mask.
   3.   And the netmask with the hostid to get the net address.

RETURNED VALUE:

   count - int
           The returned value is the number of host address returned.


*************************************************************************/

int netlist(unsigned long     *host_list,    /* output */
            int                max_host,     /* input  */
            unsigned long     *netmask)      /* output */
{
#ifdef WIN32
char                   host_name[80];
int                    rc;
int                    i;
char                   msg[512];
struct hostent        *host_data;
char                  *ip_addr;
int                    count = 0;

/***************************************************************
*  
*  Initialize the output parms to null.
*  
***************************************************************/

*netmask = 0;
netmask[0] = DEFAULT_MASK;  /* don't know how to find the netmast right now */


/***************************************************************
*  
*  The current host name.
*  
***************************************************************/

rc = gethostname(host_name, sizeof(host_name));
if (rc != 0)
   {
      snprintf(msg, sizeof(msg), "Cannot get hostname (%s)", getWSAmsg(NULL,0));
      dm_error(msg, DM_ERROR_BEEP);
      host_list[0] = DEFAULT_IP;
      netmask[0] = DEFAULT_MASK;
      return(1);
   }

host_data = gethostbyname(host_name);
if (host_data == NULL)
   {
      snprintf(msg, sizeof(msg), "Cannot get host data for %s (%s)", host_name, getWSAmsg(NULL,0));
      dm_error(msg, DM_ERROR_BEEP);
      host_list[0] = DEFAULT_IP;
      netmask[0] = DEFAULT_MASK;
      return(1);
   }

for (i = 0; host_data->h_addr_list[i]; i++)
{
   ip_addr = host_data->h_addr_list[i];
   if (count < max_host)
      {
         host_list[count++] = *((unsigned int *)host_data->h_addr_list[i]);
      }
   else
      break;
}

return(count);

/* Temporarily just return the default */
DEBUG1(fprintf(stderr,"WIN32, using default IP for now\n");)
host_list[0] = DEFAULT_IP;
netmask[0] = DEFAULT_MASK;
return(1);
#else

int                    fd;
int                    rc;
int                    i;
/*int                    base_len;*/
int                    count;
int                    host_count = 0;
unsigned long          host_addr_lcl;
char                   lcl_ifr_name[32];
char                   msg[512];
unsigned               bufsize;

struct ifreq     *ioctl_req;
struct ifconf    conf_req;
struct sockaddr_in  *sin_ptr;


/***************************************************************
*  
*  Initialize the output parms to null.
*  
***************************************************************/

*netmask = 0;


/***************************************************************
*  
*  Get a socket to work with.
*  
***************************************************************/

if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      DEBUG1(fprintf(stderr,"Cannot get a socket file descriptor (%s)\n", strerror(errno));)
      host_list[0] = DEFAULT_IP;
      netmask[0] = DEFAULT_MASK;
      return(1);
   }

/***************************************************************
*  
*  Get the number of addresses and allocate space for them.
*  
***************************************************************/

#ifdef SIOCGIFNUM
if (ioctl(fd, SIOCGIFNUM, (char *)&count) < 0)
   {
      DEBUG(fprintf(stderr, "Cannot get number of interfaces (SIOCGIFNUM:%s)\n", strerror(errno));)
      count = MAXIFS;
   } 
#else
count = MAXIFS;
#endif

bufsize = count * sizeof(struct ifreq);
ioctl_req = (struct ifreq *)malloc(bufsize);
if (!ioctl_req)
   {
      snprintf(msg, sizeof(msg), "Out of memory, could not get %d bytes", bufsize);
      dm_error(msg, DM_ERROR_BEEP);
      close(fd);
      return(0);
   }

/***************************************************************
*  
*  If no address was passed, get the default address.
*  The default address (eg. dr0, en0, le0, ...) gets put in
*  the ioctl_req.
*  
***************************************************************/

conf_req.ifc_len = bufsize;
conf_req.ifc_req = ioctl_req;
/*base_len = sizeof(struct ifreq);*/

rc = ioctl(fd, SIOCGIFCONF, (char *)&conf_req);
if (rc != 0)
   {
      DEBUG(fprintf(stderr,"Cannot get configuration name (SIOCGIFCONF:%s)\n", strerror(errno));)
      free((char *)ioctl_req);
      close(fd);
      host_list[0] = DEFAULT_IP;
      netmask[0] = DEFAULT_MASK;
      return(1);
   }
else
   {
#ifdef CYGNUS
      host_list[host_count] = ntohl(*((int *)(conf_req.ifc_ifcu.ifcu_req->ifr_addr.sa_data)));
      netmask[host_count] = ntohl(*((int *)(conf_req.ifc_ifcu.ifcu_req->ifr_netmask.sa_data)));
      host_count++;
#else
#ifndef SIOCGIFNUM
      count = (conf_req.ifc_len + 1) / sizeof(struct ifreq);
#endif
      for (i = 0;
           i < count;
           i++)
      {
         sin_ptr = (struct sockaddr_in  *)&ioctl_req[i].ifr_addr;
         DEBUG1(fprintf(stderr,"after SIOCGIFCONF(%d) name = %s, addr = %08X\n", i, ioctl_req[i].ifr_name, sin_ptr->sin_addr.s_addr);)
         if (ioctl_req[i].ifr_name[0] != '\0' && strcmp(ioctl_req[i].ifr_name, "lo0") != 0 && sin_ptr->sin_addr.s_addr != 0)
            {
               strncpy(lcl_ifr_name, ioctl_req[i].ifr_name, sizeof(lcl_ifr_name));
               /***************************************************************
               *  Get the host internet address
               ***************************************************************/

               rc = ioctl(fd, SIOCGIFADDR, (char *)&ioctl_req[i]);
               if (rc == 0)
                  {
                     host_addr_lcl = sin_ptr->sin_addr.s_addr;

                     /***************************************************************
                     *  Get the net mask
                     ***************************************************************/
                     rc = ioctl(fd, SIOCGIFNETMASK, (char *)&ioctl_req[i]);
                     if (rc == 0)
                        {
                           if (host_count < max_host)
                              {
                                 host_list[host_count] = ntohl(host_addr_lcl);
                                 netmask[host_count++] = ntohl(sin_ptr->sin_addr.s_addr);
                              }

                           DEBUG1(
                              fprintf(stderr,"interface = %5s, sin_addr = %08X, net mask  =  %08X\n",
                                              lcl_ifr_name, host_addr_lcl, netmask[host_count-1]);
                           )

                        }  /* get netmask worked */
                     else
                        {
                           snprintf(msg, sizeof(msg), "Cannot get netmask (SIOCGIFNETMASK:%s)", strerror(errno));
                           dm_error(msg, DM_ERROR_LOG);
                        } /* get netmask failed */
               
                  } /* get host addr worked */
               else
                  {
                     DEBUG1(fprintf(stderr, "Cannot get host addr (SIOCGIFADDR:%s)", strerror(errno));)
                  } /* get host addr failed */

            }
      } /* for loop walking through the interfaces */
#endif
   }  /* else SIOCGIFCONF worked */

/***************************************************************
*  
*  Make sure we found some sort of interface name.
*  
***************************************************************/

DEBUG1(
if (host_count == 0)
   fprintf(stderr, "Unable to find interface name for this machine\n");
else
   fprintf(stderr,"Internet address count %d\n", host_count);
)

free((char *)ioctl_req);
close(fd);
return(host_count);
#endif /* not WIN32 */

} /* end of netlist */


/************************************************************************

NAME:      socketfd2hostinet - Get the host address from a socket connection

PURPOSE:   This routine takes a connected tcp socket file descriptor
           (such as ConnectionNumber(display)) and returns either the
           local hosts internet address or the internet address of the
           host at the other end of the socket.

PARAMETERS:
   1.   socketfd  -  pointer to char (INPUT)
                     This is the host to connect to.
                     It may be the name or the dotted number address.
 
   2.   local      - int (INPUT)
                     If 0, get the remote host data.
                     If 1, get the local host data.
                     The netlist.h file contains the defined costants:
                     DISPLAY_PORT_LOCAL and DISPLAY_PORT_REMOTE

FUNCTIONS :

   1.   Get the address data from the file descriptor.
        This can be local or remote data based on the local parm
   2.   Get the host data from the address data.
   3.   Print the data in either verbose or terse form.
        reseved parm
   4.   Do a connect to the host.
   5.   If the connect works and debug is on, dump the socket data.

RETURNED VALUE:
   s_addr   -   The internet address in binary form (1 byte per number)
                is returned.  On error zero is returned.


*************************************************************************/

unsigned int socketfd2hostinet(int socketfd, int local)
{
struct sockaddr_in    host_addr;
/* RES 8/20/97  -  HP/UX 10.20 size_t all others int */
#if defined(_XOPEN_SOURCE_EXTENDED)
size_t                host_len;
#else
int                   host_len;
#endif

host_len = sizeof(host_addr);
if (local)
   {
      if (getsockname(socketfd, (struct sockaddr *)&host_addr, &host_len) != 0)
         {
            fflush(stderr);
            DEBUG(perror("?    getsockname");)
            return(0);
         }
   }
else
   {
      if (getpeername(socketfd, (struct sockaddr *)&host_addr, &host_len) != 0)
         {
            fflush(stderr);
            DEBUG(perror("?    getpeername");)
            return(0);
         }
   }

DEBUG(fprintf(stderr, "%s Internet addr = %s, port = %d\n",
              (local ? "Local" : "Remote"), my_inet_ntoa(ntohl(host_addr.sin_addr.s_addr)), ntohs(host_addr.sin_port));
)

return(host_addr.sin_addr.s_addr);

} /* end of socketfd2hostinet */


#ifdef DebuG
/* this routine exists because inet_ntoa takes it's parameter in
   a different form on each platform.  Some take the address of the
   s_addr, some take the s_addr, and some take the address of the
   sin_addr */
static char  *my_inet_ntoa(unsigned long   inet_addr)
{
static char    buff[80];  /* returned value */

snprintf(buff, sizeof(buff), "%d.%d.%d.%d", 
              ((inet_addr >> 24) & 0x000000FF),
              ((inet_addr >> 16) & 0x000000FF),
              ((inet_addr >> 8) & 0x000000FF),
              (inet_addr & 0x000000FF));

return(buff);

} /* end of my_inet_ntoa */
#endif


#ifdef WIN32
/************************************************************************

MODULE NAME:  getWSAmsg.c

PURPOSE:
      The WIN32 system contains no call to translate an error code returned by
      WSAGetLastError to a text message.  This routine was created by formatting
      the WSAGetLastError info viewer topic into a select statement.  Do a search
      on WSAEACCES to find this page.  See getWSAmsg.h for a description on how to
      call this routine.

ROUTINES:  This module contains routine:
           getWSAmsg           -  Call WSAGetLastError and translate the error code to text

*************************************************************************/



static char *getWSAmsg(char *long_msg, int sizeof_long_msg)
{
static char            default_msg_buff[256];
int                    error_code;
char                  *msg_s;
char                  *msg_l;

error_code = WSAGetLastError();
switch(error_code)
{
case WSAEACCES: /* (10013) */
   msg_s = "Permission denied. ";
   msg_l = "An attempt was made to access a socket in a way forbidden by its access permissions. An example is using a broadcast address for sendto without broadcast permission being set using setsockopt(SO_BROADCAST). ";
   break;

case WSAEADDRINUSE: /* (10048) */
   msg_s = "Address already in use. ";
   msg_l = "Only one usage of each socket address (protocol/IP address/port) is normally permitted. This error occurs if an application attempts to bind a socket to an IP address/port that has already been used for an existing socket, or a socket that wasn't closed properly, or one that is still in the process of closing. For server applications that need to bind multiple sockets to the same port number, consider using setsockopt(SO_REUSEADDR). Client applications usually need not call bind at all - connect will choose an unused port automatically. ";
   break;

case WSAEADDRNOTAVAIL: /* (10049) */
   msg_s = "Cannot assign requested address. ";
   msg_l = "The requested address is not valid in its context. Normally results from an attempt to bind to an address that is not valid for the local machine, or connect/sendto an address or port that is not valid for a remote machine (e.g. port 0). ";
   break;

case WSAEAFNOSUPPORT: /* (10047) */
   msg_s = "Address family not supported by protocol family. ";
   msg_l = "An address incompatible with the requested protocol was used. All sockets are created with an associated \"address family\" (i.e. AF_INET for Internet Protocols) and a generic protocol type (i.e. SOCK_STREAM). This error will be returned if an incorrect protocol is explicitly requested in the socket call, or if an address of the wrong family is used for a socket, e.g. in sendto. ";
   break;

case WSAEALREADY: /* (10037) */
   msg_s = "Operation already in progress. ";
   msg_l = "An operation was attempted on a non-blocking socket that already had an operation in progress - i.e. calling connect a second time on a non-blocking socket that is already connecting, or canceling an asynchronous request (WSAAsyncGetXbyY) that has already been canceled or completed. ";
   break;

case WSAECONNABORTED: /* (10053) */
   msg_s = "Software caused connection abort. ";
   msg_l = "An established connection was aborted by the software in your host machine, possibly due to a data transmission timeout or protocol error. ";
   break;

case WSAECONNREFUSED: /* (10061) */
   msg_s = "Connection refused. ";
   msg_l = "No connection could be made because the target machine actively refused it. This usually results from trying to connect to a service that is inactive on the foreign host - i.e. one with no server application running. ";
   break;

case WSAECONNRESET: /* (10054) */
   msg_s = "Connection reset by peer. ";
   msg_l = "A existing connection was forcibly closed by the remote host. This normally results if the peer application on the remote host is suddenly stopped, the host is rebooted, or the remote host used a \"hard close\" (see setsockopt for more information on the SO_LINGER option on the remote socket.) ";
   break;

case WSAEDESTADDRREQ: /* (10039) */
   msg_s = "Destination address required. ";
   msg_l = "A required address was omitted from an operation on a socket. For example, this error will be returned if sendto is called with the remote address of ADDR_ANY. ";
   break;

case WSAEFAULT: /* (10014) */
   msg_s = "Bad address. ";
   msg_l = "The system detected an invalid pointer address in attempting to use a pointer argument of a call. This error occurs if an application passes an invalid pointer value, or if the length of the buffer is too small. For instance, if the length of an argument which is a struct sockaddr is smaller than sizeof(struct sockaddr). ";
   break;

case WSAEHOSTDOWN: /* (10064) */
   msg_s = "Host is down. ";
   msg_l = "A socket operation failed because the destination host was down. A socket operation encountered a dead host. Networking activity on the local host has not been initiated. These conditions are more likely to be indicated by the error WSAETIMEDOUT. ";
   break;

case WSAEHOSTUNREACH: /* (10065) */
   msg_s = "No route to host. ";
   msg_l = "A socket operation was attempted to an unreachable host. See WSAENETUNREACH ";
   break;

case WSAEINPROGRESS: /* (10036) */
   msg_s = "Operation now in progress. ";
   msg_l = "A blocking operation is currently executing. Windows Sockets only allows a single blocking operation to be outstanding per task (or thread), and if any other function call is made (whether or not it references that or any other socket) the function fails with the WSAEINPROGRESS error. ";
   break;

case WSAEINTR: /* (10004) */
   msg_s = "Interrupted function call. ";
   msg_l = "A blocking operation was interrupted by a call to WSACancelBlockingCall. ";
   break;

case WSAEINVAL: /* (10022) */
   msg_s = "Invalid argument. ";
   msg_l = "Some invalid argument was supplied (for example, specifying an invalid level to the setsockopt function). In some instances, it also refers to the current state of the socket - for instance, calling accept on a socket that is not listening. ";
   break;

case WSAEISCONN: /* (10056) */
   msg_s = "Socket is already connected. ";
   msg_l = "A connect request was made on an already connected socket. Some implementations also return this error if sendto is called on a connected SOCK_DGRAM socket (For SOCK_STREAM sockets, the to parameter in sendto is ignored), although other implementations treat this as a legal occurrence. ";
   break;

case WSAEMFILE: /* (10024) */
   msg_s = "Too many open files. ";
   msg_l = "Too many open sockets. Each implementation may have a maximum number of socket handles available, either globally, per process or per thread. ";
   break;

case WSAEMSGSIZE: /* (10040) */
   msg_s = "Message too long. ";
   msg_l = "A message sent on a datagram socket was larger than the internal message buffer or some other network limit, or the buffer used to receive a datagram into was smaller than the datagram itself. ";
   break;

case WSAENETDOWN: /* (10050) */
   msg_s = "Network is down. ";
   msg_l = "A socket operation encountered a dead network. This could indicate a serious failure of the network system (i.e. the protocol stack that the WinSock DLL runs over), the network interface, or the local network itself. ";
   break;

case WSAENETRESET: /* (10052) */
   msg_s = "Network dropped connection on reset. ";
   msg_l = "The host you were connected to crashed and rebooted. May also be returned by setsockopt if an attempt is made to set SO_KEEPALIVE on a connection that has already failed. ";
   break;

case WSAENETUNREACH: /* (10051) */
   msg_s = "Network is unreachable. ";
   msg_l = "A socket operation was attempted to an unreachable network. This usually means the local software knows no route to reach the remote host. ";
   break;

case WSAENOBUFS: /* (10055) */
   msg_s = "No buffer space available. ";
   msg_l = "An operation on a socket could not be performed because the system lacked sufficient buffer space or because a queue was full. ";
   break;

case WSAENOPROTOOPT: /* (10042) */
   msg_s = "Bad protocol option. ";
   msg_l = "An unknown, invalid or unsupported option or level was specified in a getsockopt or setsockopt call. ";
   break;

case WSAENOTCONN: /* (10057) */
   msg_s = "Socket is not connected. ";
   msg_l = "A request to send or receive data was disallowed because the socket is not connected and (when sending on a datagram socket using sendto) no address was supplied. Any other type of operation might also return this error - for example, setsockopt setting SO_KEEPALIVE if the connection has been reset. ";
   break;

case WSAENOTSOCK: /* (10038) */
   msg_s = "Socket operation on non-socket. ";
   msg_l = "An operation was attempted on something that is not a socket. Either the socket handle parameter did not reference a valid socket, or for select, a member of an fd_set was not valid. ";
   break;

case WSAEOPNOTSUPP: /* (10045) */
   msg_s = "Operation not supported. ";
   msg_l = "The attempted operation is not supported for the type of object referenced. Usually this occurs when a socket descriptor to a socket that cannot support this operation, for example, trying to accept a connection on a datagram socket. ";
   break;

case WSAEPFNOSUPPORT: /* (10046) */
   msg_s = "Protocol family not supported. ";
   msg_l = "The protocol family has not been configured into the system or no implementation for it exists. Has a slightly different meaning to WSAEAFNOSUPPORT, but is interchangeable in most cases, and all Windows Sockets functions that return one of these specify WSAEAFNOSUPPORT. ";
   break;

case WSAEPROCLIM: /* (10067) */
   msg_s = "Too many processes. ";
   msg_l = "A Windows Sockets implementation may have a limit on the number of applications that may use it simultaneously. WSAStartup may fail with this error if the limit has been reached. ";
   break;

case WSAEPROTONOSUPPORT: /* (10043) */
   msg_s = "Protocol not supported. ";
   msg_l = "The requested protocol has not been configured into the system, or no implementation for it exists. For example, a socket call requests a SOCK_DGRAM socket, but specifies a stream protocol. ";
   break;

case WSAEPROTOTYPE: /* (10041) */
   msg_s = "Protocol wrong type for socket. ";
   msg_l = "A protocol was specified in the socket function call that does not support the semantics of the socket type requested. For example, the ARPA Internet UDP protocol cannot be specified with a socket type of SOCK_STREAM. ";
   break;

case WSAESHUTDOWN: /* (10058) */
   msg_s = "Cannot send after socket shutdown. ";
   msg_l = "A request to send or receive data was disallowed because the socket had already been shut down in that direction with a previous shutdown call. By calling shutdown a partial close of a socket is requested, which is a signal that sending or receiving or both has been discontinued. ";
   break;

case WSAESOCKTNOSUPPORT: /* (10044) */
   msg_s = "Socket type not supported. ";
   msg_l = "The support for the specified socket type does not exist in this address family. For example, the optional type SOCK_RAW might be selected in a socket call, and the implementation does not support SOCK_RAW sockets at all. ";
   break;

case WSAETIMEDOUT: /* (10060) */
   msg_s = "Connection timed out. ";
   msg_l = "A connection attempt failed because the connected party did not properly respond after a period of time, or established connection failed because connected host has failed to respond. ";
   break;

case WSAEWOULDBLOCK: /* (10035) */
   msg_s = "Resource temporarily unavailable. ";
   msg_l = "This error is returned from operations on non-blocking sockets that cannot be completed immediately, for example recv when no data is queued to be read from the socket. It is a non-fatal error, and the operation should be retried later. It is normal for WSAEWOULDBLOCK to be reported as the result from calling connect on a non-blocking SOCK_STREAM socket, since some time must elapse for the connection to be established. ";
   break;

case WSAHOST_NOT_FOUND: /* (11001) */
   msg_s = "Host not found. ";
   msg_l = "No such host is known. The name is not an official hostname or alias, or it cannot be found in the database(s) being queried. This error may also be returned for protocol and service queries, and means the specified name could not be found in the relevant database. ";
   break;

case WSA_INVALID_HANDLE: /* (OS dependent) */
   msg_s = "Specified event object handle is invalid. ";
   msg_l = "An application attempts to use an event object, but the specified handle is not valid. ";
   break;

case WSA_INVALID_PARAMETER: /* (OS dependent) */
   msg_s = "One or more parameters are invalid. ";
   msg_l = "An application used a Windows Sockets function which directly maps to a Win32 function. The Win32 function is indicating a problem with one or more parameters. ";
   break;

#ifdef blah 
/* not defined */
case WSAINVALIDPROCTABLE: /* (OS dependent) */
   msg_s = "Invalid procedure table from service provider. ";
   msg_l = "A service provider returned a bogus proc table to WS2_32.DLL. (Usually caused by one or more of the function pointers being NULL.) ";
   break;

/* not defined */
case WSAINVALIDPROVIDER: /* (OS dependent) */
   msg_s = "Invalid service provider version number. ";
   msg_l = "A service provider returned a version number other than 2.0. ";
   break;
#endif

case WSA_IO_PENDING: /* (OS dependent) */
   msg_s = "Overlapped operations will complete later. ";
   msg_l = "The application has initiated an overlapped operation which cannot be completed immediately. A completion indication will be given at a later time when the operation has been completed. ";
   break;

case WSA_IO_INCOMPLETE: /* (OS dependent) */
   msg_s = "Overlapped I/O event object not in signaled state. ";
   msg_l = "The application has tried to determine the status of an overlapped operation which is not yet completed. Applications that use WSAWaitForMultipleEvents in a polling mode to determine when an overlapped operation has completed will get this error code until the operation is complete. ";
   break;

case WSA_NOT_ENOUGH_MEMORY: /* (OS dependent) */
   msg_s = "Insufficient memory available. ";
   msg_l = "An application used a Windows Sockets function which directly maps to a Win32 function. The Win32 function is indicating a lack of required memory resources. ";
   break;

case WSANOTINITIALISED: /* (10093) */
   msg_s = "Successful WSAStartup not yet performed. ";
   msg_l = "Either the application hasn’t called WSAStartup or WSAStartup failed. The application may be accessing a socket which the current active task does not own (i.e. trying to share a socket between tasks), or WSACleanup has been called too many times. ";
   break;

case WSANO_DATA: /* (11004) */
   msg_s = "Valid name, no data record of requested type. ";
   msg_l = "The requested name is valid and was found in the database, but it does not have the correct associated data being resolved for. The usual example for this is a hostname -> address translation attempt (using gethostbyname or WSAAsyncGetHostByName) which uses the DNS (Domain Name Server), and an MX record is returned but no A record - indicating the host itself exists, but is not directly reachable. ";
   break;

case WSANO_RECOVERY: /* (11003) */
   msg_s = "This is a non-recoverable error. ";
   msg_l = "This indicates some sort of non-recoverable error occurred during a database lookup. This may be because the database files (e.g. BSD-compatible HOSTS, SERVICES or PROTOCOLS files) could not be found, or a DNS request was returned by the server with a severe error. ";
   break;

#ifdef blah
/* not defined */
case WSAPROVIDERFAILEDINIT: /* (OS dependent) */
   msg_s = "Unable to initialize a service provider. ";
   msg_l = "Either a service provider's DLL could not be loaded (LoadLibrary failed) or the provider's WSPStartup/NSPStartup function failed. ";
   break;
#endif

case WSASYSCALLFAILURE: /* (OS dependent) */
   msg_s = "System call failure. ";
   msg_l = "Returned when a system call that should never fail does. For example, if a call to WaitForMultipleObjects fails or one of the registry functions fails trying to manipulate theprotocol/namespace catalogs. ";
   break;

case WSASYSNOTREADY: /* (10091) */
   msg_s = "Network subsystem is unavailable. ";
   msg_l = "This error is returned by WSAStartup if the Windows Sockets implementation cannot function at this time because the underlying system it uses to provide network services is currently unavailable. Users should check:\r\n"
           " that the appropriate Windows Sockets DLL file is in the current path, \r\n"
           " that they are not trying to use more than one Windows Sockets implementation simultaneously. If there is more than one WINSOCK DLL on your system, be sure the first one in the path is appropriate for the network subsystem currently loaded.\r\n"
           " the Windows Sockets implementation documentation to be sure all necessary components are currently installed and configured correctly.";
   break;

case WSATRY_AGAIN: /* (11002) */
   msg_s = "Non-authoritative host not found. ";
   msg_l = "This is usually a temporary error during hostname resolution and means that the local server did not receive a response from an authoritative server. A retry at some time later may be successful. ";
   break;

case WSAVERNOTSUPPORTED: /* (10092) */
   msg_s = "WINSOCK.DLL version out of range. ";
   msg_l = "The current Windows Sockets implementation does not support the Windows Sockets specification version requested by the application. Check that no old Windows Sockets DLL files are being accessed. ";
   break;

case WSAEDISCON: /* (10094) */
   msg_s = "Graceful shutdown in progress. ";
   msg_l = "Returned by recv, WSARecv to indicate the remote party has initiated a graceful shutdown sequence. ";
   break;

case WSA_OPERATION_ABORTED: /* (OS dependent) */
   msg_s = "Overlapped operation aborted. ";
   msg_l = "An overlapped operation was canceled due to the closure of the socket, or the execution of the SIO_FLUSH command in WSAIoctl. ";
   break;

default:
   msg_s = default_msg_buff;
   snprintf(default_msg_buff, sizeof(default_msg_buff), "Unknown WSA Error Code %d", error_code);
   msg_l = "WSAGetLastError returned a value unknown to this routine";
   break;

} /* end of switch statement */

if (long_msg)
   {
      strncpy(long_msg, msg_l, sizeof_long_msg-1);
      long_msg[sizeof_long_msg-1] = '\0';
   }

return(msg_s);

} /* end of get_windows_WSA_msg */
#endif


