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
#include <stdio.h>

/* Application Includes */
#include <ut_control_plane.h>
#include <ut_kvp.h>

/* External libraries */
#include <libwebsockets.h>

#define UT_CONTROL_PLANE_ERROR(f_, ...) printf((f_), ##__VA_ARGS__)
#define UT_CONTROL_PLANE_DEBUG(f_, ...) (void)0

typedef struct
{
  char key[UT_CONTROL_PLANE_MAX_KEY_SIZE];
  ut_control_callback_t pCallback;
}CallbackEntry_t;

typedef struct
{
    uint32_t magic;
    struct lws_context_creation_info info;
    struct lws_context *context;
    pthread_t thread;
    pthread_t thread2;
} ut_cp_instance_internal_t;

typedef enum
{
    SERVER_START = 0,
    EXIT_REQUESTED,
    DATA_RECIEVED
}eMessage_t;
typedef struct
{
 eMessage_t status;
 char *message;
 uint32_t size;
}cp_message_t;

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

cp_message_t message_queue[MAX_MESSAGES];
int message_count = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_condition = PTHREAD_COND_INITIALIZER;

static CallbackEntry_t callbackEntryList[UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES];
static uint32_t callback_entry_index=0;
static volatile bool exit_request = false;

//static uint32_t lastFreeCallbackSlot=0; /* Must always be < UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES */

static ut_cp_instance_internal_t *validateCPInstance(ut_controlPlane_instance_t *pInstance);

void enqueue_message(cp_message_t *data)
{
    pthread_mutex_lock(&queue_mutex);
    if (message_count < MAX_MESSAGES) {
        message_queue[message_count].size = data->size;
        message_queue[message_count].status = data->status;
        message_queue[message_count].message = data->message;
        message_count++;
        pthread_cond_signal(&queue_condition);
    }
    pthread_mutex_unlock(&queue_mutex);
}

cp_message_t* dequeue_message()
{
    cp_message_t *msg;
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

void call_callback_on_match(cp_message_t *mssg)
{
    ut_kvp_instance_t *pkvpInstance = NULL;
    ut_kvp_status_t status;
    char result_kvp[UT_KVP_MAX_ELEMENT_SIZE] = {0xff};

    if (!mssg->message)
    {
        return;
    }

    pkvpInstance = ut_kvp_createInstance();
    status = ut_kvp_openMemory(pkvpInstance, mssg->message, mssg->size );
    if (status != UT_KVP_STATUS_SUCCESS)
    {
        UT_CONTROL_PLANE_ERROR("\nut_kvp_open() - Read Failure\n");
        return;
    }
    for (uint32_t i = 0; i < callback_entry_index; i++)
    {
        CallbackEntry_t entry = callbackEntryList[i];
        if (UT_KVP_STATUS_SUCCESS == ut_kvp_getStringField(pkvpInstance, entry.key, result_kvp, UT_KVP_MAX_ELEMENT_SIZE))
        {
            // call callback
            entry.pCallback(entry.key, pkvpInstance);
        }
    }
    ut_kvp_close(pkvpInstance);
    return;
}

void *thread_function2(void *data)
{
    ut_cp_instance_internal_t *pInternal = validateCPInstance((ut_controlPlane_instance_t*)data);
     if (pInternal == NULL)
    {
        return NULL;
    }


    while (!exit_request)
    {
        lws_service(pInternal->context, 0);
    }

    return NULL;
}

void *thread_function(void *data)
{
    ut_cp_instance_internal_t *pInternal = validateCPInstance((ut_controlPlane_instance_t*)data);

    if (pInternal == NULL)
    {
        return NULL;
    }
    pthread_create(&pInternal->thread2, NULL, thread_function2, data );

    while (!exit_request)
    {
        cp_message_t *msg;
        msg = dequeue_message();

        switch (msg->status)
        {
        case SERVER_START:
            UT_CONTROL_PLANE_DEBUG("SERVER START message\n");
        break;

        case EXIT_REQUESTED:
            UT_CONTROL_PLANE_DEBUG("EXIT REQUESTED in thread1. Thread1 going to exit\n");
            exit_request = true;
        break;

        case DATA_RECIEVED:
            UT_CONTROL_PLANE_DEBUG("DATA RECEIVED\n");
            call_callback_on_match(msg);
            break;

            default:
            break;
        }
    }

    if (pthread_join(pInternal->thread2, NULL) != 0)
    {
        UT_CONTROL_PLANE_ERROR("thread2 Failed to join from instance : %p \n", pInternal);
        /*TODO: need to confirm if this return is required*/
        return NULL;
    }
    UT_CONTROL_PLANE_DEBUG("Thread2 exitted\n");
    return NULL;
}

static int callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    switch (reason)
    {
    case LWS_CALLBACK_ESTABLISHED:
        UT_CONTROL_PLANE_DEBUG("Client connected\n");
        break;

    case LWS_CALLBACK_RECEIVE:
        UT_CONTROL_PLANE_DEBUG("LWS_CALLBACK_RECEIVE\n");
        cp_message_t msg;
        msg.message = malloc((int)len + 1);
        if(msg.message == NULL)
        {
            UT_CONTROL_PLANE_ERROR("Malloc failed\n");
            break;
        }
        msg.size = (int)len;
        msg.status = DATA_RECIEVED;
        memset(msg.message, 0, (int)len + 1);
        strncpy(msg.message, (const char*)in, len);
        msg.message[len] = '\0';
        UT_CONTROL_PLANE_DEBUG("Received message:\n %s\n", msg.message);
        enqueue_message(&msg);
        // Echo back received message
        //lws_write(wsi, in, len, LWS_WRITE_TEXT);
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

ut_controlPlane_instance_t *UT_ControlPlane_Init( uint32_t monitorPort )
{
    ut_cp_instance_internal_t *pInstance = calloc(1, sizeof(ut_cp_instance_internal_t));

    if ( pInstance == NULL )
    {
        //assert( pInstance != NULL );
        printf("Malloc was not able to provide memory\n");
        return NULL;
    }

    pInstance->magic = UT_CP_MAGIC;

    pInstance->info.port = monitorPort;
    pInstance->info.iface = NULL;
    pInstance->info.protocols = protocols;

    pInstance->context = lws_create_context(&pInstance->info);
    if (!pInstance->context)
    {
        printf("Error creating libwebsockets context\n");
        pInstance->magic = !UT_CP_MAGIC;
        free( pInstance );
        return NULL;
    }

    exit_request = false;

    return (ut_controlPlane_instance_t *)pInstance;
}

void UT_ControlPlane_Exit( ut_controlPlane_instance_t *pInstance )
{
    ut_cp_instance_internal_t *pInternal = validateCPInstance(pInstance);

    if (pInternal == NULL)
    {
        return;
    }

    cp_message_t msg;
    msg.status = EXIT_REQUESTED;
    UT_CONTROL_PLANE_DEBUG("EXIT_REQUESTED message from [%s] \n", __func__);
    enqueue_message(&msg);
    UT_CONTROL_PLANE_DEBUG("msg count inside CP_exit = %d\n", message_count);

    // Wait for the thread to finish
    if (pthread_join(pInternal->thread, NULL) != 0)
    {
        UT_CONTROL_PLANE_ERROR("Failed to join thread(1st) from instance = %p\n", pInternal);
        /*TODO: need to confirm if this return is required*/
        //lws_context_destroy(pInternal->context);
        //free(pInternal);
        //return;
    }
    UT_CONTROL_PLANE_DEBUG("Thread 1 is also exited from instance : %p\n", pInternal);

    if (pInternal->context != NULL)
    {
        lws_context_destroy(pInternal->context);
    }

    memset(pInternal, 0, sizeof(ut_cp_instance_internal_t));

    free(pInternal);
}

void UT_ControlPlane_Start( ut_controlPlane_instance_t *pInstance)
{
    ut_cp_instance_internal_t *pInternal = validateCPInstance(pInstance);

    if (pInternal == NULL)
    {
        return;
    }

    pthread_create(&pInternal->thread, NULL, thread_function, (void*) pInternal );

}

ut_control_plane_status_t UT_ControlPlane_RegisterCallbackOnMessage(ut_controlPlane_instance_t *pInstance, char *key, ut_control_callback_t callbackFunction)
{
    ut_cp_instance_internal_t *pInternal = (ut_cp_instance_internal_t *)pInstance;

    if ( pInternal == NULL )
    {
        UT_CONTROL_PLANE_ERROR("Invalid Handle");
        return UT_CONTROL_PLANE_STATUS_CALLBACK_LIST_INVALID_HANDLE;
    }

    if ( key == NULL )
    {
        UT_CONTROL_PLANE_ERROR("Invalid Handle");
        return UT_CONTROL_PLANE_STATUS_CALLBACK_LIST_INVALID_HANDLE;
    }

    if ( callbackFunction == NULL )
    {
        return UT_CONTROL_PLANE_INVALID_PARAM;
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
        UT_CONTROL_PLANE_ERROR("\nInvalid Handle");
        return NULL;
    }

    if (pInternal->magic != UT_CP_MAGIC)
    {
        //assert(pInternal->magic != UT_CP_MAGIC);
        UT_CONTROL_PLANE_ERROR("\nInvalid Handle - magic failure");
        return NULL;
    }

    return pInternal;
}