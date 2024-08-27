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

#include <gtest/gtest.h>
#include <ut_kvp.h>

const char *KVP_VALID_TEST_NO_FILE = "assets/this_does_not_exist.yaml";
const char *KVP_VALID_TEST_YAML_FILE = "assets/test_kvp.yaml";

// Test the creation of a KVP instance
TEST(UTKVP, CreateInstance)
{
    ut_kvp_instance_t *pInstance = ut_kvp_createInstance();
    ASSERT_NE(pInstance, nullptr) << "Failed to create KVP instance.";
    ut_kvp_destroyInstance(pInstance);
}

// Test the destruction of a KVP instance
TEST(UTKVP, DestroyInstance)
{
    ut_kvp_instance_t *pInstance = ut_kvp_createInstance();
    ASSERT_NE(pInstance, nullptr) << "Failed to create KVP instance.";
    ut_kvp_destroyInstance(pInstance);
}

// Test the open function with invalid parameters
TEST(UTKVP, OpenInvalidParams)
{
    ut_kvp_instance_t *pInstance = ut_kvp_createInstance();
    ASSERT_NE(pInstance, nullptr) << "Failed to create KVP instance.";

    ut_kvp_status_t status = ut_kvp_open(pInstance, nullptr);
    EXPECT_EQ(status, UT_KVP_STATUS_INVALID_PARAM) << "Expected invalid param error for null filename.";

    status = ut_kvp_open(nullptr, (char *)KVP_VALID_TEST_YAML_FILE);
    EXPECT_EQ(status, UT_KVP_STATUS_INVALID_INSTANCE) << "Expected invalid param error for null instance.";

    ut_kvp_destroyInstance(pInstance);
}

// Test the open function with a non-existent file
TEST(UTKVP, OpenNonExistentFile)
{
    ut_kvp_instance_t *pInstance = ut_kvp_createInstance();
    ASSERT_NE(pInstance, nullptr) << "Failed to create KVP instance.";

    ut_kvp_status_t status = ut_kvp_open(pInstance, (char *)KVP_VALID_TEST_NO_FILE);
    EXPECT_EQ(status, UT_KVP_STATUS_FILE_OPEN_ERROR) << "Expected file open error for non-existent file.";

    ut_kvp_destroyInstance(pInstance);
}

// Test the boolean retrieval function
TEST(UTKVP, GetBoolField)
{
    ut_kvp_instance_t *pInstance = ut_kvp_createInstance();
    ASSERT_NE(pInstance, nullptr) << "Failed to create KVP instance.";

    ut_kvp_open(pInstance, (char *)KVP_VALID_TEST_YAML_FILE);

    bool result = ut_kvp_getBoolField(pInstance, "decodeTest/checkBoolTRUE");
    EXPECT_TRUE(result) << "Failed to get correct boolean value for the key.";

    ut_kvp_destroyInstance(pInstance);
}

// Test the retrieval of an unsigned 8-bit integer
TEST(UTKVP, GetUInt8Field)
{
    uint8_t checkField = 0xde; // 336 decimal

    ut_kvp_instance_t *pInstance = ut_kvp_createInstance();
    ASSERT_NE(pInstance, nullptr) << "Failed to create KVP instance.";

    ut_kvp_open(pInstance, (char *)KVP_VALID_TEST_YAML_FILE);

    uint8_t value = ut_kvp_getUInt8Field(pInstance, "decodeTest/checkUint8IsDeHex");
    EXPECT_EQ(value, checkField) << "Expected a non-zero value for the key.";

    ut_kvp_destroyInstance(pInstance);
}

// Test the string retrieval function
TEST(UTKVP, GetStringField)
{
    ut_kvp_instance_t *pInstance = ut_kvp_createInstance();
    ASSERT_NE(pInstance, nullptr) << "Failed to create KVP instance.";

    ut_kvp_open(pInstance, (char *)KVP_VALID_TEST_YAML_FILE);

    char buffer[UT_KVP_MAX_ELEMENT_SIZE] = {0};
    ut_kvp_status_t status = ut_kvp_getStringField(pInstance, "decodeTest/checkStringList/0", buffer, UT_KVP_MAX_ELEMENT_SIZE);
    EXPECT_EQ(status, UT_KVP_STATUS_SUCCESS) << "Expected success in retrieving string.";
    EXPECT_STREQ(buffer, "stringA") << "Expected non-empty string in buffer.";

    ut_kvp_destroyInstance(pInstance);
}

// Add more tests as needed for other functions...

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
