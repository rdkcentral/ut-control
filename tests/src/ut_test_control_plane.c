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

static UT_test_suite_t *gpAssertSuite1 = NULL;

void test_ut_control_open()
{
    control_plane_main();
}

void register_cp_function()
{
    gpAssertSuite1 = UT_add_suite("ut-cp - assert open / close", NULL, NULL);
    assert(gpAssertSuite1 != NULL);
    UT_add_test(gpAssertSuite1, "kvp profile open()", test_ut_control_open);
    //UT_add_test(gpAssertSuite1, "kvp profile getInstance()", test_ut_kvp_get_instance);
    //UT_add_test(gpAssertSuite1, "kvp profile close()", test_ut_kvp_profile_close);
}