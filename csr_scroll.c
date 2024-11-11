
#include <stdio.h>   /* /usr/include/stdio.h  */
#include <errno.h>   /* /usr/include/errno.h  */
#include <string.h>  /* /usr/include/string.h */

#include "ceapi.h"

main (int argc, char *argv[])
{
char       buffer[1024];
int        rc;
CeStats   *ce_stats;
char      *back;
char      *pos_line;
int        row;
int        col;

rc = ceApiInit(0);
if (rc != 0)
   {
      fprintf(stderr, "ApiInit failed \n");
      exit(1);
   }


ce_stats = ceGetStats();
if (ce_stats == NULL)
   {
      fprintf(stderr, "Could not get stats\n");
      exit(1);
   }

sprintf(buffer, "pt;tt;pv %d", ce_stats->current_line);

rc = ceXdmc(buffer, 0);
if (rc != 0)
   {
      fprintf(stderr, "Could not send commands insert message\n");
      exit(rc);
   }

exit(rc);

} /* end of main */


