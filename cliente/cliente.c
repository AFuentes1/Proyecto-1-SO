#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define MAX_THREADS 10
#define FILE_SAVE_PATH "recibidos/"

/*
Como correr el cliente
gcc -o cliente cliente.c -pthread
./cliente
*/

// Función que realiza la descarga de un archivo desde el servidor
void *client_thread(void *arg) {
    char *file_name = (char *)arg;
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char output_path[256];
    int bytes_received;
    FILE *output_file;
    int header_parsed = 0;

    // Crear socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket");
        pthread_exit(NULL);
    }

    // Configurar dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Conectar con el servidor
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Conexión");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Enviar solicitud HTTP compatible con el servidor
    char request[300];
    snprintf(request, sizeof(request), "GET /%s HTTP/1.0\r\n\r\n", file_name);
    send(client_socket, request, strlen(request), 0);

    // Preparar archivo de salida
    snprintf(output_path, sizeof(output_path), "descargado_%s", file_name);
    output_file = fopen(output_path, "wb");
    if (output_file == NULL) {
        perror("Archivo de salida");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Recibir datos y escribirlos en el archivo, omitiendo cabecera HTTP si existe
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        if (!header_parsed) {
            char *body = strstr(buffer, "\r\n\r\n");
            if (body != NULL) {
                body += 4; // Saltar la cabecera HTTP
                fwrite(body, 1, bytes_received - (body - buffer), output_file);
                header_parsed = 1;
            } else {
                continue;
            }
        } else {
            fwrite(buffer, 1, bytes_received, output_file);
        }
    }

    fclose(output_file);
    close(client_socket);
    printf("Archivo '%s' descargado con éxito.\n", output_path);
    pthread_exit(NULL);
}

int main() {
    char input[512];
    printf("Ingrese el/los nombre(s) de archivo(s) a solicitar (separados por comas): ");
    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = '\0';

    char *token = strtok(input, ",");
    pthread_t threads[20];
    int i = 0;

    // Crear un hilo por archivo solicitado
    while (token != NULL && i < 20) {
        char *file = strdup(token);
        pthread_create(&threads[i], NULL, client_thread, file);
        token = strtok(NULL, ",");
        i++;
    }

    for (int j = 0; j < i; j++) {
        pthread_join(threads[j], NULL);
    }

    return 0;
}
