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
static struct fy_document *parse_yaml_file(const char *filePath);
static int download_url(const char *url, ut_kvp_download_memory_internal_t *memoryChunk);
static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp);
static void merge_yaml_document(struct fy_document *baseDoc, struct fy_document *secondaryDoc);

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
        printf("[%s] cannot be accesed", fileName);
        return UT_KVP_STATUS_FILE_OPEN_ERROR;
    }
    pInternal->fy_handle = fy_document_build_from_file(NULL, fileName);

    if (NULL == pInternal->fy_handle)
    {
        UT_LOG_ERROR("Unable to parse file/memory");
        ut_kvp_close( pInstance );
        return UT_KVP_STATUS_PARSING_ERROR;
    }

    return UT_KVP_STATUS_SUCCESS;
}

ut_kvp_status_t ut_kvp_openMemory(ut_kvp_instance_t *pInstance, char *pData, uint32_t length )
{
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

    pInternal->fy_handle = fy_document_build_from_malloc_string(NULL, pData, length);

    if (NULL == pInternal->fy_handle)
    {
        UT_LOG_ERROR("Unable to parse file/memory");
        ut_kvp_close( pInstance );
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
        printf("Value out of range for maxRange [0x%lx,%ld].", maxRange, maxRange);
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
        UT_LOG_ERROR("node not found: UT_KVP_STATUS_KEY_NOT_FOUND");
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
        UT_LOG_ERROR("node not found: UT_KVP_STATUS_KEY_NOT_FOUND");
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
        UT_LOG_ERROR("node not found: UT_KVP_STATUS_KEY_NOT_FOUND");
        return 0;
    }

    if (fy_node_is_sequence(node))
    {
        count = fy_node_sequence_item_count(node);
        if (count == -1)
        {
            UT_LOG_ERROR("fy_node_sequence_item_count() returned error\n ");
            return 0;
        }
    }

    return count;
}

void* ut_kvp_getMergedFile( ut_kvp_instance_t *pInstance, const char* pszKey )
{
    struct fy_document *includedDocument = NULL;
    ut_kvp_instance_internal_t *pInternal = validateInstance(pInstance);

    if (pInternal == NULL)
    {
        UT_LOG_ERROR("Invalid instance - pInstance");
        return 0;
    }

    if (pInternal->fy_handle == NULL)
    {
        UT_LOG_ERROR("No Data File open");
        return 0;
    }

    // Extract the "include" node (you may have multiple such nodes)
    struct fy_node *fileIncluded = fy_node_by_path(fy_document_root(pInternal->fy_handle), pszKey, -1, FYNWF_DONT_FOLLOW);

    if(fileIncluded == NULL)
    {
        UT_LOG_ERROR("Node not found/extracted");
        return 0;
    }

    if (fy_node_is_scalar(fileIncluded))
    {
        const char *filePathForIncluded = fy_node_get_scalar(fileIncluded, NULL);

        // Parse and merge the included file
        includedDocument = parse_yaml_file(filePathForIncluded);
        if (includedDocument == NULL)
        {
            UT_LOG_ERROR("parsing or merging unsuccessful");
            return 0;
        }
    }

    merge_yaml_document(pInternal->fy_handle, includedDocument);
    fy_document_destroy(includedDocument);
    // Optionally remove the included_file node after merging
    fy_node_mapping_remove_by_key(fy_document_root(pInternal->fy_handle), fileIncluded);
    return (void *)pInternal;
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
    if (strcasecmp(string, "false") == 0)
    {
        return false;
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

// Function to parse a YAML file (local or remote) into a document
static struct fy_document *parse_yaml_file(const char *filePath)
{
    struct fy_document *document = NULL;
    ut_kvp_download_memory_internal_t mChunk;

    // Check if the file path is a URL
    if (strncmp(filePath, "http://", 7) == 0 || strncmp(filePath, "https://", 8) == 0)
    {
        // Download the content from the URL
        mChunk.memory = malloc(1);
        mChunk.size = 0;

        if (download_url(filePath, &mChunk) == 0)
        {
            // Parse the downloaded content
            document = fy_document_build_from_malloc_string(NULL, mChunk.memory, mChunk.size);
            free(mChunk.memory);
        }
    }
    else
    {
        // Parse the local file
        document = fy_document_build_from_file(NULL, filePath);
    }

    if (document == NULL)
    {
        UT_LOG_ERROR("Failed to parse YAML document: %s\n", filePath);
        return NULL;
    }

    return document;
}

// Function to download content from a URL
static int download_url(const char *url, ut_kvp_download_memory_internal_t *memoryChunk)
{
    CURL *curlHandle;
    CURLcode result;

    curl_global_init(CURL_GLOBAL_ALL);

    curlHandle = curl_easy_init();
    if (curlHandle)
    {
        curl_easy_setopt(curlHandle, CURLOPT_URL, url);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, write_memory_callback);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, (void *)memoryChunk);
        curl_easy_setopt(curlHandle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        result = curl_easy_perform(curlHandle);

        if (result != CURLE_OK)
        {
            UT_LOG_ERROR("curl_easy_perform() failed: %s\n", curl_easy_strerror(result));
            curl_easy_cleanup(curlHandle);
            curl_global_cleanup();
            return 1;
        }

        curl_easy_cleanup(curlHandle);
    }

    curl_global_cleanup();
    return 0;
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

// Function to merge nodes from one document into another
static void merge_yaml_document(struct fy_document *baseDoc, struct fy_document *secondaryDoc)
{
    struct fy_node_pair *secondaryPair;
    void *secondaryIterator = NULL;
    struct fy_node_pair *base_pair;
    void *baseIterator = NULL;
    int keyFound = 0;

    struct fy_node *baseRoot = fy_document_root(baseDoc);
    struct fy_node *secondaryRoot = fy_document_root(secondaryDoc);

    if (!baseRoot || !secondaryRoot || fy_node_get_type(baseRoot) != FYNT_MAPPING || fy_node_get_type(secondaryRoot) != FYNT_MAPPING)
    {
        UT_LOG_ERROR("Unsupported YAML structure\n");
        return;
    }

    while ((secondaryPair = fy_node_mapping_iterate(secondaryRoot, &secondaryIterator)) != NULL)
    {
        struct fy_node *secondaryKey = fy_node_pair_key(secondaryPair);
        struct fy_node *secondaryValue = fy_node_pair_value(secondaryPair);

        while ((base_pair = fy_node_mapping_iterate(baseRoot, &baseIterator)) != NULL)
        {
            struct fy_node *base_key = fy_node_pair_key(base_pair);

            if (fy_node_compare(base_key, secondaryKey))
            {
                struct fy_node *duplicateValue = fy_node_copy(baseDoc, secondaryValue);
                fy_node_pair_set_value(base_pair, duplicateValue);
                keyFound = 1;
                break;
            }
        }

        if (!keyFound)
        {
            struct fy_node *dup_key = fy_node_copy(baseDoc, secondaryKey);
            struct fy_node *duplicateValue = fy_node_copy(baseDoc, secondaryValue);
            fy_node_mapping_append(baseRoot, dup_key, duplicateValue);
        }
    }
}