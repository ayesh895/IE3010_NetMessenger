#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 10480
#define SERVER_IP "127.0.0.1"


/*
 * Receiver thread.
 *
 * This thread continuously waits for
 * responses and incoming messages
 * from the server.
 */
void *receiver_thread(void *arg)
{
    int client_socket =
        *(int *)arg;

    char buffer[1200];


    while (1)
    {
        memset(buffer,
               0,
               sizeof(buffer));


        int bytes_received =
            recv(client_socket,
                 buffer,
                 sizeof(buffer) - 1,
                 0);


        if (bytes_received <= 0)
        {
            printf("\nDisconnected from server.\n");

            break;
        }


        buffer[bytes_received] =
            '\0';


        /*
         * Display server response or
         * incoming message.
         */
        printf("\n%s",
               buffer);

        printf("> ");

        fflush(stdout);
    }


    return NULL;
}


int main()
{
    int client_socket;

    struct sockaddr_in server_addr;

    char username[50];

    char message[100];

    char response[200];


    /*
     * Create IPv4 TCP socket.
     */
    client_socket =
        socket(AF_INET,
               SOCK_STREAM,
               0);


    if (client_socket < 0)
    {
        perror("socket");

        return 1;
    }


    printf("Client socket created successfully.\n");


    /*
     * Configure server address.
     */
    memset(&server_addr,
           0,
           sizeof(server_addr));


    server_addr.sin_family =
        AF_INET;

    server_addr.sin_port =
        htons(PORT);


    /*
     * Convert server IP address.
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
     * Connect to server.
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
     * Ask for username.
     */
    printf("Enter username: ");

    scanf("%49s",
          username);


    /*
     * Build REGISTER command.
     */
    snprintf(message,
             sizeof(message),
             "REGISTER %s\n",
             username);


    /*
     * Send REGISTER command.
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
     * Receive registration response.
     *
     * Receiver thread has NOT started yet,
     * so main() handles registration first.
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
        response[bytes_received] =
            '\0';


        printf("Server response: %s",
               response);


        /*
         * Registration failed.
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
     * Start receiver thread only after
     * successful registration.
     */
    pthread_t recv_thread;


    if (pthread_create(&recv_thread,
                       NULL,
                       receiver_thread,
                       &client_socket) != 0)
    {
        perror("pthread_create");

        close(client_socket);

        return 1;
    }


    pthread_detach(recv_thread);


    /*
     * Remove newline left by scanf().
     */
    getchar();


    char command[1024];


    printf("\nConnection active.\n");
    printf("Available commands:\n");
    printf("  LIST\n");
    printf("  BCAST <message>\n");
    printf("Type QUITLOCAL to disconnect.\n");


    /*
     * Main thread reads keyboard input
     * and sends commands.
     *
     * receiver_thread receives all
     * incoming network messages.
     */
    while (1)
    {
        printf("> ");

        fflush(stdout);


        if (fgets(command,
                  sizeof(command),
                  stdin) == NULL)
        {
            break;
        }


        /*
         * Local test command.
         * This is not sent to the server.
         */
        if (strcmp(command,
                   "QUITLOCAL\n") == 0)
        {
            break;
        }


        /*
         * Send command to server.
         */
        if (send(client_socket,
                 command,
                 strlen(command),
                 0) < 0)
        {
            perror("send");

            break;
        }
    }


    /*
     * Close TCP connection.
     */
    close(client_socket);


    printf("Disconnected from server.\n");


    return 0;
}
