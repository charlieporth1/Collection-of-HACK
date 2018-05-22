/*
 *    WIN32 supplement for "nte".
 */

#include <stdlib.h>

/* 2017-06-01 SMS.
 *
 * win_basename()
 *
 *    Extract the basename from a Windows file spec.
 */
static char basename[ _MAX_FNAME];

char *win_basename( char *file_spec)
{
  errno_t sts;

  sts = _splitpath_s( file_spec,
                      NULL, 0,                          /* Drive. */
                      NULL, 0,                          /* Directory. */
                      basename, sizeof( basename),      /* Name. */
                      NULL, 0);                         /* Extension. */

  return basename;
}
/*--------------------------------------------------------------------*/
