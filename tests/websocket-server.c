#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libwebsockets.h>
#include <pthread.h>

#define MAX_CALLBACKS 10
#define MAX_MESSAGE_LEN 256
#define MAX_MESSAGES 100

struct Callback {
    char key[100];
    void (*callback)(void *);
};

struct Message {
    char data[MAX_MESSAGE_LEN];
};

struct Callback callback_list[MAX_CALLBACKS];
int num_callbacks = 0;

struct Message message_queue[MAX_MESSAGES];
int message_count = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_condition = PTHREAD_COND_INITIALIZER;

void enqueue_message(const char *data) {
    pthread_mutex_lock(&queue_mutex);
    if (message_count < MAX_MESSAGES) {
        strcpy(message_queue[message_count].data, data);
        message_count++;
        pthread_cond_signal(&queue_condition);
    }
    pthread_mutex_unlock(&queue_mutex);
}

struct Message dequeue_message() {
    struct Message msg;
    pthread_mutex_lock(&queue_mutex);
    while (message_count == 0) {
        pthread_cond_wait(&queue_condition, &queue_mutex);
    }
    msg = message_queue[0];
    for (int i = 0; i < message_count - 1; i++) {
        message_queue[i] = message_queue[i + 1];
    }
    message_count--;
    pthread_mutex_unlock(&queue_mutex);
    return msg;
}

void *thread_function(void *data) {
    while (1) {
        struct Message msg = dequeue_message();
        printf("Thread received message: %s\n", msg.data);
        // Process message
        // Look into callback list and call the required callback
        // Free memory if no match
    }
    return NULL;
}

static int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    return 0;
}

static int callback_websockets(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    //char *msg = NULL;
    char msg[100] = {0};
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            printf("WebSocket connection established\n");
            break;
        case LWS_CALLBACK_RECEIVE:
            printf("Received message: %.*s\n", (int)len, (char *)in);
            lws_write(wsi, in, len, LWS_WRITE_TEXT);
 
            //msg = (char *)in;
	        sprintf(msg, "%.*s", (int)len, (char *)in);
            printf("Received message: %s\n", msg);
            enqueue_message(msg);
            break;
        default:
            break;
    }
    return 0;
}

#if 0
static struct lws_protocols protocols[] = {
    {
        "http-only",
        callback_http,
        0,0,0,NULL,0
    },
    {
        "websocket",
        callback_websockets,
        0,0,0,NULL,0
    },
    { NULL, NULL, 0, 0 }
};
#endif

#if 1
static struct lws_protocols protocols[] = {
    {
        "echo-protocol",    // protocol name
        callback_websockets,      // callback function
        0,                  // per-session data size
        0,                  // rx buffer size
        0,                  // id
        NULL,               // reserved
        0                   // tx packet size
    },
    { NULL, NULL, 0, 0 }    // end of list
};
#endif

int main() {
    struct lws_context *context;
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = 8080;
    info.protocols = protocols;
    info.extensions = NULL;
    info.gid = -1;
    info.uid = -1;

    context = lws_create_context(&info);
    if (!context) {
        printf("Failed to create libwebsockets context\n");
        return -1;
    }

    pthread_t thread;
    pthread_create(&thread, NULL, thread_function, NULL);

    // Event loop
#if 0
    while (1) {
        lws_service(context, 50);
    }
#endif
    while(lws_service(context, 100) >= 0) 
    {
    
    }

    lws_context_destroy(context);

    return 0;
}

