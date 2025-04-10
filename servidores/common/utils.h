#ifndef utils_h
#define utils_h

#include <stdint.h>

#define BUFFER_SIZE 1024
#define FILES_DIR "archivos/"

const char* get_mime_type(const char* filename);
void handle_custom_client(int client_socket, char* initial_data, int initial_size);
void handle_browser_request(int client_socket, char* initial_data);

#endif
