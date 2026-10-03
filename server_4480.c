#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 10480

int main()
{
    int server_socket;
    struct sockaddr_in server_addr;

    /* Create TCP socket */
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0)
    {
        perror("socket");
        return 1;
    }

    printf("Socket created successfully.\n");

    /* Allow the port to be reused */
    int opt = 1;

    if (setsockopt(server_socket,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &opt,
                   sizeof(opt)) < 0)
    {
        perror("setsockopt");
        close(server_socket);
        return 1;
    }

    /* Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    /* Bind socket to port */
    if (bind(server_socket,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_socket);
        return 1;
    }

    printf("Socket bound to port %d.\n", PORT);

    /* Start listening */
    if (listen(server_socket, 10) < 0)
    {
        perror("listen");
        close(server_socket);
        return 1;
    }

    printf("\n==============================\n");
    printf(" NetMessenger Server Started\n");
    printf(" Registration : IT23634480\n");
    printf(" Port         : %d\n", PORT);
    printf(" NID          : 6344\n");
    printf("==============================\n");
    printf("Waiting for clients...\n");

    while (1)
{
    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    char buffer[1024];
    char username[50];

    printf("Waiting for a client connection...\n");

    client_socket = accept(server_socket,
                           (struct sockaddr *)&client_addr,
                           &client_len);

    if (client_socket < 0)
    {
        perror("accept");
        continue;
    }

    printf("Client connected successfully. Socket FD = %d\n",
           client_socket);

    memset(buffer, 0, sizeof(buffer));

    int bytes_received = recv(client_socket,
                              buffer,
                              sizeof(buffer) - 1,
                              0);

    if (bytes_received <= 0)
    {
        printf("Client disconnected before registration.\n");
        close(client_socket);
        continue;
    }

    buffer[bytes_received] = '\0';

    printf("Received: %s", buffer);

    if (sscanf(buffer, "REGISTER %49s", username) == 1)
    {
        char response[200];

        snprintf(response,
                 sizeof(response),
                 "OK REGISTERED %s NID:6344\n",
                 username);

        send(client_socket,
             response,
             strlen(response),
             0);

        printf("User registered: %s\n", username);
    }
    else
    {
        char *error =
            "ERR 005 INVALID_COMMAND NID:6344\n";

        send(client_socket,
             error,
             strlen(error),
             0);
    }

    close(client_socket);

    printf("Client connection closed.\n");
}
    return 0;
}
