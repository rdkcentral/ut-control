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
#include <stdio.h>
#include <stdlib.h>

/* Module Includes */
#include <ut_log.h>

#ifdef __cplusplus
extern "C"
{
#endif
typedef struct
{
    char *buffer;
    long length;
}test_ut_memory_t;

/**
 * @brief Reads the content of a file into memory.
 *
 * This function opens a file specified by `filename`, reads its content, and stores it in the memory
 * provided by the `pInstance` parameter. The function returns an integer status code indicating the
 * success or failure of the operation.
 *
 * @param[in] filename The name of the file to be read.
 * @param[out] pInstance Pointer to a `test_ut_memory_t` structure where the file content will be stored.
 *
 * @return int Status code:
 * - 0 on success
 * - Non-zero error code on failure (e.g., file not found, memory allocation failure).
 */
int read_file_into_memory(const char* filename, test_ut_memory_t* pInstance);

/**
* @brief Prints the emitted KVP string and input data from a file.
*
* This function is a utility for displaying test input and output data.
* If a valid `filename` is provided, it reads the file content into memory
* and prints it to standard output. If `kvpData` is provided, it prints
* the emitted Key-Value Pair (KVP) string. Both can be printed in the
* same call if both parameters are non-NULL.
*
* @param[in] kvpData   Pointer to a null-terminated string containing the
*                      emitted KVP data to be printed. Can be NULL if only
*                      the file content should be printed.
*
* @param[in] filename  Pointer to a null-terminated string specifying the
*                      name of the file whose content should be printed.
*                      Can be NULL if only KVP data should be printed.
*
* @note
* - Uses `read_file_into_memory()` internally to load file content.
* - Frees the allocated buffer for file content after printing.
* - If both parameters are NULL, no output is produced.
*
* @return void This function does not return a value.
*/
void print_input_output(char* kvpData, const char* filename);

#ifdef __cplusplus
}
#endif
