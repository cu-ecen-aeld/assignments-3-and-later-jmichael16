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

  const char* writefile = argv[1];
  const char* writestr = argv[2];

  // open file 
  stream = fopen(writefile, "w");
  if (!stream) {
    syslog(LOG_ERR, "error opening file %s\n", writefile);
    return 1;
  }

  // write to file
  rc = fputs(writestr, stream);
  if ( rc == EOF ) {
    syslog(LOG_ERR, "cannot write %s to file %s\n", writestr, writefile);
    fclose(stream);
    return 1;
  }

  fclose(stream);

  return 0;

} // end main
