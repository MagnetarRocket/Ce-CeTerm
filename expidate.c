/*static char *sccsid = "%Z% %M% %I% - %G% %U% AG CS Phoenix";*/
/***************************************************************
*
*  module expidate.c
*
*
*  Routines:
*     expidate_date          -   Check the expiration date against the current date
*     expidate_sum           -   Check the expiration date against the current date
*                                and check the check sum for corruption.  This is
*                                called after expidate_date.
*     expidate_plain          -  Return the expiration date as a string.
*     update_licence_key      -  Update licence server key before it times out.
*     release_licence         -  Return a licence.
*
* Internal:
*     strsum                  - hash sum memory.  This routine also exists in expistamp.c
*     check_network           - get the dotted internet id for this node
*     lserv_init              - get a license from the server
*     not_licensed_at_all     - test on license server return codes
*     license_manager_missing - test on license server return codes
*
*  Note.  The structure expire_stuff which is in this module is updated
*         in the load module by program expistamp.  This allows the
*         same load module to be timestamped over and over.
*
***************************************************************/
    
#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */
#include <time.h>           /* /usr/include/time.h       */
#include <sys/types.h>      /* /usr/include/sys/types.h  */

#ifdef WIN32
#include <process.h>
#define getpid _getpid
#define getuid() -1
#endif

#include    "debug.h"
#include    "expidate.h"
#include    "netlist.h"


static struct exp_date expire_stuff = { MARKER0_VAL,  /* unique marker 1, (hopefully) */
                                        MARKER1_VAL,  /* unique marker 1, (hopefully) */
                                        1,            /* hashed expire date gets inserted here */
                                        2,            /* hashed serial number gets inserted here */
                                        3,            /* check sum goes here                     */
                                        4,            /* plain expire date gets inserted here    */
                                        6,            /* hashed network id gets inserted here    */
                                        0,            /* one byte net_count gets inserted here   */
#ifdef NO_LICENSE
                                        0,            /* license manager code not compiled in    */
#else
                                        1,            /* license manager code compiled in        */
#endif
                                        {0x31, 0x32}} ;      /* two bytes of padding                    */

typedef enum {
   License_ok         = 0,
   No_lice_avail      = 1,
   No_Server          = 2,
   No_licence         = 3
} LICENSE_STATE;


static int strsum(char *s, int l);

static int check_network(char *msg);
static char   *bad_network = "Not valid for this host";

static LICENSE_STATE lserv_init(CE_FEATURE_TYPE   feature_id,
                                LS_HANDLE        *ls_handle,
                                char             *msg);
static char   *bad_lice = "No license";

static int not_licensed_at_all(LS_STATUS_CODE      ls_rc);

static int license_manager_missing(LS_STATUS_CODE      ls_rc);

#ifdef blah
static int ce_getdisplay(char *returned_display);
#endif

/***************************************************************
*  
*  Static data used in licence management.
*  
*  feature_id_to_name  is indexed by the enumeration type CE_FEATURE_TYPE.
*
***************************************************************/

unsigned char  *feature_id_to_name[] = {(unsigned char *)"CE2", (unsigned char *)"CE3", (unsigned char *)"CE4"};

static LS_HANDLE       ls_handle = 0;   /* licence server handle */
static CE_FEATURE_TYPE feature_id_used;   
static CE_FEATURE_TYPE feature_id_needed;   
static char            feature_name_used[10] = "";

static int             lserv_bad = FALSE; /* set to true if license manager challenge test fails. */


/************************************************************************

NAME:      expidate_date

PURPOSE:    Checks the expiration date against the current date.
            It returns TRUE, if it is ok to continue and false
            if time has run out.  This is the "nice" call.
            If it returns false, print a message and exit.
            If it returns true, put in a call to expidate_sum later.

PARAMETERS:

   1.   feature_id      -  CE_FEATURE_TYPE (enum) (INPUT)
                           This is the feature id as specified in 
                           licence server.
                           Values:
                           Ce      -  Ce, the editor
                           Ceterm  -  ceterm

   2.   msg             -  pointer to char (OUTPUT)
                           If False is returned.  The reason for failure
                           is placed in the buffer whose address is passed 
                           in this parameter.
                           msg must be at least 256 chars long.

FUNCTIONS :

   1.   If we are network locked.  Check the network id and work or do not work based on
        the results.

   2.   If we are timestamped, check the time stamp for timeout.

   3.   Call the license server for a key.  If we get one, contininue.  If the
        licence manager is off line and we are root, continue.  Otherwise quit.


OUTPUTS:
   continue  -  int
                TRUE  -   Everything is ok
                FALSE -   Quit, time has run out

*************************************************************************/

int expidate_date(CE_FEATURE_TYPE   feature_id,
                  char             *msg)
{
char            datestr[9];
time_t          current_time;
struct tm     *ct_struct;
char           localtimestr[9];
LICENSE_STATE  init_rc;

#ifdef WIN32
return(TRUE); /* don't check on NT for now */
#endif

feature_id_needed = feature_id;
licence_server_active = 0;
msg[0] = '\0';

if (expire_stuff.net_count > 0)
   {
      return(check_network(msg));
   }

/***************************************************************
*  
*  If an expiration date was specified, use it.
*  
***************************************************************/

if (expire_stuff.hashed_expire_date - FUDGE_DATE)
   {
      sprintf(datestr, "%08d", expire_stuff.hashed_expire_date - FUDGE_DATE);

      current_time        = time(NULL) ;
      ct_struct           = localtime(&current_time) ;

      sprintf(localtimestr, "%04d%02d%02d", ct_struct->tm_year + 1900, ct_struct->tm_mon + 1, ct_struct->tm_mday);

      if (strcmp(localtimestr, datestr) > 0)
         {
            strcpy(msg, "demo copy has expired");
            return(FALSE);
         }
      else
         return(TRUE);
   }

/***************************************************************
*  
*  Contact the licence server.
*  
***************************************************************/

licence_server_active = 1;
ls_handle = 0;
init_rc = lserv_init(feature_id, &ls_handle, msg);
if (init_rc == License_ok)
   return(TRUE);
else
   if ((init_rc == No_Server) && !getuid())
      {
         strcat(msg, ", root allowed");
         return(TRUE);
      }
   else
      return(FALSE);

}  /* expidate_date */


/************************************************************************

NAME:      expidate_sum

PURPOSE:    Checks the expiration date against the current date.
            And then checks the hash sums to see if there is
            evidence of tampering.
            It returns TRUE, if it is ok to continue and false
            if time has run out or there has been tampering.
            If this routine returns false, do not just do an exit.
            Damage the program in such a way that it will run for
            a while and then terminate.

PARAMETERS:

   none

FUNCTIONS :

   1.   If the current date does not match the unhashed version of the current date, return False.

   2.   Process the check sum to attempt to detect corruption.

   3.   If there is corruption, return False, else return true.


OUTPUTS:
   continue  -  int
                TRUE  -   Everything is ok
                FALSE -   This program has been tampered with.

*************************************************************************/

int expidate_sum(void)
{
register int work_check_sum;
register int work_check_sum2;

#ifdef WIN32
   return(TRUE); /* skip test on NT for now */
#endif

if (lserv_bad) /* license manager was tampered with */
   return(FALSE);
if ((expire_stuff.hashed_expire_date - FUDGE_DATE) != expire_stuff.plain_date)
   return(FALSE);

work_check_sum = expire_stuff.check_sum;
expire_stuff.check_sum = FUDGE_INTERNALSUM;

work_check_sum2 = strsum((char *)&expire_stuff, sizeof(struct exp_date));

expire_stuff.check_sum = work_check_sum;

if ((work_check_sum - FUDGE_INTERNALSUM) != work_check_sum2)
   return(FALSE);
else
   return(TRUE);

}  /* expidate_sum */


/************************************************************************

NAME:      expidate_plain

PURPOSE:    Returns the character format of the non-hashed copy of the
            expiration date.

PARAMETERS:

   none

FUNCTIONS :

   1.   Copy the expiration date to a static work string and return the
        address of the string.

OUTPUTS:
   expires    - pointer to char
                This is the exipration date in the form yyyy/mm/dd

*************************************************************************/

char *expidate_plain(char *cmdname)
{
char                buff[80];
static char         buff2[80];  /* static because this is the returned value */
LS_STATUS_CODE      ls_rc;
int                 try_id;
int                 start_id;
VLSfeatureInfo      feature_info;
struct tm          *tm;


#ifdef WIN32
/***************************************************************
*  Don't expire on NT at this point.
***************************************************************/
return("");
#endif


/***************************************************************
*  If we are network stamped, there is no expiration date.
***************************************************************/
if (expire_stuff.net_count > 0)
   return("");

/***************************************************************
*  If we have a hard coded expiration date, use it.
*  Otherwise use the licence server.
***************************************************************/
if (expire_stuff.hashed_expire_date - FUDGE_DATE)
   {
      /***************************************************************
      *  For non-expiring copies, expire date in year 9999, do not
      *  print a string.
      ***************************************************************/
      if (expire_stuff.plain_date >= 99990000)
         return("");

      /***************************************************************
      *  Format the date to make it readable.
      ***************************************************************/
      sprintf(buff, "%d", expire_stuff.plain_date);
      strcpy(buff2, ",  expires xxxx/xx/xx");
      buff2[11] = buff[0];
      buff2[12] = buff[1];
      buff2[13] = buff[2];
      buff2[14] = buff[3];
      buff2[16] = buff[4];
      buff2[17] = buff[5];
      buff2[19] = buff[6];
      buff2[20] = buff[7];

      return(buff2);
   }

/***************************************************************
*  Get the data from the licence server.
*  If we have a key, use it, otherwise use ce
***************************************************************/

if (!licence_server_active)
   return("");

DEBUG1(fprintf(stderr, "VLS: Calling VLSerrorHandle\n");)
VLSerrorHandle(VLS_OFF);

if (strncmp(cmdname, "ceterm", 6) == 0)
   start_id = Ceterm;
else
   start_id = Ce;

feature_info.death_day = VLS_NO_EXPIRATION;  /* initialize for the non-license server case */

for (try_id = start_id; try_id <= MAX_CE_FEATURE_TYPE; try_id++)
{
   DEBUG1(fprintf(stderr, "VLS: Calling VLSgetFeatureInfo\n");)
   /*sprintf(buff, "%s LSRequest, pid %d", feature_id_to_name[try_id], getpid());*/
   ls_rc = VLSgetFeatureInfo(feature_id_to_name[try_id],
                      (unsigned char *)"0",
                      0, /* index, not used */
                      "",
                      &feature_info);
   if (ls_rc == LS_SUCCESS)
      {
         DEBUG1(fprintf(stderr, "VLS: Got feature info for %s\n", feature_id_to_name[try_id]);)
         break;
      }
   else
      {
         DEBUG1(fprintf(stderr, "VLS: Could not get feature info for %s\n", feature_id_to_name[try_id]);)
      }
}

if (ls_rc == LS_SUCCESS)
   if (feature_info.death_day == VLS_NO_EXPIRATION)
      return("");
   else
      {
        tm = localtime((time_t *)&feature_info.death_day);
        sprintf(buff2, ",  expires %04d/%02d/%02d", (tm->tm_year+1900), tm->tm_mon+1, tm->tm_mday);
        return(buff2);
      }
else
   return(""); /* could not get data */

}  /* expidate_plain */


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
   1.   This routine also exists in expistamp.c.  The two copies
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


/************************************************************************

NAME:      check_network

PURPOSE:    Checks the network id against the network of this machine.
            It returns TRUE, if it is ok to continue and false
            if this is the wrong network.

PARAMETERS:

   1.   msg             -  pointer to char (OUTPUT)
                           If False is returned.  The reason for failure
                           is placed in the buffer whose address is passed 
                           in this parameter.
                           msg must be at least 256 chars long.

GLOBAL DATA:
   
   The expire_stuff structure is accessed globally.

FUNCTIONS :

   1.   Unhash the network id

   2.   Get the current host id(s) and apply the netmask.

   3.   compare the saved id agaist the current id.


OUTPUTS:
   continue  -  int
                TRUE  -   Everything is ok
                FALSE -   Quit, wrong network

*************************************************************************/

#define MAX_HOSTS 20

static int check_network(char *msg)
{
int            plain_net;
unsigned long  host_ids[MAX_HOSTS];
unsigned long  netmask[MAX_HOSTS];
unsigned long  high_mask;
int            count;
unsigned int   lcl_net;
int            i;
int            shift;

plain_net = expire_stuff.hashed_net_id + FUDGE_DATE;
DEBUG1(fprintf(stderr, "check_network:  Locked net = 0x%X\n", plain_net);)

if (plain_net == DEFAULT_NET_VAL)
   {
      strcpy(msg, bad_network);
      return(FALSE);
   }



count = netlist(host_ids, MAX_HOSTS, netmask);
if (count == 0)
   {
      strcpy(msg, "Cannot get network address for this node");
      return(FALSE);
   }

shift = (4 - expire_stuff.net_count) << 3;
high_mask = (1 << (expire_stuff.net_count << 3)) - 1;
if (!high_mask)
   high_mask = -1;
DEBUG1(fprintf(stderr, "Locked cnt = %d  shift = %d  high mask = 0x%08X\n", expire_stuff.net_count, shift, high_mask);)

for (i = 0; i < count; i++)
{
   lcl_net = host_ids[i] & netmask[i];
   lcl_net = (lcl_net >> shift) & high_mask;
   DEBUG1(fprintf(stderr, "ip addr    = 0x%X\nnet addr   = 0x%X\n", host_ids[i], lcl_net);)
   if (expire_stuff.net_count == 4) /* whole ip address */
      {
         if ( host_ids[i] == (unsigned)plain_net)
            return(TRUE);
      }
   else
      {
         if (lcl_net == (plain_net & (netmask[i] >> shift)))
            return(TRUE);
      }
}

strcpy(msg, bad_network);
return(FALSE);

} /* end of check_network */


/************************************************************************

NAME:      lserv_init

PURPOSE:    Contact the license server and get a key.

PARAMETERS:

   1.   feature_id      -  CE_FEATURE_TYPE (enum) (INPUT)
                           This is the feature id as specified in 
                           licence server.
                           Values:
                           Ce      -  Ce, the editor
                           Ceterm  -  ceterm

   2. ls_handle         -  pointer to LS_HANDLE (long int) (OUTPUT)
                           This is the license server handle we get.

   3.   msg             -  pointer to char (OUTPUT)
                           If False is returned.  The reason for failure
                           is placed in the buffer whose address is passed 
                           in this parameter.
                           msg must be at least 256 chars long.

GLOBAL DATA:
   
   1. feature_id_to_name  -  array of pointer to char (INPUT)
                             This is the list of license server feature
                             names corresponding to the Ce features.

   2. feature_id_used     -  CE_FEATURE_TYPE (enum) (OUTPUT)
                             This tells what type of license key we actually got.
                             A ceterm needs a ceterm licence.  A ce can be
                             satisfied by either type of license.

   3. feature_id_needed   -  CE_FEATURE_TYPE (enum) (OUTPUT)
                             This tells whether we need a Ce or a Ceterm license.

   4. feature_name_used   -  char string (OUTPUT)
                             This is the feature name (from feature_id_to_name) that
                             was actually used to get the ls_handle.

FUNCTIONS :

   1.   For each type of applicable key, request a key.


OUTPUTS:
   rc        -  LICENSE_STATE
                License_ok    - Everything is ok
                No_lice_avail - No license available
                No_Server     - Cannot contact server
                No_licence    - No licenses at all

*************************************************************************/

static LICENSE_STATE lserv_init(CE_FEATURE_TYPE   feature_id,
                                LS_HANDLE        *ls_handle,
                                char             *msg)
{
#ifndef NO_LICENSE
LS_STATUS_CODE      ls_rc;
int                 try_id;
unsigned long       units_reqd = 1;
char                buff[256];
LS_CHALLENGE        challenge;
CHALLENGERESPONSE  *chalresp;
/* hardcoded expected response to the license server challenge */
unsigned int        c1 = 0x506BB3BC;
unsigned int        c2 = 0xC6E05A8C;
unsigned int        c3 = 0x77F696B1;
unsigned int        c4 = 0xE227DAF1;
char               *p;

unsigned int       *test_int;
#endif

#ifdef NO_LICENSE
strcpy(msg, bad_lice);
return(No_licence); /* in non-license server mode, if not expistamped, fail */
#else

#ifdef VLS_INITIALIZE
DEBUG1(fprintf(stderr, "VLS: Calling VLS_INITIALIZE");)
ls_rc = VLS_INITIALIZE();
if (ls_rc != LS_SUCCESS)
   {
      LSGetMessage(*ls_handle, ls_rc, (unsigned char *)msg, 256); /* buffer is at least 256 long */
      DEBUG1(fprintf(stderr, "VLS_INITIALIZE failed\n%s\n", msg);)
      while(p = strchr(msg, '\n')) *p = ' '; /* convert newlines to tabs */
      return(ls_rc);
   }
#endif

DEBUG1(fprintf(stderr, "VLS: Calling VLSerrorHandle\n");)
VLSerrorHandle(VLS_OFF);

#ifdef blah
Hold till we get a resolution from viman.
ls_rc = VLSsetSharedId(VLS_X_DISPLAY_NAME_ID, ce_getdisplay);
if (ls_rc != LS_SUCCESS)
   {
      LSGetMessage(*ls_handle, ls_rc, (unsigned char *)msg, 256); /* buffer is at least 256 long */
      DEBUG1(fprintf(stderr, "VLSsetSharedId failed\n%s\n", msg);)
      while(p = strchr(msg, '\n')) *p = ' '; /* convert newlines to tabs */
      return(ls_rc);
   }
#endif

#ifdef blah
for (try_id = feature_id; try_id <= MAX_CE_FEATURE_TYPE; try_id++)
{
   DEBUG1(fprintf(stderr, "VLS: Calling VLSsetRemoteRenewalTime(%s)\n", feature_id_to_name[try_id]);)
   VLSsetRemoteRenewalTime(feature_id_to_name[try_id], (unsigned char *)"0", 120);
}
#endif
DEBUG1(fprintf(stderr, "VLS: Calling VLSsetRemoteRenewalTime(%s)\n", feature_id_to_name[Ceterm]);)
VLSsetRemoteRenewalTime(feature_id_to_name[Ceterm], (unsigned char *)"0", 120);

/***************************************************************
*  set reserved field to 0
*  Fill in part of the challege data.  Fill it in piecemeal
*  so the string does not appear in tact in the load module.
***************************************************************/
challenge.ulReserved = 0;
challenge.ChallengeData[0] = 'A';
challenge.ChallengeData[1] = 'G';
challenge.ChallengeData[2] = 'C';

/***************************************************************
*  set secret index, server knows the secret corresponding to this index
***************************************************************/
challenge.ulChallengedSecret = 1;
challenge.ChallengeData[3] = 'S';
challenge.ChallengeData[4] = ' ';
challenge.ChallengeData[5] = '-';
challenge.ChallengeData[6] = ' ';
challenge.ChallengeData[7] = 'R';

/***************************************************************
*  Set challenge data
*  Any number from 0x0 to 0xFF is a valid element of the array
***************************************************************/

/***************************************************************
*  set challenge data size
***************************************************************/
challenge.ulChallengeSize = 10;
challenge.ChallengeData[8] = 'E';
challenge.ChallengeData[9] = 'S';
challenge.ChallengeData[10] = '\0';

#ifdef blah
for (try_id = feature_id; try_id <= MAX_CE_FEATURE_TYPE; try_id++)
{
   sprintf(buff, "%s LSRequest, pid %d", feature_id_to_name[try_id], getpid());
   units_reqd = 1;
   DEBUG1(fprintf(stderr, "VLS: Calling LSRequest\n");)
   ls_rc = LSRequest(LS_ANY,
                     (unsigned char *)"Enabling Technologies Group Inc.",
                     feature_id_to_name[try_id],
                     (unsigned char *)"0",
                     &units_reqd,
                     (unsigned char *)buff,
                     &challenge,
                     ls_handle
                    ); 
   if (ls_rc == LS_SUCCESS)
      {
         DEBUG1(fprintf(stderr, "VLS: Got licence key for %s\n", feature_id_to_name[try_id]);)
         strcpy(feature_name_used, (char *)feature_id_to_name[try_id]);
         feature_id_used = try_id;
         chalresp = (CHALLENGERESPONSE *)(&challenge);
         DEBUG1(fprintf(stderr, "ulResponseSize = %d, ResponseData = %02.2X%02.2X%02.2X%02.2X %02.2X%02.2X%02.2X%02.2X %02.2X%02.2X%02.2X%02.2X %02.2X%02.2X%02.2X%02.2X\n",
                                chalresp->ulResponseSize,
                                chalresp->ResponseData[0], chalresp->ResponseData[1], chalresp->ResponseData[2], chalresp->ResponseData[3], 
                                chalresp->ResponseData[4], chalresp->ResponseData[5], chalresp->ResponseData[6], chalresp->ResponseData[7], 
                                chalresp->ResponseData[8], chalresp->ResponseData[9], chalresp->ResponseData[10], chalresp->ResponseData[11], 
                                chalresp->ResponseData[12], chalresp->ResponseData[13], chalresp->ResponseData[14], chalresp->ResponseData[15]);
         )
         /* expected response: 506BB3BC C6E05A8C 77F696B1 E227DAF1 */
         test_int = (unsigned int *)&chalresp->ResponseData[0];
         if (*test_int != c1) lserv_bad = TRUE;
         test_int = (unsigned int *)&chalresp->ResponseData[4];
         if (*test_int != c2) lserv_bad = TRUE;
         test_int = (unsigned int *)&chalresp->ResponseData[8];
         if (*test_int != c3) lserv_bad = TRUE;
         test_int = (unsigned int *)&chalresp->ResponseData[12];
         if (*test_int != c4) lserv_bad = TRUE;
         if (lserv_bad)
            ls_rc = VLS_NO_SUCH_FEATURE;  /* Treat as an error if we do not like this call */
         break;
      }
   else
      {
         DEBUG1(
            LSGetMessage(*ls_handle, ls_rc, (unsigned char *)msg, 256); /* buffer is at least 256 long */
            fprintf(stderr, "Could not get licence key for %s\n%s\n", feature_id_to_name[try_id], msg);
         )
      }
}
#else
sprintf(buff, "%s LSRequest, pid %d", feature_id_to_name[Ceterm], getpid());
units_reqd = 1;
DEBUG1(fprintf(stderr, "VLS: Calling LSRequest\n");)
ls_rc = LSRequest(LS_ANY,
                  (unsigned char *)"Enabling Technologies Group Inc.",
                  feature_id_to_name[Ceterm],
                  (unsigned char *)"0",
                  NULL,      /*&units_reqd,*/
                  (unsigned char *)buff,
                  &challenge,
                  ls_handle
                 ); 
if (ls_rc == LS_SUCCESS)
   {
      DEBUG1(fprintf(stderr, "VLS: Got licence key for %s\n", feature_id_to_name[Ceterm]);)
      strcpy(feature_name_used, (char *)feature_id_to_name[Ceterm]);
      feature_id_used = Ceterm;
      chalresp = (CHALLENGERESPONSE *)(&challenge);
      DEBUG1(fprintf(stderr, "ulResponseSize = %d, ResponseData = %02.2X%02.2X%02.2X%02.2X %02.2X%02.2X%02.2X%02.2X %02.2X%02.2X%02.2X%02.2X %02.2X%02.2X%02.2X%02.2X\n",
                             chalresp->ulResponseSize,
                             chalresp->ResponseData[0], chalresp->ResponseData[1], chalresp->ResponseData[2], chalresp->ResponseData[3], 
                             chalresp->ResponseData[4], chalresp->ResponseData[5], chalresp->ResponseData[6], chalresp->ResponseData[7], 
                             chalresp->ResponseData[8], chalresp->ResponseData[9], chalresp->ResponseData[10], chalresp->ResponseData[11], 
                             chalresp->ResponseData[12], chalresp->ResponseData[13], chalresp->ResponseData[14], chalresp->ResponseData[15]);
      )
      /* expected response: 506BB3BC C6E05A8C 77F696B1 E227DAF1 */
      test_int = (unsigned int *)&chalresp->ResponseData[0];
      if (*test_int != c1) lserv_bad = TRUE;
      test_int = (unsigned int *)&chalresp->ResponseData[4];
      if (*test_int != c2) lserv_bad = TRUE;
      test_int = (unsigned int *)&chalresp->ResponseData[8];
      if (*test_int != c3) lserv_bad = TRUE;
      test_int = (unsigned int *)&chalresp->ResponseData[12];
      if (*test_int != c4) lserv_bad = TRUE;
      if (lserv_bad)
         ls_rc = VLS_NO_SUCH_FEATURE;  /* Treat as an error if we do not like this call */
   }
else
   {
      DEBUG1(
         LSGetMessage(*ls_handle, ls_rc, (unsigned char *)msg, 256); /* buffer is at least 256 long */
         fprintf(stderr, "Could not get licence key for %s\n%s\n", feature_id_to_name[Ceterm], msg);
      )
      licence_server_active = 0;
   }

#endif

if (ls_rc != LS_SUCCESS)
   LSGetMessage(*ls_handle, ls_rc, (unsigned char *)msg, 256); /* buffer is at least 256 long */
else
   msg[0] = '\0';
DEBUG1(fprintf(stderr, "LSRequest rc = %d (%s)\n", ls_rc, msg);)

if (ls_rc == LS_SUCCESS)
   {
      msg[0] = '\0';
      return(License_ok);
   }
else
   if (license_manager_missing(ls_rc))
      return(No_Server);
   else
      if (not_licensed_at_all(ls_rc))
         return(No_licence);
      else
         return(No_lice_avail);

#endif  /* NO_LICENSE not defined */

} /* end of lserv_init */


/************************************************************************

NAME:      update_licence_key

PURPOSE:    Update license server info and try to get a new key if we lost
            something.

PARAMETERS:


   1.   msg             -  pointer to char (OUTPUT)
                           If there is trouble,  the message is placed in
                           this buffer.  Otherwise the buffer is set to
                           a null string.
                           msg must be at least 256 chars long.

GLOBAL DATA:
   
   1. feature_id_to_name  -  array of pointer to char (INPUT)
                             This is the list of license server feature
                             names corresponding to the Ce features.

   2. ls_handle           -  LS_HANDLE (long int) (INPUT/OUTPUT)
                             This is the current license server handle.
                             It is used in the update call.  It is updated
                             if we have to get a new handle.
                          

   3. feature_id_used     -  CE_FEATURE_TYPE (enum) (OUTPUT)
                             This tells what type of license key we actually got.
                             A ceterm needs a ceterm licence.  A ce can be
                             satisfied by either type of license.

   4. feature_id_needed   -  CE_FEATURE_TYPE (enum) (OUTPUT)
                             This tells whether we need a Ce or a Ceterm license.

   5. feature_name_used   -  char string (OUTPUT)
                             This is the feature name (from feature_id_to_name) that
                             was actually used to get the ls_handle.

FUNCTIONS :

   1.   If we are not doing license server code, return that all is ok

   2.   If the handle is not zero, try to update it.  

   3.   If the licence server key is zero, try to get a new one.


OUTPUTS:
   beep_mode -  int
                TRUE  -   License server key retrieval failed, go into beep mode.
                FALSE -   Do not go to beep mode.  key renewal succeeded or
                          server is offline, msg contains error if there is one.

*************************************************************************/

int update_licence_key(char  *msg)
{
LS_STATUS_CODE      ls_rc;
LICENSE_STATE       init_rc;
char               *p;

msg[0] = '\0';
if (!licence_server_active || (ls_handle == 0))
   {
      DEBUG1(fprintf(stderr, "update_licence_key: Not processing licence, licence_server_active = %d, ls_handle = %d \n", licence_server_active, ls_handle);)
      return(FALSE);
   }

ls_rc  = LSUpdate(ls_handle, 1, NULL, (unsigned char *)"", NULL);
if (ls_rc != LS_SUCCESS)
   {
      LSGetMessage(ls_handle, ls_rc, (unsigned char *)msg, 256); /* buffer is at least 256 long */
      while(p = strchr(msg, '\n')) *p = ' '; /* convert newlines to tabs */
   }
DEBUG1(fprintf(stderr, "LSUpdate rc = %d (%s)\n", ls_rc, msg);)
if (ls_rc == LS_SUCCESS)
   {
      msg[0] = '\0';
      return(FALSE);
   }
else
   if (license_manager_missing(ls_rc))
      return(FALSE);
   else
      {
         init_rc = lserv_init(feature_id_needed ,&ls_handle, msg);
         if (init_rc == License_ok)
            return(FALSE);
         else
            return(TRUE);
      }

} /* end of update_licence_key */


/************************************************************************

NAME:      license_manager_missing

PURPOSE:    Check is a License server status code is one of the
            values inidicating that the license server is
            unavailable.

PARAMETERS:

   ls_rc  -  LS_STATUS_CODE (INPUT)
             This is the status code to be checked.

FUNCTIONS :

   1.   If this is one of the status codes indicating that the
        license manager is unreachable, return TRUE.

   2.   Otherwise return false.

OUTPUTS:
   missing   -  int
                TRUE  -   Code indicates that the license manager is unreachable
                FALSE -   Some other kind of error.

*************************************************************************/

static int license_manager_missing(LS_STATUS_CODE      ls_rc)
{
if ((ls_rc == LS_LICENSESYSNOTAVAILABLE) || 
    (ls_rc == LS_NORESOURCES) || 
    (ls_rc == LS_NO_NETWORK) || 
    (ls_rc == VLS_HOST_UNKNOWN) || 
    (ls_rc == VLS_NO_SERVER_FILE) || 
    (ls_rc == VLS_NO_SERVER_RUNNING) || 
    (ls_rc == VLS_INTERNAL_ERROR) || 
    (ls_rc == VLS_SEVERE_INTERNAL_ERROR) || 
    (ls_rc == VLS_NO_SERVER_RESPONSE) || 
    (ls_rc == VLS_NO_RESPONSE_TO_BROADCAST))
   return(TRUE);
else
   return(FALSE);
} /* end of license_manager_missing */


#ifndef NO_LICENSE
/************************************************************************

NAME:      not_licensed_at_all

PURPOSE:    Check is a License server status code is one of the
            values inidicating that there are no licenses for
            the product.  As opposed to no licenses available
            at this time.

PARAMETERS:

   ls_rc  -  LS_STATUS_CODE (INPUT)
             This is the status code to be checked.

FUNCTIONS :

   1.   If this is one of the status codes indicating that there
        are not licenses, return TRUE.

   2.   Otherwise return false.

OUTPUTS:
   missing   -  int
                TRUE  -   Code indicates that the license manager is unreachable
                FALSE -   Some other kind of error.

*************************************************************************/

static int not_licensed_at_all(LS_STATUS_CODE      ls_rc)
{
if ((ls_rc == LS_NOAUTHORIZATIONAVAILABLE) || 
    (ls_rc == LS_LICENSE_EXPIRED) || 
    (ls_rc == VLS_NO_LICENSE_GIVEN) || 
    (ls_rc == VLS_APP_NODE_LOCKED) || 
    (ls_rc == VLS_USER_EXCLUDED) || 
    (ls_rc == VLS_UNKNOWN_SHARED_ID) || 
    (ls_rc == VLS_NO_SUCH_FEATURE))
   return(TRUE);
else
   return(FALSE);
} /* end of not_licensed_at_all */
#endif  /* ndef NO_LICENSE */

void release_licence(void)
{
char   buff[256];

if (ls_handle)
   {
      sprintf(buff, "Release %s handle, pid %d", feature_name_used, getpid());
      DEBUG1(fprintf(stderr, "LSRelease %s\n", buff);)
      LSRelease(ls_handle, 1, (unsigned char *)"");
   }
} /* end of release_licence */


#ifdef blah
Our license server will not handle this 
static int ce_getdisplay(char *returned_display)
{
char                 *p;
char                 *q;
char                  hostname[128];
extern char          *getenv(char *name);

p = getenv("DISPLAY");
if ((p == NULL) || (*p == '\0') || (*p == ':') || (strncmp(p, "unix:", 5) == 0) || (strncmp(p, "local:", 6) == 0))
   {
      /***************************************************************
      *  Save part past colon to paste back onto the host name if needed.
      ***************************************************************/
      if (p)
         q = strchr(p, ':');
      else
         q = NULL;
      gethostname(hostname, sizeof(hostname));
      if (q)
         strcat(hostname, q);
      else
         strcat(hostname, ":0.0");
      strcpy(returned_display, hostname);
      DEBUG1(fprintf(stderr, "ce_getdisplay: building Licence server X name from the host name \"%s\"\n", hostname);)
   }
else
   {
      strcpy(returned_display, p);
      DEBUG1(fprintf(stderr, "ce_getdisplay: building Licence server X name from the display name \"%s\"\n", p);)
   }

} /* end of ce_getdisplay */
#endif


