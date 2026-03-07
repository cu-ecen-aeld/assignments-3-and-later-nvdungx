#ifndef __AESDSOCKET_H__
#define __AESDSOCKET_H__

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>

#define SOCKET_DATA_FILE "/var/tmp/aesdsocketdata"
#define SOCKET_PORT 9000
#define BUFFER_SIZE 1024

typedef struct skthread_data
{
    pthread_t thread_id;
    int client_fd;
    struct sockaddr_in client_addr;
    bool thread_complete;
    SLIST_ENTRY(skthread_data) entries;
} skthread_data_t;

SLIST_HEAD(skthread_data_list, skthread_data);

typedef struct sigaction sigaction_t;
typedef struct sockaddr_in sockaddr_in_t;
typedef struct sockaddr sockaddr_t;
#endif