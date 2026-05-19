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
#include <assert.h>

/* Module Includes */
#include <ut.h>
#include <ut_log.h>

static UT_test_suite_t *gpLogSuite = NULL;
static UT_test_suite_t *gpLogSuite2 = NULL;

/* Helper used to detect whether a macro argument is evaluated (i.e. not suppressed).
 * Marked unused to suppress -Wunused-function when all log macros that reference it
 * are suppressed by a low UT_LOG_LEVEL (e.g. UT_LOG_LEVEL_NONE). */
static int g_call_count = 0;

static __attribute__((unused)) const char *count_and_return(const char *msg)
{
    g_call_count++;
    return msg;
}

/**
 * @brief Verify that the compile-time log level constants have the correct numeric values.
 */
static void test_ut_log_level_constant_values(void)
{
    UT_LOG("test_ut_log_level_constant_values\n");

    UT_ASSERT(UT_LOG_LEVEL_NONE    == 0);
    UT_ASSERT(UT_LOG_LEVEL_ERROR   == 1);
    UT_ASSERT(UT_LOG_LEVEL_WARNING == 2);
    UT_ASSERT(UT_LOG_LEVEL_INFO    == 3);
    UT_ASSERT(UT_LOG_LEVEL_DEBUG   == 4);

    /* Ordering sanity */
    UT_ASSERT(UT_LOG_LEVEL_NONE    < UT_LOG_LEVEL_ERROR);
    UT_ASSERT(UT_LOG_LEVEL_ERROR   < UT_LOG_LEVEL_WARNING);
    UT_ASSERT(UT_LOG_LEVEL_WARNING < UT_LOG_LEVEL_INFO);
    UT_ASSERT(UT_LOG_LEVEL_INFO    < UT_LOG_LEVEL_DEBUG);

    UT_LOG("test_ut_log_level_constant_values end\n");
}

/**
 * @brief Verify that the default UT_LOG_LEVEL equals UT_LOG_LEVEL_WARNING (2)
 *        when not overridden at compile time.
 */
static void test_ut_log_default_level(void)
{
    UT_LOG("test_ut_log_default_level\n");

    UT_ASSERT(UT_LOG_LEVEL == UT_LOG_LEVEL_WARNING);

    UT_LOG("test_ut_log_default_level end\n");
}

/**
 * @brief Verify that the log macros which are active at the default WARNING level
 *        (UT_LOG_WARNING, UT_LOG_ERROR) execute without crashing and produce output.
 *
 * @note At the default UT_LOG_LEVEL_WARNING (2), UT_LOG_INFO and UT_LOG_DEBUG are
 *       suppressed and therefore not exercised here.
 */
static void test_ut_log_active_macros_at_warning_level(void)
{
    UT_LOG("test_ut_log_active_macros_at_warning_level\n");

    /* These should all expand to real calls at default UT_LOG_LEVEL = WARNING */
    UT_LOG_ERROR("error macro test: value=%d", 1);
    UT_LOG_WARNING("warning macro test: value=%d", 2);

    UT_LOG("test_ut_log_active_macros_at_warning_level end\n");
}

/**
 * @brief Verify that UT_LOG_DEBUG is suppressed at the default WARNING level
 *        and that its arguments are NOT evaluated.
 *
 * UT_LOG_DEBUG expands to:
 *     do { if (UT_LOG_ENABLED(DEBUG)) UT_logPrefix(..., arg); } while(0)
 *
 * When the level check is false the entire if-body is skipped, so
 * count_and_return() is never called.
 *
 * @note Assumes the binary is compiled at the default WARNING (2) level.
 */
static void test_ut_log_debug_suppressed_at_warning_level(void)
{
    UT_LOG("test_ut_log_debug_suppressed_at_warning_level\n");

    g_call_count = 0;
    UT_LOG_DEBUG("%s", count_and_return("debug suppressed test"));

    /* The if-body was not reached, so count_and_return() was not called. */
    UT_ASSERT(g_call_count == 0);

    UT_LOG("test_ut_log_debug_suppressed_at_warning_level end\n");
}

/**
 * @brief Verify that the active macros do not suppress their arguments at
 *        the default WARNING level (WARNING, ERROR arguments are evaluated).
 *
 * @note At the default UT_LOG_LEVEL_WARNING (2), UT_LOG_INFO is suppressed so
 *       its arguments are NOT evaluated.
 */
static void test_ut_log_active_macros_evaluate_args(void)
{
    UT_LOG("test_ut_log_active_macros_evaluate_args\n");

    g_call_count = 0;
    UT_LOG_WARNING("%s", count_and_return("warning arg"));
    UT_ASSERT(g_call_count == 1);

    g_call_count = 0;
    UT_LOG_ERROR("%s", count_and_return("error arg"));
    UT_ASSERT(g_call_count == 1);

    UT_LOG("test_ut_log_active_macros_evaluate_args end\n");
}

/**
 * @brief Verify active log macros handle multiple format specifiers correctly.
 *
 * @note Only UT_LOG_WARNING and UT_LOG_ERROR are exercised here because those
 *       are the only macros active at the default UT_LOG_LEVEL_WARNING (2).
 *       UT_LOG_INFO would silently expand to a no-op at this level.
 */
static void test_ut_log_format_specifiers(void)
{
    UT_LOG("test_ut_log_format_specifiers\n");

    UT_LOG_WARNING("int=%d str=%s float=%.2f", 42, "hello", 3.14f);
    UT_LOG_WARNING("hex=0x%x unsigned=%u", 0xDEADu, 255u);
    UT_LOG_ERROR("long=%ld char=%c", 123456L, 'Z');

    UT_LOG("test_ut_log_format_specifiers end\n");
}

void register_log_functions(void)
{
    gpLogSuite = UT_add_suite("ut-log - level constant tests", NULL, NULL);
    assert(gpLogSuite != NULL);

    UT_add_test(gpLogSuite, "log level constant values",    test_ut_log_level_constant_values);
    UT_add_test(gpLogSuite, "log default level is WARNING",  test_ut_log_default_level);
    UT_add_test(gpLogSuite, "log format specifiers",          test_ut_log_format_specifiers);

    gpLogSuite2 = UT_add_suite("ut-log - macro suppression tests", NULL, NULL);
    assert(gpLogSuite2 != NULL);

    UT_add_test(gpLogSuite2, "log active macros at WARNING level",    test_ut_log_active_macros_at_warning_level);
    UT_add_test(gpLogSuite2, "log DEBUG suppressed at WARNING level", test_ut_log_debug_suppressed_at_warning_level);
    UT_add_test(gpLogSuite2, "log active macros evaluate arguments",  test_ut_log_active_macros_evaluate_args);
}
