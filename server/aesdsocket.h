#define PORT 9000
#define BUFFER_SIZE 32768

typedef struct {
    int daemon_mode;
    int server_fd, file_fd, new_socket;
} aesdsocket_options_t;

aesdsocket_options_t *aesdsocket_parse_options(int argc, char *argv[]);
void aesdsocket_create_socket(aesdsocket_options_t *options);
