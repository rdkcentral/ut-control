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
#include <ut_kvp.h>
#include <ut_log.h>

typedef struct
{
    char *buffer;
    long length;
}test_ut_memory_t;

#define KVP_VALID_TEST_NOT_VALID_YAML_FORMATTED_FILE "assets/no_data_file.yaml"
#define KVP_VALID_TEST_ZERO_LENGTH_YAML_FILE "assets/zero_length.yaml"
#define KVP_VALID_TEST_NO_FILE "assets/this_does_not_exist.yaml"
#define KVP_VALID_TEST_YAML_FILE "assets/test_kvp.yaml"
#define KVP_VALID_TEST_JSON_FILE "assets/test_kvp.json"
#define KVP_VALID_TEST_SINGLE_INCLUDE_FILE_YAML "assets/include/single-include-file.yaml"
#define KVP_VALID_TEST_SINGLE_INCLUDE_URL_YAML "assets/include/single-include-url.yaml"
#define KVP_VALID_TEST_DEPTH_CHECK_INCLUDE_YAML "assets/include/depth_check.yaml"

static ut_kvp_instance_t *gpMainTestInstance = NULL;
static UT_test_suite_t *gpKVPSuite = NULL;
static UT_test_suite_t *gpKVPSuite2 = NULL;
static UT_test_suite_t *gpKVPSuite3 = NULL;
static UT_test_suite_t *gpKVPSuite4 = NULL;
static UT_test_suite_t *gpKVPSuite5 = NULL;
static UT_test_suite_t *gpKVPSuite6 = NULL;
static UT_test_suite_t *gpKVPSuite7 = NULL;
static UT_test_suite_t *gpKVPSuite8 = NULL;
static UT_test_suite_t *gpKVPSuite9 = NULL;
static UT_test_suite_t *gpKVPSuite10 = NULL;

static int test_ut_kvp_createGlobalYAMLInstance(void);
static int test_ut_kvp_createGlobalJSONInstance(void);
static int test_ut_kvp_createGlobalYAMLInstanceForMallocedData(void);
static int test_ut_kvp_createGlobalJSONInstanceForMallocedData(void);
static int test_ut_kvp_createGlobalKVPInstanceForMallocedData(void);
static int test_ut_kvp_freeGlobalInstance(void);

static test_ut_memory_t gKVPData;

void test_ut_kvp_testCreateDestroy(void)
{
    ut_kvp_instance_t *pInstance = NULL;
    ut_kvp_instance_t *pInstance1 = NULL;

    ut_kvp_destroyInstance(NULL);
    UT_ASSERT( pInstance == NULL );

    pInstance = ut_kvp_createInstance();
    UT_ASSERT( pInstance != NULL );

    pInstance1 = ut_kvp_createInstance();
    UT_ASSERT( pInstance1 != NULL );

    UT_ASSERT( pInstance != pInstance1 );

    ut_kvp_destroyInstance(pInstance1);
    ut_kvp_destroyInstance(pInstance);
}

void test_ut_kvp_open( void )
{
    ut_kvp_instance_t *pInstance = NULL;
    ut_kvp_status_t status;

    /* Negative Test */
    UT_LOG_STEP("ut_kvp_open( NULL, NULL )");
    status = ut_kvp_open( NULL, NULL);
    UT_ASSERT( status == UT_KVP_STATUS_INVALID_INSTANCE );

    /* Positive Test */
    UT_LOG_STEP("ut_kvp_createInstance");
    pInstance = ut_kvp_createInstance();
    UT_ASSERT( pInstance != NULL );

    /* Negative Test*/
    UT_LOG_STEP("ut_kvp_close( pInstance ) - Not been opened");
    ut_kvp_close(pInstance);

    /* Negative Read Test, NULL PARAM */
    UT_LOG_STEP("ut_kvp_open( pInstance, NULL ) - Negative");
    status = ut_kvp_open( pInstance, NULL);
    UT_ASSERT( status == UT_KVP_STATUS_INVALID_PARAM );

    /* Filename doesn't exist */
    UT_LOG_STEP("ut_kvp_open( pInstance, %s - filename doesn't exist ) - Negative", KVP_VALID_TEST_NO_FILE);
    status = ut_kvp_open( pInstance, KVP_VALID_TEST_NO_FILE);
    UT_ASSERT( status == UT_KVP_STATUS_FILE_OPEN_ERROR );

    /* Zero length file, that the library should reject because it can't parse it at all */
    UT_LOG_STEP("ut_kvp_open( pInstance, %s - zero length file ) - Negative", KVP_VALID_TEST_ZERO_LENGTH_YAML_FILE);
    status = ut_kvp_open( pInstance, KVP_VALID_TEST_ZERO_LENGTH_YAML_FILE);
    UT_ASSERT( status == UT_KVP_STATUS_PARSING_ERROR );

    /* Positive Tests */
    UT_LOG_STEP("ut_kvp_open( pInstance,  KVP_VALID_TEST_YAML_FILE ) - Positive");
    status = ut_kvp_open( pInstance, KVP_VALID_TEST_YAML_FILE);
    UT_ASSERT( status == UT_KVP_STATUS_SUCCESS );

    UT_LOG_STEP("ut_kvp_open( pInstance, %s ) - Postive", KVP_VALID_TEST_NOT_VALID_YAML_FORMATTED_FILE);
    status = ut_kvp_open( pInstance, KVP_VALID_TEST_NOT_VALID_YAML_FORMATTED_FILE);
    UT_ASSERT( status == UT_KVP_STATUS_SUCCESS );

    /* Test Destroy causes close this should work fine */
    UT_LOG_STEP("ut_kvp_destroyInstance(1) - Positive");
    ut_kvp_destroyInstance( pInstance );
    UT_LOG_STEP("ut_kvp_destroyInstance(2) - Positive");
    ut_kvp_destroyInstance( pInstance );

    /* Calling close after destroy should have no effect */
    UT_LOG_STEP("ut_kvp_close(1) - Positive");
    ut_kvp_close(pInstance);
    UT_LOG_STEP("ut_kvp_close(2) - Positive");
    ut_kvp_close(pInstance);

    /* Re-check close is actually ok by walking through and auto running */
    UT_LOG_STEP("ut_kvp_close(3) - Positive");
    pInstance = ut_kvp_createInstance();
    UT_ASSERT( pInstance != NULL );

    UT_LOG_STEP("ut_kvp_close(3) - Positive");
    ut_kvp_close(pInstance);
    UT_LOG_STEP("ut_kvp_close(4) - Positive");
    ut_kvp_close(pInstance);
}

int read_file_into_memory(const char* filename, test_ut_memory_t* pInstance)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        UT_LOG_ERROR("fopen");
        pInstance->length = 0;
        return -1;
    }

    // Negative Offset: this check should fail for zero length file
    if (fseek(file, -1, SEEK_END) != 0)
    {
        UT_LOG_ERROR("zero length file\n");
        fclose(file);
        pInstance->length = 0;
        return -1;
    }

    // Seek to the end to determine the file size
    if (fseek(file, 0, SEEK_END) != 0)
    {
        UT_LOG_ERROR("fseek");
        fclose(file);
        pInstance->length = 0;
        return -1;
    }

    pInstance->length = ftell(file);
    // if (pInstance->length < 0)
    // {
    //     UT_LOG_ERROR("ftell");
    //     fclose(file);
    //     pInstance->length = 0;
    //     return -1;
    // }
    rewind(file);

    // Allocate memory for the file contents
    pInstance->buffer = malloc(pInstance->length + 1);
    if (pInstance->buffer == NULL)
    {
        UT_LOG_ERROR("malloc");
        fclose(file);
        pInstance->length = 0;
        return -1;
    }

    // Read the file into the buffer
    if (fread(pInstance->buffer, 1, pInstance->length, file) != pInstance->length)
    {
        UT_LOG_ERROR("fread");
        free(pInstance->buffer);
        fclose(file);
        pInstance->length = 0;
        return -1;
    }

    // Null-terminate the buffer
    pInstance->buffer[pInstance->length] = '\0';

    fclose(file);
    return 0;
}

void test_ut_kvp_open_memory( void )
{
    ut_kvp_status_t status;

    /* Negative Read Test, NULL params passed in both args*/
    UT_LOG_STEP("ut_kvp_openMemory( NULL, NULL, -1 )");
    status = ut_kvp_openMemory( NULL, NULL, -1);
    UT_ASSERT( status == UT_KVP_STATUS_INVALID_INSTANCE );

    /* Negative Read Test, NULL PARAM passed in 2nd arg */
    UT_LOG_STEP("ut_kvp_openMemory( gpMainTestInstance, NULL, -1 ) - Negative");
    status = ut_kvp_openMemory( gpMainTestInstance, NULL, -1);
    UT_ASSERT( status == UT_KVP_STATUS_INVALID_PARAM );

    /* data doesn't exist */
    UT_LOG_STEP("ut_kvp_openMemory( gpMainTestInstance, %s - memory block for empty string ) - Negative");
    const char *kvp_str = "  ";
    gKVPData.buffer = strdup(kvp_str);
    if (gKVPData.buffer)
    {
        status = ut_kvp_openMemory(gpMainTestInstance, gKVPData.buffer, -1);
        UT_ASSERT(status == UT_KVP_STATUS_PARSING_ERROR);
    }

    /* Zero length file, that the library should reject because it can't parse it at all */
    UT_LOG_STEP("ut_kvp_openMemory( gpMainTestInstance, %s - zero length file ) - Negative", KVP_VALID_TEST_ZERO_LENGTH_YAML_FILE);
    if (read_file_into_memory(KVP_VALID_TEST_ZERO_LENGTH_YAML_FILE, &gKVPData) == 0)
    {
        status = ut_kvp_openMemory(gpMainTestInstance, gKVPData.buffer, gKVPData.length);
        UT_ASSERT(status == UT_KVP_STATUS_PARSING_ERROR);
    }

    /* Positive Tests */
    UT_LOG_STEP("ut_kvp_openMemory( gpMainTestInstance,  %s ) - Positive", KVP_VALID_TEST_YAML_FILE);
    if (read_file_into_memory(KVP_VALID_TEST_YAML_FILE, &gKVPData) == 0)
    {
        status = ut_kvp_openMemory(gpMainTestInstance, gKVPData.buffer, gKVPData.length);
        UT_ASSERT(status == UT_KVP_STATUS_SUCCESS);
    }

    UT_LOG_STEP("ut_kvp_openMemory( gpMainTestInstance, %s ) - Postive", KVP_VALID_TEST_NOT_VALID_YAML_FORMATTED_FILE);
    if (read_file_into_memory(KVP_VALID_TEST_NOT_VALID_YAML_FORMATTED_FILE, &gKVPData) == 0)
    {
        status = ut_kvp_openMemory(gpMainTestInstance, gKVPData.buffer, gKVPData.length);
        UT_ASSERT(status == UT_KVP_STATUS_SUCCESS);
    }

    UT_LOG_STEP("ut_kvp_openMemory( gpMainTestInstance, %s ) - Postive", KVP_VALID_TEST_JSON_FILE);
    if (read_file_into_memory(KVP_VALID_TEST_JSON_FILE, &gKVPData) == 0)
    {
        status = ut_kvp_openMemory(gpMainTestInstance, gKVPData.buffer, gKVPData.length);
        UT_ASSERT(status == UT_KVP_STATUS_SUCCESS);
    }
}

void test_ut_kvp_uint8(void)
{
    uint8_t result;
    uint8_t checkField = 0xde; // 336 decimal

    /* Positive Tests */
    result = ut_kvp_getUInt8Field( gpMainTestInstance, "decodeTest/checkUint8IsDeHex" );
    UT_ASSERT( result == checkField );

    result = ut_kvp_getUInt8Field( gpMainTestInstance, "decodeTest/checkUint8IsDeDec" );
    UT_ASSERT( result == checkField );

    /* Negative Tests */
    result = ut_kvp_getUInt8Field( gpMainTestInstance, "thisShouldNotWork/checkUint64IsDeadBeefHex" );
    UT_ASSERT( result == 0 );

    /* Decode out of range value */
    result = ut_kvp_getUInt8Field( gpMainTestInstance, "decodeTest/checkUint64IsDeadBeefHex" );
    UT_ASSERT( result == 0 );
}

void test_ut_kvp_uint16(void)
{
    uint16_t result;
    uint16_t checkField = 0xdead; //157255 Decimal

    /* Positive Tests */
    result = ut_kvp_getUInt16Field( gpMainTestInstance, "decodeTest/checkUint16IsDeadHex" );
    UT_ASSERT( result == checkField );

    result = ut_kvp_getUInt16Field( gpMainTestInstance, "decodeTest/checkUint16IsDeadDec" );
    UT_ASSERT( result == checkField );

    result = ut_kvp_getUInt16Field( gpMainTestInstance, "decodeTest.checkUint16IsDeadHex" );
    UT_ASSERT( result == checkField );

    result = ut_kvp_getUInt16Field( gpMainTestInstance, "decodeTest.checkUint16IsDeadDec" );
    UT_ASSERT( result == checkField );

    /* Negative Tests */
    result = ut_kvp_getUInt16Field( gpMainTestInstance, "thisShouldNotWork/checkUint64IsDeadBeefHex" );
    UT_ASSERT( result == 0 );

    /* Decode out of range value */
    result = ut_kvp_getUInt16Field( gpMainTestInstance, "decodeTest/checkUint64IsDeadBeefHex" );
    UT_ASSERT( result == 0 );
}

void test_ut_kvp_uint32(void)
{
    uint32_t result;
    uint32_t checkField = 0xdeadbeef; // 3735928559 Decimal

    /* Positive Tests */
    result = ut_kvp_getUInt32Field( gpMainTestInstance, "decodeTest/checkUint32IsDeadBeefHex" );
    UT_ASSERT( result == checkField );

    result = ut_kvp_getUInt32Field( gpMainTestInstance, "decodeTest/checkUint32IsDeadBeefDec" );
    UT_ASSERT( result == checkField );

    result = ut_kvp_getUInt32Field( gpMainTestInstance, "decodeTest.checkUint32IsDeadBeefHex" );
    UT_ASSERT( result == checkField );

    result = ut_kvp_getUInt32Field( gpMainTestInstance, "decodeTest.checkUint32IsDeadBeefDec" );
    UT_ASSERT( result == checkField );

    /* Negative Tests */
    result = ut_kvp_getUInt32Field( gpMainTestInstance, "thisShouldNotWork/checkUint64IsDeadBeefHex" );
    UT_ASSERT( result == 0 );

    /* Decode out of range value */
    result = ut_kvp_getUInt32Field( gpMainTestInstance, "decodeTest/checkUint64IsDeadBeefHex" );
    UT_ASSERT( result == 0 );
}

void test_ut_kvp_uint64(void)
{
    uint64_t result;
    uint64_t checkField = 0xdeadbeefdeadbeef;

    /* Positive Tests */
    result = ut_kvp_getUInt64Field( gpMainTestInstance, "decodeTest/checkUint64IsDeadBeefHex" );
    UT_ASSERT( result == checkField );

    result = ut_kvp_getUInt64Field( gpMainTestInstance, "decodeTest/checkUint64IsDeadBeefDec" );
    UT_ASSERT( result == checkField );

    result = ut_kvp_getUInt64Field( gpMainTestInstance, "decodeTest.checkUint64IsDeadBeefHex" );
    UT_ASSERT( result == checkField );

    result = ut_kvp_getUInt64Field( gpMainTestInstance, "decodeTest.checkUint64IsDeadBeefDec" );
    UT_ASSERT( result == checkField );

    /* Negative Tests */
    result = ut_kvp_getUInt64Field( gpMainTestInstance, "thisShouldNotWork/checkUint64IsDeadBeefHex" );
    UT_ASSERT( result == 0 );
}

void test_ut_kvp_list(void)
{
    ut_kvp_status_t status;
    char result_kvp[UT_KVP_MAX_ELEMENT_SIZE]={0xff};
    uint32_t result;
    int count;

    count = ut_kvp_getListCount(gpMainTestInstance, "decodeTest/checkStringList");
    UT_ASSERT( count == 3 );

    status = ut_kvp_getStringField(gpMainTestInstance, "decodeTest/checkStringList/0", result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT(status == UT_KVP_STATUS_SUCCESS );
    UT_ASSERT_STRING_EQUAL(result_kvp, "stringA" );

    status = ut_kvp_getStringField(gpMainTestInstance, "decodeTest/checkStringList/1", result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT(status == UT_KVP_STATUS_SUCCESS );
    UT_ASSERT_STRING_EQUAL(result_kvp, "stringB" );

    status = ut_kvp_getStringField(gpMainTestInstance, "decodeTest/checkStringList/2", result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT(status == UT_KVP_STATUS_SUCCESS );
    UT_ASSERT_STRING_EQUAL(result_kvp, "stringC" );

    status = ut_kvp_getStringField(gpMainTestInstance, "decodeTest", result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT(status == UT_KVP_STATUS_PARSING_ERROR );

    /* Positive Tests */
    count = ut_kvp_getListCount( gpMainTestInstance, "decodeTest/checkUint32List" );
    UT_ASSERT( count == 3 );

    result = ut_kvp_getUInt32Field( gpMainTestInstance, "decodeTest/checkUint32List/0" );
    UT_ASSERT( result == 720 );

    result = ut_kvp_getUInt32Field( gpMainTestInstance, "decodeTest/checkUint32List/1" );
    UT_ASSERT( result == 800 );

    result = ut_kvp_getUInt32Field( gpMainTestInstance, "decodeTest/checkUint32List/2" );
    UT_ASSERT( result == 1080 );
}

void test_ut_kvp_string(void)
{
    const char *checkField = "the beef is dead";
    char result_kvp[UT_KVP_MAX_ELEMENT_SIZE]={0xff};
    ut_kvp_status_t status;

    /* Check for INVALID_PARAM */
    UT_LOG_STEP("ut_kvp_getField() - Check for INVALID_PARAM");

    status = ut_kvp_getStringField(NULL, "decodeTest/checkStringDeadBeef", result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT(status == UT_KVP_STATUS_INVALID_INSTANCE );

    status = ut_kvp_getStringField(gpMainTestInstance, NULL, result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT(status == UT_KVP_STATUS_NULL_PARAM );

    status = ut_kvp_getStringField(gpMainTestInstance, NULL, NULL, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT(status == UT_KVP_STATUS_NULL_PARAM );

    /* Check for UT_KVP_STATUS_KEY_NOT_FOUND */
    UT_LOG_STEP("ut_kvp_getStringField() - Check for UT_KVP_STATUS_KEY_NOT_FOUND");
    status = ut_kvp_getStringField(gpMainTestInstance, "shouldNotWork/checkStringDeadBeef", result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT(status == UT_KVP_STATUS_KEY_NOT_FOUND );

    /* Check for UT_KVP_STATUS_PARSING_ERROR */
    // TODO
    //UT_LOG_STEP("ut_kvp_getStringField() - Check for UT_KVP_STATUS_PARSING_ERROR");
    //status = ut_kvp_getStringField(gpMainTestInstance, "shouldNotWork/checkStringDeadBeef", result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    //UT_ASSERT(status == UT_KVP_STATUS_PARSING_ERROR );

    /* Check for UT_KVP_STATUS_SUCCESS */

    /* Not supported in JSON format, but the json format one is with quotes either way */
    UT_LOG_STEP("ut_kvp_getStringField() - Check String with no quotes for UT_KVP_STATUS_SUCCESS");
    status = ut_kvp_getStringField(gpMainTestInstance, "decodeTest/checkStringDeadBeefNoQuotes", result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT(status == UT_KVP_STATUS_SUCCESS );
    UT_ASSERT_STRING_EQUAL(result_kvp, checkField);
    UT_LOG( "checkStringDeadBeefNoQuotes[%s]", result_kvp );

    UT_LOG_STEP("ut_kvp_getStringField() - Check String with Quotes for UT_KVP_STATUS_SUCCESS");
    status = ut_kvp_getStringField(gpMainTestInstance, "decodeTest/checkStringDeadBeef", result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT(status == UT_KVP_STATUS_SUCCESS );
    UT_ASSERT_STRING_EQUAL(result_kvp, checkField);
    UT_LOG( "checkStringDeadBeef[%s]", result_kvp );

    UT_LOG_STEP("ut_kvp_getStringField() - Check String with Quotes for UT_KVP_STATUS_SUCCESS");
    status = ut_kvp_getStringField(gpMainTestInstance, "decodeTest/checkStringDeadBeef2", result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT(status == UT_KVP_STATUS_SUCCESS );
    UT_ASSERT_STRING_EQUAL(result_kvp, "the beef is also dead" );
    UT_LOG( "checkStringDeadBeef2[%s]", result_kvp );

    UT_LOG_STEP("ut_kvp_getStringField() - Check String with Quotes for UT_KVP_STATUS_SUCCESS");
    status = ut_kvp_getStringField(gpMainTestInstance, "decodeTest.checkStringDeadBeef", result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT(status == UT_KVP_STATUS_SUCCESS );
    UT_ASSERT_STRING_EQUAL(result_kvp, checkField);
    UT_LOG( "checkStringDeadBeef[%s]", result_kvp );

    UT_LOG_STEP("ut_kvp_getStringField() - Check String with Quotes for UT_KVP_STATUS_SUCCESS");
    status = ut_kvp_getStringField(gpMainTestInstance, "decodeTest.checkStringDeadBeef2", result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT(status == UT_KVP_STATUS_SUCCESS );
    UT_ASSERT_STRING_EQUAL(result_kvp, "the beef is also dead" );
    UT_LOG( "checkStringDeadBeef2[%s]", result_kvp );

}

void test_ut_kvp_getFloatField( void )
{
    float result;
    float checkFloat = 5.1;

    UT_LOG_STEP("ut_kvp_getFloatField() ");
    result = ut_kvp_getFloatField(gpMainTestInstance, "decodeTest.checkFloat");
    UT_ASSERT(result == checkFloat );

    UT_LOG_STEP("ut_kvp_getFloatField() ");
    result = ut_kvp_getFloatField(gpMainTestInstance, "decodeTest/checkFloat");
    UT_ASSERT(result == checkFloat);
}

void test_ut_kvp_getDoubleField( void )
{
    double result;
    double checkDoublePi = 3.14159265358979323846;
    double checkDoubleScientific = -4.2e8;
    double checkDoubleInvalid = 0;

    UT_LOG_STEP("ut_kvp_getDoubleField() ");
    result = ut_kvp_getDoubleField(gpMainTestInstance, "decodeTest.checkDoublePi");
    UT_ASSERT(result == checkDoublePi );

    UT_LOG_STEP("ut_kvp_getDoubleField() ");
    result = ut_kvp_getDoubleField(gpMainTestInstance, "decodeTest/checkDoublePi");
    UT_ASSERT(result == checkDoublePi);

    UT_LOG_STEP("ut_kvp_getDoubleField() ");
    result = ut_kvp_getDoubleField(gpMainTestInstance, "decodeTest.checkDoubleScientific");
    UT_ASSERT(result == checkDoubleScientific );

    UT_LOG_STEP("ut_kvp_getDoubleField() ");
    result = ut_kvp_getDoubleField(gpMainTestInstance, "decodeTest/checkDoubleScientific");
    UT_ASSERT(result == checkDoubleScientific);

    UT_LOG_STEP("ut_kvp_getDoubleField() ");
    result = ut_kvp_getDoubleField(gpMainTestInstance, "decodeTest.checkDoubleInvalid");
    UT_ASSERT(result == checkDoubleInvalid );

    UT_LOG_STEP("ut_kvp_getDoubleField() ");
    result = ut_kvp_getDoubleField(gpMainTestInstance, "decodeTest/checkDoubleInvalid");
    UT_ASSERT(result == checkDoubleInvalid);
}

void test_ut_kvp_fieldPresent()
{
    bool result;

    /* Negative Tests */
    result = ut_kvp_fieldPresent( gpMainTestInstance, "shouldNotWork/checkBoolTRUE" );
    UT_ASSERT( result == false );

    result = ut_kvp_fieldPresent( gpMainTestInstance, "shouldNotWork" );
    UT_ASSERT( result == false );

    /* Positive Tests */
    result = ut_kvp_fieldPresent( gpMainTestInstance, "decodeTest/checkBoolTRUE" );
    UT_ASSERT( result == true );

    result = ut_kvp_fieldPresent( gpMainTestInstance, "decodeTest" );
    UT_ASSERT( result == true );

}

void test_ut_kvp_get_field_without_open( void )
{
    bool result;
    char result_kvp[UT_KVP_MAX_ELEMENT_SIZE]={0xff};
    ut_kvp_status_t status;

    /* Negative Tests */
    UT_LOG_STEP("ut_kvp_getBoolField() - negative");
    result = ut_kvp_getBoolField( gpMainTestInstance, "decodeTest/checkBoolTRUE" );
    UT_ASSERT( result == false );

    result = ut_kvp_getUInt32Field( gpMainTestInstance, "decodeTest/checkUint32IsDeadBeefHex" );
    UT_ASSERT( result == 0 );

    result = ut_kvp_getUInt64Field( gpMainTestInstance, "decodeTest/checkUint64IsDeadBeefHex" );
    UT_ASSERT( result == 0 );

    status = ut_kvp_getStringField(gpMainTestInstance, "decodeTest/checkStringDeadBeef", result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT( status == UT_KVP_STATUS_INVALID_INSTANCE );

    gpMainTestInstance = ut_kvp_createInstance();

    status = ut_kvp_getStringField(gpMainTestInstance, "decodeTest/checkStringDeadBeef", result_kvp, UT_KVP_MAX_ELEMENT_SIZE);
    UT_ASSERT( status == UT_KVP_STATUS_NO_DATA );

    ut_kvp_destroyInstance(gpMainTestInstance);
}

void test_ut_kvp_bool(void)
{
    bool result;

    /* Negative Tests */
    result = ut_kvp_getBoolField( gpMainTestInstance, "shouldNotWork/checkBoolTRUE" );
    UT_ASSERT( result == false );

    /* Positive Tests */
    result = ut_kvp_getBoolField( gpMainTestInstance, "decodeTest/checkBoolTRUE" );
    UT_ASSERT( result == true );

    result = ut_kvp_getBoolField( gpMainTestInstance, "decodeTest/checkBooltrue" );
    UT_ASSERT( result == true );

    result = ut_kvp_getBoolField( gpMainTestInstance, "decodeTest/checkBoolTRuE" );
    UT_ASSERT( result == true );

    result = ut_kvp_getBoolField( gpMainTestInstance, "decodeTest/checkBoolFalse" );
    UT_ASSERT( result == false );

    result = ut_kvp_getBoolField( gpMainTestInstance, "decodeTest.checkBoolTRuE" );
    UT_ASSERT( result == true );

    result = ut_kvp_getBoolField( gpMainTestInstance, "decodeTest.checkBoolFalse" );
    UT_ASSERT( result == false );
}

/*These tests test the integrated yaml for there key-value pair*/
void test_ut_kvp_bool_on_main_yaml(void)
{
     bool result;

    /* Negative Tests */
    result = ut_kvp_getBoolField( gpMainTestInstance, "6/value" );
    UT_ASSERT( result == false );

    /* Positive Tests */
    result = ut_kvp_getBoolField( gpMainTestInstance, "1/value" );
    UT_ASSERT( result == true );

    result = ut_kvp_getBoolField( gpMainTestInstance, "2.value" );
    UT_ASSERT( result == true );

    result = ut_kvp_getBoolField( gpMainTestInstance, "3/value" );
    UT_ASSERT( result == true );

    result = ut_kvp_getBoolField( gpMainTestInstance, "4.value" );
    UT_ASSERT( result == true );

    result = ut_kvp_getBoolField( gpMainTestInstance, "5/value" );
    UT_ASSERT( result == true );

}

 /*These tests test the integrated yaml for the availbility of field*/
void test_ut_kvp_fieldPresent_on_main_yaml(void)
{
    bool result;

    /* Negative Tests */
    result = ut_kvp_fieldPresent( gpMainTestInstance, "6/value" );
    UT_ASSERT( result == false );

    result = ut_kvp_fieldPresent( gpMainTestInstance, "6" );
    UT_ASSERT( result == false );

    /* Positive Tests */
    result = ut_kvp_fieldPresent( gpMainTestInstance, "1/value" );
    UT_ASSERT( result == true );

    result = ut_kvp_fieldPresent( gpMainTestInstance, "2" );
    UT_ASSERT( result == true );

    result = ut_kvp_fieldPresent( gpMainTestInstance, "3/value" );
    UT_ASSERT( result == true );

    result = ut_kvp_fieldPresent( gpMainTestInstance, "4.value" );
    UT_ASSERT( result == true );

    result = ut_kvp_fieldPresent( gpMainTestInstance, "5.value" );
    UT_ASSERT( result == true );

    result = ut_kvp_fieldPresent( gpMainTestInstance, "5" );
    UT_ASSERT( result == true );

}

static void create_delete_kvp_instance_for_given_file(const char* filename)
{
    ut_kvp_instance_t *pInstance = NULL;
    ut_kvp_status_t status;
    char* kvpData;

    pInstance = ut_kvp_createInstance();
    if ( pInstance == NULL )
    {
        assert( pInstance != NULL );
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return;
    }

    status = ut_kvp_open( pInstance, filename);
    assert( status == UT_KVP_STATUS_SUCCESS );

    if ( status != UT_KVP_STATUS_SUCCESS )
    {
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return;
    }

    kvpData = ut_kvp_getData(pInstance);

    if(kvpData != NULL)
    {
        // Print the emitted KVP string
        printf("%s\n", kvpData);

        // Free the emitted KVP string
           free(kvpData);
    }

     ut_kvp_destroyInstance( pInstance );
}

/*These tests, test for availability of include file in the given file.
**The given file only contains file path to be included
*/
void test_ut_kvp_open_singleIncludeFileWithBuildFromFile(void)
{

    create_delete_kvp_instance_for_given_file(KVP_VALID_TEST_SINGLE_INCLUDE_FILE_YAML);
}

/*These tests, test for availability of include url in the given file.
**The given file only contains urls to be included
*/
void test_ut_kvp_singleIncludeUrlsWithBuildFromFile(void)
{

    create_delete_kvp_instance_for_given_file(KVP_VALID_TEST_SINGLE_INCLUDE_URL_YAML);
}

/*These tests, test for availability of include file in the given file
 **The given file only contains file path to be included, however the
 **the included file contains file path of another file and so on.
 **KVP as now supports UT_KVP_MAX_INCLUDE_DEPTH = 5
 */
void test_ut_kvp_IncludeDepthCheckWithBuildFromFile(void)
{

    create_delete_kvp_instance_for_given_file(KVP_VALID_TEST_DEPTH_CHECK_INCLUDE_YAML);
}

static create_delete_kvp_memory_instance_for_given_file(const char* filename)
{
    test_ut_memory_t kvpMemory;
    ut_kvp_instance_t *pInstance = NULL;
    ut_kvp_status_t status;
    char* kvpData;

    pInstance = ut_kvp_createInstance();
    if ( pInstance == NULL )
    {
        assert( pInstance != NULL );
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return;
    }

    if (read_file_into_memory(filename, &kvpMemory) == 0)
    {
        status = ut_kvp_openMemory(pInstance, kvpMemory.buffer, kvpMemory.length);
        UT_ASSERT(status == UT_KVP_STATUS_SUCCESS);
    }

    if ( status != UT_KVP_STATUS_SUCCESS )
    {
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return;
    }

    kvpData = ut_kvp_getData(pInstance);

    if(kvpData != NULL)
    {
        // Print the emitted KVP string
        printf("%s\n", kvpData);

        // Free the emitted KVP string
           free(kvpData);
    }

     ut_kvp_destroyInstance( pInstance );
}

/*These tests, test for availability of include file in the malloc'd buffer
 **The malloc'd buffer, only contains files to be included
 */
void test_ut_kvp_singleIncludeFileWithBuildFromMallocedData(void)
{

    create_delete_kvp_memory_instance_for_given_file(KVP_VALID_TEST_SINGLE_INCLUDE_FILE_YAML);
}

/*These tests, test for availability of include file in the malloc'd buffer
 **The malloc'd buffer, only contains urls to be included
 */
void test_ut_kvp_singleIncludeUrlsWithBuildFromMallocedData(void)
{

    create_delete_kvp_memory_instance_for_given_file(KVP_VALID_TEST_SINGLE_INCLUDE_URL_YAML);
}

/*These tests, test for availability of include file in the malloc'd data
 **The malloc'd data only contains file path to be included, however the
 **the included file contains file path of another file and so on.
 **KVP as now supports UT_KVP_MAX_INCLUDE_DEPTH = 5
 */
void test_ut_kvp_IncludeDepthCheckWithBuildFromMallocedData(void)
{

    create_delete_kvp_memory_instance_for_given_file(KVP_VALID_TEST_DEPTH_CHECK_INCLUDE_YAML);
}

static int test_ut_kvp_createGlobalYAMLInstance( void )
{
    ut_kvp_status_t status;

    gpMainTestInstance = ut_kvp_createInstance();
    if ( gpMainTestInstance == NULL )
    {
        assert( gpMainTestInstance != NULL );
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return -1;
    }

    status = ut_kvp_open( gpMainTestInstance, KVP_VALID_TEST_YAML_FILE);
    assert( status == UT_KVP_STATUS_SUCCESS );

    if ( status != UT_KVP_STATUS_SUCCESS )
    {
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return -1;
    }

    return 0;
}

static int test_ut_kvp_createGlobalJSONInstance( void )
{
    ut_kvp_status_t status;

    gpMainTestInstance = ut_kvp_createInstance();
    if ( gpMainTestInstance == NULL )
    {
        assert( gpMainTestInstance != NULL );
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return -1;
    }

    status = ut_kvp_open( gpMainTestInstance, KVP_VALID_TEST_JSON_FILE);
    assert( status == UT_KVP_STATUS_SUCCESS );

    if ( status != UT_KVP_STATUS_SUCCESS )
    {
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return -1;
    }

    return 0;
}

static int test_ut_kvp_createGlobalYAMLInstanceForMallocedData( void )
{
    ut_kvp_status_t status;
    test_ut_memory_t kvpMemory;

    gpMainTestInstance = ut_kvp_createInstance();
    if ( gpMainTestInstance == NULL )
    {
        assert( gpMainTestInstance != NULL );
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return -1;
    }

    if (read_file_into_memory(KVP_VALID_TEST_YAML_FILE, &kvpMemory) == 0)
    {
        status = ut_kvp_openMemory(gpMainTestInstance, kvpMemory.buffer, kvpMemory.length);
        assert(status == UT_KVP_STATUS_SUCCESS);
    }

    if ( status != UT_KVP_STATUS_SUCCESS )
    {
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return -1;
    }

    return 0;
}

static int test_ut_kvp_createGlobalJSONInstanceForMallocedData( void )
{
    ut_kvp_status_t status;
    test_ut_memory_t kvpMemory;

    gpMainTestInstance = ut_kvp_createInstance();
    if ( gpMainTestInstance == NULL )
    {
        assert( gpMainTestInstance != NULL );
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return -1;
    }

    if (read_file_into_memory(KVP_VALID_TEST_JSON_FILE, &kvpMemory) == 0)
    {
        status = ut_kvp_openMemory(gpMainTestInstance, kvpMemory.buffer, kvpMemory.length);
        assert(status == UT_KVP_STATUS_SUCCESS);
    }

    if ( status != UT_KVP_STATUS_SUCCESS )
    {
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return -1;
    }

    return 0;
}

static int test_ut_kvp_createGlobalKVPInstanceForMallocedData( void )
{

    gpMainTestInstance = ut_kvp_createInstance();
    if ( gpMainTestInstance == NULL )
    {
        assert( gpMainTestInstance != NULL );
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return -1;
    }

    return 0;
}

static int test_ut_kvp_createGlobalYAMLInstanceForIncludeFileViaYaml( void)
{
    /*Creating global instance for include yaml support, so that the values
    **in integrated yaml can be tested
    */

    ut_kvp_status_t status;

    gpMainTestInstance = ut_kvp_createInstance();
    if ( gpMainTestInstance == NULL )
    {
        assert( gpMainTestInstance != NULL );
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return -1;
    }

    status = ut_kvp_open( gpMainTestInstance, KVP_VALID_TEST_DEPTH_CHECK_INCLUDE_YAML);
    assert( status == UT_KVP_STATUS_SUCCESS );

    if ( status != UT_KVP_STATUS_SUCCESS )
    {
        UT_LOG_ERROR("ut_kvp_open() - Read Failure");
        return -1;
    }

    return 0;

}

static int test_ut_kvp_freeGlobalInstance( void )
{
    ut_kvp_destroyInstance( gpMainTestInstance );
    return 0;
}

void register_kvp_functions( void )
{

    gpKVPSuite=gpKVPSuite;
    gpKVPSuite = UT_add_suite("ut-kvp - test functions ", NULL, NULL);
    assert(gpKVPSuite != NULL);

    UT_add_test(gpKVPSuite, "kvp create / destroy", test_ut_kvp_testCreateDestroy);
    UT_add_test(gpKVPSuite, "kvp read", test_ut_kvp_open);

    gpKVPSuite2 = UT_add_suite("ut-kvp - test main functions YAML Decoder ", test_ut_kvp_createGlobalYAMLInstance, test_ut_kvp_freeGlobalInstance);
    assert(gpKVPSuite2 != NULL);

    UT_add_test(gpKVPSuite2, "kvp uint8", test_ut_kvp_uint8);
    UT_add_test(gpKVPSuite2, "kvp uint16", test_ut_kvp_uint16);
    UT_add_test(gpKVPSuite2, "kvp bool", test_ut_kvp_bool);
    UT_add_test(gpKVPSuite2, "kvp string", test_ut_kvp_string);
    UT_add_test(gpKVPSuite2, "kvp uint32", test_ut_kvp_uint32);
    UT_add_test(gpKVPSuite2, "kvp uint64", test_ut_kvp_uint64);
    UT_add_test(gpKVPSuite2, "kvp list", test_ut_kvp_list);
    UT_add_test(gpKVPSuite2, "kvp float", test_ut_kvp_getFloatField);
    UT_add_test(gpKVPSuite2, "kvp double", test_ut_kvp_getDoubleField);
    UT_add_test(gpKVPSuite2, "kvp node presence", test_ut_kvp_fieldPresent);

    /* Perform the same parsing tests but use a json file instead */
    gpKVPSuite3 = UT_add_suite("ut-kvp - test main functions JSON Decoder ", test_ut_kvp_createGlobalJSONInstance, test_ut_kvp_freeGlobalInstance);
    assert(gpKVPSuite3 != NULL);

    UT_add_test(gpKVPSuite3, "kvp string", test_ut_kvp_string);
    UT_add_test(gpKVPSuite3, "kvp uint8", test_ut_kvp_uint8);
    UT_add_test(gpKVPSuite3, "kvp uint16", test_ut_kvp_uint16);

    UT_add_test(gpKVPSuite3, "kvp bool", test_ut_kvp_bool);
    UT_add_test(gpKVPSuite3, "kvp uint32", test_ut_kvp_uint32);
    UT_add_test(gpKVPSuite3, "kvp uint64", test_ut_kvp_uint64);
    UT_add_test(gpKVPSuite3, "kvp list", test_ut_kvp_list);
    UT_add_test(gpKVPSuite3, "kvp float", test_ut_kvp_getFloatField);
    UT_add_test(gpKVPSuite3, "kvp double", test_ut_kvp_getDoubleField);
    UT_add_test(gpKVPSuite3, "kvp node presence", test_ut_kvp_fieldPresent);


    gpKVPSuite4 = UT_add_suite("ut-kvp - test main functions Test without Open ", NULL, NULL);
    assert(gpKVPSuite4 != NULL);

    UT_add_test(gpKVPSuite4, "kvp read negative", test_ut_kvp_get_field_without_open);

    /* Perform the same parsing on malloc'd data*/
    gpKVPSuite5 = UT_add_suite("ut-kvp - test main functions YAML Decoder with malloc'd data", test_ut_kvp_createGlobalYAMLInstanceForMallocedData, test_ut_kvp_freeGlobalInstance);
    assert(gpKVPSuite5 != NULL);

    UT_add_test(gpKVPSuite5, "kvp uint8", test_ut_kvp_uint8);
    UT_add_test(gpKVPSuite5, "kvp uint16", test_ut_kvp_uint16);
    UT_add_test(gpKVPSuite5, "kvp bool", test_ut_kvp_bool);
    UT_add_test(gpKVPSuite5, "kvp string", test_ut_kvp_string);
    UT_add_test(gpKVPSuite5, "kvp uint32", test_ut_kvp_uint32);
    UT_add_test(gpKVPSuite5, "kvp uint64", test_ut_kvp_uint64);
    UT_add_test(gpKVPSuite5, "kvp float", test_ut_kvp_getFloatField);
    UT_add_test(gpKVPSuite5, "kvp double", test_ut_kvp_getDoubleField);
    UT_add_test(gpKVPSuite5, "kvp node presence", test_ut_kvp_fieldPresent);

    /* Perform the same parsing tests but use a json file instead */
    gpKVPSuite6 = UT_add_suite("ut-kvp - test main functions JSON Decoder with malloc'd data", test_ut_kvp_createGlobalJSONInstanceForMallocedData, test_ut_kvp_freeGlobalInstance);
    assert(gpKVPSuite6 != NULL);

    UT_add_test(gpKVPSuite6, "kvp string", test_ut_kvp_string);
    UT_add_test(gpKVPSuite6, "kvp uint8", test_ut_kvp_uint8);
    UT_add_test(gpKVPSuite6, "kvp uint16", test_ut_kvp_uint16);
    UT_add_test(gpKVPSuite6, "kvp bool", test_ut_kvp_bool);
    UT_add_test(gpKVPSuite6, "kvp uint32", test_ut_kvp_uint32);
    UT_add_test(gpKVPSuite6, "kvp uint64", test_ut_kvp_uint64);
    UT_add_test(gpKVPSuite6, "kvp float", test_ut_kvp_getFloatField);
    UT_add_test(gpKVPSuite6, "kvp double", test_ut_kvp_getDoubleField);
    UT_add_test(gpKVPSuite6, "kvp node presence", test_ut_kvp_fieldPresent);

    gpKVPSuite7 = UT_add_suite("ut-kvp - test kvp_open_memory()", test_ut_kvp_createGlobalKVPInstanceForMallocedData, test_ut_kvp_freeGlobalInstance);
    assert(gpKVPSuite7 != NULL);

    UT_add_test(gpKVPSuite7, "kvp read with malloced data", test_ut_kvp_open_memory);

    gpKVPSuite8 = UT_add_suite("ut-kvp - test main functions YAML Decoder for includes using build from files", NULL, NULL);
    assert(gpKVPSuite8 != NULL);

    UT_add_test(gpKVPSuite8, "kvp single include file", test_ut_kvp_open_singleIncludeFileWithBuildFromFile);
    UT_add_test(gpKVPSuite8, "kvp single include url", test_ut_kvp_singleIncludeUrlsWithBuildFromFile);
    UT_add_test(gpKVPSuite8, "kvp include depth check", test_ut_kvp_IncludeDepthCheckWithBuildFromFile);

    gpKVPSuite9 = UT_add_suite("ut-kvp - test main functions YAML Decoder for single include files using build from Malloced data", NULL, NULL);
    assert(gpKVPSuite9 != NULL);

    UT_add_test(gpKVPSuite9, "kvp single include file", test_ut_kvp_singleIncludeFileWithBuildFromMallocedData);
    UT_add_test(gpKVPSuite9, "kvp single include url", test_ut_kvp_singleIncludeUrlsWithBuildFromMallocedData);
    UT_add_test(gpKVPSuite9, "kvp include depth check", test_ut_kvp_IncludeDepthCheckWithBuildFromMallocedData);

    gpKVPSuite10 = UT_add_suite("ut-kvp - test main functions YAML Decoder for Yaml include support", test_ut_kvp_createGlobalYAMLInstanceForIncludeFileViaYaml, test_ut_kvp_freeGlobalInstance);
    assert(gpKVPSuite10 != NULL);

    UT_add_test(gpKVPSuite10, "kvp bool from main yaml", test_ut_kvp_bool_on_main_yaml);
    UT_add_test(gpKVPSuite10, "kvp node presence from main yaml", test_ut_kvp_fieldPresent_on_main_yaml);
}
