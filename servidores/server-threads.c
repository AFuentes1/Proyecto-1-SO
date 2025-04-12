#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdint.h>
#include "common/utils.h"

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define FILES_DIR "archivos/"

/* 
Como correr el servidor
gcc -o server-threads server-threads.c common/utils.c
./server-threads

*/

/*
Ejemplo de como correrlo en browser
http://127.0.0.1:8080/imagen.jpg
*/

const char* get_mime_type(const char* filename);

void handle_custom_client(int client_socket, char* initial_data, int initial_size);

void handle_browser_request(int client_socket, char* initial_data);

void* thread_handler(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);

    char peek[BUFFER_SIZE] = {0};
    int n = recv(client_socket, peek, sizeof(peek) - 1, MSG_PEEK);

    if (n > 0) {
        if (strncmp(peek, "GET ", 4) == 0) {
            recv(client_socket, peek, sizeof(peek) - 1, 0); // limpiar buffer
            handle_browser_request(client_socket, peek);
        } else {
            char filename[256] = {0};
            int bytes = recv(client_socket, filename, sizeof(filename) - 1, 0);
            handle_custom_client(client_socket, filename, bytes);
        }
    } else {
        close(client_socket);
    }

    return NULL;
}

int main() {
    int server_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_size = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind");
        close(server_socket);
        exit(1);
    }

    if (listen(server_socket, 10) < 0) {
        perror("Listen");
        close(server_socket);
        exit(1);
    }

    printf("Servidor THREAD escuchando en puerto %d...\n", SERVER_PORT);

    while (1) {
        int* client_socket = malloc(sizeof(int));
        *client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_size);

        if (*client_socket < 0) {
            perror("Accept");
            free(client_socket);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, thread_handler, client_socket);
        pthread_detach(tid); // No bloquea el hilo principal esperando a que termine
    }

    close(server_socket);
    return 0;
}
