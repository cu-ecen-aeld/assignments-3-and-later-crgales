#define PORT 9000
#define BUFFER_SIZE 32768

typedef struct {
    int daemon_mode;
} aesdsocket_options_t;

typedef struct {
    int socket_fd;
} socket_options_t;

typedef struct {
    int file_fd;
    pthread_mutex_t file_mutex;
} file_options_t;

typedef struct thread_list {
    pthread_t thread_id;
    TAILQ_ENTRY(thread_list) threads;
} thread_list_t;

typedef TAILQ_HEAD(head_s, thread_list) thread_list_head_t;
