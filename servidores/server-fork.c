#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdint.h>
#include "common/utils.h"

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define FILES_DIR "../archivos/" //ruta donde estan los archivos

/* 
Como correr el servidor
gcc -o server-fork server-fork.c
./server-fork

*/

/*
Ejemplo de como correrlo en browser
http://127.0.0.1:8080/imagen.jpg
*/

const char* get_mime_type(const char* filename);

void handle_custom_client(int client_socket, char* initial_data, int initial_size);

void handle_browser_request(int client_socket, char* initial_data);

void client_handler(int client_fd) {
    char buffer[BUFFER_SIZE];
    char filename[256];

    int bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes <= 0) {
        perror("Error al recibir datos del cliente");
        close(client_fd);
        return;
    }
    buffer[bytes] = '\0';

    // Determinar si la solicitud es HTTP (desde browser) o directa (cliente propio)
    if (strncmp(buffer, "GET ", 4) == 0) {
        sscanf(buffer, "GET /%s", filename);
        char *espacio = strchr(filename, ' ');
        if (espacio != NULL) *espacio = '\0';
        printf("[Hijo] Solicitud HTTP recibida: %s\n", filename);
    } else {
        strncpy(filename, buffer, sizeof(filename));
        filename[sizeof(filename) - 1] = '\0';
        printf("[Hijo] Solicitud directa recibida: %s\n", filename);
    }

    FILE *archivo = fopen(filename, "rb");
    if (archivo == NULL) {
        char *msg = "HTTP/1.0 404 Not Found\r\nContent-Type: text/plain\r\n\r\nArchivo no encontrado.\n";
        send(client_fd, msg, strlen(msg), 0);
        close(client_fd);
        return;
    }

    // Enviar header HTTP 
    char header[] = "HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n";
    send(client_fd, header, strlen(header), 0);

    // Enviar el archivo en bloques
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, archivo)) > 0) {
        send(client_fd, buffer, bytes, 0);
    }

    fclose(archivo);
    close(client_fd);
    printf("[Hijo] Transferencia finalizada y conexión cerrada.\n");
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_size = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen");
        close(server_fd);
        exit(1);
    }

    printf("Servidor fork escuchando en puerto %d...\n", SERVER_PORT);

    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_size);

        if (client_socket < 0) {
            perror("Error al aceptar conexión");
            continue;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("Error al hacer fork");
            close(client_socket);
            continue;
        }

        if (pid == 0) {
            // PROCESO HIJO
            close(server_fd); // El hijo no necesita el socket del servidor

            // Manejo del cliente
            client_handler(client_socket);

            close(client_socket); // Cierra el socket del cliente
            exit(0);              // Termina el proceso hijo

        } else {
            // PROCESO PADRE
            close(client_socket); // El padre no necesita este socket
        }
    }

    close(server_fd);
    return 0;
}
