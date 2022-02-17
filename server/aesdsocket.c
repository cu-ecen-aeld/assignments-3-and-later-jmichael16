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

/*
 * global socket variables and flags
 */
struct addrinfo* server_addr = NULL;
int sockfd = -1; 
int peerfd = -1;

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
                    "9000",         // port per A5 requirements
                    &hints,         // hints, populated above
                    &server_addr    // return address for linked list
                    );
  if (rc != 0) {
    LOG(LOG_ERR, "getaddrinfo returned !=0"); perror("getaddrinfo()");
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

  // bind socket to the server_addr returned from getaddrinfo
  rc = bind(sockfd, server_addr->ai_addr, server_addr->ai_addrlen );
  if (rc == -1) {
    LOG(LOG_ERR, "bind returned -1"); perror("bind()");
    cleanup();
    return -1;
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
    int looperror = 0;
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_size = sizeof(peer_addr);
    char peer_addr_str[INET_ADDRSTRLEN];
    
    // store file descriptor of accepted connection
    peerfd = accept(sockfd, (struct sockaddr *) &peer_addr, &peer_addr_size);
    if (peerfd == -1) {
      LOG(LOG_ERR, "accept returned -1"); perror("accept");
      looperror = -1;
      break;
    } 

    get_ip_str(&peer_addr, peer_addr_str);
    LOG(LOG_INFO, "Accepted connection from %s", peer_addr_str);

    while(1)
    {
      // initialize the recieve buffer
      char *recv_buf = NULL;
      int recv_buf_size = 0;

      while(1) 
      {
        int recv_temp_size = 256;
        char recv_temp[recv_temp_size];

        // read from socket until '\n' 
        int ret = recv(peerfd, &recv_temp[0], recv_temp_size, 0);
        if (ret == -1 && errno != EINTR) {
          LOG(LOG_ERR, "recv returned -1"); perror("recv()");
          break;
        }
        if (ret > 0) {
          recv_buf = realloc(recv_buf, recv_buf_size + ret);
        }
        memcpy(recv_buf + recv_buf_size, recv_temp, ret);
        recv_buf_size += ret;
          
        // if the temporary buffer holds '\n', then we can flush 
        // recv_buf to file
        char* packet_delimiter = strchr(recv_temp, '\n');
        if (packet_delimiter != NULL) {
          // delimeter found
          break;
        }
      }

      // write to file
      int tempfd = open(TEMPFILE, O_RDWR | O_CREAT | O_APPEND, 0644);
      if (tempfd == -1) {
        LOG(LOG_ERR, "open() returned -1"); perror("open()");
        looperror = -2;
        break;
      }
      if (-1 == write_wrapper(tempfd, recv_buf, recv_buf_size)) {
        LOG(LOG_ERR, "write_wrapper returned -1");
        looperror = -3;
        break;
      }

      // free the recv buffer
      free(recv_buf);

      // write file contents to socket
      // get length of file
      int templen = lseek(tempfd, 0, SEEK_END);
      lseek(tempfd, 0, SEEK_SET);
      char *buf = malloc(templen);
      
      free(buf);
      close(tempfd);
      
    }

  } // end while

  if (looperror < 0) 
  {
    LOG(LOG_ERR, "looperror returned %d", looperror);
    cleanup();
    return -1;
  }

  cleanup();
  return 0;

} // end main

int write_wrapper(int fd, char* writestr, int len) 
{
  int written = 0;
  while(len) {
    written = write(fd, writestr, len);
    if (written == -1 && errno != EINTR) {
      return -1;
    }
    len -= written;
    writestr += written;
  }

  return len;
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

  // do any cleanup steps
  cleanup();
   
  // delete /var/tmp/aesdsocket

  exit(EXIT_SUCCESS);

} // end signal_handler

