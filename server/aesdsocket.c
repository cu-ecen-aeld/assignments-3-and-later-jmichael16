/* ----------------------------------------------------------------------------
 * @file aesdsocket.c
 * @brief A socket example program
 *
 * Opens stream socket bound to port 9000, listening for and accepting a 
 * connection. Appends data from connection to /var/tmp/aesdsocketdata with
 * newline as packet separator. Sends the full content of /var/tmp/aesdsocketdata
 * to client upon packet reciept. Cleans up and exits on SIGINT or SIGTERM 
 * signals. 
 * 
 * @author Jake Michael, jami1063@colorado.edu
 * @resources various code is leveraged from Beej's Guide to Network Programming:
 *            https://beej.us/guide/bgnet/html/
 *---------------------------------------------------------------------------*/

#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>    
#include <arpa/inet.h>
#include <netdb.h>

// by default logs should go to syslog, but can be optionally redirected 
// to printf for debug purposes by setting macro below to 1
#define REDIRECT_LOG_TO_PRINTF (1)
#if REDIRECT_LOG_TO_PRINTF
  #define LOG(LOG_LEVEL, msg, ...) printf(msg "\n", ##__VA_ARGS__)
#else
  #define LOG(LOG_LEVEL, msg, ...) syslog(LOG_LEVEL, msg, ##__VA_ARGS__)  
#endif

#define PORT "9000"
#define TEMPFILE "/var/tmp/aesdsocketdata"


/* ============================================================================
 *    FUNCTION HEADERS
 * ===========================================================================*/

/* @brief  registers signal handlers
 * @param  none
 * @return 0 upon success, -1 on error
 */
static int register_signal_handlers();

/*
 * @brief   SIGINT and SIGTERM handler
 * @param   signo is the signal identifier
 * @return  none
 */
static void signal_handler(int signo);


/* @brief  cleans up upon error or before exit/return
 * @param  none
 * @return none
 */
void cleanup();


/* @brief  gets human readable ip address string (assuming IPv4)
 * @param  sa, ptr to sockaddr_in to convert
 * @param  dst, ptr to destination buffer with minsize IPNET_ADDRSTRLEN
 * @return dst or NULL if error
 */
char *get_ip_str(const struct sockaddr_in *sa, char *dst);


/* @brief  a finite state machine which sequences through states, guiding 
 *         the flow through accepting a connection, reading a packet, writing 
 *         the packet to tempfile, and then sending tempfile contents back 
 *         through the socket connection
 * @param  none
 * @return -1 on error, 0 on success
 */
int finite_state_machine();


/* @brief  a writing wrapper utility which writes all bytes to fd
 * @param  writestr, ptr to the string to write
 * @param  len, number of bytes to write
 * @return 0 on success, -1 on error
 */
int write_wrapper(int fd, char* writestr, int len);


/* ============================================================================
 *    GLOBALS
 * ===========================================================================*/
static struct addrinfo* server_addr = NULL;
static int sockfd = -1; 
static int peerfd = -1;
static char *recv_buf = NULL;
static int recv_buf_size = 0;



/* ============================================================================
 *      FUNCTION DEFINITIONS
 *============================================================================*/

/* 
 * @brief  the main function
 * @param  argc, the argument count
 * @param  argv[], the array of arguments
 * @return 0 upon success, -1 on error
 */
int main(int argc, char* argv[])
{ 
  int rc; 
  int daemonize_flag = 0;

  // handle daemonize flag from args
  if (argc >= 2) {
    if (!strcmp(argv[1], "-d")) {
      daemonize_flag = 1;
    } else {
      printf("Invalid option: %s\n", argv[1]);
      printf("Usage: %s [options]\n", argv[0]);
      printf("Options: \n\t -d \t Run application as a daemon\n");
      return -1;
    }
  }
  if (daemonize_flag)
    LOG(LOG_INFO, "set to daemonize");

  // register signal handlers
  rc = register_signal_handlers();
  if (rc == -1) exit(EXIT_FAILURE);

  // initialize hints struct
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags    = AI_PASSIVE;   // fills in IP address automatically
  hints.ai_family   = AF_INET;      // IPV4 or IPV6 
  hints.ai_socktype = SOCK_STREAM;  // TCP stream

  // get addrinfo
  rc = getaddrinfo( NULL,           // ip to connect to (NULL for loopback) 
                    PORT,         // port per A5 requirements
                    &hints,         // hints, populated above
                    &server_addr    // return address for linked list
                    );
  if (rc != 0) {
    LOG(LOG_ERR, "getaddrinfo returned !=0"); perror("getaddrinfo()");
    cleanup();
    return -1;
  }

  // open socket associated with getaddrinfo
  sockfd = socket(server_addr->ai_family, server_addr->ai_socktype, 
                  server_addr->ai_protocol);
  if (sockfd == -1) {
    LOG(LOG_ERR, "socket returned -1"); perror("socket()");
    cleanup();
    return -1;
  }

  // set socket options
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1},	sizeof(int)) < 0 ) {
    perror("setsockopt()");
    cleanup();
    return -1;
  } 

  // bind socket to the server_addr returned from getaddrinfo
  rc = bind(sockfd, server_addr->ai_addr, server_addr->ai_addrlen );
  if (rc == -1) {
    LOG(LOG_ERR, "bind returned -1"); perror("bind()");
    cleanup();
    return -1;
  }
  freeaddrinfo(server_addr); // no longer needed as we have sockfd 
  server_addr = NULL;

  if (daemonize_flag) {
    // ignore signals
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    pid_t pid = fork();
    if (pid < 0) {
      cleanup();
      return -1;
    }
    if (pid > 0) {
      // parent exits
      exit(EXIT_SUCCESS); 
    }

    // create a new session and set process group ID
    if (setsid() == -1) {
      perror("setsid()");
    }

    // change working directory to root
    chdir("/");

    // redirect STDIOs to /dev/null
    int devnull = open("/dev/null", O_RDWR);
    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    close(devnull);
  }

  // listen on the socket
  rc = listen(sockfd, 5); // listen, allow up to 5 connections to queue
  if (rc == -1)  {
    LOG(LOG_ERR, "listen returned -1"); perror("listen()");
    cleanup();
    return -1;
  } 

  while(1) 
  {
    int ret = finite_state_machine();
    if (ret == -1) {
      cleanup();
      return -1;
    }
  }

  cleanup();
  return 0; // should never return

} // end main


typedef enum states {
  ACCEPTING_CONNECTIONS,
  READ_PACKET,
  WRITE_FILE_SOCKET_ECHO,
  NUM_STATES
} states_t;

int finite_state_machine() 
{
  static states_t state = ACCEPTING_CONNECTIONS;
  struct sockaddr_in peer_addr;
  socklen_t peer_addr_size = sizeof(peer_addr);

  switch(state)
  {
    case ACCEPTING_CONNECTIONS:
    {  
      LOG(LOG_INFO, "state: ACCEPTING_CONNECTIONS");
      // store file descriptor of accepted connection 
      peerfd = accept(sockfd, (struct sockaddr *) &peer_addr, &peer_addr_size);
      if (peerfd == -1) {
        LOG(LOG_ERR, "accept returned -1"); perror("accept");
        return -1;
      } else {
        char peer_addr_str[INET_ADDRSTRLEN];
        get_ip_str(&peer_addr, peer_addr_str);
        LOG(LOG_INFO, "Accepted connection from %s", peer_addr_str);
        // reset recv_buf
        recv_buf = realloc(recv_buf, 0);
        recv_buf_size = 0;
        state = READ_PACKET;
      } 
      break;

    }

    case READ_PACKET:
    { 
      LOG(LOG_INFO, "state: READ_PACKET");
      int recv_temp_size = 256;
      char recv_temp[recv_temp_size];

      // read from socket until '\n' 
      int ret = recv(peerfd, &recv_temp[0], recv_temp_size, 0);
      LOG(LOG_INFO, "recv returned %d bytes", ret);
      if (ret == -1 && errno != EINTR) {
        LOG(LOG_ERR, "recv returned -1"); perror("recv()");
        free(recv_buf); recv_buf_size = 0; recv_buf = NULL;
        return -1;
      } else if (ret == 0) { // end of file (peer socket shutdown)
        LOG(LOG_INFO, "peer socket shutdown");
        state = ACCEPTING_CONNECTIONS; 
        break;
      } else if (ret > 0) {
        recv_buf = realloc(recv_buf, recv_buf_size + ret);
        memcpy(recv_buf + recv_buf_size, recv_temp, ret);
        recv_buf_size += ret;
        char* packet_delimiter = strchr(recv_temp, '\n');
        if (packet_delimiter != NULL) {  // delimeter found
          state = WRITE_FILE_SOCKET_ECHO;
        }
      }
      break;
    } 

    case WRITE_FILE_SOCKET_ECHO:
    {
      LOG(LOG_INFO, "state: WRITE_FILE_SOCKET_ECHO");
      // write to file
      int tempfd = open(TEMPFILE, O_RDWR | O_CREAT | O_APPEND, 0644);
      if (tempfd == -1) {
        LOG(LOG_ERR, "open() returned -1"); perror("open()");
        free(recv_buf); recv_buf_size = 0; recv_buf = NULL;
        return -1;
      }
      if (-1 == write_wrapper(tempfd, recv_buf, recv_buf_size)) {
        free(recv_buf); recv_buf_size = 0; recv_buf = NULL;
        close(tempfd);
        return -1;
      } 

      // realloc the recieve buffer size to 0
      recv_buf = realloc(recv_buf, 0); recv_buf_size = 0;

      // echo entire file contents to socket
      lseek(tempfd, 0, SEEK_SET);
      char chunk[128];
      int nread = -1;
      while (1) {
        nread = read(tempfd, chunk, 128);
        if (nread == -1 && errno != EINTR) {
          LOG(LOG_ERR, "nread() returned -1"); perror("read()");
          close(tempfd);
          return -1;
        } else if (nread == 0) {
          LOG(LOG_INFO, "EOF detected");
          break;
        }
        LOG(LOG_INFO, "forward to socket");
        write_wrapper(peerfd, chunk, nread);
      }
      
      close(tempfd);
      state = READ_PACKET;
      break;
    }

    default: 
    {
      LOG(LOG_ERR, "OMG how did I get here?");
      return -1;
    }
    
  } // end switch 

  return 0;
} // end finite_state_machine


int write_wrapper(int fd, char* writestr, int len) 
{
  int written = 0;
  while(len) {
    written = write(fd, writestr, len);
    if (written == -1 && errno != EINTR) {
      LOG(LOG_ERROR, "write() returned -1"); perror("write()");
      return -1;
    }
    len -= written;
    writestr += written;
  }

  return 0; 
}

char *get_ip_str(const struct sockaddr_in *sa, char *dst) 
{
  if (dst == NULL)
    return NULL;

  inet_ntop(AF_INET, &(sa->sin_addr), dst, INET_ADDRSTRLEN);
  return dst;
}


void cleanup() 
{
  if (server_addr != NULL) {
    freeaddrinfo(server_addr);
  }

  // check if file exists and if so, delete it
  if (access(TEMPFILE, F_OK) == 0)  {
    remove(TEMPFILE);
  }

  if (recv_buf != NULL) {
    free(recv_buf);
  }

  if (sockfd < 0) {
    close(sockfd);
  }

  if (peerfd < 0) {
    close(peerfd);
  }

}

static int register_signal_handlers() 
{
  // register signal handlers
  if (signal(SIGINT, signal_handler) == SIG_ERR) {
    LOG(LOG_ERR, "cannot register SIGINT"); 
    return -1;
  }
  if (signal(SIGTERM, signal_handler) == SIG_ERR) {
    LOG(LOG_ERR, "cannot register SIGTERM"); 
    return -1;
  }
  return 0;
}


static void signal_handler(int signo)
{
  LOG(LOG_WARN, "Caught signal, exiting");
  cleanup();
  exit(EXIT_SUCCESS);
  
} // end signal_handler

