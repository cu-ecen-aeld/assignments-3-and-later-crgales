#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/queue.h>
#include <pthread.h>

#include "aesdsocket.h"

aesdsocket_options_t options;
file_options_t file_options;
thread_list_head_t thread_list_head;

void handle_signal(int signal) {
    thread_list_t *thread_list_entry;

    if (signal == SIGINT || signal == SIGTERM) {
        syslog(LOG_INFO, "Received signal %d", signal);
        printf("Received signal %d\n", signal);

        system("rm -f /var/tmp/aesdsocketdata");

        pthread_mutex_destroy(&file_options.file_mutex);

        while (!TAILQ_EMPTY(&thread_list_head)) {
            thread_list_entry = TAILQ_FIRST(&thread_list_head);
            TAILQ_REMOVE(&thread_list_head, thread_list_entry, threads);
            pthread_join(thread_list_entry->thread_id, NULL);
            free(thread_list_entry);
        }

        exit(0);
    }
}

void setup_signal_handler() {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(-1);
    }

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction");
        exit(-1);
    }
}

void parse_command_line_options(int argc, char *argv[], aesdsocket_options_t *options) {
    int opt;
    options->daemon_mode = 0;
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
            case 'd':
                options->daemon_mode = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
                exit(-1);
        }
    }
}

void timestamp() {
    char buffer[BUFFER_SIZE];
    time_t rawtime;
    struct tm *timeinfo;

    while (1) {
        // Acquire the mutex before accessing the file
        if (pthread_mutex_lock(&file_options.file_mutex)) {
            perror("pthread_mutex_lock");
            exit(-1);
        }

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(buffer, BUFFER_SIZE, "timestamp:%Y-%m-%d %H:%M:%S\n", timeinfo);

        // Move the file pointer to the end of the file
        lseek(file_options.file_fd, 0, SEEK_END);

        write(file_options.file_fd, buffer, strlen(buffer));
        
        // Release the mutex after accessing the file
        if (pthread_mutex_unlock(&file_options.file_mutex)) {
            perror("pthread_mutex_unlock");
            exit(-1);
        }

        sleep(10);
    }
}

void handle_socket(void *arguments) {
    char buffer[BUFFER_SIZE];  // Allocate a thread-specific buffer
    int valread;
    socket_options_t *socket = (socket_options_t *)arguments;

    memset(buffer, 0, BUFFER_SIZE);
    // Reading data from the client
    while ((valread = read(socket->socket_fd, buffer, BUFFER_SIZE)) > 0) {

        // Acquire the mutex before accessing the file
        if (pthread_mutex_lock(&file_options.file_mutex)) {
            perror("pthread_mutex_lock");
            exit(-1);
        }

        // Move the file pointer to the end of the file
        lseek(file_options.file_fd, 0, SEEK_END);

        // Writing data to the file
        write(file_options.file_fd, buffer, strlen(buffer));

        // Rewind the file pointer to the beginning
        lseek(file_options.file_fd, 0, SEEK_SET);

        // Clear the buffer
        memset(buffer, 0, BUFFER_SIZE);
        
        // Reading data from the file
        read(file_options.file_fd, buffer, BUFFER_SIZE);

        // Sending buffer to the client
        send(socket->socket_fd, buffer, strlen(buffer), 0);

        // Release the mutex after accessing the file
        if (pthread_mutex_unlock(&file_options.file_mutex)) {
            perror("pthread_mutex_unlock");
            exit(-1);
        }

        // Clear the buffer for the next iteration
        memset(buffer, 0, BUFFER_SIZE);
    }

    printf("Client disconnected\n");
    syslog(LOG_INFO, "Client disconnected");

    // Close the socket and free the memory
    close(socket->socket_fd);
    free(socket);
    socket = NULL;

    pthread_exit(NULL);
}

void aesdsocket_create_socket() {
    int server_fd, accept_fd;
    socket_options_t *new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    thread_list_t *thread_list_entry;
    pthread_t thread_id;

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(-1);
    }

    // Forcefully attaching socket to the port 9000
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        exit(-1);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        exit(-1);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Binding the socket to the port 9000
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(-1);
    }

    // Create a daemon if the daemon_mode is set
    if (options.daemon_mode) {
        daemon(0, 0);
    }

    // Listening for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        exit(-1);
    }

    printf("Server listening on port %d\n", PORT);
    syslog(LOG_INFO, "Server listening on port %d", PORT);

    // Accepting incoming connection
    while ((accept_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) > 0) {

        // Log the accept message to syslog
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(address.sin_addr));
        printf("Accepted connection from %s\n", inet_ntoa(address.sin_addr));

        // Create a new socket_options_t struct to pass to the thread
        new_socket = malloc(sizeof(socket_options_t));
        new_socket->socket_fd = accept_fd;

        // Create a new thread to handle the socket
        if (pthread_create(&thread_id, NULL, (void *)handle_socket, (void *)new_socket) < 0) {
            perror("pthread_create");
            close(accept_fd);
            close(server_fd);
            close(file_options.file_fd);
            closelog();
            exit(-1);
        }

        // Create a new thread_list_entry
        thread_list_entry = (thread_list_t *)malloc(sizeof(thread_list_t));
        if (thread_list_entry == NULL) {
            perror("malloc");
            close(accept_fd);
            close(server_fd);
            close(file_options.file_fd);
            closelog();
            exit(-1);
        }
        thread_list_entry->thread_id = thread_id;
        TAILQ_INSERT_TAIL(&thread_list_head, thread_list_entry, threads);
    }
    
    // It is possible that the accept call failed, so close the server socket and exit
    perror("accept");
    
    // Closing the socket
    close(accept_fd);
    close(server_fd);
    close(file_options.file_fd);
    closelog();

    system("rm -f /var/tmp/aesdsocketdata");

    exit(0);
}

int main(int argc, char *argv[]) {
    pthread_t thread_id;

    parse_command_line_options(argc, argv, &options);

    TAILQ_INIT(&thread_list_head);

    setup_signal_handler();

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    // Open the file /var/tmp/aesdsocketdata for read/write
    file_options.file_fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT, 0644);
    if (file_options.file_fd < 0) {
        perror("open failed");
        exit(-1);
    }

    // Initialize the mutex
    if (pthread_mutex_init(&file_options.file_mutex, NULL) != 0) {
        perror("pthread_mutex_init");
        close(file_options.file_fd);
        closelog();
        exit(-1);
    }

    // Create a thread to write the timestamp to the file
    if (pthread_create(&thread_id, NULL, (void *)timestamp, NULL) < 0) {
        perror("pthread_create");
        close(file_options.file_fd);
        closelog();
        exit(-1);
    }
    
    // Run sockets in the main thread
    aesdsocket_create_socket(&options);

    return 0;
}