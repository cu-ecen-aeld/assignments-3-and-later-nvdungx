#include "aesdsocket.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <error.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <syslog.h>
#include <argp.h>

typedef struct sigaction sigaction_t;
typedef struct sockaddr_in sockaddr_in_t;
typedef struct sockaddr sockaddr_t;

#define SOCKET_DATA_FILE "/var/tmp/aesdsocketdata"
#define SOCKET_PORT 9000
#define BUFFER_SIZE 1024

volatile sig_atomic_t exit_flag = 0;

static void signal_handler(int signo)
{
    exit_flag = 1;
    if (signo == SIGTERM || signo == SIGINT)
    {
        syslog(LOG_INFO, "Signal caught, exit!!");
    }
}

int register_signal_handler(void)
{
    int ret = 0;
    sigaction_t act;
    memset(&act, 0, sizeof(sigaction_t));
    act.sa_handler = signal_handler;
    if(sigaction(SIGINT, &act, NULL) != 0)
    {
        syslog(LOG_ERR, "Error: fail to register SIGINT - %s", strerror(errno));
        ret = -1;
    }
    if(sigaction(SIGTERM, &act, NULL) != 0)
    {
        syslog(LOG_ERR, "Error: fail to register SIGTERM - %s", strerror(errno));
        ret = -1;
    }
    if (ret == 0)
    {
        syslog(LOG_INFO, "signal_handler is registered");
    }
    return ret;
}

void handle_client(int client_fd, struct sockaddr_in *client_addr)
{
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));

    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    char *packet = NULL;
    size_t total_size = 0;
    char buffer[BUFFER_SIZE];

    while (1)
    {
        ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);

        if (bytes <= 0)
        {
            break;
        }

        char *new_packet = realloc(packet, total_size + bytes);
        if (!new_packet)
        {
            syslog(LOG_ERR, "Memory allocation failed");
            free(packet);
            break;
        }

        packet = new_packet;
        memcpy(packet + total_size, buffer, bytes);
        total_size += bytes;

        if (memchr(buffer, '\n', bytes))
        {
            break;
        }
    }

    int fd = open(SOCKET_DATA_FILE, O_CREAT | O_APPEND | O_WRONLY, 0644);

    if (fd >= 0 && packet)
    {
    	syslog(LOG_INFO, "Open %s success", SOCKET_DATA_FILE);
        ssize_t wr_bytes = write(fd, packet, total_size);
        if(wr_bytes < total_size)
        {
            syslog(LOG_ERR, "ERROR: Failed write to %s", SOCKET_DATA_FILE);
        }
        else
        {
            syslog(LOG_INFO, "Data appended to %s", SOCKET_DATA_FILE);
        }
        close(fd);
    }

    fd = open(SOCKET_DATA_FILE, O_RDONLY);
    if (fd >= 0)
    {
        ssize_t bytes_read;

		while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
		{
			ssize_t bytes_sent = send(client_fd, buffer, bytes_read, 0);
			if (bytes_sent < 0)
			{
				syslog(LOG_ERR, "ERROR: Send failed - %s", strerror(errno));
				break;
			}
		}

		if (bytes_read < 0)
		{
			syslog(LOG_ERR, "ERROR: Read failed - %s", strerror(errno));
		}
        close(fd);
    }

    syslog(LOG_INFO, "Closed connection from %s", client_ip);

    free(packet);
    close(client_fd);
}

void run_bg()
{
    pid_t pid = fork();
    if (pid < 0)
	{
		syslog(LOG_ERR, "ERROR: Fork failed - %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
    if (pid > 0)
    {
		syslog(LOG_INFO, "Fork success");
        exit(EXIT_SUCCESS);
	}
	if (setsid() < 0)
	{
		syslog(LOG_ERR, "ERROR: setsid failed - %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

    umask(0);
	if (chdir("/") < 0)
	{
		syslog(LOG_ERR, "chdir failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0)
    {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
    }
}

int main(int argc, char **argv)
{
    int bg_run_flag = 0;

    if (argc == 2 && strcmp(argv[1], "-d") == 0)
        bg_run_flag = 1;

    openlog("aesdsocket", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Init aesdsocket syslog");

    if (0 != register_signal_handler())
    {
        return -1;
    }

    // 1. Opens a stream socket bound to port 9000, failing and returning -1
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        syslog(LOG_ERR, "Error: Socket creation failed - %s", strerror(errno));
        return -1;
    }
    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
    {
        syslog(LOG_ERR, "Error: setsockopt failed - %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    syslog(LOG_INFO, "open socket sucessful");

    if (bg_run_flag)
        run_bg();

    // 2. Listens for and accepts a connection
    sockaddr_in_t local_addr;
    memset(&local_addr, 0, sizeof(sockaddr_in_t));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(SOCKET_PORT);
    if (bind(sockfd, (sockaddr_t*)&local_addr, sizeof(sockaddr_in_t)) < 0)
    {
        syslog(LOG_ERR, "Error: fail to bind socket - %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    syslog(LOG_INFO, "bind socket sucessful");

    if (listen(sockfd, 5) < 0)
    {
        syslog(LOG_ERR, "Error: listen failed - %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    syslog(LOG_INFO, "start listen!");

    while (!exit_flag)
    {
        sockaddr_in_t client_addr;
        socklen_t addrlen = sizeof(client_addr);

        int client_fd = accept(sockfd, (sockaddr_t *)&client_addr, &addrlen);
        if (client_fd < 0)
        {
            syslog(LOG_INFO, "Client accept failed");
            break;
        }
        else
        {
            handle_client(client_fd, &client_addr);
        }
    }

    close(sockfd);
    remove(SOCKET_DATA_FILE);
    closelog();
    syslog(LOG_INFO, "cleanup complete");
    return 0;
}