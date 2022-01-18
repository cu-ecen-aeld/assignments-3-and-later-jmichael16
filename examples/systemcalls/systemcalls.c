#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include "systemcalls.h"

/**
 * @param cmd the command to execute with system()
 * @return true if the commands in ... with arguments @param arguments were executed 
 *   successfully using the system() call, false if an error occurred, 
 *   either in invocation of the system() command, or if a non-zero return 
 *   value was returned by the command issued in @param.
*/
bool do_system(const char *cmd)
{
  int rc;
  
  openlog(NULL, 0, LOG_USER);

  rc = system(cmd);

  if (rc == 0 && cmd == NULL) {

    syslog(LOG_ERR, "no shell available for system()\n"); 
    return false;

  } else if (rc == -1) {

    syslog(LOG_ERR, "child process could not be created\n");
    return false;

  } else if (rc != 0) {

    syslog(LOG_INFO, "child shell returned non-zero\n");
    return false;

  }

  // no error
  return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
* followed by arguments to pass to the command
* Since exec() does not perform path expansion, the command to execute needs
* to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
* The first is always the full path to the command to execute with execv()
* The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
* using the execv() call, false if an error occurred, either in invocation of the 
* fork, waitpid, or execv() command, or if a non-zero return value was returned
* by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
  va_list args;
  va_start(args, count);
  char * command[count+1];
  int i, status, rc;
  pid_t pid;

  for(i=0; i<count; i++)
  {
    command[i] = va_arg(args, char *);
  }
  // null terminate args (required)
  command[count] = NULL;

  va_end(args);

  /*
   *   Execute a system command by calling fork, execv(),
   *   and wait instead of system (see LSP page 161).
   *   Use the command[0] as the full path to the command to execute
   *   (first argument to execv), and use the remaining arguments
   *   as second argument to the execv() command.
   */

  openlog(NULL, 0, LOG_USER);

  pid = fork();

  if (pid == -1) {

    // fork failure
    syslog(LOG_ERR, "fork failure\n");
    return false;

  } else if (pid == 0) {

    // child process executes execv - only returns if there was
    // an error
    rc = execv(command[0], command);
    syslog(LOG_INFO, "child process has returned %d\n", rc);
    return false;

  } else {
    
    // the parent process executes the following:
    waitpid(pid, &status, 0);
    
    // see if child terminated abnormally
    if ( WIFEXITED(status) && (WEXITSTATUS(status) != 0) ) {

      // child failed
      syslog(LOG_INFO, "child process terminated, WEXITSTATUS %d\n", WEXITSTATUS(status));
      return false;

    }

  } 

  return true;
}

/**
* @param outputfile - The full path to the file to write with command output.  
* This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
  va_list args;
  va_start(args, count);
  char * command[count+1];
  int i, status, rc;
  pid_t pid;

  for(i=0; i<count; i++)
  {
    command[i] = va_arg(args, char *);
  }
  command[count] = NULL;
  
  va_end(args);

  /*
   *   redirect standard out to a file specified by outputfile.
   *   The rest of the behaviour is same as do_exec()
   *   reference: https://stackoverflow.com/questions/14543443/in-c-how-do-you-redirect-stdin-stdout-stderr-to-files-when-making-an-execvp-or
  */

  openlog(NULL, 0, LOG_USER);

  int redirect_fd = open(outputfile, O_WRONLY|O_CREAT);

  pid = fork();

  if (pid == -1) {

    // fork failure
    syslog(LOG_ERR, "fork failure\n");
    return false;

  } else if (pid == 0) {
    
    // redirect stdout to redirect_fd
    if ( (rc = dup2(redirect_fd, STDOUT_FILENO)) < 0 )  {
      syslog(LOG_ERR, "dup2 fail code %d\n", rc);
    }
    close(redirect_fd);

    // child process executes execv - only returns if there was
    // an error
    rc = execv(command[0], command);
    syslog(LOG_INFO, "child process has returned %d\n", rc);
    return false;

  } else {
    
    // the parent process waits
    waitpid(pid, &status, 0);
    
    if ( !WIFEXITED(status) ) {

      syslog(LOG_ERR, "child process terminated abnormally\n");
      return false;

    }

    if (WEXITSTATUS(status)) {

      // child process exit with nonzero return
      syslog(LOG_INFO, "child process WEXITSTATUS %d\n", WEXITSTATUS(status));
      return false;

    }

  } 

  return true;
}
