/*******************************************************************/
/*                                                                 */
/*         Copyright (C) 1991-1994 Viman Software, Inc.            */
/*                   All Rights Reserved                           */
/*                                                                 */
/*     This Module contains Proprietary Information of Viman       */
/*     Software, Inc., and should be treated as Confidential.      */
/*******************************************************************/

# ifndef _LSERV_H_
# define _LSERV_H_

/*H****************************************************************
* FILENAME    : lserv.h
*
* DESCRIPTION :
*           Contains public function prototypes, macros and defines
*           needed for licensing an app using LicenseServ library.
*
* USAGE       :
*           This file to be included by all users of LicenseServ
*           client library.
*H*/

/* Define _VWINDLL_ for the Windows DLL */

#ifdef _VWINDLL_
# define LSFAR __far
#else
# define LSFAR
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__STDC__) || defined(__cplusplus)
#define LSPROTO
#endif

/* Header file for libraries in LicenseServ Ver 3.2d */

/*------------------------------------------------------------------------*/
/* To inactivate licensing completely, use the following macro which      */
/* will make all LicenseServ functions void:                              */
/*------------------------------------------------------------------------*/

/*
#define NO_LICENSE
*/

/*------------------------------------------------------------------------*/
/* LSAPI constants                                                        */
/*------------------------------------------------------------------------*/

#define  LS_DEFAULT_UNITS  (LS_STATUS_CODE)0xFFFFFFFF
#define  LS_ANY            0x0
#define  LS_USE_LAST       (LS_STATUS_CODE)0x0800FFFF

/*------------------------------------------------------------------------*/
/* Trace level                                                            */
/*------------------------------------------------------------------------*/

#define VLS_NO_TRACE         1  /* This is the default value */
#define VLS_TRACE_KEYS       2
#define VLS_TRACE_FUNCTIONS  4
#define VLS_TRACE_ERRORS     8
#define VLS_TRACE_ALL       16

/*------------------------------------------------------------------------*/
/* Error handling                                                         */
/*------------------------------------------------------------------------*/

#define VLS_ON   1  /* This is the default value */
#define VLS_OFF  0

/*------------------------------------------------------------------------*/
/* Error handling                                                         */
/*------------------------------------------------------------------------*/
#define VLS_EH_SET_ALL  0

/*------------------------------------------------------------------------*/
/* True/False                                                             */
/*------------------------------------------------------------------------*/

#define VLS_TRUE   0
#define VLS_FALSE  1

/*------------------------------------------------------------------------*/
/* Sharing criteria                                                       */
/*------------------------------------------------------------------------*/

#define VLS_NO_SHARING            0
#define VLS_USER_NAME_ID          1
#define VLS_CLIENT_HOST_NAME_ID   2
#define VLS_X_DISPLAY_NAME_ID     3
#define VLS_VENDOR_SHARED_ID      4

/*------------------------------------------------------------------------*/
/* Holding criteria                                                       */
/*------------------------------------------------------------------------*/

#define VLS_HOLD_NONE     0
#define VLS_HOLD_VENDOR   1
#define VLS_HOLD_CODE     2

/*------------------------------------------------------------------------*/
/* Client-server lock mode                                                */
/*------------------------------------------------------------------------*/

#define VLS_NODE_LOCKED        0
#define VLS_FLOATING           1
#define VLS_DEMO_MODE          2
#define VLS_CLIENT_NODE_LOCKED 3

/*------------------------------------------------------------------------*/
/* Locking criteria                                                       */
/*------------------------------------------------------------------------*/

#define VLS_HOST_ID_LOCKING 0
#define VLS_IP_ADDR_LOCKING 1

/*------------------------------------------------------------------------*/
/* License does not have an expiration date                               */
/*------------------------------------------------------------------------*/

#define VLS_NO_EXPIRATION   -1

/*------------------------------------------------------------------------*/
/* Type definitions                                                       */
/*------------------------------------------------------------------------*/

typedef  unsigned long                LS_STATUS_CODE;
typedef  unsigned long                LS_HANDLE;
typedef  struct client_info_struct    VLSclientInfo;
typedef  struct feature_info_struct   VLSfeatureInfo;

/*------------------------------------------------------------------------*/
/* Challenge, ChallengeResponse structs                                  */
/*------------------------------------------------------------------------*/

typedef struct {
  unsigned long   ulReserved;
  unsigned long   ulChallengedSecret;
  unsigned long   ulChallengeSize;
  unsigned char   ChallengeData[30];
} CHALLENGE;

typedef  CHALLENGE                    LS_CHALLENGE;

typedef struct {
  unsigned long ulResponseSize;
  unsigned char ResponseData[16];
} CHALLENGERESPONSE;

/*------------------------------------------------------------------------*/
/* Client and feature information structures                              */
/* To be used in VLSgetClientInfo, VLSgetFeatureInfo and VLShandleInfo    */
/*------------------------------------------------------------------------*/

#define MAXLEN 32

/* Client Information structure */
struct client_info_struct {
  char    user_name[MAXLEN];
  long    host_id;
  char    group[MAXLEN];
  long    start_time;
  long    hold_time;
  long    end_time;
  long    key_id;
  char    host_name[MAXLEN];
  char    x_display_name[MAXLEN];
  char    shared_id_name[MAXLEN];
  int     num_units;
  int     q_wait_time;
  int     is_holding;  /* 0 - yes;  1 - no  */
  int     is_sharing;  /* 0 - yes;  1 - no  */
};

/* Feature Information structure */
struct feature_info_struct {

  char    feature_name[MAXLEN];
  char    version[MAXLEN];
  long    death_day;
  int     num_licenses;
  int     total_resv;
  int     lic_from_resv;
  int     lic_from_free_pool;
  int     is_node_locked; /* 0 - yes node locked; 1 - no */
  int     concurrency;
  int     sharing_crit;
  int     locking_crit;
  int     holding_crit;
  int     num_subnets;
  char    site_license_info[200];
  long    hold_time;
  int     meter_value;
  char    vendor_info[200];
};

/*------------------------------------------------------------------------*/
/* Client version information structure                                   */
/* To be used in VLSgetLibInfo                                            */
/*------------------------------------------------------------------------*/

#define VERSTRLEN 32

typedef struct {
    unsigned long ulInfoCode;
    char szVersion  [VERSTRLEN];
    char szProtocol [VERSTRLEN];
    char szPlatform [VERSTRLEN];
    char szUnused1  [VERSTRLEN];
    char szUnused2  [VERSTRLEN];
    } LS_LIBVERSION;


/*------------------------------------------------------------------------*/
/* Macros for status codes                                                */
/* prefix LS  :  LSAPI status codes                                       */
/* prefix VLS :  Viman status codes                                       */
/*------------------------------------------------------------------------*/

/* The function completed successfully                                    */
#define LS_SUCCESS                   0x0

/* Handle used on call did not describe a valid licensing system context  */
#define LS_BADHANDLE                 (LS_STATUS_CODE)0xC8001001

/* Licensing system could not locate enough available licensing resources */
/* to satisfy the request                                                 */
#define LS_INSUFFICIENTUNITS         (LS_STATUS_CODE)0xC8001002

/* No licensing system could be found with which to perform the function  */
/* invoked                                                                */
#define LS_LICENSESYSNOTAVAILABLE    (LS_STATUS_CODE)0xC8001003

/* The licensing system has determined that the resources used to satisfy */
/* a previous request are no longer granted to the calling software       */
#define LS_LICENSETERMINATED         (LS_STATUS_CODE)0xC8001004

/* The licensing system has no licensing resources that could satisfy the */
/* request.                                                               */
#define LS_NOAUTHORIZATIONAVAILABLE  (LS_STATUS_CODE)0xC8001005

/* The licensing system has licensing resources that could satisfy the    */
/* request, but they are not available at the time of the request         */
#define LS_NOLICENSESAVAILABLE       (LS_STATUS_CODE)0xC8001006

/* Insufficient resources (such as memory) are available to complete the  */
/* request                                                                */
#define LS_NORESOURCES               (LS_STATUS_CODE)0xC8001007

/* The network is unavailable                                             */
#define LS_NO_NETWORK                (LS_STATUS_CODE)0xC8001008

/* A warning occured while looking up an error messge string for the      */
/* LSGetMessage() function                                                */
#define LS_NO_MSG_TEXT               (LS_STATUS_CODE)0xC8001009

/* An unrecognized status code was passed into the LSGetMessage() function*/
#define LS_UNKNOWN_STATUS            (LS_STATUS_CODE)0xC800100A

/* An invalid index was specified in LSEnumProviders() or LSQuery License */
#define LS_BAD_INDEX                 (LS_STATUS_CODE)0xC800100B

/* No additional units are available                                      */
#define LS_NO_MORE_UNITS             (LS_STATUS_CODE)0xC800100C

/* The license associated with the current context has expired. This may  */
/* be due to a time-restriction on the license                            */
#define LS_LICENSE_EXPIRED           (LS_STATUS_CODE)0xC800100D

/* Input buffer is too small, need a bigger buffer                        */
#define LS_BUFFER_TOO_SMALL          (LS_STATUS_CODE)0xC800100E

/* 1. Generic error when a license is denied by a server.
 * If reasons are known, more specific errors are given */
#define VLS_NO_LICENSE_GIVEN 1

/* 2. Application has not been given a name. */
#define VLS_APP_UNNAMED 2

/* 3. Unkown host (Application is given a server name but that 
 * server name doesnt seem to exist) */
#define VLS_HOST_UNKNOWN 3

/* 4. No FILE giving license server name (Application cannot figure 
 * out the license server. User may type in - default) */
#define VLS_NO_SERVER_FILE 4

/* 5. On the specified machine, license server is not RUNNING. */
#define VLS_NO_SERVER_RUNNING 5

/* 6. This /feature is node locked but the request for a key came
 * from a machine other than the host running the LicenseServ server. */
#define VLS_APP_NODE_LOCKED 6

/* 7. LSrelease called when this copy of the application had not 
 * received a valid key from the LicenseServ server. */
#define VLS_NO_KEY_TO_RETURN 7

/* 8. Failed to return the key issued to this copy of the application */
#define VLS_RETURN_FAILED 8

/* 9. End of clients on calling VLSgetClientInfo */
#define VLS_NO_MORE_CLIENTS 9

/* 10. End of features on calling VLSgetFeatureInfo */
#define VLS_NO_MORE_FEATURES 10

/* 11. General error by vendor in calling function etc.  */
#define VLS_CALLING_ERROR 11

/* 12. Internal error in LicenseServ */
#define VLS_INTERNAL_ERROR 12

/* 13. Irrecoverable Internal error in LicenseServ */
#define VLS_SEVERE_INTERNAL_ERROR 13

/* 14. On the specified machine, license server is not responding.
 * (Probable cause - network down, wrong port number, some other
 * application on that port etc.) */
#define VLS_NO_SERVER_RESPONSE 14

/* 15. User/machine excluded */
#define VLS_USER_EXCLUDED 15

/* 16. Unknown shared id */
#define VLS_UNKNOWN_SHARED_ID 16

/* 17. No servers responded to client broadcast */
#define VLS_NO_RESPONSE_TO_BROADCAST 17

/* 18. No such feature recognized by server */
#define VLS_NO_SUCH_FEATURE  18

/* 19. Failed to add license */
#define VLS_ADD_LIC_FAILED  19

/* 20. Failed to delete license */
#define VLS_DELETE_LIC_FAILED  20

/* 21. Last update was done locally */
#define VLS_LOCAL_UPDATE  21

/* 22. Last update was done by the LicenseServ server */
#define VLS_REMOTE_UPDATE  22

/*------------------------------------------------------------------------*/
/* Function Prototypes                                                    */
/*------------------------------------------------------------------------*/

#if defined (_VMSWIN_) || defined (_DOS16_)

#  ifndef WINAPI
#  define WINAPI _far _pascal
#  endif

#else
   /* Not a MS Windows or DOS application */

#  ifndef WINAPI
#  define WINAPI
#  endif

#endif /* MS Windows */

LS_STATUS_CODE  WINAPI LSRequest (
#ifdef LSPROTO
  unsigned char LSFAR *license_system,
  unsigned char LSFAR *publisher_name,
  unsigned char LSFAR *product_name,
  unsigned char LSFAR *version,
  unsigned long LSFAR *units_reqd,
  unsigned char LSFAR *log_comment,
  LS_CHALLENGE  LSFAR *challenge,
  LS_HANDLE     LSFAR *lshandle
#endif /* LSPROTO */
);

LS_STATUS_CODE  WINAPI LSRelease (
#ifdef LSPROTO
  LS_HANDLE        lshandle,
  unsigned long    units_consumed,
  unsigned char    LSFAR *log_comment
#endif /* LSPROTO */
);

LS_STATUS_CODE WINAPI LSUpdate (
#ifdef LSPROTO
  LS_HANDLE      lshandle,
  unsigned long  units_consumed,
  long           LSFAR *new_units_reqd,
  unsigned char  LSFAR *log_comment,
  LS_CHALLENGE   LSFAR *challenge
#endif /* LSPROTO */
);

LS_STATUS_CODE WINAPI LSGetMessage (
#ifdef LSPROTO
  LS_HANDLE        lshandle,
  LS_STATUS_CODE   Value,
  unsigned char    LSFAR *Buffer,
  unsigned long    BufferSize 
#endif
);

LS_STATUS_CODE WINAPI VLSlicense(
#ifdef LSPROTO
  unsigned char LSFAR *feature_name,
  unsigned char LSFAR *version,
  LS_HANDLE     LSFAR *handle
#endif  /* LSPROTO */
);

LS_STATUS_CODE WINAPI VLSsetTraceLevel(
#ifdef LSPROTO
  int trace_level
#endif /* LSPROTO*/
);

LS_STATUS_CODE WINAPI VLSsetServerName(
#ifdef LSPROTO
  char LSFAR *server_name
#endif /* LSPROTO */
);

/* extended only */
LS_STATUS_CODE WINAPI VLSsetHoldTime (
#ifdef LSPROTO
  unsigned char   LSFAR *feature_name,
  unsigned char   LSFAR *version,
  int  hold_time
#endif
);

LS_STATUS_CODE WINAPI VLSerrorHandle ( 
#ifdef LSPROTO
  int errorHandle 
#endif
);

/* Replaces the default error handler for the specified error.
 * Error Handlers are automatically called on error.
 */
LS_STATUS_CODE WINAPI VLSsetErrorHandler(
#ifdef LSPROTO
  LS_STATUS_CODE (*myErrorHandler)(LS_STATUS_CODE, char LSFAR *),
  LS_STATUS_CODE LS_ErrorType
#endif /* LSPROTO */
);

/* extended only */
LS_STATUS_CODE WINAPI VLSsetSharedId (
#ifdef LSPROTO
  int shared_id, 
  int (*mySharedIdFunc) (char LSFAR *)
#endif
);

LS_STATUS_CODE WINAPI VLSsetRemoteRenewalTime(
#ifdef LSPROTO
  unsigned char LSFAR *feature_name,
  unsigned char LSFAR *version,
  int renewal_time     /* renewal time in secs */
#endif /* LSPROTO */
);

LS_STATUS_CODE  WINAPI VLSwhere (
#ifdef LSPROTO
  unsigned char LSFAR *feature_name,
  unsigned char LSFAR *version,
  unsigned char LSFAR *log_comment,
  int           bufferSize,
  char          LSFAR *server_names
#endif
);

LS_STATUS_CODE  WINAPI VLSaddFeature (
#ifdef LSPROTO
  unsigned char LSFAR *license_string,
  unsigned char LSFAR *log_comment,
  LS_CHALLENGE  LSFAR *challenge 
#endif
);

LS_STATUS_CODE  WINAPI VLSdeleteFeature (
#ifdef LSPROTO
  unsigned char LSFAR *feature_name,
  unsigned char LSFAR *version,
  unsigned char LSFAR *log_comment,
  LS_CHALLENGE  LSFAR *challenge 
#endif
);

/* extended only */
LS_STATUS_CODE WINAPI VLSgetVersions (
#ifdef LSPROTO
  char      LSFAR *feature_name, 
  int       bufferSize,
  char      LSFAR *versionList,
  char      LSFAR *log_comment
#endif
);

LS_STATUS_CODE WINAPI VLSgetHandleInfo (
#ifdef LSPROTO
  LS_HANDLE       lshandle,
  VLSclientInfo   LSFAR *client_info
#endif
);

/* Get information about client */
LS_STATUS_CODE WINAPI VLSgetClientInfo (
#ifdef LSPROTO
  unsigned char   LSFAR *feature_name,
  unsigned char   LSFAR *version,
  int             index,
  char            LSFAR *log_comment,
  VLSclientInfo   LSFAR *client_info
#endif /* LSPROTO */
);

/* Get information about feature */
LS_STATUS_CODE WINAPI VLSgetFeatureInfo(
#ifdef LSPROTO
  unsigned char   LSFAR *feature_name,
  unsigned char   LSFAR *version,
  int             index,
  char            LSFAR *log_comment,
  VLSfeatureInfo  LSFAR *feature_info
#endif /* LSPROTO */
);


LS_STATUS_CODE WINAPI VLSgetFeatureFromHandle (
#ifdef LSPROTO
  LS_HANDLE       lshandle,
  char    LSFAR *Buffer,
  unsigned long    BufferSize 
#endif
);

LS_STATUS_CODE WINAPI VLSgetVersionFromHandle (
#ifdef LSPROTO
  LS_HANDLE       lshandle,
  char    LSFAR *Buffer,
  unsigned long    BufferSize 
#endif
);

/* 
Returns the value VLS_LOCAL_UPDATE or VLS_REMOTE_UPDATE
depending on whether the last SUCCESSFUL update was locally done or
done by the LicenseServ server. 
*/
LS_STATUS_CODE WINAPI VLSgetRenewalStatus (
#ifdef LSPROTO
  void
#endif
);

/*
Calling this function makes all future update calls
go directly to the LicenseServ server. 
*/
LS_STATUS_CODE WINAPI VLSdisableLocalRenewal(
#ifdef LSPROTO
  void
#endif
);

/*
Calling this function allows the client libraries to process each
future update and send only those updates which are necessary
to the server. This is the default behaviour and please read the
user manual for further description on the default behaviour.
*/
LS_STATUS_CODE WINAPI VLSenableLocalRenewal(
#ifdef LSPROTO
  void
#endif
);

/* Functions to set and get server port number */
void VLSsetServerPort(
#ifdef LSPROTO
int port_num
#endif
);

/* Function to changed default get hostid func */
LS_STATUS_CODE WINAPI VLSsetHostIdFunc(
#ifdef __STDC__
long (*myGetHostIdFunc)()
#endif
);

/*
Call this function to get a description of the client library version
*/
LS_STATUS_CODE WINAPI VLSgetLibInfo (
#ifdef LSPROTO
LS_LIBVERSION LSFAR * pInfo
#endif
);


long WINAPI VLSgetNWerrno(
#ifdef LSPROTO
  void
#endif
);

int VLSgetServerPort(
#ifdef LSPROTO
  void
#endif
);

LS_STATUS_CODE  WINAPI VLSinitialize(
#ifdef LSPROTO
  void
#endif
);
   
LS_STATUS_CODE WINAPI  VLScleanup(
#ifdef LSPROTO
  void
#endif
);

LS_STATUS_CODE WINAPI VLSbiosWait(
#ifdef LSPROTO
unsigned waitTime /* In seconds */
#endif
);

LS_STATUS_CODE  WINAPI VLSsetTimeoutInterval
(
#ifdef __STDC__
long interval
#endif
);

LS_STATUS_CODE  WINAPI VLSsetBroadcastInterval
(
#ifdef __STDC__
long interval
#endif
);

/*------------------------------------------------------------------------*/
/* Macros with default licensing values                                   */
/*------------------------------------------------------------------------*/

#define VLS_REQUEST(feature_name, version, handle_addr)                  \
        LSRequest(LS_ANY,(unsigned char LSFAR *) "Viman LicenseServ User", \
        (unsigned char LSFAR *)feature_name, (unsigned char LSFAR *)version, \
        (unsigned long LSFAR *)NULL, (unsigned char LSFAR *)NULL, \
        (LS_CHALLENGE LSFAR *)NULL, handle_addr)

#define VLS_RELEASE(handle)                                              \
        LSRelease(handle, LS_DEFAULT_UNITS, (unsigned char LSFAR *)NULL)

#define VLS_UPDATE(handle)                                               \
        LSUpdate(handle, LS_DEFAULT_UNITS, (long LSFAR *)NULL, \
        (unsigned char LSFAR *)NULL, (LS_CHALLENGE LSFAR *)NULL)

#if defined (_VMSWIN_) || defined (_DOS16_)
#define sleep(X) \
    VLSbiosWait(X)       
#endif
/*
For backward compatibility, we retain the macros and set them
to corresponding functions
*/
#define VLS_INITIALIZE() VLSinitialize()
#define VLS_CLEANUP() VLScleanup()

/*------------------------------------------------------------------------*/
/* Macros which will make all LicenseServ functions void:                 */
/*------------------------------------------------------------------------*/

#ifdef NO_LICENSE
#define LSRequest(a1,a2,a3,a4,a5,a6,a7,a8)  (LS_SUCCESS)
#define LSRelease(a1,a2,a3)                 (LS_SUCCESS)
#define LSUpdate(a1,a2,a3,a4,a5)            (LS_SUCCESS)
#define LSGetMessage(a1,a2,a3,a4)           (LS_SUCCESS)
#define VLSLicense(a1,a2,a3)                (LS_SUCCESS)
#define VLSsetTraceLevel(a1)                (LS_SUCCESS)
#define VLSsetServerName(a1)                (LS_SUCCESS)
#define VLSsetHoldTime(a1)                  (LS_SUCCESS)
#define VLSerrorHandle(a1)                  (LS_SUCCESS)
#define VLSsetErrorHandler(a1,a2)           (LS_SUCCESS)
#define VLSsetSharedID(a1,a2)               (LS_SUCCESS)
#define VLSsetRemoteRenewalTime(a1,a2,a3)   (LS_SUCCESS)
#define VLSwhere(a1,a2,a3,a4,a5)            (LS_SUCCESS)
#define VLSaddFeature(a1,a2,a3)             (LS_SUCCESS)
#define VLSdeleteFeature(a1,a2,a3,a4)       (LS_SUCCESS)
#define VLSgetVersions(a1,a2,a3,a4)         (VLS_NO_SUCH_FEATURE)
#define VLSgetHandleInfo(a1,a2)             (LS_BADHANDLE)
#define VLSgetFeatureFromHandle(a1,a2,a3) (LS_BADHANDLE)
#define VLSgetVersionFromHandle(a1,a2,a3) (LS_BADHANDLE)
#define VLSgetRenewalStatus()     (VLS_LOCAL_UPDATE)
#define VLSgetClientInfo(a1,a2,a3,a4,a5)    (VLS_NO_MORE_CLIENTS)
#define VLSgetFeatureInfo(a1,a2,a3,a4,a5)   (VLS_NO_MORE_FEATURES)
#define VLSdisableLocalRenewal()    (LS_SUCCESS)
#define VLSenableLocalRenewal()     (LS_SUCCESS)
#define  VLSinitialize()      (LS_SUCCESS)
#define VLScleanup()        (LS_SUCCESS)
#define VLSgetNWerrno()       (LS_SUCCESS)
#define VLSbiosWait()       (LS_SUCCESS)
#define VLSsetTimeoutInterval(a1)   (LS_SUCCESS)
#define VLSsetBroadcastInterval(a1)   (LS_SUCCESS)
#define VLSgetLibInfo (a1)    (LS_SUCCESS)

#endif /* NO_LICENSE */

#ifdef __cplusplus
}
#endif

# endif /* _LSERV_H_ */

