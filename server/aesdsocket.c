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
 * @resources 
 * (+)  various code is leveraged from Beej's Guide to Network Programming:
 * https://beej.us/guide/bgnet/html/
 * (+)  used example code for FreeBSD queue.h as found here: 
 * https://blog.taborkelly.net/programming/c/2016/01/09/sys-queue-example.html
 *---------------------------------------------------------------------------*/

#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>    
#include <arpa/inet.h>
#include <netdb.h>
#include "queue.h"

// by default logs should go to syslog, but can be optionally redirected 
// to printf for debug purposes by setting macro below to 1
#define REDIRECT_LOG_TO_PRINTF (0)
#if REDIRECT_LOG_TO_PRINTF
  #define LOG(LOG_LEVEL, msg, ...) printf(msg "\n", ##__VA_ARGS__)
#else
  #define LOG(LOG_LEVEL, msg, ...) syslog(LOG_LEVEL, msg, ##__VA_ARGS__)  
#endif

#define PORT "9000"
#define TEMPFILE "/var/tmp/aesdsocketdata"

/* ============================================================================
 *    GLOBALS
 * ===========================================================================*/
static int sockfd = -1; // socket file descriptor
static int tempfd = -1; // TEMPFILE file descriptor
static volatile bool global_abort = false;
typedef TAILQ_HEAD(head_s, node) head_t;

/* ============================================================================
 *    THREAD VARIABLES AND SYNCHRONIZATION
 * ===========================================================================*/
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
  int peerfd;
} thread_params_t;

typedef struct node {
  pthread_t thread;
  TAILQ_ENTRY(node) nodes;
} node_t;

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


/* @brief  gets human readable ip address string (assuming IPv4)
 * @param  sa, ptr to sockaddr_in to convert
 * @param  dst, ptr to destination buffer with minsize IPNET_ADDRSTRLEN
 * @return dst or NULL if error
 */
static char *get_ip_str(const struct sockaddr_in *sa, char *dst);


/* @brief  a writing wrapper utility which writes all bytes to fd
 * @param  writestr, ptr to the string to write
 * @param  len, number of bytes to write
 * @return 0 on success, -1 on error
 */
static int write_wrapper(int fd, char* writestr, int len);


/* @brief  turns the process into a daemon
 * @param  none
 * @return int, 0 on success, -1 on error
 */
static int daemonize_proc();


/* @brief  handles a socket connection
 * @param  void* param, ptr to data to pass into thread
 * @return void*, a return pointer
 */
void* connection_thread(void *param);


/* @brief  handles printing timestamp
 * @param  void* param, ptr to data to pass into thread
 * @return void*, a return pointer
 */
void* timestamp_thread(void *param);



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
  struct addrinfo* server_addr = NULL;
  
  // if we can access the temfile, it is stale
  if (access(TEMPFILE, F_OK) == 0) {
    remove(TEMPFILE); // no lock necessary, all the threads have joined
  }

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
  if (rc == -1) {
    LOG(LOG_ERR, "could not register signal handlers");
    exit(EXIT_FAILURE);
  } 

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
    return -1;
  }

  // open socket associated with getaddrinfo
  sockfd = socket(server_addr->ai_family, server_addr->ai_socktype, 
                  server_addr->ai_protocol);
  if (sockfd == -1) {
    LOG(LOG_ERR, "socket returned -1"); perror("socket()");
    freeaddrinfo(server_addr); 
    return -1;
  }

  // set socket options
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1},	sizeof(int)) < 0 ) {
    perror("setsockopt()");
    freeaddrinfo(server_addr); 
    return -1;
  } 

  // bind socket to the server_addr returned from getaddrinfo
  rc = bind(sockfd, server_addr->ai_addr, server_addr->ai_addrlen );
  if (rc == -1) {
    LOG(LOG_ERR, "bind returned -1"); perror("bind()");
    freeaddrinfo(server_addr); 
    return -1;
  }
  freeaddrinfo(server_addr); // no longer needed as we have sockfd 

  if (daemonize_flag) {
    if ( (rc = daemonize_proc()) == -1) {
      LOG(LOG_ERR, "process cannot be daemonized");
      return -1;
    }
  }

  // listen on the socket
  rc = listen(sockfd, 10); // listen, allow connections to queue
  if (rc == -1)  {
    LOG(LOG_ERR, "listen returned -1"); perror("listen()");
    return -1;
  } 

  pthread_t tsthread;
  rc = pthread_create(&tsthread, NULL, timestamp_thread, NULL);
  if (rc != 0) {
    LOG(LOG_ERR, "timestamp thread could not be created, returned %d", rc);
    return -1;
  }

  // initialize linked-list
  head_t head;
  TAILQ_INIT(&head);

  struct pollfd pfd[1];
  pfd[0].fd = sockfd;
  pfd[0].events = POLLIN;
  int timeout = 500;

  while(!global_abort) 
  {
    int peerfd_temp = -1;
    int thread_count = 0;
    int rc = -1;
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_size = sizeof(peer_addr);

    poll(pfd, 1, timeout);
    if (pfd[0].revents != POLLIN) {
      continue;
    }
    // store file descriptor of accepted connection 
    peerfd_temp = accept(sockfd, (struct sockaddr *) &peer_addr, &peer_addr_size);
    if (peerfd_temp == -1) {
      LOG(LOG_ERR, "accept returned -1"); perror("accept");
      continue;
    } else {
      // print human-readable IP address
      char peer_addr_str[INET_ADDRSTRLEN];
      get_ip_str(&peer_addr, peer_addr_str);
      LOG(LOG_INFO, "Accepted connection from %s, spawning new thread", peer_addr_str);

      // malloc all the things
      struct node* new_node = malloc(sizeof(struct node));
      thread_params_t* new_params = malloc(sizeof(thread_params_t));
      if ( !new_node || !new_params ) {
        LOG(LOG_ERR, "malloc fail"); perror("malloc");
        exit(EXIT_FAILURE); // we cannot recover from this
      }

      // populate all the things
      new_params->peerfd = peerfd_temp;
      // spawn new thread
      rc = pthread_create(&(new_node->thread), NULL, connection_thread, (void*) new_params);
      if (rc != 0) {
        LOG(LOG_ERR, "pthread_create returned %d", rc);
        shutdown(peerfd_temp, SHUT_RDWR);
        close(peerfd_temp);
        free(new_params); free(new_node);
        continue;
      }
      LOG(LOG_INFO, "Thread spawn success");
      TAILQ_INSERT_TAIL(&head, new_node, nodes); 
      thread_count++;
    } 
  } // end while()

  if ( shutdown(sockfd, SHUT_RDWR) == -1)
  {
    LOG(LOG_ERR, "shutdown fail"); perror("shutdown");
  }
  close(sockfd);

  // join and cleanup all the socket threads
  LOG(LOG_INFO, "Joining all threads");
  struct node* anode = NULL;
  void *retval = NULL;
  while (!TAILQ_EMPTY(&head)) {
    // get first element, join thread remove from tailqueue and free
    anode = TAILQ_FIRST(&head);
    rc = pthread_join(anode->thread, &retval);
    TAILQ_REMOVE(&head, anode, nodes);
    free(anode);
    anode = NULL;
  }

  // join timestamp thread
  pthread_join(tsthread, &retval);

  if (access(TEMPFILE, F_OK) == 0) {
    remove(TEMPFILE); // no lock necessary, all the threads have joined
  }
  
  return 0; 

} // end main


void* connection_thread(void* params) 
{
  // copy value from params
  int peerfd = 0; 
  int size_step = 128;
  int recv_buf_size = size_step;
  char* recv_buf = calloc(size_step, sizeof(char));
  int recv_buf_nbytes = 0;
  peerfd = ((thread_params_t*) params)->peerfd;
  free(params);
  
  while(!global_abort) // continuously read/write 
  {
    recv_buf_nbytes = 0;

    while(!global_abort) // read from socket until '\n' 
    {
      int recv_temp_size = 32;
      char recv_temp[recv_temp_size];
      memset(recv_temp, 0, recv_temp_size);
      int ret = recv(peerfd, &recv_temp[0], recv_temp_size-1, 0);
      if (ret == -1 && errno != EINTR) {
        LOG(LOG_ERR, "recv returned -1"); perror("recv()");
        goto handle_errors;
      } else if (ret == 0) { // end of file (peer socket shutdown)
        LOG(LOG_INFO, "Peer socket shutdown");
        goto handle_errors;
      } else if (ret > 0) {
        /*recv_buf = realloc(recv_buf, recv_buf_size + ret);
        memcpy(recv_buf + recv_buf_size, recv_temp, ret);
        recv_buf_size += ret;
        char* packet_delimiter = strchr(recv_temp, '\n');
        if (packet_delimiter != NULL) {  // delimeter found
          break;
        }*/
        // recv_buf not big enough, need to realloc
        if (recv_buf_nbytes + ret > recv_buf_size) {
          recv_buf = realloc(recv_buf, recv_buf_size + size_step);
          recv_buf_size += size_step;
        } 
        memcpy(&recv_buf[recv_buf_nbytes], &recv_temp[0], ret);
        recv_buf_nbytes += ret;
       
        char* delimiter = NULL;
        if ( (delimiter = strchr(recv_temp, '\n')) != NULL ) {  // delimeter found
          break;
        }
      }
    } // end while()

    // write to file
    // wait for the lock
    pthread_mutex_lock(&file_lock);
    tempfd = open(TEMPFILE, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (tempfd == -1) {
      LOG(LOG_ERR, "open() returned -1"); perror("open()");
      goto handle_errors;
    }
    if (-1 == write_wrapper(tempfd, recv_buf, recv_buf_nbytes)) {
      goto handle_errors;
    } 

    // echo entire file contents to socket
    lseek(tempfd, 0, SEEK_SET);
    int chunk_size = 256;
    char chunk[chunk_size];
    int nread = -1;

    while (!global_abort) 
    {
      nread = read(tempfd, chunk, chunk_size);
      if (nread == -1 && errno != EINTR) {
        LOG(LOG_ERR, "read() returned -1"); perror("read()");
        LOG(LOG_ERR, "fd %d", tempfd);
        goto handle_errors;
      } else if (nread == 0) {
        LOG(LOG_INFO, "EOF detected, socket send complete");
        break;
      }
      if (-1 == write_wrapper(peerfd, chunk, nread)) {
        goto handle_errors;
      }
    } // end while()

    close(tempfd);
    pthread_mutex_unlock(&file_lock);

  } // end while()

  shutdown(peerfd, SHUT_RDWR);
  close(peerfd);
  pthread_exit(NULL);

handle_errors:
  if (recv_buf != NULL)
    free(recv_buf);
  close(tempfd);
  pthread_mutex_unlock(&file_lock);
  shutdown(peerfd, SHUT_RDWR);
  close(peerfd);
  pthread_exit(NULL);

} // end connection_thread

void* timestamp_thread(void *param) 
{
  char timestr[128];
  struct tm *timestamp;
  time_t t;
  struct timespec start_time = { 0, 0 };
  int rc;

  while(!global_abort) 
  {
      if (-1 == clock_gettime(CLOCK_MONOTONIC, &start_time)) {
        LOG(LOG_ERR, "clock gettime returned -1"); perror("clock_gettime");
      }
      memset(timestr, 0, 128);
      t = time(NULL);
      timestamp = localtime(&t);
      if (timestamp == NULL) {
        LOG(LOG_ERR, "localtime returned NULL");
        pthread_exit(NULL);
      }
      if(strftime(timestr, sizeof(timestr), 
         "timestamp:%a, %d %b %Y %T %z\n", timestamp) == 0) {
        LOG(LOG_ERR, "strftime returned 0");
        pthread_exit(NULL);
      }
      pthread_mutex_lock(&file_lock);
      tempfd = open(TEMPFILE, O_RDWR | O_CREAT | O_APPEND, 0644);
      if (tempfd == -1) {
        LOG(LOG_ERR, "timestamp_thread open() returned -1"); perror("open()");
        pthread_mutex_unlock(&file_lock);
        break;
      }
      if (-1 == write_wrapper(tempfd, timestr, strlen(timestr))) {
        LOG(LOG_ERR, "timestamp_thread write_wrapper fail");
        close(tempfd);
        pthread_mutex_unlock(&file_lock);
        break;
      }
      
      close(tempfd);
      pthread_mutex_unlock(&file_lock);
      start_time.tv_sec += 10;
      if ( (rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &start_time, NULL)) != 0) {
        if (errno == EINTR) { // signal handler interrupt
          break;
        }
        else {
          LOG(LOG_ERR, "clock_nanosleep error %d", rc);
        }
      }
  } // end while()

  pthread_exit(NULL);
}


static int daemonize_proc() 
{
  // ignore signals
  signal(SIGCHLD, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  pid_t pid = fork();
  if (pid < 0) {
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
  return 0;
}


static int write_wrapper(int fd, char* writestr, int len) 
{
  int written = 0;
  while(len) {
    written = write(fd, writestr, len);
    if (written == -1 && errno != EINTR) {
      LOG(LOG_ERR, "write() returned -1"); perror("write()");
      LOG(LOG_ERR, "fd %d", fd); 
      return -1;
    }
    len -= written;
    writestr += written;
  }

  return 0; 
}

static char *get_ip_str(const struct sockaddr_in *sa, char *dst) 
{
  if (dst == NULL)
    return NULL;

  inet_ntop(AF_INET, &(sa->sin_addr), dst, INET_ADDRSTRLEN);
  return dst;
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
  LOG(LOG_WARNING, "Caught signal, setting abort flag");
  global_abort = true;
} // end signal_handler

