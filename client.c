#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[1024];

    sock = socket(AF_INET, SOCK_STREAM, 0); // Connexion en protocole TCP
    if (sock == -1) {
        perror("socket failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Etablir la connexion sur quel port ?

    // Tentative de connexion avec le SERVICE
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect failed");
        exit(1);
    }

    // Envoyer un message
    const char *message = "Hello, server!";
    send(sock, message, strlen(message), 0);

    recv(sock, buffer, sizeof(buffer), 0);
    printf("Received from server: %s\n", buffer);

    close(sock);

    return 0;
}
