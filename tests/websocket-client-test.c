#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

// Callback function to handle incoming data
size_t websocket_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total_size = size * nmemb;
    printf("Received data: %.*s\n", (int)total_size, ptr);
    return total_size;
}

int main(void) {
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    const char *data_to_send = "Hello, WebSocket!";

    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        // Set the WebSocket URL
        curl_easy_setopt(curl, CURLOPT_URL, "ws://localhost:8080");

        // Set the WebSocket protocols
        headers = curl_slist_append(headers, "Sec-WebSocket-Protocol: chat, superchat");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set the callback function to handle received data
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, websocket_callback);

        // Set the data to send
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data_to_send);

        // Start the WebSocket connection
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        // Cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return 0;
}
