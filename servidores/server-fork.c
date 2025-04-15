#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdint.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

void client_handler(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Recibir la solicitud
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        perror("Error al recibir la solicitud");
        close(client_socket);
        return;
    }
    buffer[bytes_received] = '\0';

    char filename[256];
    int es_http = 0;

    // Detectar si es una solicitud HTTP
    if (strncmp(buffer, "GET ", 4) == 0) {
        es_http = 1;
        sscanf(buffer, "GET /%s", filename);

        // Limpiar " HTTP/1.1" del final si existe
        char *espacio = strchr(filename, ' ');
        if (espacio != NULL) *espacio = '\0';

        printf("Solicitud HTTP detectada. Archivo solicitado: %s\n", filename);
    } else {
        strncpy(filename, buffer, sizeof(filename));
        filename[sizeof(filename)-1] = '\0';
        printf("Solicitud directa detectada. Archivo solicitado: %s\n", filename);
    }

    // Construir ruta al archivo
    char ruta_archivo[300];
    snprintf(ruta_archivo, sizeof(ruta_archivo), "archivos/%s", filename);
    FILE *file = fopen(ruta_archivo, "rb");

    if (!file) {
        if (es_http) {
            char *not_found = "HTTP/1.1 404 Not Found\r\n"
                              "Content-Type: text/plain\r\n"
                              "Connection: close\r\n\r\n"
                              "Archivo no encontrado.\n";
            send(client_socket, not_found, strlen(not_found), 0);
        } else {
            int64_t error = -1;
            send(client_socket, &error, sizeof(error), 0);
        }
        perror("Archivo no encontrado");
        return;
    }

    // Si es HTTP, enviar cabecera HTTP adecuada
    if (es_http) {
        char header[BUFFER_SIZE];
        char *extension = strrchr(filename, '.');
        const char *mime = "application/octet-stream";

        if (extension) {
            if (strcmp(extension, ".jpg") == 0 || strcmp(extension, ".jpeg") == 0)
                mime = "image/jpeg";
            else if (strcmp(extension, ".png") == 0)
                mime = "image/png";
            else if (strcmp(extension, ".html") == 0)
                mime = "text/html";
        }

        snprintf(header, sizeof(header),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Disposition: inline; filename=\"%s\"\r\n"
                 "Connection: close\r\n\r\n", mime, filename);

        send(client_socket, header, strlen(header), 0);
    } else {
        // Obtener tamaño del archivo y enviarlo al cliente personalizado
        fseek(file, 0, SEEK_END);
        int64_t filesize = ftell(file);
        rewind(file);
        send(client_socket, &filesize, sizeof(filesize), 0);
    }

    // Enviar el contenido del archivo
    int bytes;
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(client_socket, buffer, bytes, 0);
    }

    fclose(file);
    printf("Archivo '%s' enviado exitosamente.\n", filename);
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

    printf("Servidor FORK listo en puerto %d...\n", SERVER_PORT);

    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_size);

        if (client_socket < 0) {
            perror("Error al aceptar conexión");
            continue;
        }

        pid_t pid = fork();

        if (pid == 0) {
            close(server_fd);
            client_handler(client_socket);
            close(client_socket);
            exit(0);
        } else if (pid > 0) {
            close(client_socket);
        } else {
            perror("Error al hacer fork");
            close(client_socket);
        }
    }

    close(server_fd);
    return 0;
}

