/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Standard Libraries */
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <signal.h>

/* Application Includes */
#include <ut_control_plane.h>

/* External libraries */
#include <libwebsockets.h>


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
    DATA_RECIEVED,
    SERVER_START
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

volatile static eMessage_t g_status = SERVER_START;

static ut_cp_instance_internal_t *validateCPInstance(ut_controlPlane_instance_t *pInstance);
static void cp_sigint_handler(int sig);

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

ut_control_callback_t *get_callback_from_list(char* key)
{
    for(uint32_t i = 0; i < callback_entry_index; i++)
    {
        printf("\nkey = %s\n", key);
        printf("\nc-key = %s\n", callbackEntryList[i].key);
        printf("\nc-cb = %p\n", callbackEntryList[i].pCallback);
        if(strstr(key, callbackEntryList[i].key))
        {
            //return callbackEntryList[i].pCallback;
            callbackList[callback_list_index] = callbackEntryList[i].pCallback;
            callback_list_index++;
        }
    }
    return callbackList;

}

void *thread_function(void *data)
{
    pthread_t *thread_id = (pthread_t *)data;
    char* val;
    ut_control_callback_t *callbacks = NULL;
    while (1)
    {
        ut_cp_message_t *msg = dequeue_message();
        g_status = msg->status;

        switch (msg->status)
        {
        case EXIT_REQUESTED:
            printf("EXIT REQUESTED in thread. Thread going to exit\n");
            pthread_exit(thread_id);
            /* code */
            break;

        case DATA_RECIEVED:
            printf("DATA RECEIVED\n");
            printf("Thread received message:\n %s\n", msg->message);
            printf("g_status: %d\n", g_status);
            if(!msg->message)
            {
                break;
            }
            val = strstr(msg->message, "key: ") + strlen("key: ");
            printf("val = %s\n", val);
            callbacks = get_callback_from_list(val);
            if(callbacks)
            {
                for(int i = 0; i < callback_list_index; i++)
                {
                    callbacks[i](NULL, NULL);
                }
            }
            if(msg->message)
            {
                printf("\n2. msg->message pointer =%p\n", msg->message);
                free(msg->message);
                msg->message = NULL;
            }
            //free(msg->message);
            //free(msg);

            /**code*/
            break;

        default:
            break;
        }
        // Process message
        // Look into callback list and call the required callback
        // Free memory if no match
        //free(msg->message);
        //free(msg);
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
        ut_cp_message_t msg;
        msg.message = malloc((int)len + 1);
        printf("\n1. msg->message pointer =%p\n", msg.message);
        msg.size = (int)len;
        msg.status = DATA_RECIEVED;
        memset(msg.message, 0, (int)len + 1);
        strncpy(msg.message, (const char*)in, len);
        msg.message[len] = '\0';
        printf("Received message:\n %s\n", msg.message);
        enqueue_message(&msg);
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

    pInstance->info.port = monitorPort;
    pInstance->info.iface = NULL;
    pInstance->info.protocols = protocols;

    pInstance->context = lws_create_context(&pInstance->info);
    if (!pInstance->context)
    {
        fprintf(stderr, "Error creating libwebsockets context\n");
        return NULL;
    }
    pthread_create(&pInstance->thread, NULL, thread_function, (void *)&pInstance->thread);

    signal(SIGINT, cp_sigint_handler);

    return (ut_controlPlane_instance_t *)pInstance;

}

void UT_ControlPlane_Exit( ut_controlPlane_instance_t *pInstance )
{

    ut_cp_instance_internal_t *pInternal = validateCPInstance(pInstance);

    if (pInternal == NULL)
    {
        return;
    }

    ut_cp_message_t msg;
    msg.status = EXIT_REQUESTED;
    printf("Received message: %d\n", msg.status);
    enqueue_message(&msg);

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
    if (pInternal == NULL)
    {
        return;
    }

    while (g_status)
    {
        lws_service(pInternal->context, 1000);
    }
}

void testCallback(char *key, ut_kvp_instance_t *instance)
{
    printf("\n*************testCallback is called********************\n");
}

void testRMFCallback(char *key, ut_kvp_instance_t *instance)
{
    printf("\n**************testRMFCallback is called****************\n");
}

#if 0
void main()
{
    printf("CP enter\n");
    ut_controlPlane_instance_t *instance_t = UT_ControlPlane_Init(8080);

    if (instance_t == NULL)
    {
        printf("Contorl panel instance not created\n");
        return;
    }
    UT_ControlPlane_RegisterCallbackOnMessage(instance_t, "hdmicec/command", &testCallback);
    UT_ControlPlane_RegisterCallbackOnMessage(instance_t, "rmfAudio/a", &testRMFCallback);
    UT_ControlPlane_RegisterCallbackOnMessage(instance_t, "hdmicec/command", &testRMFCallback);
    UT_ControlPlane_Service(instance_t);
    UT_ControlPlane_Exit(instance_t);
    printf("CP exit \n");

    return;
}
#endif

CallbackListStatus_t UT_ControlPlane_RegisterCallbackOnMessage(ut_controlPlane_instance_t *pInstance, char *key, ut_control_callback_t callbackFunction)
{
    ut_cp_instance_internal_t *pInternal = (ut_cp_instance_internal_t *)pInstance;

    if ( pInstance == NULL )
    {
        //assert(pInternal == NULL);
        printf("\nInvalid Handle");
        return UT_CONTROL_PLANE_STATUS_CALLBACK_LIST_INVALID_HANDLE;
    }

    if ( callback_entry_index >=UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES ) 
    { 
        return UT_CONTROL_PLANE_STATUS_CALLBACK_LIST_FULL;
    } 
    strncpy(callbackEntryList[callback_entry_index].key, key,UT_CONTROL_PLANE_MAX_KEY_SIZE);
    callbackEntryList[callback_entry_index].pCallback = callbackFunction;
    callback_entry_index++;
    printf("callback_entry_index : %d\n", callback_entry_index);
    return UT_CONTROL_PLANE_STATUS_CALLBACK_LIST_OK;
}

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

static void cp_sigint_handler(int sig)
{
    g_status = EXIT_REQUESTED;
}