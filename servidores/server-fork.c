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

    // Recibir nombre del archivo
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_received <= 0) {
        perror("Error al recibir el nombre del archivo");
        close(client_socket);
        return;
    }
    buffer[bytes_received] = '\0'; // Asegurarse de que el buffer esté terminado

    char filename[256];

    // Detectar si la solicitud es HTTP o texto plano
    if (strncmp(buffer, "GET ", 4) == 0) {
        // Extraer desde solicitud tipo: "GET /archivo.ext HTTP/1.0"
        sscanf(buffer, "GET /%s", filename);
        
        // Limpiar el " HTTP/1.0" si está incluido al final
        char *espacio = strchr(filename, ' ');
        if (espacio != NULL) *espacio = '\0';
        
        printf("Solicitud HTTP detectada. Archivo solicitado: %s\n", filename);
    } else {
        // Solicitud simple: solo viene el nombre del archivo
        strncpy(filename, buffer, sizeof(filename));
        filename[sizeof(filename)-1] = '\0'; // aseguramos terminación
        printf("Solicitud directa detectada. Archivo solicitado: %s\n", filename);
    }

    // Abrir el archivo
    char ruta_archivo[300];
    snprintf(ruta_archivo, sizeof(ruta_archivo), "archivos/%s", filename);
    FILE *file = fopen(ruta_archivo, "rb");

    if (!file) {
        // Enviar filesize = -1 para indicar error
        int64_t error = -1;
        send(client_socket, &error, sizeof(error), 0);
        perror("Archivo no encontrado");
        return;
    }

    // Obtener tamaño del archivo
    fseek(file, 0, SEEK_END);
    int64_t filesize = ftell(file);
    rewind(file);

    // Enviar tamaño del archivo
    send(client_socket, &filesize, sizeof(filesize), 0);

    // Enviar contenido del archivo
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
            // Proceso hijo
            close(server_fd);
            client_handler(client_socket);
            close(client_socket);
            exit(0);
        } else if (pid > 0) {
            // Proceso padre
            close(client_socket);
        } else {
            perror("Error al hacer fork");
            close(client_socket);
        }
    }

    close(server_fd);
    return 0;
}
