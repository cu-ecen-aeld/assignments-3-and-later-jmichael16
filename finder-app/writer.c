/* ---------------------------------------------------------------------------- 
 * @file writer.c
 * @brief A writing application which writes the string writestr to writefile
 * @usage ./writer </path/to/writefile> <writestr>
 *        where directory /path/to/ must exist on the filesystem, but writefile
 *        will be created / overwritten
 * @author Jake Michael, jami1063@colorado.edu
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <sys/types.h>
#include <syslog.h>

#define CLI_MAX_ARGS (3) 

int main(int argc, char **argv) {
  
  FILE *stream;
  int rc;

  // open the syslog
  openlog(NULL, 0, LOG_USER);

  // check number of arguments is valid
  if (argc < CLI_MAX_ARGS) {
    syslog(LOG_ERR, "invalid number of arguments, %d provided 3 expected", argc);
    return 1;
  }

  // open file 
  stream = fopen(argv[1], "w");
  if (!stream) {
    syslog(LOG_ERR, "error opening file %s\n", argv[1]);
    return 1;
  }

  // write to file
  rc = fputs(argv[2], stream);
  if ( rc == EOF ) {
    syslog(LOG_ERR, "cannot write %s to file %s\n", argv[2], argv[1]);
    fclose(stream);
    return 1;
  }

  fclose(stream);

  return 0;

} // end main
