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

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define FILES_DIR "archivos/"

const char* get_mime_type(const char* filename) {
    if (strstr(filename, ".jpg") || strstr(filename, ".jpeg")) return "image/jpeg";
    if (strstr(filename, ".png")) return "image/png";
    if (strstr(filename, ".html")) return "text/html";
    if (strstr(filename, ".txt")) return "text/plain";
    return "application/octet-stream";
}

void handle_custom_client(int client_socket, char* initial_data, int initial_size) {
    char filename[256] = {0};
    char filepath[512];
    char buffer[BUFFER_SIZE];

    strncpy(filename, initial_data, initial_size);
    filename[initial_size] = '\0';

    snprintf(filepath, sizeof(filepath), "%s%s", FILES_DIR, filename);
    FILE* file = fopen(filepath, "rb");

    if (!file) {
        int64_t err = -1;
        send(client_socket, &err, sizeof(err), 0);
        printf("Archivo no encontrado (cliente personalizado): %s\n", filename);
        close(client_socket);
        return;
    }

    fseek(file, 0L, SEEK_END);
    int64_t filesize = ftell(file);
    rewind(file);

    send(client_socket, &filesize, sizeof(filesize), 0);

    int bytes;
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(client_socket, buffer, bytes, 0);
    }

    printf("Archivo '%s' enviado al cliente personalizado (%ld bytes)\n", filename, filesize);
    fclose(file);
    close(client_socket);
}

void handle_browser_request(int client_socket, char* initial_data) {
    char filename[256] = {0};
    char filepath[512];
    char buffer[BUFFER_SIZE];

    sscanf(initial_data, "GET /%255s", filename);

    if (strlen(filename) == 0 || strstr(filename, "..")) {
        const char* bad_request = "HTTP/1.1 400 Bad Request\r\n\r\nArchivo inválido.\n";
        send(client_socket, bad_request, strlen(bad_request), 0);
        close(client_socket);
        return;
    }

    snprintf(filepath, sizeof(filepath), "%s%s", FILES_DIR, filename);
    FILE* file = fopen(filepath, "rb");

    if (!file) {
        const char* not_found = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nArchivo no encontrado.\n";
        send(client_socket, not_found, strlen(not_found), 0);
        close(client_socket);
        return;
    }

    fseek(file, 0L, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    const char* content_type = get_mime_type(filename);
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n",
             content_type, filesize);

    send(client_socket, header, strlen(header), 0);

    int bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    printf("Archivo '%s' enviado al navegador (%ld bytes)\n", filename, filesize);
    fclose(file);
    close(client_socket);
}

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
