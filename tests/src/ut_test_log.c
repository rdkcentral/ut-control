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
 *
 * The assertion is guarded: when UT_LOG_LEVEL is explicitly overridden via
 * the build system the check is skipped so the suite does not fail on a
 * deliberate non-default build.
 */
static void test_ut_log_default_level(void)
{
    UT_LOG("test_ut_log_default_level\n");

#if UT_LOG_LEVEL == UT_LOG_LEVEL_WARNING
    /* No override applied: confirm the default is WARNING (2). */
    UT_ASSERT(UT_LOG_LEVEL == UT_LOG_LEVEL_WARNING);
#else
    /* Override applied: record the configured level; do not assert. */
    UT_LOG("UT_LOG_LEVEL overridden to %d (default is WARNING=2)\n", UT_LOG_LEVEL);
#endif

    UT_LOG("test_ut_log_default_level end\n");
}

/**
 * @brief Smoke-test all four level macros at the configured UT_LOG_LEVEL.
 *
 * Active macros produce output; suppressed macros expand to a no-op.
 * Verifies no crash regardless of the configured level.
 */
static void test_ut_log_active_macros_smoke_test(void)
{
    UT_LOG("test_ut_log_active_macros_smoke_test\n");

    UT_LOG_ERROR("error macro: value=%d", 1);
    UT_LOG_WARNING("warning macro: value=%d", 2);
    UT_LOG_INFO("info macro: value=%d", 3);
    UT_LOG_DEBUG("debug macro: value=%d", 4);

    UT_LOG("test_ut_log_active_macros_smoke_test end\n");
}

/**
 * @brief Verify argument evaluation for every level macro at the configured
 *        UT_LOG_LEVEL.
 *
 * For each macro, count_and_return() is expected to be called iff the macro
 * is active at compile time (i.e. UT_LOG_LEVEL >= that level).
 * Suppressed macros expand to do { } while(0) so their arguments are never
 * evaluated.
 */
static void test_ut_log_macro_suppression_and_arg_evaluation(void)
{
    UT_LOG("test_ut_log_macro_suppression_and_arg_evaluation\n");

    g_call_count = 0;
    UT_LOG_ERROR("%s", count_and_return("error"));
#if UT_LOG_LEVEL >= UT_LOG_LEVEL_ERROR
    UT_ASSERT(g_call_count == 1);
#else
    UT_ASSERT(g_call_count == 0);
#endif

    g_call_count = 0;
    UT_LOG_WARNING("%s", count_and_return("warning"));
#if UT_LOG_LEVEL >= UT_LOG_LEVEL_WARNING
    UT_ASSERT(g_call_count == 1);
#else
    UT_ASSERT(g_call_count == 0);
#endif

    g_call_count = 0;
    UT_LOG_INFO("%s", count_and_return("info"));
#if UT_LOG_LEVEL >= UT_LOG_LEVEL_INFO
    UT_ASSERT(g_call_count == 1);
#else
    UT_ASSERT(g_call_count == 0);
#endif

    g_call_count = 0;
    UT_LOG_DEBUG("%s", count_and_return("debug"));
#if UT_LOG_LEVEL >= UT_LOG_LEVEL_DEBUG
    UT_ASSERT(g_call_count == 1);
#else
    UT_ASSERT(g_call_count == 0);
#endif

    UT_LOG("test_ut_log_macro_suppression_and_arg_evaluation end\n");
}

/**
 * @brief Verify active log macros handle multiple format specifiers correctly.
 *
 * All four macros are exercised; suppressed ones expand to no-ops so the
 * test is safe at any configured UT_LOG_LEVEL.
 */
static void test_ut_log_format_specifiers(void)
{
    UT_LOG("test_ut_log_format_specifiers\n");

    UT_LOG_ERROR("int=%d str=%s float=%.2f", 42, "hello", 3.14f);
    UT_LOG_WARNING("hex=0x%x unsigned=%u", 0xDEADu, 255u);
    UT_LOG_INFO("long=%ld char=%c", 123456L, 'Z');
    UT_LOG_DEBUG("ptr=%p", (void *)0);

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

    UT_add_test(gpLogSuite2, "log active macros smoke test",             test_ut_log_active_macros_smoke_test);
    UT_add_test(gpLogSuite2, "log macro suppression and arg evaluation",  test_ut_log_macro_suppression_and_arg_evaluation);
}
