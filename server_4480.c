#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 10480
#define MAX_CLIENTS 10
#define USERNAME_SIZE 50

typedef struct
{
    int socket;
    char username[USERNAME_SIZE];
    int active;
} Client;

Client clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


/* Find a free client slot */
int find_free_client_slot()
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active == 0)
        {
            return i;
        }
    }

    return -1;
}


/* Check whether a username already exists */
int username_exists(const char *username)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active &&
            strcmp(clients[i].username, username) == 0)
        {
            return 1;
        }
    }

    return 0;
}


/* Remove disconnected client */
void remove_client(int socket)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active &&
            clients[i].socket == socket)
        {
            printf("Removing user: %s\n",
                   clients[i].username);

            clients[i].active = 0;
            clients[i].socket = -1;
            clients[i].username[0] = '\0';

            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}


/* Handle LIST command */
void handle_list(int client_socket)
{
    char response[1024];

    strcpy(response, "OK USERS ");

    pthread_mutex_lock(&clients_mutex);

    int first = 1;

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active)
        {
            if (!first)
            {
                strcat(response, ",");
            }

            strcat(response,
                   clients[i].username);

            first = 0;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    strcat(response,
           " NID:6344\n");

    send(client_socket,
         response,
         strlen(response),
         0);
}


/* Handle BCAST command */
void handle_bcast(int sender_socket,
                  const char *sender_username,
                  const char *message)
{
    char outgoing[1200];

    snprintf(outgoing,
             sizeof(outgoing),
             "MSG BCAST %s %s\n",
             sender_username,
             message);

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active &&
            clients[i].socket != sender_socket)
        {
            send(clients[i].socket,
                 outgoing,
                 strlen(outgoing),
                 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    char *response =
        "OK SENT NID:6344\n";

    send(sender_socket,
         response,
         strlen(response),
         0);
}


/* Handle PMSG command */
void handle_pmsg(int sender_socket,
                 const char *sender_username,
                 const char *target_username,
                 const char *message)
{
    int target_socket = -1;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active &&
            strcmp(clients[i].username,
                   target_username) == 0)
        {
            target_socket = clients[i].socket;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);


    /*
     * Target user does not exist.
     */
    if (target_socket == -1)
    {
        char *error =
            "ERR 002 USER_NOT_FOUND NID:6344\n";

        send(sender_socket,
             error,
             strlen(error),
             0);

        return;
    }


    /*
     * Send private message to target.
     *
     * MSG lines do NOT contain NID.
     */
    char outgoing[1200];

    snprintf(outgoing,
             sizeof(outgoing),
             "MSG PRIV %s %s\n",
             sender_username,
             message);

    send(target_socket,
         outgoing,
         strlen(outgoing),
         0);


    /*
     * Confirm success to sender.
     */
    char *response =
        "OK SENT NID:6344\n";

    send(sender_socket,
         response,
         strlen(response),
         0);
}


/* Handle one connected client */
void *client_handler(void *arg)
{
    int client_socket =
        *(int *)arg;

    free(arg);

    char buffer[1024];
    char username[USERNAME_SIZE];

    memset(buffer,
           0,
           sizeof(buffer));

    memset(username,
           0,
           sizeof(username));


    /*
     * First command must be REGISTER.
     */
    int bytes_received =
        recv(client_socket,
             buffer,
             sizeof(buffer) - 1,
             0);


    if (bytes_received <= 0)
    {
        printf("Client disconnected before registration.\n");

        close(client_socket);

        return NULL;
    }


    buffer[bytes_received] =
        '\0';


    printf("Received: %s",
           buffer);


    /*
     * Validate REGISTER command.
     */
    if (sscanf(buffer,
               "REGISTER %49s",
               username) != 1)
    {
        char *error =
            "ERR 005 INVALID_COMMAND NID:6344\n";

        send(client_socket,
             error,
             strlen(error),
             0);

        printf("Invalid registration command.\n");

        close(client_socket);

        return NULL;
    }


    /*
     * Lock shared client list.
     */
    pthread_mutex_lock(&clients_mutex);


    /*
     * Check duplicate username.
     */
    if (username_exists(username))
    {
        pthread_mutex_unlock(&clients_mutex);

        char *error =
            "ERR 001 USERNAME_TAKEN NID:6344\n";

        send(client_socket,
             error,
             strlen(error),
             0);

        printf("Registration rejected. Username already exists: %s\n",
               username);

        close(client_socket);

        return NULL;
    }


    /*
     * Find free slot.
     */
    int slot =
        find_free_client_slot();


    if (slot == -1)
    {
        pthread_mutex_unlock(&clients_mutex);

        char *error =
            "ERR 008 SERVER_FULL NID:6344\n";

        send(client_socket,
             error,
             strlen(error),
             0);

        printf("Server full. Connection rejected.\n");

        close(client_socket);

        return NULL;
    }


    /*
     * Store client information.
     */
    clients[slot].socket =
        client_socket;

    strncpy(clients[slot].username,
            username,
            USERNAME_SIZE - 1);

    clients[slot].username[USERNAME_SIZE - 1] =
        '\0';

    clients[slot].active =
        1;


    pthread_mutex_unlock(&clients_mutex);


    /*
     * Registration successful.
     */
    char response[200];

    snprintf(response,
             sizeof(response),
             "OK REGISTERED %s NID:6344\n",
             username);

    send(client_socket,
         response,
         strlen(response),
         0);


    printf("User registered: %s\n",
           username);

    printf("Client %s is now connected.\n",
           username);


    /*
     * Main command loop.
     */
    while (1)
    {
        memset(buffer,
               0,
               sizeof(buffer));


        bytes_received =
            recv(client_socket,
                 buffer,
                 sizeof(buffer) - 1,
                 0);


        /*
         * Client closed connection.
         */
        if (bytes_received == 0)
        {
            printf("Client %s disconnected.\n",
                   username);

            break;
        }


        /*
         * recv() error.
         */
        if (bytes_received < 0)
        {
            perror("recv");

            break;
        }


        buffer[bytes_received] =
            '\0';


        printf("Command from %s: %s",
               username,
               buffer);


        /*
         * LIST
         */
        if (strcmp(buffer,
                   "LIST\n") == 0)
        {
            handle_list(client_socket);
        }


        /*
         * BCAST <message>
         */
        else if (strncmp(buffer,
                         "BCAST ",
                         6) == 0)
        {
            char message[900];

            strcpy(message,
                   buffer + 6);

            /*
             * Remove newline.
             */
            message[strcspn(message, "\n")] =
                '\0';


            if (strlen(message) == 0)
            {
                char *error =
                    "ERR 007 INVALID_FORMAT NID:6344\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);
            }
            else
            {
                handle_bcast(client_socket,
                             username,
                             message);
            }
        }


        /*
         * PMSG <username> <message>
         */
        else if (strncmp(buffer,
                         "PMSG ",
                         5) == 0)
        {
            char target[USERNAME_SIZE];
            char message[900];

            memset(target,
                   0,
                   sizeof(target));

            memset(message,
                   0,
                   sizeof(message));


            if (sscanf(buffer,
                       "PMSG %49s %899[^\n]",
                       target,
                       message) == 2)
            {
                handle_pmsg(client_socket,
                            username,
                            target,
                            message);
            }
            else
            {
                char *error =
                    "ERR 007 INVALID_FORMAT NID:6344\n";

                send(client_socket,
                     error,
                     strlen(error),
                     0);
            }
        }


        /*
         * Unknown command.
         */
        else
        {
            char *error =
                "ERR 005 INVALID_COMMAND NID:6344\n";

            send(client_socket,
                 error,
                 strlen(error),
                 0);
        }
    }


    /*
     * Remove client from user list.
     */
    remove_client(client_socket);


    close(client_socket);


    return NULL;
}


int main()
{
    int server_socket;

    struct sockaddr_in server_addr;


    /*
     * Initialise client table.
     */
    for (int i = 0;
         i < MAX_CLIENTS;
         i++)
    {
        clients[i].socket = -1;
        clients[i].active = 0;
        clients[i].username[0] = '\0';
    }


    /*
     * Create IPv4 TCP socket.
     */
    server_socket =
        socket(AF_INET,
               SOCK_STREAM,
               0);


    if (server_socket < 0)
    {
        perror("socket");

        return 1;
    }


    printf("Socket created successfully.\n");


    /*
     * Enable address reuse.
     */
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


    /*
     * Configure server address.
     */
    memset(&server_addr,
           0,
           sizeof(server_addr));


    server_addr.sin_family =
        AF_INET;

    server_addr.sin_addr.s_addr =
        INADDR_ANY;

    server_addr.sin_port =
        htons(PORT);


    /*
     * Bind server to port.
     */
    if (bind(server_socket,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");

        close(server_socket);

        return 1;
    }


    printf("Socket bound to port %d.\n",
           PORT);


    /*
     * Listen for incoming clients.
     */
    if (listen(server_socket,
               MAX_CLIENTS) < 0)
    {
        perror("listen");

        close(server_socket);

        return 1;
    }


    printf("\n");
    printf("==============================\n");
    printf(" NetMessenger Server Started\n");
    printf(" Registration : IT23634480\n");
    printf(" Port         : %d\n", PORT);
    printf(" NID          : 6344\n");
    printf("==============================\n");
    printf("Waiting for clients...\n");


    /*
     * Accept clients continuously.
     */
    while (1)
    {
        struct sockaddr_in client_addr;

        socklen_t client_len =
            sizeof(client_addr);


        int *client_socket =
            malloc(sizeof(int));


        if (client_socket == NULL)
        {
            perror("malloc");

            continue;
        }


        printf("Waiting for a client connection...\n");


        *client_socket =
            accept(server_socket,
                   (struct sockaddr *)&client_addr,
                   &client_len);


        if (*client_socket < 0)
        {
            perror("accept");

            free(client_socket);

            continue;
        }


        printf("Client connected successfully. Socket FD = %d\n",
               *client_socket);


        pthread_t thread_id;


        /*
         * Create one thread per client.
         */
        if (pthread_create(&thread_id,
                           NULL,
                           client_handler,
                           client_socket) != 0)
        {
            perror("pthread_create");

            close(*client_socket);

            free(client_socket);

            continue;
        }


        pthread_detach(thread_id);
    }


    close(server_socket);

    return 0;
}
