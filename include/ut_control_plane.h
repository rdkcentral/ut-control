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


#include <ut_kvp.h>

#define UT_CONTROL_PLANE_MAX_KEY_SIZE (64)  /*!< Maximum length for a control plane key (bytes). */
#define UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES (32) /*!< Maximum number of registered callback entries. */


/**!
 * @brief Status codes for control plane operations.
 */
typedef enum
{
    UT_CONTROL_PLANE_STATUS_LIST_FULL = 0,        /*!< Callback list is full. */
    UT_CONTROL_PLANE_STATUS_INVALID_HANDLE,       /*!< Invalid control plane instance handle. */
    UT_CONTROL_PLANE_STATUS_LIST_OK,              /*!< Callback operation successful. */
    UT_CONTROL_PLANE_STATUS_INVALID_PARAM,        /*!< Invalid parameter passed. */
    UT_CONTROL_PLANE_STATUS_OK = 0                /*!< Operation successful. */
} ut_control_plane_status_t;

typedef void ut_controlPlane_instance_t; /*!< Handle to a control plane instance */

/** @brief  Callback function type for handling control plane messages. */
typedef void (*ut_control_callback_t)( char *key, ut_kvp_instance_t *instance );

/**
 * @brief Initializes a control plane instance.
 * @param monitorPort - Port number to monitor for incoming messages.
 * @returns Handle to the created control plane instance, or NULL on failure.
 */
ut_controlPlane_instance_t* UT_ControlPlane_Init( uint32_t monitorPort );

/**
 * @brief Registers a callback function for a specific message key.
 * @param pInstance - Handle to the control plane instance.
 * @param key - Null-terminated string representing the message key to trigger the callback.
 * @param callbackFunction - Callback function to be invoked when the key is received.
 * @returns Status of the registration operation (`ut_control_plane_status_t`).
 * @retval UT_CONTROL_PLANE_STATUS_OK - Success
 * @retval UT_CONTROL_PLANE_STATUS_INVALID_HANDLE  - Invalid control plane instance handle.
 * @retavl UT_CONTROL_PLANE_STATUC_INVALID_PARAM - Invalid parameter passed
 * @retval UT_CONTROL_PLANE_STATUS_CALLBACK_LIST_FULL,  - Callback list is full
 */
ut_control_plane_status_t UT_ControlPlane_RegisterCallbackOnMessage(
    ut_controlPlane_instance_t *pInstance,
    char *key,
    ut_control_callback_t callbackFunction
);

/**
 * @brief Starts the control plane to listen for incoming messages.
 * @param pInstance - Handle to the control plane instance.
 */
void UT_ControlPlane_Start( ut_controlPlane_instance_t *pInstance);

/**
 * @brief Releases resources and destroys a control plane instance.
 * @param pInstance - Handle to the control plane instance to be destroyed.
 */
void UT_ControlPlane_Exit( ut_controlPlane_instance_t *pInstance );
