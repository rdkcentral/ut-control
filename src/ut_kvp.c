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
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <assert.h>
#include <curl/curl.h>

/* Application Includes */
#include <ut_kvp.h>
#include <ut_log.h>

/* External libraries */
#include <libfyaml.h>

ut_kvp_instance_t *gKVP_Instance = NULL;

#define UT_KVP_MAGIC (0xdeadbeef)
#define UT_KVP_MAX_INCLUDE_DEPTH 5

typedef struct
{
    uint32_t magic;
    struct fy_document *fy_handle;
} ut_kvp_instance_internal_t;

// Struct to store the downloaded data
typedef struct
{
    char *memory;
    size_t size;
} ut_kvp_download_memory_internal_t;

/* Static functions */
static ut_kvp_instance_internal_t *validateInstance(ut_kvp_instance_t *pInstance);
static unsigned long getUIntField( ut_kvp_instance_t *pInstance, const char *pszKey, unsigned long maxRange );
static bool str_to_bool(const char *string);
static ut_kvp_status_t ut_kvp_getField(ut_kvp_instance_t *pInstance, const char *pszKey, char *pszResult);
static void convert_dot_to_slash(const char *key, char *output);
static struct fy_node* process_node(struct fy_node *node, int depth);
static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp);
static struct fy_node* process_include(const char *filename, int depth, struct fy_document *doc);
static void merge_nodes(struct fy_node *mainNode, struct fy_node *includeNode);
static void remove_include_keys(struct fy_node *node);

ut_kvp_instance_t *ut_kvp_createInstance(void)
{
    ut_kvp_instance_internal_t *pInstance = malloc(sizeof(ut_kvp_instance_internal_t));

    if ( pInstance == NULL )
    {
        return NULL;
    }

    memset(pInstance, 0, sizeof(ut_kvp_instance_internal_t));

    pInstance->magic = UT_KVP_MAGIC;

    return (ut_kvp_instance_t *)pInstance;
}

void ut_kvp_destroyInstance(ut_kvp_instance_t *pInstance)
{
    ut_kvp_instance_internal_t *pInternal = validateInstance(pInstance);

    if ( pInternal == NULL )
    {
        return;
    }

    ut_kvp_close( pInstance );

    memset(pInternal, 0, sizeof(ut_kvp_instance_internal_t));

    free(pInternal);
    pInternal = NULL;
}

ut_kvp_status_t ut_kvp_open(ut_kvp_instance_t *pInstance, char *fileName)
{
    struct fy_node *node;
    ut_kvp_instance_internal_t *pInternal = validateInstance(pInstance);

    if (pInstance == NULL)
    {
        return UT_KVP_STATUS_INVALID_INSTANCE;
    }

    if (fileName == NULL)
    {
        UT_LOG_ERROR( "Invalid Param [fileName]" );
        return UT_KVP_STATUS_INVALID_PARAM;
    }

    if (access(fileName, F_OK) != 0)
    {
        UT_LOG_ERROR("[%s] cannot be accesed", fileName);
        return UT_KVP_STATUS_FILE_OPEN_ERROR;
    }

    if(pInternal->fy_handle)
    {
        merge_nodes(fy_document_root(pInternal->fy_handle), fy_document_root(fy_document_build_from_file(NULL, fileName)));
    }
    else
    {
        pInternal->fy_handle = fy_document_build_from_file(NULL, fileName);
    }

    if (NULL == pInternal->fy_handle)
    {
        UT_LOG_ERROR("Unable to parse file/memory");
        ut_kvp_close( pInstance );
        return UT_KVP_STATUS_PARSING_ERROR;
    }

    node = process_node(fy_document_root(pInternal->fy_handle), 0);
    remove_include_keys(node);

    if (node == NULL)
    {
        UT_LOG_ERROR("Unable to process node");
        ut_kvp_close(pInstance);
        return UT_KVP_STATUS_PARSING_ERROR;
    }

    return UT_KVP_STATUS_SUCCESS;
}

ut_kvp_status_t ut_kvp_openMemory(ut_kvp_instance_t *pInstance, char *pData, uint32_t length )
{
    struct fy_node *node;
    ut_kvp_instance_internal_t *pInternal = validateInstance(pInstance);

    if (pInstance == NULL)
    {
        return UT_KVP_STATUS_INVALID_INSTANCE;
    }

    if (pData == NULL)
    {
        UT_LOG_ERROR( "Invalid Param [string]" );
        return UT_KVP_STATUS_INVALID_PARAM;
    }

    if (pInternal->fy_handle)
    {
        merge_nodes(fy_document_root(pInternal->fy_handle), fy_document_root(fy_document_build_from_malloc_string(NULL, pData, length)));
    }
    else
    {
        pInternal->fy_handle = fy_document_build_from_malloc_string(NULL, pData, length);
    }

    if (NULL == pInternal->fy_handle)
    {
        UT_LOG_ERROR("Unable to parse file/memory");
        ut_kvp_close( pInstance );
        return UT_KVP_STATUS_PARSING_ERROR;
    }

    node = process_node(fy_document_root(pInternal->fy_handle), 0);
    remove_include_keys(node);

    if (node == NULL)
    {
        UT_LOG_ERROR("Unable to process node");
        ut_kvp_close(pInstance);
        return UT_KVP_STATUS_PARSING_ERROR;
    }

    return UT_KVP_STATUS_SUCCESS;
}

void ut_kvp_close(ut_kvp_instance_t *pInstance)
{
    ut_kvp_instance_internal_t *pInternal = validateInstance(pInstance);

    if (pInternal == NULL)
    {
        return;
    }

    if ( pInternal->fy_handle != NULL)
    {
        fy_document_destroy(pInternal->fy_handle);
        pInternal->fy_handle = NULL;
    }
}

char* ut_kvp_getData( ut_kvp_instance_t *pInstance )
{
     ut_kvp_instance_internal_t *pInternal = validateInstance(pInstance);
     char *kvp_yaml_output = NULL;

     if (pInternal == NULL)
    {
        return NULL;
    }

    if ( pInternal->fy_handle == NULL)
    {
        return NULL;
    }

    kvp_yaml_output = fy_emit_document_to_string(pInternal->fy_handle, FYECF_DEFAULT);
    if (kvp_yaml_output == NULL)
    {
        UT_LOG_ERROR("Failed to emit YAML document\n");
        return NULL;
    }
    return kvp_yaml_output;
}

static ut_kvp_status_t ut_kvp_getField(ut_kvp_instance_t *pInstance, const char *pszKey, char *pzResult)
{
    ut_kvp_instance_internal_t *pInternal = validateInstance(pInstance);
    char zEntry[UT_KVP_MAX_ELEMENT_SIZE * 2];
    char zKey[UT_KVP_MAX_ELEMENT_SIZE];
    char *str = "%s";
    int fy_result;

    if (pInternal == NULL)
    {
        return UT_KVP_STATUS_INVALID_INSTANCE;
    }

    if (pszKey == NULL)
    {
        UT_LOG_ERROR("Invalid Param - pszKey");
        return UT_KVP_STATUS_INVALID_PARAM;
    }

    if (pzResult == NULL)
    {
        UT_LOG_ERROR("Invalid Param - pzResult");
        return UT_KVP_STATUS_INVALID_PARAM;
    }

    if ( pInternal->fy_handle == NULL )
    {
        UT_LOG_ERROR("No Data File open");
        return UT_KVP_STATUS_NO_DATA;
    }

    convert_dot_to_slash(pszKey, zKey);

    snprintf( zEntry, sizeof(zEntry), "%s %s", zKey, str );
    fy_result = fy_document_scanf(pInternal->fy_handle, zEntry, pzResult);
    if ( fy_result <= 0 )
    {
        UT_LOG_ERROR("Parsing Error");
        return UT_KVP_STATUS_PARSING_ERROR;
    }

    return UT_KVP_STATUS_SUCCESS;
}

bool ut_kvp_getBoolField( ut_kvp_instance_t *pInstance, const char *pszKey )
{
    char result[UT_KVP_MAX_ELEMENT_SIZE];
    ut_kvp_status_t status;
    bool bValue;

    status = ut_kvp_getField(pInstance, pszKey, result);
    if ( status != UT_KVP_STATUS_SUCCESS )
    {
        return false;
    }

    bValue = str_to_bool(result);
    return bValue;
}

static unsigned long getUIntField( ut_kvp_instance_t *pInstance, const char *pszKey, unsigned long maxRange )
{
    char *pEndptr;
    unsigned long uValue;
    char result[UT_KVP_MAX_ELEMENT_SIZE];
    ut_kvp_status_t status;
    char *pField = &result[0];

    errno = 0; // Clear the stdlib errno

    status = ut_kvp_getField(pInstance, pszKey, result);
    if ( status != UT_KVP_STATUS_SUCCESS )
    {
        return 0;
    }

    if (strstr(result, "0x"))
    {
        uValue = strtoul(pField, &pEndptr, 16); // Base 16 conversion
    }
    else
    {
        uValue = strtoul(pField, &pEndptr, 10); // Base 10 conversion
    }

    // Error checking
    if (pField == pEndptr)
    {
        UT_LOG_ERROR("No conversion performed!");
        return 0;
    }
    else if (*pEndptr != '\0')
    {
        UT_LOG_ERROR("Invalid characters in the string.");
        return 0;
    }
    else if (errno == ERANGE || uValue > maxRange)
    {
        UT_LOG_DEBUG("Value out of range for maxRange [0x%lx,%ld].", maxRange, maxRange);
        return 0;
    }

    //UT_LOG_DEBUG("Converted value: %u", uValue);
    return uValue;
}

uint8_t ut_kvp_getUInt8Field( ut_kvp_instance_t *pInstance, const char *pszKey )
{
    unsigned long value;
    value = getUIntField( pInstance, pszKey, UINT8_MAX );

    return (uint8_t)value;
}

uint16_t ut_kvp_getUInt16Field( ut_kvp_instance_t *pInstance, const char *pszKey )
{
    unsigned long value;
    value = getUIntField( pInstance, pszKey, UINT16_MAX );

    return (uint16_t)value;
}

uint32_t ut_kvp_getUInt32Field( ut_kvp_instance_t *pInstance, const char *pszKey )
{
    unsigned long value;
    value = getUIntField( pInstance, pszKey, UINT32_MAX );

    return (uint32_t)value;
}

uint64_t ut_kvp_getUInt64Field( ut_kvp_instance_t *pInstance, const char *pszKey )
{
    char *pEndptr;
    uint64_t u64Value;
    char result[UT_KVP_MAX_ELEMENT_SIZE];
    ut_kvp_status_t status;
    char *pField = &result[0];

    errno = 0; // Clear the stdlib errno
    status = ut_kvp_getField(pInstance, pszKey, result);
    if ( status != UT_KVP_STATUS_SUCCESS )
    {
        return 0;
    }

    if(strstr(result, "0x"))
    {
        u64Value = strtoull(pField, &pEndptr, 16); // Base 16 conversion
    }
    else
    {
        u64Value = strtoull(pField, &pEndptr, 10); // Base 10 conversion
    }

    // Error checking
    if (pField == pEndptr) 
    {
        UT_LOG_ERROR("No conversion performed!");
        return 0;
    }
    else if (*pEndptr != '\0') 
    {
        UT_LOG_ERROR("Invalid characters in the string.");
        return 0;
    } 
    else if (errno == ERANGE || u64Value > UINT64_MAX) 
    {
        UT_LOG_ERROR("Value out of range for uint64_t.");
        return 0;
    }

    //UT_LOG_DEBUG("Converted value: %llu", u64Value);
    return u64Value;
}

float ut_kvp_getFloatField( ut_kvp_instance_t *pInstance, const char *pszKey)
{
    char result[UT_KVP_MAX_ELEMENT_SIZE];
    ut_kvp_status_t status;
    float fValue;
    char *endPtr;

    status = ut_kvp_getField(pInstance, pszKey, result);
    if ( status != UT_KVP_STATUS_SUCCESS )
    {
        return 0;
    }

    fValue = strtof(result, &endPtr);

    if (*endPtr != '\0')
    {
        UT_LOG_ERROR("Error: Invalid floating-point string: '%s'\n", result);
        return 0;
    }
    return fValue;
}

double ut_kvp_getDoubleField( ut_kvp_instance_t *pInstance, const char *pszKey)
{
    char result[UT_KVP_MAX_ELEMENT_SIZE];
    ut_kvp_status_t status;
    double dValue;
    char *endPtr;

    status = ut_kvp_getField(pInstance, pszKey, result);
    if ( status != UT_KVP_STATUS_SUCCESS )
    {
        return 0;
    }

    dValue = strtod(result, &endPtr);

    if (*endPtr != '\0')
    {
        UT_LOG_ERROR("Error: Invalid floating-point string: '%s'\n", result);
        return 0;
    }
    return dValue;
}

bool ut_kvp_fieldPresent( ut_kvp_instance_t *pInstance, const char *pszKey)
{
    struct fy_node *node = NULL;
    struct fy_node *root = NULL;
    char zKey[UT_KVP_MAX_ELEMENT_SIZE];

    ut_kvp_instance_internal_t *pInternal = validateInstance(pInstance);

    if (pInternal == NULL)
    {
        UT_LOG_ERROR("Invalid Param - pInternal");
        return false;
    }

    if (pszKey == NULL)
    {
        UT_LOG_ERROR("Invalid Param - pszKey");
        return false;
    }

    if ( pInternal->fy_handle == NULL )
    {
        UT_LOG_ERROR("No Data File open");
        return false;
    }
    // Get the root node
    root = fy_document_root(pInternal->fy_handle);
    if ( root == NULL )
    {
        UT_LOG_ERROR("Empty document");
        return false;
    }

    convert_dot_to_slash(pszKey, zKey);

    // Find the node corresponding to the key
    node = fy_node_by_path(root, zKey, -1, FYNWF_DONT_FOLLOW);
    if ( node == NULL )
    {
        //UT_LOG_DEBUG("node not found for key = [%s] : UT_KVP_STATUS_KEY_NOT_FOUND", pszKey);
        //Commented this out now, will uncomment it once logging module is upgraded
        return false;
    }

    return true;
}

ut_kvp_status_t ut_kvp_getStringField( ut_kvp_instance_t *pInstance, const char *pszKey, char *pszReturnedString, uint32_t uStringSize )
{
    struct fy_node *node = NULL;
    struct fy_node *root = NULL;
    const char *pString = NULL;
    char zKey[UT_KVP_MAX_ELEMENT_SIZE];

    ut_kvp_instance_internal_t *pInternal = validateInstance(pInstance);

    if (pInternal == NULL)
    {
        return UT_KVP_STATUS_INVALID_INSTANCE;
    }

    if (pszKey == NULL)
    {
        UT_LOG_ERROR("Invalid Param - pszKey");
        return UT_KVP_STATUS_NULL_PARAM;
    }

    if ( pszReturnedString == NULL )
    {
        UT_LOG_ERROR("Invalid Param - pszReturnedString");
        return UT_KVP_STATUS_NULL_PARAM;
    }

    /* Make sure we populate the returned string with zt before any other action */
    *pszReturnedString=0;
    if ( pInternal->fy_handle == NULL )
    {
        UT_LOG_ERROR("No Data File open");
        return UT_KVP_STATUS_NO_DATA;
    }
    // Get the root node
    root = fy_document_root(pInternal->fy_handle);
    if ( root == NULL )
    {
        UT_LOG_ERROR("Empty document");
        return UT_KVP_STATUS_PARSING_ERROR;
    }

    convert_dot_to_slash(pszKey, zKey);

    // Find the node corresponding to the key
    node = fy_node_by_path(root, zKey, -1, FYNWF_DONT_FOLLOW);
    if ( node == NULL )
    {
        UT_LOG_ERROR("node not found for key = [%s] : UT_KVP_STATUS_KEY_NOT_FOUND", pszKey);
        return UT_KVP_STATUS_KEY_NOT_FOUND;
    }

    if (fy_node_is_scalar(node) == false)
    {
        UT_LOG_ERROR("invalid key");
        return UT_KVP_STATUS_PARSING_ERROR;
    }

    //Get the string value
    pString = fy_node_get_scalar0(node);
    if (pString == NULL)
    {
        UT_LOG_ERROR("field not found: UT_KVP_STATUS_KEY_NOT_FOUND");
        return UT_KVP_STATUS_KEY_NOT_FOUND;
    }

    strncpy( pszReturnedString, pString, uStringSize );
    return UT_KVP_STATUS_SUCCESS;
}

uint32_t ut_kvp_getListCount( ut_kvp_instance_t *pInstance, const char *pszKey)
{
    struct fy_node *node = NULL;
    struct fy_node *root = NULL;
    uint32_t count;
    char zKey[UT_KVP_MAX_ELEMENT_SIZE];

    ut_kvp_instance_internal_t *pInternal = validateInstance(pInstance);

    if (pInternal == NULL)
    {
        UT_LOG_ERROR("Invalid instance - pInstance");
        return 0;
    }

    if (pszKey == NULL)
    {
        UT_LOG_ERROR("Invalid Param - pszKey");
        return 0;
    }

    if ( pInternal->fy_handle == NULL )
    {
        UT_LOG_ERROR("No Data File open");
        return 0;
    }
    // Get the root node
    root = fy_document_root(pInternal->fy_handle);
    if ( root == NULL )
    {
        /* The file has no content that can be decoded.*/
        UT_LOG_ERROR("Empty document");
        return 0;
    }

    convert_dot_to_slash(pszKey, zKey);

    // Find the node corresponding to the key
    node = fy_node_by_path(root, zKey, -1, FYNWF_DONT_FOLLOW);
    if ( node == NULL )
    {
        UT_LOG_ERROR("node not found for key = [%s] : UT_KVP_STATUS_KEY_NOT_FOUND", pszKey);
        return 0;
    }

    if (fy_node_is_sequence(node) == false)
    {
        return 0; // Early return on negative check
    }

    count = fy_node_sequence_item_count(node);
    if (count == -1)
    {
        UT_LOG_ERROR("fy_node_sequence_item_count() returned error\n");
        return 0;
    }

    return count;
}

unsigned char* ut_kvp_getDataBytes(ut_kvp_instance_t *pInstance, const char *pszKey, int *size)
{
    struct fy_node *node = NULL;
    struct fy_node *root = NULL;
    const char *byteString = NULL;
    char zKey[UT_KVP_MAX_ELEMENT_SIZE];
    char *token;
    int byte_count = 0;
    size_t buffer_size = 16; // Initial buffer size

    ut_kvp_instance_internal_t *pInternal = validateInstance(pInstance);

    if (pInternal == NULL)
    {
        return NULL;
    }

    if (pszKey == NULL)
    {
        UT_LOG_ERROR("Invalid Param - pszKey");
        return NULL;
    }

    if (size == NULL)
    {
        UT_LOG_ERROR("Invalid address passed");
        return NULL;
    }
    *size = 0; // Ensuring size is 0, initially

    if (pInternal->fy_handle == NULL)
    {
        UT_LOG_ERROR("No Data File open");
        return NULL;
    }
    // Get the root node
    root = fy_document_root(pInternal->fy_handle);
    if (root == NULL)
    {
        UT_LOG_ERROR("Empty document");
        return NULL;
    }

    convert_dot_to_slash(pszKey, zKey);

    // Find the node corresponding to the key
    node = fy_node_by_path(root, zKey, -1, FYNWF_DONT_FOLLOW);
    if (node == NULL)
    {
        UT_LOG_ERROR("node not found for key = [%s] : UT_KVP_STATUS_KEY_NOT_FOUND", pszKey);
        return NULL;
    }

    if (fy_node_is_scalar(node) == false)
    {
        UT_LOG_ERROR("invalid key");
        return NULL;
    }

    // Get the string value
    byteString = fy_node_get_scalar0(node);
    if (byteString == NULL)
    {
        UT_LOG_ERROR("field not found: UT_KVP_STATUS_KEY_NOT_FOUND");
        return NULL;
    }

    unsigned char *output_bytes = (unsigned char *)malloc(buffer_size);
    if (!output_bytes)
    {
        UT_LOG_ERROR("Initial memory allocation error");
        return NULL;
    }

    char *input_copy = strdup(byteString);
    if (!input_copy)
    {
        UT_LOG_ERROR("Memory allocation error.");
        free(output_bytes);
        return NULL;
    }

    token = strtok(input_copy, ", ");
    while (token != NULL)
    {
        // Trim leading/trailing whitespace (if any)
        while (*token == ' ')
        {
            token++;
        }

        char *endptr;
        unsigned long value;

        // Try converting as hexadecimal (starts with "0x")
        if (strncmp(token, "0x", 2) == 0 || strncmp(token, "0X", 2) == 0)
        {
            value = strtoul(token, &endptr, 16); // Base 16 for hexadecimal
        }
        else
        {
            value = strtoul(token, &endptr, 10); // Try decimal
        }

        // Error checking
        if (*endptr != '\0' || (value == ULONG_MAX && errno == ERANGE) || value > 255)
        {
            UT_LOG_ERROR("Invalid byte value: %s", token);
            free(input_copy);
            free(output_bytes); // Free the output buffer as well
            return NULL;
        }

        // Check if we need to expand the output buffer
        if (byte_count >= buffer_size)
        {
            buffer_size *= 2; // Double the buffer size
            output_bytes = (unsigned char *)realloc(output_bytes, buffer_size);
            if (!output_bytes)
            {
                UT_LOG_ERROR("Memory reallocation error.\n");
                free(input_copy);
                return NULL;
            }
        }

        output_bytes[byte_count++] = (unsigned char)value;
        token = strtok(NULL, ", ");
    }

    free(input_copy);
    *size = byte_count;
    return output_bytes;
}

/** Static Functions */
static ut_kvp_instance_internal_t *validateInstance(ut_kvp_instance_t *pInstance)
{
    ut_kvp_instance_internal_t *pInternal = (ut_kvp_instance_internal_t *)pInstance;

    if (pInstance == NULL)
    {
        UT_LOG_ERROR("Invalid Handle");
        return NULL;
    }

    if (pInternal->magic != UT_KVP_MAGIC)
    {
        UT_LOG_ERROR("Invalid Handle - magic failure");
        return NULL;
    }

    return pInternal;
}

static bool str_to_bool(const char *string)
{
    if (strcasecmp(string, "true") == 0)
    {
        return true;
    }
    /* String is neither true or false, ensure we inform the caller*/
    return false;
}

static void convert_dot_to_slash(const char *key, char *output)
{
    if (strchr(key, '.'))
    {
        for (int i = 0; i <= UT_KVP_MAX_ELEMENT_SIZE; i++)
        {
            char key_val = key[i];
            if (key_val == '\0')
            {
                break;
            }
            if (key_val == '.')
            {
                output[i] = '/';
            }
            else
            {
                output[i] = key_val;
            }
        }
        output[strlen(key)] = '\0';
    }
    else
    {
        snprintf(output, UT_KVP_MAX_ELEMENT_SIZE, "%s", key);
    }
}

// Callback function for libcurl to write downloaded data into a MemoryStruct
static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    ut_kvp_download_memory_internal_t *downloadMemory = (ut_kvp_download_memory_internal_t *)userp;

    char *ptr = realloc(downloadMemory->memory, downloadMemory->size + realsize + 1);
    if (ptr == NULL)
    {
        // Out of memory
        UT_LOG_ERROR("Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    downloadMemory->memory = ptr;
    memcpy(&(downloadMemory->memory[downloadMemory->size]), contents, realsize);
    downloadMemory->size += realsize;
    downloadMemory->memory[downloadMemory->size] = 0;

    return realsize;
}

static struct fy_node* process_node(struct fy_node *node, int depth)
{
     struct fy_document *includeDoc = NULL;

    if (node == NULL)
    {
        UT_LOG_ERROR( "Error: Invalid node.\n");
        return NULL;
    }

    if (depth >= UT_KVP_MAX_INCLUDE_DEPTH)
    {
        UT_LOG_ERROR( "Error: Maximum include depth exceeded.\n");
        return NULL;
    }

    if (fy_node_is_scalar(node))
    {
        return node; // Nothing to do for scalar nodes
    }

    if (fy_node_is_sequence(node))
    {
        UT_LOG_DEBUG("includes inside a sequence is currently not supported");
    }

    if (fy_node_is_mapping(node))
    {
        struct fy_node_pair *pair;
        struct fy_node *includeNode = NULL;
        const char *includeFilename = NULL;

        void *iter = NULL;
        while ((pair = fy_node_mapping_iterate(node, &iter)) != NULL)
        {
            struct fy_node *key = fy_node_pair_key(pair);
            struct fy_node *value = fy_node_pair_value(pair);
            const char *key_str = fy_node_get_scalar(key, NULL);
            // UT_LOG_DEBUG("KEY_STR = %s", key_str);

            if (key_str && strstr(key_str, "include"))
            {
                includeFilename = fy_node_get_scalar(value, NULL);
                if (includeFilename)
                {
                    includeNode = process_include(includeFilename, depth, includeDoc);
                    if (includeNode)
                    {
                        merge_nodes(node, includeNode);
                        fy_document_destroy(includeDoc);
                        includeDoc = NULL;
                    }
                }
            }
        }
    }

    return node;
}

static void merge_nodes(struct fy_node *mainNode, struct fy_node *includeNode)
{

    if (mainNode == NULL)
    {
        UT_LOG_ERROR("Main node is invalid");
        return;
    }

    if (includeNode == NULL)
    {
        UT_LOG_ERROR("Included node is invalid");
        return;
    }

    if (fy_node_is_scalar(mainNode))
    {
        fy_node_create_scalar_copy(fy_node_document(mainNode), fy_node_get_scalar(includeNode, NULL), fy_node_get_scalar_length(includeNode));
    }
    else if (fy_node_is_sequence(mainNode) && fy_node_is_sequence(includeNode))
    {
        UT_LOG_DEBUG("includes inside a sequence is currently not supported");
    }
    else if (fy_node_is_mapping(mainNode) && fy_node_is_mapping(includeNode))
    {
        if (fy_node_insert(mainNode, includeNode) != 0)
        {
            UT_LOG_ERROR("Node merge failed");
        }
    }
    else
    {
        UT_LOG_ERROR("Warning: Cannot merge nodes of incompatible types\n");
    }
}

static struct fy_node* process_include(const char *filename, int depth, struct fy_document *doc)
{
    ut_kvp_download_memory_internal_t mChunk;

    if (depth >= UT_KVP_MAX_INCLUDE_DEPTH)
    {
        UT_LOG_ERROR( "Error: Maximum include depth exceeded.\n");
        return NULL;
    }

   if (strncmp(filename, "http:", 5) == 0 || strncmp(filename, "https:", 6) == 0) 
   {
        // URL include
        mChunk.memory = malloc(1);
        mChunk.size = 0;

        if (!mChunk.memory)
        {
            UT_LOG_ERROR( "Error: Not enough memory to store curl response\n");
            return NULL;
        }

        CURL *curl = curl_easy_init();
        if (!curl)
        {
            UT_LOG_ERROR( "Error: Could not initialize curl\n");
            free(mChunk.memory);
            return NULL;
        }

        CURLcode res;
        curl_easy_setopt(curl, CURLOPT_URL, filename);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&mChunk);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            UT_LOG_ERROR( "Error: curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(mChunk.memory);
            curl_easy_cleanup(curl);
            return NULL;
        }

        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code != 200)
        {
            UT_LOG_ERROR( "Error: HTTP request failed with code %ld\n", response_code);
            free(mChunk.memory);
            curl_easy_cleanup(curl);
            return NULL;
        }

        doc = fy_document_build_from_malloc_string(NULL, mChunk.memory, mChunk.size);
        if (doc == NULL)
        {
            UT_LOG_ERROR("Error: Cannot parse included content\n");
            free(mChunk.memory);
            curl_easy_cleanup(curl);
            return NULL;
        }

        process_node(fy_document_root(doc), depth + 1);
        // UT_LOG_DEBUG("%s memory chunk = \n%s\n", __FUNCTION__, mChunk.memory);

        // free(mChunk.memory); // fy_document_build_from_malloc_string():  The string is expected to have been allocated by malloc(3) and when the document is destroyed it will be automatically freed.
        curl_easy_cleanup(curl);
        return fy_document_root(doc);
    }
    else
    {
        // Local file include
        FILE *file = fopen(filename, "r");
        if (!file)
        {
            UT_LOG_ERROR("Error: Cannot open include file '%s'.\n", filename);
            return NULL;
        }

        // struct fy_document *doc;
        doc = fy_document_build_from_file(NULL, filename);
        if (doc == NULL)
        {
            UT_LOG_ERROR("Error: Cannot parse include file '%s'.\n", filename);
            fclose(file);
            return NULL;
        }

        struct fy_node *root;
        root = fy_document_root(doc);
        root = process_node(root, depth + 1);
        fclose(file);
        return fy_document_root(doc);
    }
}

// Function to recursively remove nodes with keys containing "include"
static void remove_include_keys(struct fy_node *node)
{
    struct fy_node *key, *value;
    const char *key_str;
    struct fy_node_pair *pair;
    void *iter = NULL;

    size_t capacity = 10; // Initial capacity for the list of keys to remove
    size_t remove_count = 0;

    // Allocate memory to hold keys that match the "include" condition and eventually remove them
    struct fy_node **keys_to_remove = malloc(capacity * sizeof(*keys_to_remove));
    memset(keys_to_remove, 0, capacity * sizeof(*keys_to_remove));

    if (!keys_to_remove)
    {
        UT_LOG_ERROR("Memory allocation failed\n");
        return;
    }

    if (node == NULL)
    {
        UT_LOG_ERROR("Error: Invalid node.\n");
        free(keys_to_remove);
        return;
    }

    // Process only if the node is a mapping (i.e., key-value pairs)
    if (fy_node_is_mapping(node))
    {
        // UT_LOG_DEBUG("Node pairs = %d\n", fy_node_mapping_item_count(node));
        while ((pair = fy_node_mapping_iterate(node, &iter)) != NULL)
        {
            key = fy_node_pair_key(pair);
            value = fy_node_pair_value(pair);
            key_str = fy_node_get_scalar(key, NULL);

            if (key_str && fy_node_get_scalar(value, NULL) && strstr(key_str, "include"))
            {
                // Expand capacity if needed
                if (remove_count >= capacity)
                {
                    capacity += 10; // Increase capacity incrementally by 10
                    // Reallocate memory for keys_to_remove
                    struct fy_node **new_keys = realloc(keys_to_remove, capacity * sizeof(*new_keys));
                    if (!new_keys)
                    {
                        UT_LOG_ERROR("Reallocation failed\n");
                        free(keys_to_remove);
                        return;
                    }
                    keys_to_remove = new_keys;
                }
                // Store the key to be removed later
                keys_to_remove[remove_count++] = key;
            }
        }

        // Remove collected keys
        for (size_t i = 0; i < remove_count; i++)
        {
            fy_node_mapping_remove_by_key(node, keys_to_remove[i]);
        }
    }
    else if (fy_node_is_sequence(node))
    {
        UT_LOG_DEBUG("includes inside a sequence is currently not supported");
    }

    free(keys_to_remove);
}
