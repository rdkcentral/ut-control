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

#define UT_CONTROL_PLANE_MAX_KEY_SIZE (64)
#define UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES (32)

typedef enum
{
    UT_CONTROL_PLANE_STATUS_CALLBACK_LIST_FULL = 0,
    UT_CONTROL_PLANE_STATUS_CALLBACK_LIST_INVALID_HANDLE,
    UT_CONTROL_PLANE_STATUS_CALLBACK_LIST_OK
}CallbackListStatus_t;

/**! Handle to a control plane instance. */
typedef void ut_controlPlane_instance_t;

/* Callback function that will be triggered */
typedef void (*ut_control_callback_t)( char *key, ut_kvp_instance_t *instance );

/* Init the control plane and create instance */
ut_controlPlane_instance_t* UT_ControlPlane_Init();

/* pInstance, will be freed on return from the callback */
CallbackListStatus_t UT_ControlPlane_RegisterCallbackOnMessage(ut_controlPlane_instance_t *pInstance, char *key, ut_control_callback_t callbackFunction);

/* Exit the controlPlane instance and release */
void UT_ControlPlane_Exit( ut_controlPlane_instance_t *pInstance );

typedef struct
{
  char key[UT_CONTROL_PLANE_MAX_KEY_SIZE];
  ut_control_callback_t pCallback;
}CallbackEntry_t;

static CallbackEntry_t callbackEntryList[UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES];
static uint32_t callback_entry_index=0;
static ut_control_callback_t callbackList[UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES];
static uint32_t callback_list_index=0;
static uint32_t lastFreeCallbackSlot=0; /* Must always be < UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES */