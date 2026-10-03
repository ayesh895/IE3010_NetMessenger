#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 10480
#define SERVER_IP "127.0.0.1"

int main()
{
    int client_socket;
    struct sockaddr_in server_addr;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket < 0)
    {
        perror("socket");
        return 1;
    }

    printf("Client socket created successfully.\n");

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET,
                  SERVER_IP,
                  &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(client_socket);
        return 1;
    }

    printf("Connecting to %s:%d...\n", SERVER_IP, PORT);

    if (connect(client_socket,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(client_socket);
        return 1;
    }

    printf("Connected to NetMessenger server successfully.\n");

    close(client_socket);

    return 0;
}
