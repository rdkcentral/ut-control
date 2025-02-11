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
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>

/* Module Includes */
#include <ut.h>
#include <ut_log.h>

#include <ut_control_plane.h>

#include "ut_test_common.h"

#define UT_CONTROL_YAML_FILE "example.yaml"
#define UT_CONTROL_JSON_FILE "example.json"
#define UT_CONTROL_GET_FILE_JSON "get_example.json"
#define UT_CONTROL_GET_FILE_YAML "get_example.yaml"

static UT_test_suite_t *gpAssertSuite1 = NULL;
static UT_test_suite_t *gpAssertSuite2 = NULL;
static UT_test_suite_t *gpAssertSuite3 = NULL;

static ut_controlPlane_instance_t *gInstance = NULL;
static volatile bool gMessageRecievedYAML = false;
static volatile bool gMessageRecievedJSON = false;
static test_ut_memory_t gUserDataYaml, gUserDataJson;

const static ut_control_keyStringMapping_t numericMaptable [] = {
  { "one", (int32_t)1 },
  { "two", (int32_t)2 },
  { "three", (int32_t)3 }
};

char* testRestJSONCallback(const char *restApi, const char* requestType, ut_kvp_instance_t *instance, void* userData);
char* testRestYamlCallback(const char *restApi, const char* requestType, ut_kvp_instance_t *instance, void* userData);

/* L1 Function tests */
static void test_ut_control_l1_testInitExit()
{
    ut_controlPlane_instance_t *pInstance = NULL;
    ut_controlPlane_instance_t *pInstance1;

    UT_LOG("\ntest_ut_control_l1_testInitExit\n");

    /* Negative */
    UT_ControlPlane_Exit(NULL);
    UT_ASSERT( pInstance == NULL );

    pInstance = UT_ControlPlane_Init(0);
    UT_ASSERT( pInstance == NULL );

    /* Postive */
    pInstance = UT_ControlPlane_Init(8080);
    UT_ASSERT( pInstance != NULL );

    pInstance1 = UT_ControlPlane_Init(9000);
    UT_ASSERT( pInstance1 != NULL);

    UT_ASSERT( pInstance != pInstance1 );

    UT_ControlPlane_Exit(pInstance);
    UT_ControlPlane_Exit(pInstance1);

    UT_LOG("test_ut_control_l1_testInitExit\n");

}

static void test_ut_control_l1_testStartStop()
{
    ut_controlPlane_instance_t *pInstance;
    ut_controlPlane_instance_t *pInstance1;

    UT_LOG("\ntest_ut_control_l1_testStartStop\n");

    /* Negative */
    UT_ControlPlane_Start(NULL);
    UT_ControlPlane_Stop(NULL);

    /* Positive Instance*/
    UT_LOG("Control Plan Init\n");
    pInstance = UT_ControlPlane_Init(8080);
    UT_ASSERT(pInstance != NULL);

    /* Testing operation of start / stop */
    UT_ControlPlane_Start(pInstance);
    UT_ControlPlane_Stop(pInstance);

    /* Positive Instance */
    pInstance1 = UT_ControlPlane_Init(9000);
    UT_ASSERT(pInstance != NULL);

    UT_ControlPlane_Start(pInstance1);
    UT_ControlPlane_Stop(pInstance1);

    UT_ASSERT( pInstance != pInstance1 );

    UT_ControlPlane_Exit(pInstance1);

    /* Exit the other instance */
    UT_ControlPlane_Exit(pInstance);

    UT_LOG("test_ut_control_l1_testStartStop\n");
}

static void test_ut_control_l1_regsiterCallback()
{
    ut_controlPlane_instance_t *pInstance;
    ut_control_plane_status_t status;
    void* userData = (void* )strdup("testJSONCallbackStringInvalidParam");

    UT_LOG("\test_ut_control_l1_regsiterCallback\n");

    pInstance = UT_ControlPlane_Init(9000);
    UT_ASSERT(pInstance != NULL);

    status = UT_ControlPlane_RegisterCallbackOnMessage(pInstance, NULL, (ut_control_callback_t)&testRestYamlCallback, userData);
    UT_ASSERT_EQUAL(status, UT_CONTROL_PLANE_STATUS_INVALID_PARAM);

    status = UT_ControlPlane_RegisterCallbackOnMessage(pInstance, "test/yamlData", NULL, userData);
    UT_ASSERT_EQUAL(status, UT_CONTROL_PLANE_STATUS_INVALID_PARAM);

    status = UT_ControlPlane_RegisterCallbackOnMessage(pInstance, "test/yamlData", (ut_control_callback_t)&testRestYamlCallback, NULL);
    UT_ASSERT_EQUAL(status, UT_CONTROL_PLANE_STATUS_INVALID_PARAM);

    status = UT_ControlPlane_RegisterCallbackOnMessage(NULL, "test/yamlData", (ut_control_callback_t)&testRestYamlCallback, userData);
    UT_ASSERT_EQUAL(status, UT_CONTROL_PLANE_STATUS_INVALID_HANDLE);

    status = UT_ControlPlane_RegisterEndPointCallback(pInstance, "GET", NULL, &testRestJSONCallback, userData);
    UT_ASSERT_EQUAL(status, UT_CONTROL_PLANE_STATUS_INVALID_PARAM);

    status = UT_ControlPlane_RegisterEndPointCallback(pInstance, "GET", "/v1/callMyFunction2", NULL, userData);
    UT_ASSERT_EQUAL(status, UT_CONTROL_PLANE_STATUS_INVALID_PARAM);

    status = UT_ControlPlane_RegisterEndPointCallback(NULL, "GET", "/v1/callMyFunction2", &testRestJSONCallback, userData);
    UT_ASSERT_EQUAL(status, UT_CONTROL_PLANE_STATUS_INVALID_HANDLE);

    status = UT_ControlPlane_RegisterEndPointCallback(pInstance, NULL, "/v1/callMyFunction2", &testRestJSONCallback, userData);
    UT_ASSERT_EQUAL(status, UT_CONTROL_PLANE_STATUS_INVALID_PARAM);

    status = UT_ControlPlane_RegisterCallbackOnMessage(pInstance, "ttest/yamlData", (ut_control_callback_t)&testRestYamlCallback, userData);
    UT_ASSERT_EQUAL(status, UT_CONTROL_PLANE_STATUS_OK);
    free(userData); //freeing the userData after registration

    userData = (void* )strdup("testRestJSONCallback");
    status = UT_ControlPlane_RegisterEndPointCallback(pInstance, "GET", "/v1/callMyFunction2", &testRestJSONCallback, userData);
    UT_ASSERT_EQUAL(status, UT_CONTROL_PLANE_STATUS_OK);
    free(userData); //freeing the userData after registration

    userData = (void* )strdup("testJSONCallbackString");
    for (int i = 0; i< UT_CONTROL_PLANE_MAX_CALLBACK_ENTRIES - 2; i++ )
    {
        status = UT_ControlPlane_RegisterEndPointCallback(pInstance, "GET", "/v1/callMyFunction2", &testRestJSONCallback, userData);
        UT_ASSERT_EQUAL(status, UT_CONTROL_PLANE_STATUS_OK);
    }
    free(userData); //freeing the userData after registration

    userData = (void* )strdup("testJSONCallbackString");
    status = UT_ControlPlane_RegisterEndPointCallback(pInstance, "GET", "/v1/callMyFunction2", &testRestJSONCallback, userData);
    UT_ASSERT_EQUAL(status, UT_CONTROL_PLANE_STATUS_LIST_FULL);
    free(userData); //freeing the userData after registration

    UT_ControlPlane_Exit(pInstance);

    UT_LOG("test_ut_control_l1_regsiterCallback\n");
}

/* L2 Testing functions */
char* testRestJSONCallback(const char *restApi, const char* requestType, ut_kvp_instance_t *instance, void* userData)
{
    if (strcmp(requestType, "GET") == 0)
    {
        UT_LOG("**************testJSONCallback GET is called****************\n");
        char *jsonString = (char *)malloc(512); // Adjust size as needed
        if (!jsonString)
        {
            return NULL; // Memory allocation failed
        }

        // Construct the JSON string using sprintf
        sprintf(jsonString,
                "{\n"
                "  \"name\": \"John Doe\",\n"
                "  \"age\": %d,\n"
                "  \"email\": \"%s\",\n"
                "  \"isMarried\": %s,\n"
                "  \"children\": [\n"
                "    {\n"
                "      \"name\": \"%s\",\n"
                "      \"age\": %d\n"
                "    },\n"
                "    {\n"
                "      \"name\": \"%s\",\n"
                "      \"age\": %d\n"
                "    }\n"
                "  ],\n"
                "  \"hobbies\": [\n"
                "    \"%s\",\n"
                "    \"%s\",\n"
                "    \"%s\"\n"
                "  ]\n"
                "}",
                30, "johndoe@example.com", "false",
                "Alice", 5,
                "Bob", 3,
                "reading", "cycling", "traveling");

        return jsonString; // Return the allocated JSON string
    }
    else
    {
        char *kvpData;
        UT_LOG("**************testJSONCallback POST is called****************\n");
        kvpData = ut_kvp_getData(instance);

        if (kvpData != NULL)
        {
            // Print the emitted KVP string
            printf("%s\n", kvpData);

            // Free the emitted KVP string
            free(kvpData);
        }

        // UT_ASSERT_STRING_EQUAL(kvpData, data);
        gMessageRecievedJSON = true;
        return NULL;
    }
}

char* testRestYamlCallback(const char *restApi, const char* requestType, ut_kvp_instance_t *instance, void* userData)
{
    if (strcmp(requestType, "GET") == 0)
    {
        UT_LOG("**************testRestYamlCallback GET is called****************\n");
        char *yamlString = (char *)malloc(512); // Adjust size as needed
        if (!yamlString)
        {
            return NULL; // Memory allocation failed
        }

        // Construct the YAML string using sprintf
        sprintf(yamlString,
                "---\n"
                "name: John Doe\n"
                "age: %d\n"
                "email: %s\n"
                "isMarried: %s\n"
                "children:\n"
                "  - name: %s\n"
                "    age: %d\n"
                "  - name: %s\n"
                "    age: %d\n"
                "hobbies:\n"
                "  - %s\n"
                "  - %s\n"
                "  - %s\n",
                30, "johndoe@example.com", "false",
                "Alice", 5,
                "Bob", 3,
                "reading", "cycling", "traveling");

        return yamlString; // Return the allocated Yaml string
    }
    else
    {
        char *kvpData;
        UT_LOG("**************testRestYamlCallback POST is called****************\n");
        kvpData = ut_kvp_getData(instance);

        if (kvpData != NULL)
        {
            // Print the emitted KVP string
            printf("%s\n", kvpData);

            // Free the emitted KVP string
            free(kvpData);
        }

        // UT_ASSERT_STRING_EQUAL(kvpData, data);
        gMessageRecievedYAML = true;
        return NULL;
    }
}

static void UT_ControlPlane_Sigint_Handler(int sig)
{
    UT_LOG("Signal Handler invoked\n");
    UT_ControlPlane_Exit(gInstance);
}

static void test_ut_control_performInit( void )
{
    UT_LOG("test_ut_control_performInit()\n");
    gInstance = UT_ControlPlane_Init(8080);
    UT_ASSERT(gInstance != NULL);
    signal(SIGINT, UT_ControlPlane_Sigint_Handler);
}

static void test_ut_control_performStart()
{
    UT_LOG("UT_ControlPlane_RegisterCallbackOnMessage() client testRestYamlCallback - Negative\n");
    UT_ControlPlane_RegisterCallbackOnMessage(gInstance, "postBinaryYaml", (ut_control_callback_t)&testRestYamlCallback, NULL);

    UT_LOG("UT_ControlPlane_RegisterEndPointCallback() client testRestJSONCallback - Negative\n");
    UT_ControlPlane_RegisterEndPointCallback(gInstance, NULL, "/v1/callMyFunction2", NULL, NULL);

    if (read_file_into_memory(UT_CONTROL_YAML_FILE, &gUserDataYaml) == 0)
    {
        if (gUserDataYaml.buffer != NULL)
        {
            printf("Original Yaml file\n%s", (char*)gUserDataYaml.buffer);
        }

        UT_LOG("UT_ControlPlane_RegisterCallbackOnMessage() client testRestYamlCallback - Positive\n");
        UT_ControlPlane_RegisterEndPointCallback(gInstance, "POST", "postYaml", &testRestYamlCallback, &gUserDataYaml.buffer);
    }

    gMessageRecievedYAML = false;

    UT_LOG("UT_ControlPlane_RegisterEndPointCallback() client testRestJSONCallback - Positive \n");
    UT_ControlPlane_RegisterEndPointCallback(gInstance, "GET", "/v1/callMyFunction2", &testRestJSONCallback, NULL);

    UT_ControlPlane_Start(gInstance);

    /* This should still work after start */
    if (read_file_into_memory(UT_CONTROL_JSON_FILE, &gUserDataJson) == 0)
    {
        if (gUserDataJson.buffer != NULL)
        {
            printf("Original Json file\n%s", (char*)gUserDataJson.buffer);
        }

        UT_LOG("UT_ControlPlane_RegisterCallbackOnMessage() client testRestJSONCallback - Positive \n");
        UT_ControlPlane_RegisterCallbackOnMessage(gInstance, "postJson", (ut_control_callback_t)&testRestJSONCallback, &gUserDataJson.buffer);
    }

    gMessageRecievedJSON = false;

    UT_LOG("UT_ControlPlane_RegisterEndPointCallback() client testRestYamlCallback - Positive \n");
    UT_ControlPlane_RegisterEndPointCallback(gInstance, "GET", "/v1/callMyFunction", &testRestYamlCallback, NULL);
}

void run_client_function()
{
    uint32_t countdown = 15;
    bool receivedYAML = false;
    bool receivedJSON = false;

#ifdef WEBSOCKET_SERVER
    UT_LOG("Please Run the command `./python-client-send-json.py or/& ./python-client-send-yaml.py` from another terminal and press return;'\n");
    UT_LOG("In order to pass the test you need to run each of the python scripts'\n");
#else
    UT_LOG("Please Run the command `test_script_for_curl_request.sh` from another terminal and press return;'\n");
    UT_LOG("In order to pass the test you need to run each of the curl scripts'\n");
#endif

    while (countdown > 0)
    { 
        if (gMessageRecievedYAML)
        {
            /* TODO: Check the result so we can automate the test */
            //UT_ASSERT_STRING_EQUAL (MyExpectedMessage, MyMessageRecieved );
            gMessageRecievedYAML = false;
            receivedYAML = true;
        }

        if (gMessageRecievedJSON)
        {
            /* TODO: Check the result so we can automate the test */
            //UT_ASSERT_STRING_EQUAL (MyExpectedMessageRMF, MyMessageRecievedRMF );
            gMessageRecievedJSON = false;
            receivedJSON = true;
        }
        sleep(1); /* Sleep for 1 second and re-check for global message from the callback */
        countdown--;
    }

    UT_ASSERT_EQUAL(receivedYAML, true);
    UT_ASSERT_EQUAL(receivedJSON, true);
}

void test_ut_control_performStop( void )
{
    UT_LOG("test_ut_control_performStop()\n");
    UT_ControlPlane_Stop(gInstance);
}

void test_ut_control_performExit( void )
{
    UT_LOG("test_ut_control_performExit()\n");
    signal(SIGINT, NULL);
    if (gUserDataJson.buffer)
    {
        free(gUserDataJson.buffer);
    }

    if (gUserDataYaml.buffer)
    {
        free(gUserDataYaml.buffer);
    }

    UT_ControlPlane_Exit(gInstance);
}

void test_ut_control_get_map_value( void )
{
    UT_LOG("test_ut_control_get_map_value()\n");
    UT_ASSERT(UT_Control_GetMapValue(numericMaptable, "three", 0) == 3);
}

void test_ut_control_get_map_string( void )
{
    UT_LOG("test_ut_control_get_map_string()\n");
    UT_ASSERT_STRING_EQUAL(UT_Control_GetMapString(numericMaptable, 1), "one");
}

void register_cp_function()
{
    /* L1 - ut_control function tests */
    gpAssertSuite1 = UT_add_suite("L1 - ut_control function tests", NULL, NULL);
    assert(gpAssertSuite1 != NULL);
    UT_add_test(gpAssertSuite1, "ut-cp Init Exit", test_ut_control_l1_testInitExit);
    UT_add_test(gpAssertSuite1, "ut-cp register on message callback", test_ut_control_l1_regsiterCallback);
    UT_add_test(gpAssertSuite1, "ut-cp websocket service", test_ut_control_l1_testStartStop);

    /* L2 - ut_control Module tests */
    gpAssertSuite2 = UT_add_suite("L2 - ut_control Module tests", NULL, NULL);
    assert(gpAssertSuite2 != NULL);
    UT_add_test(gpAssertSuite2, "ut-cp Init", test_ut_control_performInit);
    UT_add_test(gpAssertSuite2, "ut-cp Start", test_ut_control_performStart);
    UT_add_test(gpAssertSuite2, "ut-cp run client", run_client_function);
    UT_add_test(gpAssertSuite2, "ut-cp Stop", test_ut_control_performStop);
    UT_add_test(gpAssertSuite2, "ut-cp Exit", test_ut_control_performExit);

    gpAssertSuite3 = UT_add_suite("L1 - ut_control mapping tests", NULL, NULL);
    assert(gpAssertSuite3 != NULL);
    UT_add_test(gpAssertSuite3, "ut-control Get Map Value", test_ut_control_get_map_value);
    UT_add_test(gpAssertSuite3, "ut-control get Map String", test_ut_control_get_map_string);

}