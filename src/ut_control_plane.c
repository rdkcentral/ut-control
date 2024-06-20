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
#include <assert.h>

/* Application Includes */
#include <ut_control_plane.h>
#include <ut_kvp.h>
#include <ut_log.h>

/* External libraries */
#include <libwebsockets.h>

#define UT_CONTROL_PLANE_ERROR(f_, ...) UT_LOG_ERROR((f_), ##__VA_ARGS__)
#define UT_CONTROL_PLANE_DEBUG(f_, ...) UT_LOG_DEBUG((f_), ##__VA_ARGS__)

#define MAX_MESSAGES 32

#define UT_CP_MAGIC (0xdeadbeef)

typedef struct
{
  char key[UT_KVP_MAX_ELEMENT_SIZE];
  ut_control_callback_t pCallback;
}CallbackEntry_t;

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
    cp_message_t message_queue[MAX_MESSAGES];
    uint32_t message_count;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_condition;
} ut_cp_instance_internal_t;

/* Static protypes */
static void enqueue_message(cp_message_t *data, ut_cp_instance_internal_t *pInternal );
static cp_message_t* dequeue_message(ut_cp_instance_internal_t *pInternal);
static void call_callback_on_match(cp_message_t *mssg, ut_cp_instance_internal_t *pInternal);
static void *service_ws_requests(void *data);
static void *service_state_machine(void *data);
static int callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
static ut_cp_instance_internal_t *validateCPInstance(ut_controlPlane_instance_t *pInstance);

/* Local Fucntions*/
static void enqueue_message(cp_message_t *data, ut_cp_instance_internal_t *pInternal )
{
    pthread_mutex_lock(&pInternal->queue_mutex);
    if (pInternal->message_count < MAX_MESSAGES)
    {
        pInternal->message_queue[pInternal->message_count].size = data->size;
        pInternal->message_queue[pInternal->message_count].status = data->status;
        pInternal->message_queue[pInternal->message_count].message = data->message;
        pInternal->message_count++;
        pthread_cond_signal(&pInternal->queue_condition);
    }
    pthread_mutex_unlock(&pInternal->queue_mutex);
}

static cp_message_t* dequeue_message(ut_cp_instance_internal_t *pInternal)
{
    cp_message_t *msg;

    pthread_mutex_lock(&pInternal->queue_mutex);
    while (pInternal->message_count == 0)
    {
        pthread_cond_wait(&pInternal->queue_condition, &pInternal->queue_mutex);
    }
    msg = &pInternal->message_queue[0];
    for (int i = 0; i < pInternal->message_count - 1; i++)
    {
        pInternal->message_queue[i] = pInternal->message_queue[i + 1];
    }
    pInternal->message_count--;
    pthread_mutex_unlock(&pInternal->queue_mutex);
    return msg;
}

static void call_callback_on_match(cp_message_t *mssg, ut_cp_instance_internal_t *pInternal)
{
    ut_kvp_instance_t *pkvpInstance = NULL;
    ut_kvp_status_t status;
    char result_kvp[UT_KVP_MAX_ELEMENT_SIZE] = {0xff};

    if (mssg->message == NULL)
    {
        return;
    }

    pkvpInstance = ut_kvp_createInstance();

    /* Note: mssg-message data will be freed by the destoryInstance() function */
    status = ut_kvp_openMemory(pkvpInstance, mssg->message, mssg->size );
    if (status != UT_KVP_STATUS_SUCCESS)
    {
        UT_CONTROL_PLANE_ERROR("ut_kvp_open() - Read Failure\n");
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

static void *service_ws_requests(void *data)
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

static void *service_state_machine(void *data)
{
    ut_cp_instance_internal_t *pInternal = validateCPInstance((ut_controlPlane_instance_t*)data);
    cp_message_t *msg;

    if (pInternal == NULL)
    {
        return NULL;
    }
    pthread_create(&pInternal->ws_thread_handle, NULL, service_ws_requests, data );
    UT_CONTROL_PLANE_DEBUG("pthread id 2 = %ld\n", pInternal->ws_thread_handle);

    while (!pInternal->exit_request)
    {
        msg = dequeue_message( pInternal );

        switch (msg->status)
        {
            case EXIT_REQUESTED:
            {
                UT_CONTROL_PLANE_DEBUG("EXIT REQUESTED in thread1. Thread1 going to exit\n");
                pInternal->exit_request = true;
            }
            break;

            case DATA_RECIEVED:
            {
                UT_CONTROL_PLANE_DEBUG("DATA RECEIVED\n");
                call_callback_on_match(msg, pInternal);
            }
            break;

            default:
            {

            }
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
    cp_message_t msg;
    ut_cp_instance_internal_t *pInternal = (ut_cp_instance_internal_t* )lws_context_user(lws_get_context(wsi));

    switch (reason)
    {
        case LWS_CALLBACK_ESTABLISHED:
        {
            UT_CONTROL_PLANE_DEBUG("Client connected\n");
        }
        break;

        case LWS_CALLBACK_RECEIVE:
        {
            UT_CONTROL_PLANE_DEBUG("LWS_CALLBACK_RECEIVE\n");
            msg.message = malloc((int)len + 1);
            assert( msg.message != NULL);
            if (msg.message == NULL)
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
            enqueue_message(&msg, pInternal);
            // Echo back received message
            //lws_write(wsi, in, len, LWS_WRITE_TEXT);
        }
        break;

        default:
        {
        }
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
    ut_cp_instance_internal_t *pInstance;
  
    pInstance = malloc(sizeof(ut_cp_instance_internal_t));
    memset(pInstance, 0, sizeof(ut_cp_instance_internal_t));

    if ( pInstance == NULL )
    {
        //assert( pInstance != NULL );
        UT_CONTROL_PLANE_ERROR("Malloc was not able to provide memory\n");
        return NULL;
    }

    if ( monitorPort == 0 )
    {
        //assert( pInstance != NULL );
        UT_CONTROL_PLANE_ERROR("port cannot be 0\n");
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

    UT_ControlPlane_Stop(pInstance);

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

void UT_ControlPlane_Stop( ut_controlPlane_instance_t *pInstance )
{
    ut_cp_instance_internal_t *pInternal = validateCPInstance(pInstance);
    cp_message_t msg = {0};

    if (pInternal == NULL)
    {
        return;
    }

    if ( pInternal->state_machine_thread_handle )
    {
        memset(&msg, 0, sizeof(msg));
        msg.status = EXIT_REQUESTED;
        enqueue_message(&msg, pInternal);
        if (pthread_join(pInternal->state_machine_thread_handle, NULL) != 0)
        {
            UT_CONTROL_PLANE_ERROR("Failed to join state_machine_thread_handle(1st) from instance = %p\n", pInternal);
            /*TODO: need to confirm if this return is required*/
        }
    }
    pInternal->state_machine_thread_handle = 0;
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
    strncpy(pInternal->callbackEntryList[pInternal->callback_entry_index].key, key,UT_KVP_MAX_ELEMENT_SIZE);
    pInternal->callbackEntryList[pInternal->callback_entry_index].pCallback = callbackFunction;
    pInternal->callback_entry_index++;
    UT_CONTROL_PLANE_DEBUG("callback_entry_index : %d\n", pInternal->callback_entry_index);
    return UT_CONTROL_PLANE_STATUS_OK;
}

/** Static Functions */
static ut_cp_instance_internal_t *validateCPInstance(ut_controlPlane_instance_t *pInstance)
{
    ut_cp_instance_internal_t *pInternal = (ut_cp_instance_internal_t *)pInstance;

    if ( pInstance == NULL )
    {
        //assert(pInternal == NULL);
        UT_CONTROL_PLANE_ERROR("Invalid Handle\n");
        return NULL;
    }

    if (pInternal->magic != UT_CP_MAGIC)
    {
        //assert(pInternal->magic != UT_CP_MAGIC);
        UT_CONTROL_PLANE_ERROR("Invalid Handle - magic failure\n");
        return NULL;
    }

    return pInternal;
}