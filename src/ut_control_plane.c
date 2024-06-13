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
//#define UT_CONTROL_PLANE_DEBUG(f_, ...) printf((f_), ##__VA_ARGS__)

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
    struct lws *wsi;
    pthread_t state_machine_thread_handle;
    pthread_t ws_thread_handle;
    volatile bool exit_request;
    CallbackEntry_t callbackEntryList[UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES];
    uint32_t callback_entry_index;
} ut_cp_instance_internal_t;

typedef enum
{
    DATA_RECIEVED = 0,
    EXIT_REQUESTED
}eMessage_t;
typedef struct
{
    eMessage_t status;
    char *message;
    uint32_t size;
} cp_message_t;

#define MAX_CALLBACKS 10
#define MAX_MESSAGE_LEN 256
#define MAX_MESSAGES 100

#define UT_CP_MAGIC (0xdeadbeef)

cp_message_t message_queue[MAX_MESSAGES];
uint32_t message_count = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_condition = PTHREAD_COND_INITIALIZER;

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

void call_callback_on_match(cp_message_t *mssg, void* data)
{
    ut_kvp_instance_t *pkvpInstance = NULL;
    ut_kvp_status_t status;
    char result_kvp[UT_KVP_MAX_ELEMENT_SIZE] = {0xff};
    ut_cp_instance_internal_t *pInternal = validateCPInstance((ut_controlPlane_instance_t*)data);

    if (pInternal == NULL)
    {
        return;
    }

    if (mssg->message == NULL)
    {
        return;
    }

    pkvpInstance = ut_kvp_createInstance();
    status = ut_kvp_openMemory(pkvpInstance, mssg->message, mssg->size );
    if (status != UT_KVP_STATUS_SUCCESS)
    {
        UT_CONTROL_PLANE_ERROR("\nut_kvp_open() - Read Failure\n");
        ut_kvp_destroyInstance(pkvpInstance);
        return;
    }
    for (uint32_t i = 0; i < pInternal->callback_entry_index; i++)
    {
        CallbackEntry_t entry = pInternal->callbackEntryList[i];
        if (UT_KVP_STATUS_SUCCESS == ut_kvp_getStringField(pkvpInstance, entry.key, result_kvp, UT_KVP_MAX_ELEMENT_SIZE))
        {
            // call callback
            entry.pCallback(entry.key, pkvpInstance);
        }
    }
    ut_kvp_destroyInstance(pkvpInstance);
    return;
}

void *service_ws_requests(void *data)
{
    ut_cp_instance_internal_t *pInternal = validateCPInstance((ut_controlPlane_instance_t*)data);
     if (pInternal == NULL)
    {
        return NULL;
    }


    while (!pInternal->exit_request)
    {
        //TODO: Needs to be fixed in future
        lws_service(pInternal->context, 0);
        usleep(500);
    }

    return NULL;
}

void *service_state_machine(void *data)
{
    ut_cp_instance_internal_t *pInternal = validateCPInstance((ut_controlPlane_instance_t*)data);

    if (pInternal == NULL)
    {
        return NULL;
    }
    pthread_create(&pInternal->ws_thread_handle, NULL, service_ws_requests, data );
    UT_CONTROL_PLANE_DEBUG("pthread id 2 = %ld\n", pInternal->ws_thread_handle);

    while (!pInternal->exit_request)
    {
        cp_message_t *msg;
        msg = dequeue_message();

        switch (msg->status)
        {
        case EXIT_REQUESTED:
            UT_CONTROL_PLANE_DEBUG("EXIT REQUESTED in thread1. Thread1 going to exit\n");
            pInternal->exit_request = true;
            break;

        case DATA_RECIEVED:
            UT_CONTROL_PLANE_DEBUG("DATA RECEIVED\n");
            call_callback_on_match(msg, data);
            break;

        default:
            break;
        }
    }

    if (pthread_join(pInternal->ws_thread_handle, NULL) != 0)
    {
        UT_CONTROL_PLANE_ERROR("ws_thread_handle Failed to join from instance : %p \n", pInternal);
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
        UT_CONTROL_PLANE_ERROR("Malloc was not able to provide memory\n");
        return NULL;
    }

    pInstance->info.port = monitorPort;
    pInstance->info.iface = NULL;
    pInstance->info.protocols = protocols;
    pInstance->info.user = pInstance;

    pInstance->context = lws_create_context(&pInstance->info);
    if (pInstance->context == NULL)
    {
        UT_CONTROL_PLANE_ERROR("Error creating libwebsockets context\n");
        free( pInstance );
        return NULL;
    }

    pInstance->exit_request = false;
    pInstance->callback_entry_index = 0;
    pInstance->magic = UT_CP_MAGIC;

    return (ut_controlPlane_instance_t *)pInstance;
}

void UT_ControlPlane_Exit( ut_controlPlane_instance_t *pInstance )
{
    ut_cp_instance_internal_t *pInternal = validateCPInstance(pInstance);

    if (pInternal == NULL)
    {
        return;
    }

    if (pInternal->context == NULL)
    {
        UT_CONTROL_PLANE_ERROR("libwebsockets context was not created\n");
        return;
    }

    if (pInternal->state_machine_thread_handle )
    {
        cp_message_t msg;
        msg.status = EXIT_REQUESTED;
        UT_CONTROL_PLANE_ERROR("EXIT_REQUESTED message from [%s] \n", __func__);
        enqueue_message(&msg);
        if (pthread_join(pInternal->state_machine_thread_handle, NULL) != 0)
        {
            UT_CONTROL_PLANE_ERROR("Failed to join state_machine_thread_handle(1st) from instance = %p\n", pInternal);
            /*TODO: need to confirm if this return is required*/
            //lws_context_destroy(pInternal->context);
            //free(pInternal);
            // return;
        }
    }

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

    pthread_create(&pInternal->state_machine_thread_handle, NULL, service_state_machine, (void*) pInternal );
    UT_CONTROL_PLANE_DEBUG("pthread id = %ld\n", pInternal->state_machine_thread_handle);

}

ut_control_plane_status_t UT_ControlPlane_RegisterCallbackOnMessage(ut_controlPlane_instance_t *pInstance, char *key, ut_control_callback_t callbackFunction)
{
    ut_cp_instance_internal_t *pInternal = (ut_cp_instance_internal_t *)pInstance;

    if ( pInternal == NULL )
    {
        UT_CONTROL_PLANE_ERROR("Invalid Handle\n");
        return UT_CONTROL_PLANE_STATUS_INVALID_HANDLE;
    }

    if ( key == NULL )
    {
        UT_CONTROL_PLANE_ERROR("Invalid Param\n");
        return UT_CONTROL_PLANE_STATUS_INVALID_PARAM;
    }

    if ( callbackFunction == NULL )
    {
        UT_CONTROL_PLANE_ERROR("NULL callbackFunction\n");
        return UT_CONTROL_PLANE_STATUS_INVALID_PARAM;
    }


    if ( pInternal->callback_entry_index >= UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES )
    { 
        return UT_CONTROL_PLANE_STATUS_LIST_FULL;
    } 
    strncpy(pInternal->callbackEntryList[pInternal->callback_entry_index].key, key,UT_CONTROL_PLANE_MAX_KEY_SIZE);
    pInternal->callbackEntryList[pInternal->callback_entry_index].pCallback = callbackFunction;
    pInternal->callback_entry_index++;
    UT_CONTROL_PLANE_DEBUG("callback_entry_index : %d\n", pInternal->callback_entry_index);
    return UT_CONTROL_PLANE_STATUS_LIST_OK;
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