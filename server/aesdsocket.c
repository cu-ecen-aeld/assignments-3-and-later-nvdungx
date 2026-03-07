#include "aesdsocket.h"


volatile sig_atomic_t g_exit_flag = 0;
pthread_mutex_t g_log_file_mutex = PTHREAD_MUTEX_INITIALIZER;


static void signal_handler(int signo)
{
    if (signo == SIGTERM || signo == SIGINT)
    {
        syslog(LOG_INFO, "Signal caught, exit!!");
        g_exit_flag = 1;
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
        syslog(LOG_ERR, "ERROR: fail to register SIGINT - %s", strerror(errno));
        ret = -1;
    }
    if(sigaction(SIGTERM, &act, NULL) != 0)
    {
        syslog(LOG_ERR, "ERROR: fail to register SIGTERM - %s", strerror(errno));
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
            syslog(LOG_ERR, "ERROR: Memory allocation failed");
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
        syslog(LOG_ERR, "ERROR: chdir failed - %s", strerror(errno));
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

// append timestamp to log file every 10s
void create_timestamp(void)
{
    while(!g_exit_flag)
    {
        struct timespec ts;
        ts.tv_sec = 10;
        ts.tv_nsec = 0;

        if(nanosleep(&ts, NULL) != 0 && g_exit_flag)
        {
            break;
        }

        if(g_exit_flag) break;

        time_t rawtime;
        struct tm *info;
        char time_buffer[255];
        char out_buffer[255];

        time(&rawtime);
        info = localtime(&rawtime);

        strftime(time_buffer, sizeof(time_buffer), "%a, %d %b %Y %H:%M:%S %z", info);
        snprintf(out_buffer, sizeof(out_buffer), "timestamp:%s\n", time_buffer);

        pthread_mutex_lock(&g_log_file_mutex);

        int fd = open(SOCKET_DATA_FILE, O_CREAT | O_APPEND | O_WRONLY, 0644);
        if (fd >= 0)
        {
            ssize_t wr_bytes = write(fd, out_buffer, strlen(out_buffer));
            if(wr_bytes < 0)
            {
                syslog(LOG_ERR, "ERROR: writing timestamp - %s", strerror(errno));
            }
            close(fd);
        }
        else
        {
            syslog(LOG_ERR, "ERROR: opening file for timestamp - %s", strerror(errno));
        }

        pthread_mutex_unlock(&g_log_file_mutex);
    }
}

void *handle_client_connection(void *param)
{
    skthread_data_t *data = (skthread_data_t *)param;
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &data->client_addr.sin_addr, client_ip, sizeof(client_ip));

    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    char *packet = NULL;
    size_t total_size = 0;
    char buffer[BUFFER_SIZE];

    while (!g_exit_flag)
    {
        ssize_t bytes = recv(data->client_fd, buffer, sizeof(buffer), 0);

        if (bytes <= 0)
        {
            break;
        }

        char *new_packet = realloc(packet, total_size + bytes);
        if (!new_packet)
        {
            syslog(LOG_ERR, "ERROR: Memory allocation failed");
            free(packet);
            packet = NULL;
            break;
        }

        packet = new_packet;
        memcpy(packet + total_size, buffer, bytes);
        total_size += bytes;

        if (memchr(buffer, '\n', bytes))
        {
            pthread_mutex_lock(&g_log_file_mutex);

            int fd = open(SOCKET_DATA_FILE, O_CREAT | O_APPEND | O_WRONLY, 0644);
            if (fd >= 0)
            {
                if(write(fd, packet, total_size) < 0) {
                    syslog(LOG_ERR, "ERROR: Failed write to %s", SOCKET_DATA_FILE);
                }
                close(fd);
            }
            else
            {
                syslog(LOG_ERR, "ERROR: Failed open %s", SOCKET_DATA_FILE);
            }

            fd = open(SOCKET_DATA_FILE, O_RDONLY);
            if (fd >= 0)
            {
                ssize_t bytes_read;
                char send_buffer[BUFFER_SIZE];
                while ((bytes_read = read(fd, send_buffer, sizeof(send_buffer))) > 0)
                {
                    if (send(data->client_fd, send_buffer, bytes_read, 0) < 0)
                    {
                        syslog(LOG_ERR, "ERROR: Send failed - %s", strerror(errno));
                        break;
                    }
                }
                close(fd);
            }

            pthread_mutex_unlock(&g_log_file_mutex);

            free(packet);
            packet = NULL;
            total_size = 0;
        }
    }

    if(packet) free(packet);

    close(data->client_fd);
    syslog(LOG_INFO, "Closed connection from %s", client_ip);

    data->thread_complete = true;
    return NULL;
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
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        syslog(LOG_ERR, "ERROR: Socket creation failed - %s", strerror(errno));
        return -1;
    }
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        syslog(LOG_ERR, "ERROR: setsockopt failed - %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    syslog(LOG_INFO, "open socket sucessful");

    if (bg_run_flag)
        run_bg();

    sockaddr_in_t local_addr;
    memset(&local_addr, 0, sizeof(sockaddr_in_t));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(SOCKET_PORT);
    if (bind(sockfd, (sockaddr_t*)&local_addr, sizeof(sockaddr_in_t)) < 0)
    {
        syslog(LOG_ERR, "ERROR: fail to bind socket - %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    syslog(LOG_INFO, "bind socket sucessful");

    if (listen(sockfd, 5) < 0)
    {
        syslog(LOG_ERR, "ERROR: listen failed - %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    syslog(LOG_INFO, "start listen!");

    pthread_t timestamp_thread;
    if (pthread_create(&timestamp_thread, NULL, create_timestamp, NULL) != 0) {
        syslog(LOG_ERR, "ERROR: Failed to create timestamp thread - %s", strerror(errno));
    }

    struct skthread_data_list head;
    SLIST_INIT(&head);

    while (!g_exit_flag)
    {
        sockaddr_in_t client_addr;
        socklen_t addrlen = sizeof(client_addr);

        int client_fd = accept(sockfd, (sockaddr_t *)&client_addr, &addrlen);
        if (client_fd < 0)
        {
            syslog(LOG_INFO, "Client accept failed");
            continue;
        }
        else
        {
            skthread_data_t *new_thread_data = malloc(sizeof(skthread_data_t));
            if (new_thread_data == NULL) {
                syslog(LOG_ERR, "ERROR: Failed to allocate memory for thread data");
                close(client_fd);
                continue;
            }

            new_thread_data->client_fd = client_fd;
            new_thread_data->client_addr = client_addr;
            new_thread_data->thread_complete = false;

            if (pthread_create(&new_thread_data->thread_id, NULL, handle_client_connection, new_thread_data) != 0)
            {
                syslog(LOG_ERR, "ERROR: Failed to create thread");
                close(client_fd);
                free(new_thread_data);
                continue;
            }

            SLIST_INSERT_HEAD(&head, new_thread_data, entries);

            skthread_data_t *ptr;
            SLIST_FOREACH(ptr, &head, entries) {
                if (ptr->thread_complete) {
                    pthread_join(ptr->thread_id, NULL);
                    SLIST_REMOVE(&head, ptr, skthread_data, entries);
                    free(ptr);
                }
            }
        }
    }

    syslog(LOG_INFO, "Exiting...");

    pthread_join(timestamp_thread, NULL);

    skthread_data_t *ptr;
    while (!SLIST_EMPTY(&head)) {
        ptr = SLIST_FIRST(&head);

        shutdown(ptr->client_fd, SHUT_RDWR);

        pthread_join(ptr->thread_id, NULL);
        SLIST_REMOVE_HEAD(&head, entries);
        free(ptr);
    }

    close(sockfd);
    remove(SOCKET_DATA_FILE);
    pthread_mutex_destroy(&g_log_file_mutex);
    closelog();
    syslog(LOG_INFO, "cleanup complete");
    return 0;
}