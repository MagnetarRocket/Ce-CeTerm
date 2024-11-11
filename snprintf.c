#ifndef HAVE_SNPRINTF
/***************************************************************
*  
*  snprintf for machines which do not have it, like Dec Alpha.
*  
***************************************************************/



#include <stdio.h>
#include <stdarg.h>

int snprintf(char * buf,
             size_t size,
             const char * format,
             ...) {

  int ret;
  va_list args;

  va_start(args, format);
  ret = vsprintf(buf,
                 format,
                 args);
  va_end(args);
  return ret;
}
#endif

