/*
 *
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

#ifndef UT_CONTROL_PLANE_H
#define UT_CONTROL_PLANE_H

#include <ut_kvp.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Definition */
#define UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES (32) /*!< Maximum number of registered callback entries. */

/**!
 * @brief Status codes for control plane operations.
 */
typedef enum
{
    UT_CONTROL_PLANE_STATUS_OK = 0,           /*!< Operation successful. */
    UT_CONTROL_PLANE_STATUS_LIST_FULL,        /*!< Callback list is full. */
    UT_CONTROL_PLANE_STATUS_INVALID_HANDLE,   /*!< Invalid control plane instance handle. */
    UT_CONTROL_PLANE_STATUS_INVALID_PARAM     /*!< Invalid parameter passed. */
} ut_control_plane_status_t;

typedef struct
{
    char *string;
    int32_t value;
} ut_control_keyStringMapping_t;

typedef void ut_controlPlane_instance_t; /*!< Handle to a control plane instance */

/**
 * @typedef ut_control_callback_t
 * @brief Synchronous control-plane message callback.
 *
 * This callback is invoked when a registered message key is received.
 * The call is synchronous; return promptly.
 *
 * ## Lifetime & ownership
 * - `instance` is a framework-owned handle valid **only for the duration of
 *   this callback**. Do not store it or any pointers into its internals.
 * - To retain payload beyond the callback, extract a serialized copy with
 *   `ut_kvp_getData()`, copy that data into your own storage/IPC buffer, and
 *   then `free()` the buffer returned by `ut_kvp_getData()` **before returning**.
 * - Parsing (e.g., `ut_kvp_openMemory()`) should be done later, outside the
 *   callback, using the copy you made.
 *
 * ## Typical workflow
 * - In-callback: fast decisions via typed getters; or stage the message by
 *   copying the serialized blob to a queue/shared memory for deferred work.
 * - Out-of-callback: reconstruct a KVP instance from the staged bytes with
 *   `ut_kvp_openMemory()`.
 *
 * @param key       Null-terminated message key that triggered the callback.
 * @param instance  Transient KVP handle containing message data.
 * @param userData  User pointer provided at registration.
 */
typedef void (*ut_control_callback_t)(char *key, ut_kvp_instance_t *instance, void *userData);

/**
 * @brief Initializes a control plane instance.
 * @param monitorPort - Port number to monitor for incoming messages.
 * @returns Handle to the created control plane instance, or NULL on failure.
 */
ut_controlPlane_instance_t* UT_ControlPlane_Init( uint32_t monitorPort );

/**
* @brief Registers a synchronous callback function for a specific message key.
*
* The callback is invoked immediately when the specified key is received.
*
* ## Data lifetime of callback parameters
* - The `instance` parameter provided to the callback is framework-owned and
*   valid only for the duration of the callback execution.
* - Do not attempt to free or retain `instance` directly. It is released
*   automatically after the callback returns.
* - If the application needs to retain message contents beyond the callback,
*   it must make a copy. The common pattern is:
*   - Use `ut_kvp_getData(instance)` to obtain a serialized payload (caller-owned).
*   - Copy that payload into application-owned memory (e.g. queue, IPC buffer).
*   - Free the buffer returned by `ut_kvp_getData()` before returning.
*   - Later, reconstruct a KVP instance with `ut_kvp_openMemory()`.
*
* @param pInstance - Handle to the control plane instance.
* @param key - Null-terminated string representing the message key to trigger the callback.
* @param callbackFunction - Callback function to be invoked synchronously when the key is received.
* @param userData - User-provided handle passed back to the callback.
* @returns Status of the registration operation (`ut_control_plane_status_t`).
* @retval UT_CONTROL_PLANE_STATUS_OK - Success.
* @retval UT_CONTROL_PLANE_STATUS_INVALID_HANDLE - Invalid control plane instance handle.
* @retval UT_CONTROL_PLANE_STATUS_INVALID_PARAM - Invalid parameter passed.
* @retval UT_CONTROL_PLANE_STATUS_CALLBACK_LIST_FULL - Callback list is full.
*/
ut_control_plane_status_t UT_ControlPlane_RegisterCallbackOnMessage(ut_controlPlane_instance_t *pInstance,
                                                                    char *key,
                                                                    ut_control_callback_t callbackFunction,
                                                                    void *userData);

/**
 * @brief Starts the control plane listening for incoming messages.
 * @param pInstance - Handle to the control plane instance.
 */
void UT_ControlPlane_Start( ut_controlPlane_instance_t *pInstance );

/**
 * @brief Stops the control plane listening for incoming messages.
 * @param pInstance - Handle to the control plane instance.
 */
void UT_ControlPlane_Stop(ut_controlPlane_instance_t *pInstance);

/**
 * @brief Releases resources and destroys a control plane instance.
 * @param pInstance - Handle to the control plane instance to be destroyed.
 */
void UT_ControlPlane_Exit( ut_controlPlane_instance_t *pInstance );

/**
 * @brief Retrieves the mapped value corresponding to the given search string from the key map table.
 *
 * This function searches the provided key map table (`conversionMap`) for the specified `searchString`.
 * If the `searchString` is found in the map, the corresponding mapped value is returned.
 * If the `searchString` is not found, the function returns the `onNotFoundValue`.
 *
 * @param conversionMap Pointer to the key map table that contains key-value pairs.
 * @param searchString  Pointer to the string that is being searched for in the map.
 * @param onNotFoundValue The value to return if the `searchString` is not found in the map.
 * @return The mapped value associated with `searchString` if found, otherwise `onNotFoundValue`.
 */
uint32_t UT_Control_GetMapValue(const ut_control_keyStringMapping_t *conversionMap, char *searchString, int onNotFoundValue);

/**
 * @brief Retrieves the mapped string corresponding to the given key from the key map table.
 *
 * This function searches the provided key map table (`conversionMap`) for the specified integer `key`.
 * If the `key` is found in the map, the corresponding string is returned.
 * If the `key` is not found, the function returns `NULL`.
 *
 * @param conversionMap Pointer to the key map table that contains key-string pairs.
 * @param key The integer key for which the mapped string is being searched.
 * @return Pointer to the string associated with `key` if found, otherwise `NULL`.
 */
char *UT_Control_GetMapString(const ut_control_keyStringMapping_t *conversionMap, int32_t key);

#ifdef __cplusplus
}
#endif

#endif /* UT_CONTROL_PLANE_H */
