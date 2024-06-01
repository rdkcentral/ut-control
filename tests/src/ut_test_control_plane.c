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

/* Module Includes */
#include <ut.h>
#include <ut_log.h>
#include <ut_control_plane.h>

#define KVP_VALID_TEST_NOT_VALID_YAML_FORMATTED_FILE "assets/no_data_file.yaml"
#define KVP_VALID_TEST_ZERO_LENGTH_YAML_FILE "assets/zero_length.yaml"
#define KVP_VALID_TEST_NO_FILE "assets/this_does_not_exist.yaml"
#define KVP_VALID_TEST_YAML_FILE "assets/test_kvp.yaml"
#define KVP_VALID_TEST_JSON_FILE "assets/test_kvp.json"

//static ut_kvp_instance_t *gpMainTestInstance = NULL;
static UT_test_suite_t *gpKVPSuite = NULL;

//static int test_ut_kvp_createGlobalYAMLInstance(void);


void test_ut_kvp_testCreateDestroy(void)
{
   
}

void test_ut_kvp_open( void )
{
   
}

void test_ut_kvp_open_malloced( void )
{
    
}

void register_cp_functions( void )
{
    gpKVPSuite=gpKVPSuite;
    gpKVPSuite = UT_add_suite("ut-kvp - test functions ", NULL, NULL);
    assert(gpKVPSuite != NULL);

    UT_add_test(gpKVPSuite, "kvp create / destroy", test_ut_kvp_testCreateDestroy);
    UT_add_test(gpKVPSuite, "kvp read", test_ut_kvp_open);
    UT_add_test(gpKVPSuite, "kvp read with malloced data", test_ut_kvp_open_malloced);

}
