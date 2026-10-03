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

    char username[50];
    char message[100];
    char response[200];

    /*
     * Create IPv4 TCP socket
     */
    client_socket = socket(AF_INET,
                           SOCK_STREAM,
                           0);

    if (client_socket < 0)
    {
        perror("socket");
        return 1;
    }

    printf("Client socket created successfully.\n");


    /*
     * Configure server address
     */
    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family = AF_INET;

    server_addr.sin_port = htons(PORT);


    /*
     * Convert 127.0.0.1 into binary IP format
     */
    if (inet_pton(AF_INET,
                  SERVER_IP,
                  &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");

        close(client_socket);

        return 1;
    }


    /*
     * Connect to NetMessenger server
     */
    printf("Connecting to %s:%d...\n",
           SERVER_IP,
           PORT);

    if (connect(client_socket,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");

        close(client_socket);

        return 1;
    }

    printf("Connected to NetMessenger server successfully.\n");


    /*
     * Ask user for username
     */
    printf("Enter username: ");

    scanf("%49s", username);


    /*
     * Build REGISTER command
     */
    snprintf(message,
             sizeof(message),
             "REGISTER %s\n",
             username);


    /*
     * Send REGISTER command
     */
    if (send(client_socket,
             message,
             strlen(message),
             0) < 0)
    {
        perror("send");

        close(client_socket);

        return 1;
    }


    /*
     * Receive server registration response
     */
    memset(response,
           0,
           sizeof(response));

    int bytes_received =
        recv(client_socket,
             response,
             sizeof(response) - 1,
             0);


    if (bytes_received > 0)
    {
        response[bytes_received] = '\0';

        printf("Server response: %s",
               response);


        /*
         * If server returns ERR,
         * registration has failed.
         */
        if (strncmp(response,
                    "ERR",
                    3) == 0)
        {
            printf("Registration failed. Closing connection.\n");

            close(client_socket);

            return 1;
        }
    }
    else
    {
        printf("No response from server.\n");

        close(client_socket);

        return 1;
    }


    /*
     * Registration successful.
     *
     * Keep the connection open so that
     * multiple clients can stay connected
     * at the same time.
     */
    printf("\nConnection active.\n");
    printf("Press ENTER to disconnect...\n");


    /*
     * The previous scanf() leaves the newline
     * character in stdin.
     *
     * First getchar() removes that newline.
     * Second getchar() waits for the user.
     */
    getchar();
    getchar();


    /*
     * Close connection
     */
    close(client_socket);

    printf("Disconnected from server.\n");

    return 0;
}
