/*static char *sccsid = "%Z% %M% %I% - %G% %U% AG CS Phoenix";*/
/***************************************************************
*
*  module expistamp.c
*
*  Main program for exipration date stamping command expistamp.
*
*  Usage:  expistamp  [-r] [-n <netaddr>] [-d <yyyy/mm/dd>] <serial> <file_to_stamp>
*
*  Routines:
*     main
*
* Internal:
*     validate_yyyymmdd  - Validate the yyyy/mm/dd string
*     mem_index          - Find one arbitrary string within another
*
***************************************************************/
    
#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h      */
#include <time.h>           /* /usr/include/time.h       */
#include <sys/types.h>      /* /usr/include/sys/types.h  */
#include <sys/stat.h>       /* /usr/include/sys/stat.h   */
#include <fcntl.h>          /* /usr/include/fcntl.h      */
#include <malloc.h>

#include    "expidate.h"



/***************************************************************
*  
*  Local routine prototypes
*  
***************************************************************/

static int mem_index(int            start_index,
                     unsigned char *searched_string,
                     int            searched_string_len,
                     unsigned char *string_to_find,
                     int            string_to_find_len);

static int validate_yyyymmdd(char *s);

static int strsum(char *s, int l);

int reverse_endian(int in);

int parse_netaddr(char           *netaddr_parm,     /* input  */
                  unsigned int   *net_addr_lock,    /* output */
                  char           *net_addr_count);  /* output - really is a pointer to a char (one) */


/***************************************************************
*  
*  Prototypes for some system routines which cause trouble
*  when included from unistd.h
*  
***************************************************************/

/* Symbolic constants for the "lseek" routine: */
#ifndef SEEK_SET
#define	SEEK_SET	0	/* Set file pointer to "offset" */
#endif
#ifndef SEEK_CUR
#define	SEEK_CUR	1	/* Set file pointer to current plus "offset" */
#endif
#ifndef SEEK_END
#define	SEEK_END	2	/* Set file pointer to EOF plus "offset" */
#endif

int getopt();
extern char *optarg;
extern int optind, opterr; 


/***************************************************************
*  
*  Main program
*  
***************************************************************/



int main(int argc, char *argv[])
{
int              ch;
struct stat      statrec ;
unsigned char   *contents ;
int              n ;
FILE            *fd ;
char             date[16];
/*int              i ;*/
int              rc ;
int              bytes_remaining ;
unsigned char   *buff_target;
int              exp_index;
int              idx;
int              expire_date;
int              serial_number;
int              count;
struct exp_date  exp_work;
unsigned int     search_pattern[2];
char             junk[256];
int              reverse_endian_mode = 0;
char            *target_file;
char            *serial_parm;
char             date_parm[32];
char             netaddr_parm[32];
unsigned int     net_addr_lock;
char             net_addr_count;
static char     *usage = "expistamp  [-r] [-n <netaddr>] [-d <yyyy/mm/dd>] <serial> <file_to_stamp>\n";

/*----------------------------------------------------------------------------*/
/*
 *
 *  Usage:  expistamp  [-r] [-n <netaddr>] [-d <yyyy/mm/dd>]  serial  file_to_stamp
 *                         
 *
*/
/*----------------------------------------------------------------------------*/

date_parm[0] = '\0';
netaddr_parm[0] = '\0';

while ((ch = getopt(argc, argv, "d:n:r")) != EOF)
   switch (ch)
   {
   case 'd' :   
      strncpy(date_parm, optarg, sizeof(date_parm)); 
      break;

   case 'n' :   
      strncpy(netaddr_parm, optarg, sizeof(netaddr_parm)); 
      break;

   case 'r' :   
      reverse_endian_mode++;
      break;

   default  :                                                     
      fprintf(stderr, "Unknown dash variable -%c\n", ch);
      fprintf(stderr, usage);
      return(1);

   }   /* end of switch on ch */

if ((argc - optind) != 2)
   {
      fprintf(stderr, "Wrong number of many positional arguments (%d) should be 2\n", (argc - optind));
      fprintf(stderr, usage);
      return(1) ;
   }
else
   {
      serial_parm = argv[optind];
      target_file = argv[optind+1];
   }

/* first stat the file to get its size */
if (stat(target_file, &statrec) != 0)
   {
      fprintf(stderr, "Unable to stat \"%s\". (%s)\n", target_file, strerror(errno)) ;
      return(1) ;
   }

/* validate our date */
if (date_parm[0])
   if (!validate_yyyymmdd(date_parm))
      {
         fprintf (stderr, "Invalid date \"%s\" specified.\n", date_parm) ;
         return(1) ;
      }


/***************************************************************
*  
*  Transform the passed date into a string of digits.  This
*  involves removing the slashes. yyyy/mm/dd -> yyyymmdd.
*  
***************************************************************/

if (date_parm[0])
   {
      /* transform our date into a string of digits */
      date[0] = date_parm[0] ;
      date[1] = date_parm[1] ;
      date[2] = date_parm[2] ;
      date[3] = date_parm[3] ;
      date[4] = date_parm[5] ;
      date[5] = date_parm[6] ;
      date[6] = date_parm[8] ;
      date[7] = date_parm[9] ;
      date[8] = '\0';

      /* convert our date to an integer */
      count = sscanf(date, "%d%s", &expire_date, junk) ;
      if (count != 1)
         {
            fprintf(stderr, "Invalid date number %s, format is yyyy/mm/dd.\n", date_parm) ;
            return(1) ;
         }
   }
else
   expire_date = 0;

/***************************************************************
*  
*  If the network address was specified, process it.
*  
***************************************************************/

if (netaddr_parm[0])
   {
      if (parse_netaddr(netaddr_parm, &net_addr_lock, &net_addr_count) != 0)
         {
            fprintf(stderr, "Network address must be in dotted decimal format n.n.n, %s\n", netaddr_parm);
            return(1);
         }
   }
else
   {
      net_addr_lock  = DEFAULT_NET_VAL;
      net_addr_count = 0;
   }

/* scan our serial number into an int ... */
count = sscanf(serial_parm, "%d%s", &serial_number, junk) ;
if (count != 1)
   {
      fprintf(stderr, "Invalid serial number \"%s\";  Must be all decimal digits.\n", serial_parm) ;
      return(1) ;
   }
if ((serial_number < 1000) || (serial_number > 999999999))
   {
      fprintf(stderr, "Invalid serial number %d;  must be between 1000 and 999999999.\n", serial_number) ;
      return(1) ;
   }


/* allocate enough space to hold the file */
if ((contents = (unsigned char *) malloc(statrec.st_size)) == NULL)
   {
      fprintf(stderr, "Unable to allocate %d bytes to hold file %s .\n", statrec.st_size, argv[1]) ;
      return(1) ;
   }

/* open the file for read */
if ((fd = fopen(target_file, "r+b")) == NULL)
   {
      fprintf(stderr, "Unable to open %s (%s).\n", target_file, strerror(errno)) ;
      free(contents) ;
      return(1) ;
   }

/* read in the entire file contents */
bytes_remaining = statrec.st_size;
buff_target = contents;

while(bytes_remaining > 0)
{
   n = fread(buff_target, 1, bytes_remaining, fd) ;
   if (n <= 0)
      {
         fprintf(stderr, "Read of %d bytes from %s failed (%s), n = %d\n", bytes_remaining, target_file, strerror(errno), n) ;
         fclose(fd) ;
         free(contents) ;
         return(1) ;
      }
   else
      {
         bytes_remaining -= n;
         buff_target += n;
      }
}


/***************************************************************
*  
*  Find the placeholder.  Scan the whole file to make sure
*  it occurs only once.
*  
***************************************************************/
if (reverse_endian_mode)
   {
      search_pattern[0] = reverse_endian(MARKER0_VAL);
      search_pattern[1] = reverse_endian(MARKER1_VAL);
   }
else
   {
      search_pattern[0] = MARKER0_VAL;
      search_pattern[1] = MARKER1_VAL;
   }

exp_index = -1;
idx = 0;

while(idx != -1)
{
   idx = mem_index(idx, contents, statrec.st_size, (unsigned char *)search_pattern, sizeof(search_pattern));
   if (idx != -1)
      if (exp_index != -1)
         {
            /***************************************************************
            *  Second occurence found, flag an error and quit.
            ***************************************************************/
            fprintf(stderr, "Second occurance of unique search pattern found, change pattern\n");
            fclose(fd);
            return(2);
         }
      else
         {
            /***************************************************************
            *  First occurance, save it.
            ***************************************************************/
            exp_index = idx;
            idx++; /* start scan past this occurance */
         }
}

/***************************************************************
*  
*  If we did not find the search pattern, quit with a message.
*  
***************************************************************/

if (exp_index == -1)
   {
      fprintf(stderr, "Could not find marker in file %s\n", target_file);
      fclose(fd);
      return(3);
   }

/***************************************************************
*  
*  Stick in the new data into the memory copy of the file.
*  Be careful about modifying this code.  Only some of the 
*  fields get changed, but the entire structure
*  gets written back to the file.  We copy the whole structure
*  to local storage to take care of potential alignment problems.
*  
***************************************************************/

memcpy((char *)&exp_work, (char *)&contents[exp_index], sizeof(struct exp_date));

if (reverse_endian_mode)
   {
      exp_work.hashed_expire_date   = reverse_endian(expire_date   + FUDGE_DATE);
      exp_work.hashed_serial_number = reverse_endian(serial_number + FUDGE_SERIALNO);
      exp_work.plain_date           = reverse_endian(expire_date);
      exp_work.hashed_net_id        = reverse_endian(net_addr_lock - FUDGE_DATE);
      exp_work.net_count            = net_addr_count;
      exp_work.check_sum            = reverse_endian(FUDGE_INTERNALSUM);
      exp_work.check_sum            = reverse_endian(strsum((char *)&exp_work, sizeof(struct exp_date)) + FUDGE_INTERNALSUM);
   }
else
   {
      exp_work.hashed_expire_date   = expire_date   + FUDGE_DATE;
      exp_work.hashed_serial_number = serial_number + FUDGE_SERIALNO;
      exp_work.plain_date           = expire_date;
      exp_work.hashed_net_id        = net_addr_lock - FUDGE_DATE;
      exp_work.net_count            = net_addr_count;
      exp_work.check_sum            = FUDGE_INTERNALSUM;
      exp_work.check_sum           += strsum((char *)&exp_work, sizeof(struct exp_date));
   }

/***************************************************************
*  
*  Copy the data back to the file.
*  
***************************************************************/

rc = fseek(fd, exp_index, SEEK_SET);
if (rc == -1)
   {
      fprintf(stderr, "Unable to fseek to %d in %s (%s).\n", exp_index, target_file, strerror(errno)) ;
      fclose(fd);
      return(4) ;
   }
   
n = fwrite((char *)&exp_work, 1, sizeof(struct exp_date), fd);

if (n != sizeof(struct exp_date))
   {
       if (n == -1)
         fprintf(stderr, "Unable to write %d bytes to %s (%s).\n", sizeof(struct exp_date), argv[1], strerror(errno)) ;
      else
         fprintf(stderr, "Unable to write %d bytes to %s wrote %d instead.\n", sizeof(struct exp_date), argv[1], n) ;

       fclose(fd) ;
       return(5) ;
    }
else
   {
      fprintf(stderr, "Timestamp set successfully completed.\n");
      fclose(fd) ;
      return(0);
   }

}  /* main */

int reverse_endian(int in)
{
char    *in_ovly;
char    *out_ovly;
int      out;

in_ovly = (char *)&in;
out_ovly = (char *)&out;

out_ovly[0] = in_ovly[3];
out_ovly[1] = in_ovly[2];
out_ovly[2] = in_ovly[1];
out_ovly[3] = in_ovly[0];

return(out);

} /* end of reverse_endian */

/************************************************************************

NAME:      strsum

PURPOSE:    Create a hash sum from a string.

PARAMETERS:

   1.   s   -  pointer to char (INPUT)
               This is the string to be hashed.  It can contain arbitrary
               characters and is not null terminated.

   2.   l   -  int (INPUT)
               This is the length of string s to process.

FUNCTIONS :

   1.   Hash the values of the string into a number and return it.


OUTPUTS:
   hash      -  int
                This is the hashed sum of the string.

NOTES:
   1.   This routine also exists in expidate.c.  The two copies
        must be maintained identically.

*************************************************************************/

static int strsum(char *s, int l)
{
    int              i ;
    unsigned int     r ;

    r = 0 ;
    for (i = 0 ;  i < l ;  i++)
        r += (unsigned char)s[i];
    return(r % 65535) ;

}  /* strsum */


static int validate_yyyymmdd(char *s) /*  s - string to evaluate */
{
/*----------------------------------------------------------------------------*/
    short       yyyy ;
    short       mm ;
    short       dd ;
    short       leap ;
/*----------------------------------------------------------------------------*/

    if (strlen(s) != 10) return (FALSE) ;

    yyyy = ((s[0] - '0') * 1000) + ((s[1] - '0') * 100) +
           ((s[2] - '0') * 10) + (s[3] - '0') ;
    if (((yyyy % 4 == 0) && (yyyy % 100 != 0)) || (yyyy % 100 == 0))
       leap = TRUE ;
    else
       leap = FALSE ;
    mm = ((s[5] - '0') * 10) + (s[6] - '0') ;
    if ((mm < 1) || (mm > 12)) return(FALSE) ;
    dd = ((s[8] - '0') * 10) + (s[9] - '0') ;
    if (dd < 1) return(FALSE) ;
    switch (mm) {
        case 1:
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
            if (dd > 31) return(FALSE) ;
            break ;
        case 4:
        case 6:
        case 9:
        case 11:
            if (dd > 30) return(FALSE) ;
            break ;
        case 2:
            if (leap && (dd > 29)) return(FALSE) ;
            if (!leap && (dd > 28)) return(FALSE) ;
        }
    return(TRUE) ;

}  /* validate_yyyymmdd */


/************************************************************************

NAME:      mem_index

PURPOSE:    This routine scans a string of arbitrary bytes for
            another string of arbitrary bytes and returns the index
            of the string to find in the searched string. or -1
            for not found.

PARAMETERS:

   1.  start_index         - int (INPUT)
                             This is the offset into the searched string to start the
                             search.  Code zero to start at the start of the string.

   2.  searched_string     - pointer to unsigned char (INPUT)
                             This is the string to scan.  It is not null terminated.
                             Its length is determined from parm searched_string_len.

   3.  searched_string_len - int (INPUT)
                             This is the length of searched_string.

   4.  string_to_find      - pointer to unsigned char (INPUT)
                             This is the string to look for in searched_string  It is not null terminated.
                             Its length is determined from parm string_to_find_len.

   5.  string_to_find_len  - int (INPUT)
                             This is the length of string_to_find

FUNCTIONS :

   1.   If the string to find is longer than the searched string, there is no chance
        of a match, so return -1.

   2.   Scan the searched string doing a quick check for the first character and
        a memcmp if that works.  Keep trying character by character until a match
        is found or we run out string.


OUTPUTS:
   found_idx -  int
                The offset into searched_string string where string_to_find
                starts.  -1 is returned if string_to_find was not found.

*************************************************************************/

static int mem_index(int            start_index,
                     unsigned char *searched_string,
                     int            searched_string_len,
                     unsigned char *string_to_find,
                     int            string_to_find_len)
{
int             i ;         /* index into string 'a' */

/* can't possibly match if b is longer than a */
if ((string_to_find_len > searched_string_len) || (searched_string_len == 0))
   return(-1);

/* loop through the big string */
for (i = start_index;  i <= searched_string_len - string_to_find_len ;  i++)
    if (searched_string[i] == string_to_find[0])
       if (memcmp(&(searched_string[i]), string_to_find, string_to_find_len) == 0)
          return(i);

return(-1);

}  /* end of mem_index */


/************************************************************************

NAME:      parse_netaddr

PURPOSE:    This routine scans a dotted internet address, validates it 
            and converts it to an int.  It also counts the number
            of pieces.

PARAMETERS:

   1.  netaddr_parm        - pointer to char (INPUT)
                             This is the dotted network address in character form
                             search.  Code zero to start at the start of the string.

   2.  net_addr_lock       - pointer to unsigned int (OUTPUT)
                             This is the dotted network address in integer form.

   3.  net_addr_count      - int (INPUT)
                             This is the number of address places specified.

FUNCTIONS :

   1.   Count the number of dotted name segments and make sure the 

   2.   Scan the searched string doing a quick check for the first character and
        a memcmp if that works.  Keep trying character by character until a match
        is found or we run out string.


OUTPUTS:
   rc        -  int
                0  -  Network address was null or ok
                -1 -  Network address was invalid.

*************************************************************************/

#ifdef WIN32
#include <winsock.h>
#else
#include <sys/socket.h>
#ifdef OMVS
typedef  unsigned short ushort;
#endif
#include <netinet/in.h>
#ifndef OMVS
#include <arpa/inet.h>
#endif
#endif

int parse_netaddr(char           *netaddr_parm,     /* input  */
                  unsigned int   *net_addr_lock,    /* output */
                  char           *net_addr_count)   /* output - really is a pointer to a char (one) */
{
char   *p;
int     na[4];
int     i;

p = netaddr_parm;
*net_addr_lock =  0;
*net_addr_count = 0;
na[0] = 0;
na[1] = 0;
na[2] = 0;
na[3] = 0;

if (*p)
   while(*p)
   {
      if (*p == '.')
         (*net_addr_count)++;
      else
         na[*net_addr_count] = (na[*net_addr_count] * 10) + (*p & 0x0F);
      p++;
   }
else
   return(0);

(*net_addr_count)++;

for (i = 0; i < *net_addr_count; i++)
    *net_addr_lock = ((*net_addr_lock) << 8) + na[i];

if (*net_addr_lock == -1)
   {
      fprintf(stderr, "Invalid dotted interent address \"%s\"\n", netaddr_parm);
      return(-1);
   }

return(0);

} /* end of parse_netaddr */






