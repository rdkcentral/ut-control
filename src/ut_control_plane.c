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
    ut_control_on_message_callback_t pCallback;
    ut_control_endpoint_callback_t pStringCallback;
}Callback_type_t;

typedef struct
{
    char key[UT_KVP_MAX_ELEMENT_SIZE];
    Callback_type_t pCallbackType;
    void *userData;
} CallbackEntry_t;

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
#ifdef WEBSOCKET_SERVER
static int callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
#else
static int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
#define MAX_POST_DATA_SIZE 4096

struct per_session_data_http
{
    char post_data[MAX_POST_DATA_SIZE];
    int post_data_len;
};
#endif
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

    msg = (cp_message_t*)malloc(sizeof(cp_message_t));
    assert(msg != NULL);
    if (msg == NULL)
    {
        pthread_mutex_unlock(&pInternal->queue_mutex);
        return NULL; // Return NULL if memory allocation fails
    }

    // Copy the message content to the newly allocated memory
    msg->size = pInternal->message_queue[0].size;
    msg->status = pInternal->message_queue[0].status;
    msg->message = pInternal->message_queue[0].message;

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
        UT_CONTROL_PLANE_ERROR("ut_kvp_openMemory() - Read Failure\n");
        ut_kvp_destroyInstance(pkvpInstance);
        return;
    }
    for (uint32_t i = 0; i < pInternal->callback_entry_index; i++)
    {
        CallbackEntry_t entry = pInternal->callbackEntryList[i];
        if (UT_KVP_STATUS_SUCCESS == ut_kvp_getStringField(pkvpInstance, entry.key, result_kvp, UT_KVP_MAX_ELEMENT_SIZE))
        {
            // call callback
            entry.pCallbackType.pCallback(entry.key, pkvpInstance, entry.userData);
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
                free(msg);
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

#ifdef WEBSOCKET_SERVER
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
#else

// Function to compare strings with handling of leading '/'
static int compareStrings(const char *str1, const char *str2)
{
    // If str2 starts with '/', skip it for comparison
    if (str2[0] == '/')
    {
        str2++; // Move pointer to skip the '/'
    }

    if (str1[0] == '/')
    {
        str1++; // Move pointer to skip the '/'
    }
    return strcmp(str1, str2); // Compare adjusted strings
}

static char* create_response(ut_cp_instance_internal_t *pInternal, const char* key, const char* type)
{
    char* response = NULL;
    char* kvpData = NULL;
    ut_kvp_instance_t *pkvpInstance = NULL;
    ut_kvp_status_t status;

    for (uint32_t i = 0; i < pInternal->callback_entry_index; i++)
    {
        CallbackEntry_t entry = pInternal->callbackEntryList[i];

        if (compareStrings(entry.key, key) == 0)
        {
            kvpData = entry.pCallbackType.pStringCallback((char *)key, entry.userData);

            pkvpInstance = ut_kvp_createInstance();
            // The `kvpData` memory passed gets freed as part of destroy instance
            status = ut_kvp_openMemory(pkvpInstance, kvpData, strlen(kvpData));
            if (status != UT_KVP_STATUS_SUCCESS)
            {
                UT_CONTROL_PLANE_ERROR("ut_kvp_openMemory() - Read Failure\n");
                ut_kvp_destroyInstance(pkvpInstance);
                return NULL;
            }
            response = ut_kvp_getDataOfType(pkvpInstance, type);
            ut_kvp_destroyInstance(pkvpInstance);
        }
    }

    return response;
}

// Helper function to send error response
static int send_error_response(struct lws *wsi, int status, const char *content_type, const char *body)
{
    unsigned char buffer[LWS_PRE + 1024];
    unsigned char *p = buffer + LWS_PRE, *end = buffer + sizeof(buffer);

    // Add HTTP headers
    if (lws_add_http_common_headers(wsi, status, content_type, strlen(body), &p, end) < 0)
    {
        return -1;
    }

    // Finalize headers
    if (lws_finalize_http_header(wsi, &p, end) < 0)
    {
        return -1;
    }

    // Write headers
    if (lws_write(wsi, buffer + LWS_PRE, p - (buffer + LWS_PRE), LWS_WRITE_HTTP_HEADERS) < 0)
    {
        return -1;
    }

    // Write body
    if (lws_write(wsi, (unsigned char *)body, strlen(body), LWS_WRITE_HTTP_FINAL) < 0)
    {
        return -1;
    }

    return 0;
}

// Function to send error response
static int send_error(void *wsi, int status, const char *accept_header, const char *json_msg, const char *yaml_msg)
{
    const char *response_format = "application/json";
    const char *error_response = json_msg;
    int result;

    if (strncmp(accept_header, "application/x-yaml", 18) == 0)
    {
        response_format = "application/x-yaml";
        error_response = yaml_msg;
    }

    result = send_error_response(wsi, status, response_format, error_response) < 0 ? -1 : -1;
    return result;
}

// Function to validate Accept header
static int validate_accept_header(void *wsi, char *accept_header, size_t size)
{
    if (lws_hdr_copy(wsi, accept_header, size, WSI_TOKEN_HTTP_ACCEPT) <= 0 ||
        strncmp(accept_header, "application", 11) != 0)
    {
        UT_CONTROL_PLANE_ERROR("Missing or Invalid Accept header\n");
        return send_error(wsi, HTTP_STATUS_BAD_REQUEST, accept_header,
                          "{\"error\": \"Missing or Invalid Accept header\"}",
                          "error: Missing or Invalid Accept header\n");
    }
    return 0; // Success
}

// Function to extract key from URI
static int extract_key(const char *requested_uri, char *key, size_t size)
{
    char *key_start = (char* )requested_uri + 5; // Skip "/api/"
    if (strlen(key_start) == 0)
    {
        UT_CONTROL_PLANE_ERROR("Missing key in the URI\n");
        return -1;
    }
    strncpy(key, key_start, size - 1);
    UT_CONTROL_PLANE_DEBUG("Extracted Key: %s\n", key);
    return 0; // Success
}

// Function to determine response format
static char *determine_response_format(void *pInternal, const char *key, const char *accept_header)
{
    if (strncmp(accept_header, "application/json", 16) == 0)
    {
        return create_response(pInternal, key, "json");
    }
    else if (strncmp(accept_header, "application/x-yaml", 18) == 0)
    {
        return create_response(pInternal, key, "yaml");
    }

    UT_CONTROL_PLANE_ERROR("Internal Server Error\n");
    return NULL;
}

static int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    cp_message_t msg;
    int result = 0;
    ut_cp_instance_internal_t *pInternal = (ut_cp_instance_internal_t *)lws_context_user(lws_get_context(wsi));
    struct per_session_data_http *perSessionData = (struct per_session_data_http *)user;

    switch (reason)
    {

        case LWS_CALLBACK_HTTP:
        {
            UT_CONTROL_PLANE_DEBUG("LWS_CALLBACK_HTTP\n");

            char *requested_uri = (char *)in;     // Use the 'in' parameter to get the URI
            char accept_header[128] = {0};        // Buffer for the Accept header
            unsigned char buffer[LWS_PRE + 1024]; // Allocate buffer for headers and body

            UT_CONTROL_PLANE_DEBUG("Requested URI: %s\n", requested_uri);

            // Handle POST request based on Token-Type
            if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI)) {
                lwsl_user("Received a POST request\n");
                lws_callback_on_writable(wsi);
                return 0; // Let the body handling process continue
            }

            // Handle GET request for /api/<REST_API_NAME>

            // Validate Accept header
            if (validate_accept_header(wsi, accept_header, sizeof(accept_header)) < 0)
            {
                return -1;
            }

            // Validate URI
            if (strncmp(requested_uri, "/api/", 5) != 0)
            {
                UT_CONTROL_PLANE_ERROR("Invalid URI: %s\n", requested_uri);
                result = send_error(wsi, HTTP_STATUS_NOT_FOUND, accept_header,
                                  "{\"error\": \"Internal Server Error\"}",
                                  "error: Internal Server Error\n");
                return result;
            }

            // Extract key
            char key[256] = {0};
            if (extract_key(requested_uri, key, sizeof(key)) < 0)
            {
                result = send_error(wsi, HTTP_STATUS_BAD_REQUEST, accept_header,
                                  "{\"error\": \"Missing key in URI\"}",
                                  "error: Missing key in URI\n");
                return result;
            }

            // Determine response format
            char *response = determine_response_format(pInternal, key, accept_header);
            if (response == NULL)
            {
                result = send_error(wsi, HTTP_STATUS_BAD_REQUEST, accept_header,
                                  "{\"error\": \"Internal Server Error\"}",
                                  "error: Internal Server Error\n");
                return result;
            }

            // Send response
            unsigned char *p = buffer + LWS_PRE, *end = buffer + sizeof(buffer) - 1;
            if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK, accept_header, strlen(response), &p, end) < 0)
            {
                free(response);
                return -1;
            }

            if (lws_finalize_http_header(wsi, &p, end) < 0)
            {
                free(response);
                return -1;
            }

            if (lws_write(wsi, buffer + LWS_PRE, p - (buffer + LWS_PRE), LWS_WRITE_HTTP_HEADERS) < 0)
            {
                free(response);
                return -1;
            }

            if (lws_write(wsi, (unsigned char *)response, strlen(response), LWS_WRITE_HTTP_FINAL) < 0)
            {
                free(response);
                return -1;
            }

            free(response);
            return 1; // HTTP request handled successfully

            break;
        }
        case LWS_CALLBACK_HTTP_BODY:
        {
            UT_CONTROL_PLANE_DEBUG("LWS_CALLBACK_HTTP\n");
            if (perSessionData != NULL)
            {
                UT_CONTROL_PLANE_DEBUG("LWS_CALLBACK_HTTP, perSessionData not NULL\n");
                if ((perSessionData->post_data_len + len) < MAX_POST_DATA_SIZE)
                {
                    memcpy(perSessionData->post_data + perSessionData->post_data_len, in, len);
                    perSessionData->post_data_len += len;
                }
                else
                {
                    // POST data too large
                    UT_CONTROL_PLANE_ERROR("POST data too large\n");
                    return -1;
                }
            }
            break;
        }

        case LWS_CALLBACK_HTTP_BODY_COMPLETION:
        {
            UT_CONTROL_PLANE_DEBUG("LWS_CALLBACK_HTTP_BODY_COMPLETION\n");
            if (perSessionData != NULL)
            {
                UT_CONTROL_PLANE_DEBUG("LWS_CALLBACK_HTTP_BODY_COMPLETION, perSessionData not NULL\n");
                msg.message = malloc((int)perSessionData->post_data_len + 1);
                assert(msg.message != NULL);
                if (msg.message == NULL)
                {
                    UT_CONTROL_PLANE_ERROR("Malloc failed\n");
                    break;
                }
                msg.size = (int)perSessionData->post_data_len;
                msg.status = DATA_RECIEVED;
                strncpy(msg.message, (const char *)perSessionData->post_data, perSessionData->post_data_len);
                msg.message[perSessionData->post_data_len] = '\0';
                // UT_CONTROL_PLANE_DEBUG("Received message:\n %s\n", msg.message);
                enqueue_message(&msg, pInternal);
                // char response[] = "{\"status\": \"success\"}";
                // lws_write(wsi, (unsigned char *)response, strlen(response), LWS_WRITE_HTTP);
                return 1; // HTTP request handled
            }
            break;
        }

        default:
            break;
        }
            return 0;
}

#endif

#ifdef WEBSOCKET_SERVER
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
#else
static struct lws_protocols protocols[] = {
    {
        "http-only",
        callback_http,
        sizeof(struct per_session_data_http),
        MAX_POST_DATA_SIZE,
    },
    { NULL, NULL, 0, 0 }
};
#endif

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
    pInstance->info.protocols = protocols;
    pInstance->info.user = pInstance;

#ifdef WEBSOCKET_SERVER
    pInstance->info.iface = NULL;
#else
    pInstance->info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
#endif

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
        }
    }
    pInternal->state_machine_thread_handle = 0;
}

ut_control_plane_status_t UT_ControlPlane_RegisterCallbackOnMessage(ut_controlPlane_instance_t *pInstance, char *key, ut_control_on_message_callback_t callbackFunction, void *userData)
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

    if ( userData == NULL )
    {
        UT_CONTROL_PLANE_ERROR("NULL userData\n");
        return UT_CONTROL_PLANE_STATUS_INVALID_PARAM;
    }

    if ( pInternal->callback_entry_index >= UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES )
    { 
        return UT_CONTROL_PLANE_STATUS_LIST_FULL;
    } 
    strncpy(pInternal->callbackEntryList[pInternal->callback_entry_index].key, key,UT_KVP_MAX_ELEMENT_SIZE);
    pInternal->callbackEntryList[pInternal->callback_entry_index].pCallbackType.pCallback = callbackFunction;
    pInternal->callbackEntryList[pInternal->callback_entry_index].userData = userData;
    pInternal->callbackEntryList[pInternal->callback_entry_index].pCallbackType.pStringCallback = NULL;
    pInternal->callback_entry_index++;
    UT_CONTROL_PLANE_DEBUG("callback_entry_index : %d\n", pInternal->callback_entry_index);
    return UT_CONTROL_PLANE_STATUS_OK;
}

ut_control_plane_status_t UT_ControlPlane_RegisterEndPointCallback(ut_controlPlane_instance_t *pInstance, char *restAPI, ut_control_endpoint_callback_t callbackFunction, void *userData)

{
    ut_cp_instance_internal_t *pInternal = (ut_cp_instance_internal_t *)pInstance;

    if (pInternal == NULL)
    {
        UT_CONTROL_PLANE_ERROR("Invalid Handle\n");
        return UT_CONTROL_PLANE_STATUS_INVALID_HANDLE;
    }

    if (restAPI == NULL)
    {
        UT_CONTROL_PLANE_ERROR("Invalid Param\n");
        return UT_CONTROL_PLANE_STATUS_INVALID_PARAM;
    }

    if (callbackFunction == NULL)
    {
        UT_CONTROL_PLANE_ERROR("NULL callbackFunction\n");
        return UT_CONTROL_PLANE_STATUS_INVALID_PARAM;
    }

    if (userData == NULL)
    {
        UT_CONTROL_PLANE_ERROR("NULL userData\n");
    }

    if (pInternal->callback_entry_index >= UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES)
    {
        return UT_CONTROL_PLANE_STATUS_LIST_FULL;
    }
    strncpy(pInternal->callbackEntryList[pInternal->callback_entry_index].key, restAPI, UT_KVP_MAX_ELEMENT_SIZE);
    pInternal->callbackEntryList[pInternal->callback_entry_index].pCallbackType.pCallback = NULL;
    pInternal->callbackEntryList[pInternal->callback_entry_index].userData = userData;
    pInternal->callbackEntryList[pInternal->callback_entry_index].pCallbackType.pStringCallback = callbackFunction;
    pInternal->callback_entry_index++;
    UT_CONTROL_PLANE_DEBUG("callback_entry_index : %d\n", pInternal->callback_entry_index);
    return UT_CONTROL_PLANE_STATUS_OK;
}

uint32_t UT_Control_GetMapValue(const ut_control_keyStringMapping_t *conversionMap, char *searchString, int defaultValue)
{
    if (conversionMap == NULL || searchString == NULL)
    {
        return defaultValue;
    }

    for (int32_t i = 0; conversionMap[i].string != NULL; i++)
    {
        if (strcmp(searchString, conversionMap[i].string) == 0)
        {
            return conversionMap[i].value;
        }
    }
    return defaultValue;
}

char *UT_Control_GetMapString(const ut_control_keyStringMapping_t *conversionMap, int32_t key)
{
    if (conversionMap == NULL)
    {
        return NULL;
    }

    for (int32_t i = 0; conversionMap[i].string != NULL; i++)
    {
        if (key == conversionMap[i].value)
        {
            return conversionMap[i].string;
        }
    }
    return NULL;
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