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

static UT_test_suite_t *gpAssertSuite1 = NULL;
static UT_test_suite_t *gpAssertSuite2 = NULL;

static ut_controlPlane_instance_t *gInstance = NULL;
static bool gMessageRecieved = false;

void test_ut_control_testInitExit()
{
    ut_controlPlane_instance_t *pInstance = NULL;
    ut_controlPlane_instance_t *pInstance1 = NULL;

    UT_ControlPlane_Exit(NULL);
    UT_ASSERT( pInstance == NULL );

    pInstance = UT_ControlPlane_Init(8080);
    UT_ASSERT( pInstance != NULL );

    pInstance1 = UT_ControlPlane_Init(9000);
    UT_ASSERT( pInstance1 != NULL);

    UT_ASSERT( pInstance != pInstance1 );

    UT_ControlPlane_Exit(pInstance);
    UT_ControlPlane_Exit(pInstance1);

}

void test_ut_control_testStart()
{
    ut_controlPlane_instance_t *pInstance = NULL;
    ut_controlPlane_instance_t *pInstance1 = NULL;

    pInstance = UT_ControlPlane_Init(8080);
    UT_ControlPlane_Start(NULL);
    UT_ControlPlane_Start(pInstance);

    pInstance1 = UT_ControlPlane_Init(9000);
    UT_ControlPlane_Start(pInstance1);

    UT_ASSERT( pInstance != pInstance1 );
    UT_ControlPlane_Exit(pInstance);

    UT_ASSERT( pInstance != pInstance1 );
    UT_ASSERT( pInstance != NULL );

    UT_ControlPlane_Exit(pInstance1);
    UT_ASSERT( pInstance != pInstance1 );
}

void testCallback(char *key, ut_kvp_instance_t *instance)
{
    printf("*******************************Inside testCallback************************\n");
    ut_kvp_print( instance );
    gMessageRecieved = true;
}

void testRMFCallback(char *key, ut_kvp_instance_t *instance)
{
    UT_LOG("\n**************testRMFCallback is called****************\n");
    ut_kvp_print( instance );
    gMessageRecieved = true;
}

static void UT_ControlPlane_Sigint_Handler(int sig)
{
    UT_ControlPlane_Exit(gInstance);
}

void test_ut_control_performInit( void )
{
    gInstance = UT_ControlPlane_Init(8080);
    UT_ASSERT(gInstance != NULL);

    signal(SIGINT, UT_ControlPlane_Sigint_Handler);
}

void test_ut_control_performStart()
{

    UT_ControlPlane_RegisterCallbackOnMessage(gInstance, "test/myCommand", &testCallback);
    UT_LOG("\n Please run this command from the command line");
    UT_LOG("\n Waiting client test/myCommand to send yaml/json file");

    gMessageRecieved = false;

    UT_ControlPlane_Start(gInstance);

    UT_ControlPlane_RegisterCallbackOnMessage(gInstance, "test2/myCommand1", &testRMFCallback);
    UT_LOG("\nWaiting client test/myCommand2 to send yaml/json file");
}

void wait_function()
{
    UT_LOG("Please Run the command `test1DataSend.sh` and press return;'");
    uint32_t countdown = 15;

    while (countdown > 0)
    { 
        if (gMessageRecieved)
        {
            //UT_ASSERT_STRING_EQUAL (MyExpectedMessage, MyMessageRecieved ) ;
            break;
        }
        sleep(1); /* Sleep for 1 second and re-check for global message from the callback */
        countdown--;
    }
}

void test_ut_control_performExit( void )
{
    signal(SIGINT, NULL);
    UT_ControlPlane_Exit(gInstance);
}

void register_cp_function()
{
    /* L1 - ut _control function tests */
    gpAssertSuite1 = UT_add_suite("ut-cp - system control", NULL, NULL);
    assert(gpAssertSuite1 != NULL);
    UT_add_test(gpAssertSuite1, "ut-cp Init Exit", test_ut_control_testInitExit);
    UT_add_test(gpAssertSuite1, "ut-cp websocket service", test_ut_control_testStart);

    /* L2 - ut_control Module tests */
    gpAssertSuite2 = UT_add_suite("ut-cp - client control", NULL, NULL);
    assert(gpAssertSuite2 != NULL);
    UT_add_test(gpAssertSuite2, "ut-cp Init", test_ut_control_performInit);
    UT_add_test(gpAssertSuite2, "ut-cp Start", test_ut_control_performStart);
    UT_add_test(gpAssertSuite2, "ut-cp Wait", wait_function);
    UT_add_test(gpAssertSuite2, "ut-cp Exit", test_ut_control_performExit);

}