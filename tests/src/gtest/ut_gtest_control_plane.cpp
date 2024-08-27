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
#include <ut_control_plane.h>

const char* UT_CONTROL_YAML_FILE = "example.yaml";
const char* UT_CONTROL_JSON_FILE = "example.json";

void testYAMLCallback(char *key, ut_kvp_instance_t *instance, void *userData)
{
    std::unique_ptr<char, void (*)(void *)> kvpData(ut_kvp_getData(instance), free);
    std::string originalData = static_cast<const char *>(userData);

    std::cout << "*******************************Inside testYAMLCallback************************\n";

    if (kvpData)
    {
        // Print the emitted KVP string
        std::cout << kvpData.get() << std::endl;
    }

    if (!originalData.empty())
    {
        std::cout << "Original YAML file\n"
                  << originalData << std::endl;
    }
}

// Test fixture for the control plane tests
class ControlPlaneTest : public ::testing::Test
{
protected:
    ut_controlPlane_instance_t *pInstance;

    void SetUp() override
    {
        pInstance = UT_ControlPlane_Init(8080); // Initialize control plane instance on port 8080
        ASSERT_NE(pInstance, nullptr);          // Ensure initialization was successful
    }

    void TearDown() override
    {
        if (pInstance)
        {
            UT_ControlPlane_Exit(pInstance); // Clean up resources
        }
    }
};

// Test initialization of the control plane
TEST_F(ControlPlaneTest, InitSuccess)
{
    // Ensure the instance is initialized
    ASSERT_NE(pInstance, nullptr);
}

void* userData = static_cast<void*>(new std::string("testJSONCallbackStringInvalidParam"));
// Test callback registration success
TEST_F(ControlPlaneTest, RegisterCallbackSuccess)
{
    ut_control_plane_status_t status = UT_ControlPlane_RegisterCallbackOnMessage(
        pInstance, (char *)"test/yamlData", testYAMLCallback, userData);
    ASSERT_EQ(status, UT_CONTROL_PLANE_STATUS_OK);
}

// Test callback registration failure due to invalid instance
TEST_F(ControlPlaneTest, RegisterCallbackInvalidHandle)
{
    ut_control_plane_status_t status = UT_ControlPlane_RegisterCallbackOnMessage(
        nullptr, (char *)"test/yamlData", testYAMLCallback, nullptr);
    ASSERT_EQ(status, UT_CONTROL_PLANE_STATUS_INVALID_HANDLE);
}

// Test callback registration failure due to invalid parameter
TEST_F(ControlPlaneTest, RegisterCallbackInvalidParam)
{
    ut_control_plane_status_t status = UT_ControlPlane_RegisterCallbackOnMessage(
        pInstance, nullptr, testYAMLCallback, nullptr);
    ASSERT_EQ(status, UT_CONTROL_PLANE_STATUS_INVALID_PARAM);
}

// Test starting and stopping the control plane
TEST_F(ControlPlaneTest, StartStopControlPlane)
{
    // Start the control plane
    ASSERT_NO_THROW(UT_ControlPlane_Start(pInstance));

    // Stop the control plane
    ASSERT_NO_THROW(UT_ControlPlane_Stop(pInstance));
}

// Test cleanup of control plane resources
TEST_F(ControlPlaneTest, ExitControlPlane)
{
    ASSERT_NO_THROW(UT_ControlPlane_Exit(pInstance));
    pInstance = nullptr; // Prevent TearDown from trying to clean up again
}
