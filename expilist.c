/*static char *sccsid = "%Z% %M% %I% - %G% %U% AG CS Phoenix";*/
/***************************************************************
*
*  module expilist.c
*
*  Main program for to list out date and serial set by
*  program expistamp.
*
*  Usage:  stamp  [-r] <path>
*
*  -r allows a little endian executable to be processed on a
*     big endian machine.  That is, you can process a dec executable
*     which has the bytes swapped around on an IBM RS6000 which uses
*     normal format integers.  The reverse is also true.
*
*  Routines:
*     main
*
* Internal:
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
#include <malloc.h>         /* /usr/include/malloc.h     */

#define B1 1  /* use binary mode on fopen for PC's unless it breaks something*/

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

static int strsum(char *s, int l);

int reverse_indian(int in);

static char  *my_inet_ntoa(unsigned long   inet_addr);



/***************************************************************
*  
*  Main program
*  
***************************************************************/



int main(int argc, char *argv[])
{
struct stat      statrec ;
unsigned char   *contents ;
int              n ;
FILE            *fd;
int              i ;
int              bytes_remaining ;
unsigned char   *buff_target;
int              exp_index;
int              idx;
struct exp_date  exp_work;
int              search_pattern[2];
register int     work_check_sum;
register int     work_check_sum2;
int              reverse_indian_mode = 0;
char             buff[64];
char             buff2[64];

/*----------------------------------------------------------------------------*/
/*
 *
 *  Usage:  expilist [-r]   <path> 
 *
*/
/*----------------------------------------------------------------------------*/

if (argc > 2)
   {
      if (strcmp(argv[1], "-r") == 0)
         {
            reverse_indian_mode++;
            for (i = 1; i <= argc; i++)
               argv[i] = argv[i+1];
            argc--;
         }
   }

if (argc != 2)
   {
      fprintf(stderr, "usage: expilist [-r] <path>\n") ;
      return(1) ;
   }

/* first stat the file to get its size */
if (stat(argv[1], &statrec) != 0)
   {
      fprintf(stderr, "Unable to stat \"%s\". (%s)\n", argv[1], strerror(errno)) ;
      return(1) ;
   }


/* allocate enough space to hold the file */
if ((contents = (unsigned char *) malloc(statrec.st_size)) == NULL)
   {
      fprintf(stderr, "Unable to allocate %d bytes to hold file %s .\n", statrec.st_size, argv[1]) ;
      return(1) ;
   }

/* open the file for read */
#ifdef B1
if ((fd = fopen(argv[1], "rb")) == NULL)
#else
if ((fd = fopen(argv[1], "r")) == NULL)
#endif
   {
      fprintf(stderr, "Unable to open %s (%s).\n", argv[1], strerror(errno)) ;
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
         fprintf(stderr, "Read of %d bytes from %s failed (%s), n = %d\n", bytes_remaining, argv[1], strerror(errno), n) ;
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

if (reverse_indian_mode)
   {
      search_pattern[0] = reverse_indian(MARKER0_VAL);
      search_pattern[1] = reverse_indian(MARKER1_VAL);
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
      fprintf(stderr, "Could not find marker in file %s\n", argv[1]);
      fclose(fd);
      return(3);
   }

/***************************************************************
*  
*  Convert the endian mode if needed.
*  
***************************************************************/

memcpy((char *)&exp_work, (char *)&contents[exp_index], sizeof(struct exp_date));

if (reverse_indian_mode)
   {
      exp_work.hashed_expire_date   = reverse_indian(exp_work.hashed_expire_date);
      exp_work.plain_date           = reverse_indian(exp_work.plain_date);
      exp_work.hashed_serial_number = reverse_indian(exp_work.hashed_serial_number);
      exp_work.check_sum            = reverse_indian(exp_work.check_sum);
      exp_work.hashed_net_id        = reverse_indian(exp_work.hashed_net_id);
   }

/***************************************************************
*  
*  Format the date to make it readable.
*  
***************************************************************/
sprintf(buff, "%08d", exp_work.hashed_expire_date - FUDGE_DATE);
buff2[0] = buff[0];
buff2[1] = buff[1];
buff2[2] = buff[2];
buff2[3] = buff[3];
buff2[4] = '/';
buff2[5] = buff[4];
buff2[6] = buff[5];
buff2[7] = '/';
buff2[8] = buff[6];
buff2[9] = buff[7];
buff2[10] = '\0';

fprintf(stdout, "Expire date  = %s\n", buff2);
fprintf(stdout, "Serial num   = %08d\n", exp_work.hashed_serial_number - FUDGE_SERIALNO);

sprintf(buff, "%08d", exp_work.plain_date);
buff2[0] = buff[0];
buff2[1] = buff[1];
buff2[2] = buff[2];
buff2[3] = buff[3];
buff2[4] = '/';
buff2[5] = buff[4];
buff2[6] = buff[5];
buff2[7] = '/';
buff2[8] = buff[6];
buff2[9] = buff[7];
buff2[10] = '\0';
fprintf(stdout, "Plain date   = %s\n", buff2);

if (exp_work.net_count)
   {
      fprintf(stdout, "Net Count    = %d\n", exp_work.net_count);
      fprintf(stdout, "Net Id       = %08X  (%s)\n", exp_work.hashed_net_id + FUDGE_DATE,
              my_inet_ntoa((exp_work.hashed_net_id + FUDGE_DATE)<<((4-exp_work.net_count)<<3)));
   }
else
   fprintf(stdout, "Net Count    = 0\n");



work_check_sum = exp_work.check_sum;
exp_work.check_sum = FUDGE_INTERNALSUM;

work_check_sum2 = strsum((char *)&exp_work, sizeof(struct exp_date));

if ((work_check_sum - FUDGE_INTERNALSUM) != work_check_sum2)
   fprintf(stdout, "Check sum is   NO GOOD\n");
else
   fprintf(stdout, "Check sum is   OK\n");

if (exp_work.lice_present)
   fprintf(stdout, "License Manager code is present\n");
else
   fprintf(stdout, "NO License Manager code is present\n");

if (exp_work.net_count)
   fprintf(stdout, "Program is network locked\n");
else
   if (exp_work.plain_date)
      if (exp_work.plain_date >= 99990000)
         fprintf(stdout, "Program is unlocked\n");
      else
         fprintf(stdout, "Program is date locked with expistamp\n");
   else
      if (exp_work.lice_present)
         fprintf(stdout, "Program will use License Manager\n");
      else
         fprintf(stdout, "Program will refuse to execute\n");

   

fclose(fd) ;
return(0);

}  /* main */

static int strsum(char *s, int l)
{
    int              i ;
    unsigned int     r ;

    r = 0 ;
    for (i = 0 ;  i < l ;  i++)
        r += (unsigned char)s[i];
    return(r % 65535) ;

}  /* strsum */





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

   2.  searched_string     - pointer to char (INPUT)
                             This is the string to scan.  It is not null terminated.
                             Its length is determined from parm searched_string_len.

   3.  searched_string_len - int (INPUT)
                             This is the length of searched_string.

   4.  string_to_find      - pointer to char (INPUT)
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
    if ((searched_string[i] == string_to_find[0]) && (memcmp(&(searched_string[i]), string_to_find, string_to_find_len) == 0))
        return(i);

return(-1);

}  /* end of mem_index */

int reverse_indian(int in)
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

} /* end of reverse_indian */



/* this routine exists because inet_ntoa takes it's parameter in
   a different form on each platform.  Some take the address of the
   s_addr, some take the s_addr, and somme take the address of the
   sin_addr */
static char  *my_inet_ntoa(unsigned long   inet_addr)
{
static char    buff[80];  /* returned value */

sprintf(buff, "%d.%d.%d.%d", 
              ((inet_addr >> 24) & 0x000000FF),
              ((inet_addr >> 16) & 0x000000FF),
              ((inet_addr >> 8) & 0x000000FF),
              (inet_addr & 0x000000FF));

return(buff);

} /* end of my_inet_ntoa */


