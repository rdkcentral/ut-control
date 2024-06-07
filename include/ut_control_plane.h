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


 // UT_CONTROL_PLANE_STATUS_OK = 0,
 // UT_CONTROL_PLANE_INVALID_HANDLE,

typedef enum
{
  UT_CONTROL_PLANE_STATUS_CALLBACK_LIST_FULL = 0,
  UT_CONTROL_PLANE_INVALID_PARAM,
  UT_CONTROL_PLANE_STATUS_CALLBACK_LIST_INVALID_HANDLE,
  UT_CONTROL_PLANE_STATUS_CALLBACK_LIST_OK
}ut_control_plane_status_t;

/**! Handle to a control plane instance. */
typedef void ut_controlPlane_instance_t;

/* Callback function that will be triggered */
typedef void (*ut_control_callback_t)( char *key, ut_kvp_instance_t *instance );

/* Init the control plane and create instance */
ut_controlPlane_instance_t* UT_ControlPlane_Init( uint32_t monitorPort );

/* pInstance, will be freed on return from the callback */
ut_control_plane_status_t UT_ControlPlane_RegisterCallbackOnMessage(ut_controlPlane_instance_t *pInstance, char *key, ut_control_callback_t callbackFunction);

/* Start the control Plane */
void UT_ControlPlane_Start( ut_controlPlane_instance_t *pInstance);

/* Exit the controlPlane instance and release */
void UT_ControlPlane_Exit( ut_controlPlane_instance_t *pInstance );

//TODO: to be deleted
//void control_plane_main();
///void testCallback(char *key, ut_kvp_instance_t *instance);
//void testRMFCallback(char *key, ut_kvp_instance_t *instance);

