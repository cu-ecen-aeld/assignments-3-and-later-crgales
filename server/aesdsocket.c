#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>

#include "aesdsocket.h"

aesdsocket_options_t options;

void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        syslog(LOG_INFO, "Received signal %d", signal);
        system("rm -f /var/tmp/aesdsocketdata");
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

void aesdsocket_create_socket(aesdsocket_options_t *options) {
    int server_fd, file_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    int valread;

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    // Open the file /var/tmp/aesdsocketdata for read/write
    file_fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT, 0644);
    if (file_fd < 0) {
        perror("open failed");
        exit(EXIT_FAILURE);
    }

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
    if (options->daemon_mode) {
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

        // Reading data from the client
        while ((valread = read(new_socket, buffer, BUFFER_SIZE)) > 0) {
            printf("Received: %s\n", buffer);

            // Move the file pointer to the end of the file
            lseek(file_fd, 0, SEEK_END);

            // Writing data to the file
            write(file_fd, buffer, strlen(buffer));

            // Rewind the file pointer to the beginning
            lseek(file_fd, 0, SEEK_SET);

            // Clear the buffer
            memset(buffer, 0, BUFFER_SIZE);
            
            // Reading data from the file
            read(file_fd, buffer, BUFFER_SIZE);

            // Sending buffer to the client
            send(new_socket, buffer, strlen(buffer), 0);
            printf("\nResponse sent: %s\n", buffer);

            // Clear the buffer for the next iteration
            memset(buffer, 0, BUFFER_SIZE);
        }

        printf("Client disconnected\n");
    }
    // Closing the socket
    close(new_socket);
    close(server_fd);
    close(file_fd);
    closelog();

    system("rm -f /var/tmp/aesdsocketdata");

    exit(0);
}

int main(int argc, char *argv[]) {

    parse_command_line_options(argc, argv, &options);

    setup_signal_handler();

    aesdsocket_create_socket(&options);

    return 0;
}