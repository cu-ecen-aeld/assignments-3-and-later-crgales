#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <semaphore.h>
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

        system("rm -f /var/tmp/aesdsocketdata");

        TAILQ_FOREACH(thread_list_entry, &thread_list_head, threads) {
            pthread_join(thread_list_entry->thread_id, NULL);
            free(thread_list_entry);
            thread_list_entry = NULL;
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
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
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
                exit(EXIT_FAILURE);
        }
    }
}

void timestamp() {
    char buffer[BUFFER_SIZE];
    time_t rawtime;
    struct tm *timeinfo;

    while (1) {
        // Acquire the semaphore before accessing the file
        if (sem_wait(&file_options.semaphore) < 0) {
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(buffer, BUFFER_SIZE, "timestamp:%Y-%m-%d %H:%M:%S\n", timeinfo);

        // Move the file pointer to the end of the file
        lseek(file_options.file_fd, 0, SEEK_END);

        write(file_options.file_fd, buffer, strlen(buffer));
        
        // Release the semaphore after accessing the file  
        if (sem_post(&file_options.semaphore) < 0) {
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        sleep(10);
    }
}

void handle_socket(void *arguments) {
    char buffer[BUFFER_SIZE];  // Allocate a thread-specific buffer
    int valread;
    int socket = *((int *)arguments);
    thread_list_t *thread_list_entry;

    memset(buffer, 0, BUFFER_SIZE);
    // Reading data from the client
    while ((valread = read(socket, buffer, BUFFER_SIZE)) > 0) {
        printf("Received: %s\n", buffer);

        // Acquire the semaphore before accessing the file
        if (sem_wait(&file_options.semaphore) < 0) {
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }

        // Move the file pointer to the end of the file
        lseek(file_options.file_fd, 0, SEEK_END);

        // Writing data to the file
        write(file_options.file_fd, buffer, strlen(buffer));
        printf("Wrote to the file: %s\n", buffer);

        // Rewind the file pointer to the beginning
        lseek(file_options.file_fd, 0, SEEK_SET);

        // Clear the buffer
        memset(buffer, 0, BUFFER_SIZE);
        
        // Reading data from the file
        read(file_options.file_fd, buffer, BUFFER_SIZE);
        printf("Read from the file: %s\n", buffer);

        // Sending buffer to the client
        send(socket, buffer, strlen(buffer), 0);
        printf("\nResponse sent: %s\n", buffer);

        // Release the semaphore after accessing the file  
        if (sem_post(&file_options.semaphore) < 0) {
            perror("sem_post");
            exit(EXIT_FAILURE);
        }

        // Clear the buffer for the next iteration
        memset(buffer, 0, BUFFER_SIZE);
    }

    printf("Client disconnected\n");
    syslog(LOG_INFO, "Client disconnected");

    // Close the socket and remove the thread from the thread list
    close(socket);
    TAILQ_FOREACH(thread_list_entry, &thread_list_head, threads) {
        if (thread_list_entry->thread_id == pthread_self()) {
            TAILQ_REMOVE(&thread_list_head, thread_list_entry, threads);
            free(thread_list_entry);
            thread_list_entry = NULL;
            break;
        }
    }

}

void aesdsocket_create_socket() {
    int server_fd, new_socket;
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
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
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
    while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) > 0) {

        // Log the accept message to syslog
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(address.sin_addr));
        printf("Accepted connection from %s\n", inet_ntoa(address.sin_addr));

        // Create a new thread to handle the socket
        if (pthread_create(&thread_id, NULL, (void *)handle_socket, (void *)&new_socket) < 0) {
            perror("pthread_create");
            close(new_socket);
            close(server_fd);
            close(file_options.file_fd);
            closelog();
            exit(-1);
        }

        // Create a new thread_list_entry
        thread_list_entry = (thread_list_t *)malloc(sizeof(thread_list_t));
        if (thread_list_entry == NULL) {
            perror("malloc");
            close(new_socket);
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
    close(new_socket);
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
        exit(EXIT_FAILURE);
    }

    if (sem_init(&file_options.semaphore, 0, 1) < 0) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    // Create a thread to write the timestamp to the file
    if (pthread_create(&thread_id, NULL, (void *)timestamp, NULL) < 0) {
        perror("pthread_create");
        close(file_options.file_fd);
        closelog();
        exit(EXIT_FAILURE);
    }
    
    // Run sockets in the main thread
    aesdsocket_create_socket(&options);

    return 0;
}