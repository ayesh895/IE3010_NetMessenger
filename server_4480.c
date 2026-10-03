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

#define MAX_ROOMS 20
#define ROOM_NAME_SIZE 50


typedef struct
{
    int socket;
    char username[USERNAME_SIZE];
    int active;
} Client;


typedef struct
{
    char name[ROOM_NAME_SIZE];

    int member_sockets[MAX_CLIENTS];
    int member_count;

    int active;
} Room;


/* ---------------- GLOBAL DATA ---------------- */

Client clients[MAX_CLIENTS];
Room rooms[MAX_ROOMS];

pthread_mutex_t clients_mutex =
    PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t rooms_mutex =
    PTHREAD_MUTEX_INITIALIZER;


/* ---------------- CLIENT FUNCTIONS ---------------- */

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


int username_exists(const char *username)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].active &&
            strcmp(clients[i].username,
                   username) == 0)
        {
            return 1;
        }
    }

    return 0;
}


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


/* ---------------- ROOM FUNCTIONS ---------------- */

int find_room(const char *room_name)
{
    for (int i = 0; i < MAX_ROOMS; i++)
    {
        if (rooms[i].active &&
            strcmp(rooms[i].name,
                   room_name) == 0)
        {
            return i;
        }
    }

    return -1;
}


int find_free_room_slot()
{
    for (int i = 0; i < MAX_ROOMS; i++)
    {
        if (rooms[i].active == 0)
        {
            return i;
        }
    }

    return -1;
}


int client_is_room_member(int room_index,
                          int client_socket)
{
    for (int i = 0;
         i < rooms[room_index].member_count;
         i++)
    {
        if (rooms[room_index].member_sockets[i]
            == client_socket)
        {
            return 1;
        }
    }

    return 0;
}


void remove_client_from_all_rooms(int client_socket)
{
    pthread_mutex_lock(&rooms_mutex);

    for (int r = 0; r < MAX_ROOMS; r++)
    {
        if (!rooms[r].active)
        {
            continue;
        }

        for (int m = 0;
             m < rooms[r].member_count;
             m++)
        {
            if (rooms[r].member_sockets[m]
                == client_socket)
            {
                for (int j = m;
                     j < rooms[r].member_count - 1;
                     j++)
                {
                    rooms[r].member_sockets[j] =
                        rooms[r].member_sockets[j + 1];
                }

                rooms[r].member_count--;

                m--;
            }
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
}


/* ---------------- LIST USERS ---------------- */

void handle_list(int client_socket)
{
    char response[1024];

    strcpy(response,
           "OK USERS ");

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


/* ---------------- BROADCAST ---------------- */

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


/* ---------------- PRIVATE MESSAGE ---------------- */

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
            target_socket =
                clients[i].socket;

            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);


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


    char *response =
        "OK SENT NID:6344\n";

    send(sender_socket,
         response,
         strlen(response),
         0);
}


/* ---------------- JOIN ROOM ---------------- */

void handle_join(int client_socket,
                 const char *room_name)
{
    pthread_mutex_lock(&rooms_mutex);

    int room_index =
        find_room(room_name);


    /*
     * Create room if it does not exist.
     */
    if (room_index == -1)
    {
        room_index =
            find_free_room_slot();

        if (room_index == -1)
        {
            pthread_mutex_unlock(&rooms_mutex);

            char *error =
                "ERR 009 ROOM_LIMIT_REACHED NID:6344\n";

            send(client_socket,
                 error,
                 strlen(error),
                 0);

            return;
        }


        rooms[room_index].active = 1;

        strncpy(rooms[room_index].name,
                room_name,
                ROOM_NAME_SIZE - 1);

        rooms[room_index]
            .name[ROOM_NAME_SIZE - 1] =
            '\0';

        rooms[room_index].member_count = 0;


        printf("Room created: %s\n",
               room_name);
    }


    /*
     * Avoid adding same client twice.
     */
    if (!client_is_room_member(room_index,
                               client_socket))
    {
        if (rooms[room_index].member_count <
            MAX_CLIENTS)
        {
            rooms[room_index]
                .member_sockets[
                    rooms[room_index].member_count
                ] = client_socket;

            rooms[room_index].member_count++;
        }
    }


    pthread_mutex_unlock(&rooms_mutex);


    char response[200];

    snprintf(response,
             sizeof(response),
             "OK JOINED %s NID:6344\n",
             room_name);

    send(client_socket,
         response,
         strlen(response),
         0);
}


/* ---------------- LEAVE ROOM ---------------- */

void handle_leave(int client_socket,
                  const char *room_name)
{
    pthread_mutex_lock(&rooms_mutex);

    int room_index =
        find_room(room_name);


    if (room_index == -1)
    {
        pthread_mutex_unlock(&rooms_mutex);

        char *error =
            "ERR 003 ROOM_NOT_FOUND NID:6344\n";

        send(client_socket,
             error,
             strlen(error),
             0);

        return;
    }


    for (int i = 0;
         i < rooms[room_index].member_count;
         i++)
    {
        if (rooms[room_index].member_sockets[i]
            == client_socket)
        {
            for (int j = i;
                 j <
                 rooms[room_index].member_count - 1;
                 j++)
            {
                rooms[room_index]
                    .member_sockets[j] =
                    rooms[room_index]
                        .member_sockets[j + 1];
            }

            rooms[room_index].member_count--;

            break;
        }
    }


    pthread_mutex_unlock(&rooms_mutex);


    char response[200];

    snprintf(response,
             sizeof(response),
             "OK LEFT %s NID:6344\n",
             room_name);

    send(client_socket,
         response,
         strlen(response),
         0);
}


/* ---------------- LIST ROOMS ---------------- */

void handle_rooms(int client_socket)
{
    char response[1024];

    strcpy(response,
           "OK ROOMS ");

    pthread_mutex_lock(&rooms_mutex);

    int first = 1;

    for (int i = 0; i < MAX_ROOMS; i++)
    {
        if (rooms[i].active)
        {
            if (!first)
            {
                strcat(response, ",");
            }

            strcat(response,
                   rooms[i].name);

            first = 0;
        }
    }

    pthread_mutex_unlock(&rooms_mutex);


    strcat(response,
           " NID:6344\n");


    send(client_socket,
         response,
         strlen(response),
         0);
}


/* ---------------- ROOM MESSAGE ---------------- */

void handle_rmsg(int sender_socket,
                 const char *sender_username,
                 const char *room_name,
                 const char *message)
{
    pthread_mutex_lock(&rooms_mutex);

    int room_index =
        find_room(room_name);


    if (room_index == -1)
    {
        pthread_mutex_unlock(&rooms_mutex);

        char *error =
            "ERR 003 ROOM_NOT_FOUND NID:6344\n";

        send(sender_socket,
             error,
             strlen(error),
             0);

        return;
    }


    char outgoing[1200];

    snprintf(outgoing,
             sizeof(outgoing),
             "MSG ROOM %s %s %s\n",
             room_name,
             sender_username,
             message);


    /*
     * Deliver only to room members.
     */
    for (int i = 0;
         i < rooms[room_index].member_count;
         i++)
    {
        int target_socket =
            rooms[room_index].member_sockets[i];

        send(target_socket,
             outgoing,
             strlen(outgoing),
             0);
    }


    pthread_mutex_unlock(&rooms_mutex);


    char *response =
        "OK SENT NID:6344\n";

    send(sender_socket,
         response,
         strlen(response),
         0);
}


/* ---------------- CLIENT THREAD ---------------- */

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

        close(client_socket);

        return NULL;
    }


    pthread_mutex_lock(&clients_mutex);


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

        close(client_socket);

        return NULL;
    }


    clients[slot].socket =
        client_socket;

    strncpy(clients[slot].username,
            username,
            USERNAME_SIZE - 1);

    clients[slot]
        .username[USERNAME_SIZE - 1] =
        '\0';

    clients[slot].active =
        1;


    pthread_mutex_unlock(&clients_mutex);


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


    /* ---------------- COMMAND LOOP ---------------- */

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


        if (bytes_received == 0)
        {
            printf("Client %s disconnected.\n",
                   username);

            break;
        }


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


        /* LIST */
        if (strcmp(buffer,
                   "LIST\n") == 0)
        {
            handle_list(client_socket);
        }


        /* ROOMS */
        else if (strcmp(buffer,
                        "ROOMS\n") == 0)
        {
            handle_rooms(client_socket);
        }


        /* BCAST <message> */
        else if (strncmp(buffer,
                         "BCAST ",
                         6) == 0)
        {
            char message[900];

            strcpy(message,
                   buffer + 6);

            message[strcspn(message,
                            "\n")] =
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


        /* PMSG <username> <message> */
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


        /* JOIN <room> */
        else if (strncmp(buffer,
                         "JOIN ",
                         5) == 0)
        {
            char room_name[ROOM_NAME_SIZE];


            if (sscanf(buffer,
                       "JOIN %49s",
                       room_name) == 1)
            {
                handle_join(client_socket,
                            room_name);
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


        /* LEAVE <room> */
        else if (strncmp(buffer,
                         "LEAVE ",
                         6) == 0)
        {
            char room_name[ROOM_NAME_SIZE];


            if (sscanf(buffer,
                       "LEAVE %49s",
                       room_name) == 1)
            {
                handle_leave(client_socket,
                             room_name);
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


        /* RMSG <room> <message> */
        else if (strncmp(buffer,
                         "RMSG ",
                         5) == 0)
        {
            char room_name[ROOM_NAME_SIZE];
            char message[900];

            memset(room_name,
                   0,
                   sizeof(room_name));

            memset(message,
                   0,
                   sizeof(message));


            if (sscanf(buffer,
                       "RMSG %49s %899[^\n]",
                       room_name,
                       message) == 2)
            {
                handle_rmsg(client_socket,
                            username,
                            room_name,
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


        /* Unknown command */
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
     * Clean up disconnected client.
     */
    remove_client_from_all_rooms(client_socket);

    remove_client(client_socket);

    close(client_socket);

    return NULL;
}


/* ---------------- MAIN ---------------- */

int main()
{
    int server_socket;

    struct sockaddr_in server_addr;


    /* Initialise client table */
    for (int i = 0;
         i < MAX_CLIENTS;
         i++)
    {
        clients[i].socket = -1;
        clients[i].active = 0;
        clients[i].username[0] = '\0';
    }


    /* Initialise room table */
    for (int i = 0;
         i < MAX_ROOMS;
         i++)
    {
        rooms[i].active = 0;
        rooms[i].member_count = 0;
        rooms[i].name[0] = '\0';
    }


    /* Create socket */
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


    /* Address reuse */
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


    /* Server address */
    memset(&server_addr,
           0,
           sizeof(server_addr));


    server_addr.sin_family =
        AF_INET;

    server_addr.sin_addr.s_addr =
        INADDR_ANY;

    server_addr.sin_port =
        htons(PORT);


    /* Bind */
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


    /* Listen */
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


    /* Accept loop */
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
