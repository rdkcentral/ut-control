#include <libwebsockets.h>
#include <stdio.h>
#include <ut_control_plane.h>
#include <pthread.h>

char *test_message="HI FROM SERVER";

typedef struct
{
    uint32_t magic;
    struct lws_context_creation_info info;
    struct lws_context *context;
    pthread_t thread;
} ut_cp_instance_internal_t;

typedef enum
{
    EXIT_REQUESTED = 0,
    DATA_RECIEVED
}eMessage_t;
typedef struct
{
 eMessage_t status;
 char *message;
 uint32_t size;
}ut_cp_message_t;

#define MAX_CALLBACKS 10
#define MAX_MESSAGE_LEN 256
#define MAX_MESSAGES 100

typedef struct{
    char key[100];
    void (*callback)(void *);
}ut_cp_callback_t;

ut_cp_callback_t callback_list[MAX_CALLBACKS];
int num_callbacks = 0;

#define UT_CP_MAGIC (0xdeadbeef)

ut_cp_message_t message_queue[MAX_MESSAGES];
int message_count = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_condition = PTHREAD_COND_INITIALIZER;

static ut_cp_instance_internal_t *validateCPInstance(ut_controlPlane_instance_t *pInstance);

void enqueue_message(ut_cp_message_t *data)
{
    pthread_mutex_lock(&queue_mutex);
    if (message_count < MAX_MESSAGES) {
        //memcpy(message_queue[message_count], data);
        //message_queue[message_count] = data;
        message_queue[message_count].size = data->size;
        message_queue[message_count].status = data->status;
        message_queue[message_count].message = data->message;
        message_count++;
        pthread_cond_signal(&queue_condition);
    }
    pthread_mutex_unlock(&queue_mutex);
}

ut_cp_message_t* dequeue_message()
{
    ut_cp_message_t *msg;
    pthread_mutex_lock(&queue_mutex);
    while (message_count == 0) {
        pthread_cond_wait(&queue_condition, &queue_mutex);
    }
    msg = &message_queue[0];
    for (int i = 0; i < message_count - 1; i++) {
        message_queue[i] = message_queue[i + 1];
    }
    message_count--;
    pthread_mutex_unlock(&queue_mutex);
    return msg;
}

void *thread_function(void *data)
{
    while (1)
    {
        ut_cp_message_t *msg = dequeue_message();
        printf("Thread received message: %s\n", msg->message);

        switch (msg->status)
        {
        case EXIT_REQUESTED:
            printf("EXIT REQUESTED\n");
            /* code */
            break;

        case DATA_RECIEVED:
            printf("DATA RECEIVED\n");
            /**code*/
            break;

        default:
            break;
        }
        // Process message
        // Look into callback list and call the required callback
        // Free memory if no match
        free(msg->message);
        free(msg);
    }
    return NULL;
}

static int callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    //char msg[100] = {0};
    switch (reason)
    {
    case LWS_CALLBACK_ESTABLISHED:
        printf("Client connected\n");
        break;

    case LWS_CALLBACK_RECEIVE:
        printf("LWS_CALLBACK_RECEIVE\n");
        ut_cp_message_t *msg = malloc(sizeof(ut_cp_callback_t));
        msg->message = malloc((int)len + 1);
        msg->size = (int)len;
        msg->status = DATA_RECIEVED;
        //sprintf(msg, "%.*s", (int)len, (char *)in);
        strncpy(msg->message, (const char*)in, len);
        msg->message[len] = '\0';
        printf("Received message: %s\n", msg->message);
        enqueue_message(msg);
        // Echo back received message
        lws_write(wsi, in, len, LWS_WRITE_TEXT);
        break;

    default:
        break;
    }

    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "echo-protocol",    // protocol name
        callback_echo,      // callback function
        0,                  // per-session data size
        0,                  // rx buffer size
        0,                  // id
        NULL,               // reserved
        0                   // tx packet size
    },
    { NULL, NULL, 0, 0 }    // end of list
};

ut_controlPlane_instance_t *UT_ControlPlane_Init( int monitorPort )
{
    ut_cp_instance_internal_t *pInstance = malloc(sizeof(ut_cp_instance_internal_t));

    if ( pInstance == NULL )
    {
        //assert( pInstance != NULL );
        printf("Malloc was not able to provide memory\n");
        return NULL;
    }
    //int port = 8080;

    memset(pInstance, 0, sizeof(ut_cp_instance_internal_t));
    memset(&pInstance->info, 0, sizeof(struct lws_context_creation_info));

    pInstance->magic = UT_CP_MAGIC;

    pInstance->info.port = 8080;
    pInstance->info.iface = NULL;
    pInstance->info.protocols = protocols;

    pInstance->context = lws_create_context(&pInstance->info);
    if (!pInstance->context)
    {
        fprintf(stderr, "Error creating libwebsockets context\n");
        return NULL;
    }
    pthread_create(&pInstance->thread, NULL, thread_function, NULL);

    return (ut_controlPlane_instance_t *)pInstance;

}

void UT_ControlPlane_Exit( ut_controlPlane_instance_t *pInstance )
{
    ut_cp_instance_internal_t *pInternal = validateCPInstance(pInstance);

    if (pInternal == NULL)
    {
        return;
    }

    if (pInternal->context != NULL)
    {
        lws_context_destroy(pInternal->context);
    }

    // Wait for the thread to finish
    if (pthread_join(pInternal->thread, NULL) != 0)
    {
        printf("Failed to join thread");
        return;
    }

    memset(pInternal, 0, sizeof(ut_cp_instance_internal_t));

    free(pInternal);
    pInternal = NULL;
}

void UT_ControlPlane_Service( ut_controlPlane_instance_t *pInstance )
{
    ut_cp_instance_internal_t *pInternal = validateCPInstance(pInstance);

    while (1)
    {
        lws_service(pInternal->context, 50);
    }
}

void main()
{
    printf("CP enter\n");
    ut_controlPlane_instance_t *instance_t = UT_ControlPlane_Init(8080);

    if (instance_t == NULL)
    {
        printf("Contorl panel instance not created\n");
        return;
    }
    UT_ControlPlane_Service(instance_t);
    UT_ControlPlane_Exit(instance_t);
    printf("CP exit \n");

    return;
}

// void UT_ControlPlane_RegisterCallbackOnMessage(ut_controlPlane_instance_t *pInstance, char *key, ut_control_callback_t callbackFunction)
// {
// }

/** Static Functions */
static ut_cp_instance_internal_t *validateCPInstance(ut_controlPlane_instance_t *pInstance)
{
    ut_cp_instance_internal_t *pInternal = (ut_cp_instance_internal_t *)pInstance;

    if ( pInstance == NULL )
    {
        //assert(pInternal == NULL);
        printf("\nInvalid Handle");
        return NULL;
    }

    if (pInternal->magic != UT_CP_MAGIC)
    {
        //assert(pInternal->magic != UT_CP_MAGIC);
        printf("\nInvalid Handle - magic failure");
        return NULL;
    }

    return pInternal;
}