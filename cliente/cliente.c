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
#define FILE_SAVE_PATH "Recibidos/"

void* request_file(void* arg) {
    char* filename = (char*)arg;
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    FILE* file;
    char filepath[256];

    // Crear socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket");
        pthread_exit(NULL);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Conectar con servidor
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect");
        close(sock);
        pthread_exit(NULL);
    }

    // Enviar nombre del archivo
    send(sock, filename, strlen(filename), 0);

    // Leer tamaño del archivo (debe ser enviado como un entero de 8 bytes por el servidor)
    int64_t filesize;
    int bytes_read = recv(sock, &filesize, sizeof(filesize), 0);
    if (bytes_read <= 0 || filesize <= 0) {
        printf("El archivo '%s' no existe o hubo un error.\n", filename);
        close(sock);
        pthread_exit(NULL);
    }

    // Crear la ruta completa donde se guardará el archivo
    snprintf(filepath, sizeof(filepath), "%s%s", FILE_SAVE_PATH, filename);
    
    // Crear carpeta de destino si no existe
    struct stat st = {0};
    if (stat(FILE_SAVE_PATH, &st) == -1) {
        mkdir(FILE_SAVE_PATH, 0700);
    }

    // Crear archivo para guardar el contenido recibido
    file = fopen(filepath, "wb");
    if (!file) {
        perror("File");
        close(sock);
        pthread_exit(NULL);
    }

    printf("Descargando '%s' (%.2f KB)...\n", filename, filesize / 1024.0);

    int64_t total_received = 0;
    int percent = 0, last_percent = -1;
    int bytes;

    // Recibir datos del servidor
    while ((bytes = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes, file);
        total_received += bytes;

        percent = (int)((total_received * 100) / filesize);
        if (percent != last_percent) {
            printf("\rProgreso [%3d%%]", percent);
            fflush(stdout);
            last_percent = percent;
        }
    }

    printf("\nArchivo '%s' recibido completamente.\n", filename);
    fclose(file);
    close(sock);
    pthread_exit(NULL);
}

int main() {
    char input[256];
    printf("Ingrese el/los nombre/s del/los archivo/s (separados por comas): ");
    fgets(input, sizeof(input), stdin);

    // Quitar salto de línea
    input[strcspn(input, "\n")] = '\0';

    // Separar por comas
    char* token = strtok(input, ",");
    pthread_t threads[MAX_THREADS];
    int count = 0;

    while (token && count < MAX_THREADS) {
        char* filename = strdup(token);
        pthread_create(&threads[count++], NULL, request_file, filename);
        token = strtok(NULL, ",");
    }

    for (int i = 0; i < count; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
