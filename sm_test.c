#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>                    /* /usr/include/time.h         */
#include <X11/SM/SMlib.h>
#include <X11/ICE/ICElib.h>
#ifndef FD_ZERO
#include <sys/select.h>
#endif
#include <signal.h> 
#include <sys/param.h>



/***************************************************************
*  
*  Definition for CURRENT_TIME
*  This macro may be substituted where you need a pointer to
*  char.  such as:
*  printf("%s\n", CURRENT_TIME);
*  The current time mis what the pointer points to.  You can
*  change _t1_ and _t3_ to something else if needed.
*  the _t3_ manipulations take off the \n from the string
*  returned by asctime.
*  
***************************************************************/

time_t       _t1_;
char        *_t3_;
#define CURRENT_TIME    ((_t3_ = asctime(localtime((_t1_ = time(NULL), &_t1_)))),(_t3_[strlen(_t3_)-1]='\0'), _t3_ )



#define MAX_ICE_FDS 50

typedef struct
{
   FILE                   *output_stream;
   char                   *client_id_ptr;
   char                   *prev_id_ptr;
   SmcConn                 sm_connection;
   int                     ice_fd_count;
   int                     ice_fds[MAX_ICE_FDS];
   IceConn                 ice_connections[MAX_ICE_FDS];
}  CALLBACK_DATA;


int xsmc_startup(CALLBACK_DATA *callback_data);


static void ice_watch_proc(IceConn      ice_conn,
                           IcePointer   client_data,
                           Bool         opening, 
                           IcePointer  *watch_data);

void process_xsmp_orders(CALLBACK_DATA   *callback_data,
                         fd_set          *ice_fdset);

static void SaveYourselfProc(SmcConn    smc_conn,
                             SmPointer  client_data,
                             int        save_type,
                             Bool       shutdown,
                             int        interact_style,
                             Bool       fast);

static void DieProc(SmcConn    smc_conn,
                    SmPointer  client_data);

static void SaveCompleteProc(SmcConn    smc_conn,
                             SmPointer   client_data);

static void ShutdownCancelledProc(SmcConn    smc_conn,
                                  SmPointer   client_data);

void daemon_start(int  ignsigcld,
                  int  close_files);

static void catch_quit(int sig);



static CALLBACK_DATA            *callback_data_p; /*global copy for signal catcher */

int main(int argc, char *argv[])
{
CALLBACK_DATA            callback_data;
fd_set                   ice_fdset;
int                      done = False;
int                      i;
int                      rc;
int                      max_fd;
int                      count;
char                     prev_id[80];
int                      ch;

prev_id[0] = '\0';

while ((ch = getopt(argc, argv, "p:")) != EOF)
   switch (ch)
   {
   case 'p':
      strncpy(prev_id, optarg, sizeof(prev_id)); 
      break;

   default  :                                                     
      fprintf(stderr, "sm_test [-p prev_sm_client_id]\n");
      return(1);


   }   /* end of switch on ch */



if (optind == argc)
   callback_data.output_stream  = stderr;
else
   {
      /*daemon_start(True, True);*/
      if ((callback_data.output_stream = fopen(argv[optind], "a")) == NULL)
         {
            fprintf(stderr, "Cannot open %s for append (%s)\n", argv[optind], strerror(errno));
            exit(1);
         }
      else
         setbuf(callback_data.output_stream, NULL);
   }




callback_data.ice_fd_count = 0;
if (prev_id[0])
   callback_data.prev_id_ptr = prev_id;
else
   callback_data.prev_id_ptr = NULL;

rc = xsmc_startup(&callback_data);
if (rc != 0)
   return(rc);

callback_data_p = &callback_data;
signal(SIGHUP, catch_quit);


while(!done)
{
   max_fd = -1;
   FD_ZERO(&ice_fdset);
   for (i = 0; i < callback_data.ice_fd_count; i++)
   {
      if (callback_data.ice_fds[i] >= 0)
         {
            FD_SET(callback_data.ice_fds[i], &ice_fdset);
            if (callback_data.ice_fds[i] > max_fd)
               max_fd = callback_data.ice_fds[i];
         }
   }
   if (max_fd < 0)
      {
         fprintf(callback_data.output_stream, "No ice_fd's to watch, quitting\n");
         rc = 5;
         done = True;
      }
   else
      {
         while((count = select(max_fd+1, &ice_fdset, NULL, NULL, NULL)) < 0)
            fprintf(callback_data.output_stream, "select interupted (%s)\n", strerror(errno));
         process_xsmp_orders(&callback_data, &ice_fdset);
      }

}


} /* end of main */


int xsmc_startup(CALLBACK_DATA *callback_data)
{
char                     msg[1024];
SmcCallbacks             smc_callback_funcs;
Bool                     ok;

callback_data->ice_fd_count = 0;

ok = IceAddConnectionWatch(ice_watch_proc, callback_data);
if (!ok)
   {
      fprintf(callback_data->output_stream, "Call to IceAddConnectionWatch failed\n");
      return(-1);
   }

smc_callback_funcs.save_yourself.callback         = SaveYourselfProc;
smc_callback_funcs.save_yourself.client_data      = (SmPointer)callback_data;
smc_callback_funcs.die.callback                   = DieProc;
smc_callback_funcs.die.client_data                = (SmPointer)callback_data;
smc_callback_funcs.save_complete.callback         = SaveCompleteProc;
smc_callback_funcs.save_complete.client_data      = (SmPointer)callback_data;
smc_callback_funcs.shutdown_cancelled.callback    = ShutdownCancelledProc;
smc_callback_funcs.shutdown_cancelled.client_data = (SmPointer)callback_data;

/*
The context argument indicates how willing the client is to share the ICE connection with other protocols.
If context is NULL, then the caller is always willing to share the connection. If context is not NULL, then
the caller is not willing to use a previously opened ICE connection that has a different non-NULL context
associated with it.
*/

callback_data->sm_connection =  SmcOpenConnection(
                      NULL,  /* networkIdsList, NULL means use env var SESSION_MANAGER */
                      NULL,  /* SmPointer      context */
                      SmProtoMajor,            /* xsmpMajorRev */
                      SmProtoMinor,            /* xsmpMinorRev */
                      SmcSaveYourselfProcMask | SmcDieProcMask | SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask,  /* mask */
                      &smc_callback_funcs,     /* SmcCallbacks * callbacks */
                      callback_data->prev_id_ptr,              /* char *      previousId */
                      &(callback_data->client_id_ptr),         /* char **        /* clientIdRet */
                      sizeof(msg),           /* errorLength */
                      msg);


if (callback_data->sm_connection == NULL)
   {
      fprintf(stderr, "Call to SmcOpenConnection failed (%s)\n", msg);
      return(-2);
  }


#ifdef blah
SmcRequestSaveYourself(callback_data->sm_connection, /* smcConn */
                       SmSaveLocal,                  /* saveType */
                       False,                        /* shutdown */
                       SmInteractStyleNone,          /* interactStyle */
                       False,                        /* fast */
                       False                         /* global */
);
#endif

return(0);

} /* end of xsmc_startup */


static void ice_watch_proc(IceConn      ice_conn,
                           IcePointer   client_data,
                           Bool         opening, 
                           IcePointer  *watch_data)
{
CALLBACK_DATA             *callback_data = (CALLBACK_DATA *)client_data;
int                        i;
int                        temp_fd;

fprintf(callback_data->output_stream, "%s *** ice_watch_proc called opening = %s\n ", CURRENT_TIME,
        (opening ? "True" : "False"));

if (opening)
   {
      callback_data->ice_fds[callback_data->ice_fd_count] =  IceConnectionNumber(ice_conn);
      callback_data->ice_connections[callback_data->ice_fd_count] = ice_conn;
      fprintf(callback_data->output_stream, "    ice_fds[%d] = %d\n", callback_data->ice_fd_count, callback_data->ice_fds[callback_data->ice_fd_count]);
      callback_data->ice_fd_count++;
      *watch_data = client_data; /* ice doc tells us to do this */
   }
else
   {
      temp_fd = IceConnectionNumber(ice_conn);
      for (i = 0; i < callback_data->ice_fd_count; i++)
      {
         if (temp_fd == callback_data->ice_fds[i])
            {
               callback_data->ice_fds[i] = -1;
               if (i == (callback_data->ice_fd_count - 1))
                  callback_data->ice_fd_count--;
            }
      }
         
   }

} /* end of ice_watch_proc */


void process_xsmp_orders(CALLBACK_DATA   *callback_data,
                         fd_set          *ice_fdset)
{
int                      i;

for (i = 0; i < callback_data->ice_fd_count; i++)
   {
       if ((callback_data->ice_fds[i] >= 0) && FD_ISSET(callback_data->ice_fds[i], ice_fdset))
         {
            fprintf(callback_data->output_stream, "%s *** ice_commands for ice_fds[%d]\n", CURRENT_TIME, i);
            IceProcessMessages(callback_data->ice_connections[i], NULL, NULL);
         }
   }
 
         

} /* end of process_xsmp_orders */


static void SaveYourselfProc(SmcConn    smc_conn,
                             SmPointer  client_data,
                             int        save_type,
                             Bool       shutdown,
                             int        interact_style,
                             Bool       fast)
{
CALLBACK_DATA             *callback_data = (CALLBACK_DATA *)client_data;
int                        smc_property_count;
SmProp                     restart_properties[4];                    
SmProp                    *restart_properites_p[4];
SmPropValue                cmd_argv[2];
SmPropValue                restart_argv[4];
SmPropValue                userid_val;
SmPropValue                progname_val;

fprintf(callback_data->output_stream, "%s *** SaveYourselfProc called Save type = %s, shutdown = %s, interact_style = %s, fast = %s\n ",
        CURRENT_TIME,
        ((save_type = SmSaveGlobal) ? "SmSaveGlobal" : ((save_type = SmSaveLocal) ? "SmSaveLocal" : "SmSaveBoth")),
        (shutdown ? "True" : "False"),
        ((interact_style = SmInteractStyleNone) ? "SmInteractStyleNone" : ((interact_style = SmInteractStyleErrors) ? "SmInteractStyleErrors" : "SmInteractStyleAny")),
        (fast ? "True" : "False"));

cmd_argv[0].value  = "/home/stymar/crpad/sm_test";
cmd_argv[0].length = strlen(cmd_argv[0].value);

cmd_argv[1].value  = "/usr/tmp/sm_test.txt";
cmd_argv[1].length = strlen(cmd_argv[1].value);

restart_argv[0].value  = "/home/stymar/crpad/sm_test";
restart_argv[0].length = strlen(restart_argv[0].value);

restart_argv[1].value  = "-p";
restart_argv[1].length = strlen(restart_argv[1].value);

restart_argv[2].value  = callback_data->client_id_ptr;
restart_argv[2].length = strlen(restart_argv[2].value);

restart_argv[3].value  = "/usr/tmp/sm_test.txt";
restart_argv[3].length = strlen(restart_argv[3].value);

userid_val.value  = getenv("USER");
userid_val.length = strlen(userid_val.value);

progname_val.value  = "sm_test";
progname_val.length = strlen(progname_val.value);

restart_properites_p[0] = &restart_properties[0];
restart_properites_p[1] = &restart_properties[1];
restart_properites_p[2] = &restart_properties[2];
restart_properites_p[3] = &restart_properties[3];

restart_properties[0].name     = SmCloneCommand;
restart_properties[0].type     = SmLISTofARRAY8;
restart_properties[0].num_vals = 2;
restart_properties[0].vals     = cmd_argv;

restart_properties[1].name     = SmProgram;
restart_properties[1].type     = SmARRAY8;
restart_properties[1].num_vals = 1;
restart_properties[1].vals     = &progname_val;

restart_properties[2].name     = SmRestartCommand;
restart_properties[2].type     = SmLISTofARRAY8;
restart_properties[2].num_vals = 4;
restart_properties[2].vals     = restart_argv;

restart_properties[3].name     = SmUserID;
restart_properties[3].type     = SmARRAY8;
restart_properties[3].num_vals = 1;
restart_properties[3].vals     = &userid_val;

SmcSetProperties(smc_conn, 
                 4,                    /* numProps */
                 restart_properites_p  /* ptr to props array */
);

SmcSaveYourselfDone(smc_conn, True); /* True is sucess */

} /* end of SaveYourselfProc */


static void DieProc(SmcConn    smc_conn,
                    SmPointer  client_data)
{
CALLBACK_DATA             *callback_data = (CALLBACK_DATA *)client_data;

fprintf(callback_data->output_stream, "%s *** DieProc called\n ", CURRENT_TIME);

IceRemoveConnectionWatch(ice_watch_proc, client_data);
SmcCloseConnection(smc_conn, 0, NULL);
fclose(callback_data->output_stream);
exit(0);

} /* end of DieProc */


static void SaveCompleteProc(SmcConn     smc_conn,
                             SmPointer   client_data)
{
CALLBACK_DATA             *callback_data = (CALLBACK_DATA *)client_data;

fprintf(callback_data->output_stream, "%s *** SaveCompleteProc called\n ", CURRENT_TIME);


} /* end of SaveCompleteProc */

static void ShutdownCancelledProc(SmcConn     smc_conn,
                                  SmPointer   client_data)
{
CALLBACK_DATA             *callback_data = (CALLBACK_DATA *)client_data;

fprintf(callback_data->output_stream, "*** ShutdownCancelledProc called\n ");


} /* end of ShutdownCancelledProc */




static void catch_quit(int sig)
{

fprintf(callback_data_p->output_stream, "%s *** Caught signal %d\n ", CURRENT_TIME, sig);

IceRemoveConnectionWatch(ice_watch_proc, callback_data_p);
SmcCloseConnection(callback_data_p->sm_connection, 0, NULL);
fclose(callback_data_p->output_stream);
exit(0);

}  /* end of catch_quit */


