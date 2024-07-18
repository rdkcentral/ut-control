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

/* Module Includes */
#include "ut_control_common.h"

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