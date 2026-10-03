#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 10480

#define MAX_CLIENTS 10
#define USERNAME_SIZE 50

#define MAX_ROOMS 20
#define ROOM_NAME_SIZE 50

#define MAX_FILE_SIZE (10 * 1024 * 1024)

#define LOG_FILE "netmsg_IT23634480.log"


/* =========================================================
   DATA STRUCTURES
   ========================================================= */

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


/* =========================================================
   GLOBAL DATA
   ========================================================= */

Client clients[MAX_CLIENTS];

Room rooms[MAX_ROOMS];

pthread_mutex_t clients_mutex =
    PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t rooms_mutex =
    PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t log_mutex =
    PTHREAD_MUTEX_INITIALIZER;


/* =========================================================
   LOGGING
   ========================================================= */

void log_event(const char *format, ...)
{
    FILE *fp;

    time_t now;

    struct tm time_info;

    char timestamp[64];

    pthread_mutex_lock(&log_mutex);


    fp = fopen(LOG_FILE, "a");

    if (fp == NULL)
    {
        perror("log fopen");

        pthread_mutex_unlock(&log_mutex);

        return;
    }


    time(&now);

    localtime_r(&now,
                &time_info);


    strftime(timestamp,
             sizeof(timestamp),
             "%Y-%m-%d %H:%M:%S",
             &time_info);


    fprintf(fp,
            "[%s] ",
            timestamp);


    va_list args;

    va_start(args,
             format);

    vfprintf(fp,
             format,
             args);

    va_end(args);


    fprintf(fp,
            "\n");


    fclose(fp);


    pthread_mutex_unlock(&log_mutex);
}


/* =========================================================
   TCP HELPERS
   ========================================================= */

ssize_t send_all(int socket_fd,
                 const void *buffer,
                 size_t length)
{
    size_t total_sent = 0;

    const char *data =
        (const char *)buffer;


    while (total_sent < length)
    {
        ssize_t sent =
            send(socket_fd,
                 data + total_sent,
                 length - total_sent,
                 0);

        if (sent <= 0)
        {
            return -1;
        }

        total_sent +=
            (size_t)sent;
    }

    return (ssize_t)total_sent;
}


ssize_t recv_exact(int socket_fd,
                   void *buffer,
                   size_t length)
{
    size_t total_received = 0;

    char *data =
        (char *)buffer;


    while (total_received < length)
    {
        ssize_t received =
            recv(socket_fd,
                 data + total_received,
                 length - total_received,
                 0);

        if (received <= 0)
        {
            return received;
        }

        total_received +=
            (size_t)received;
    }

    return (ssize_t)total_received;
}


ssize_t recv_line(int socket_fd,
                  char *buffer,
                  size_t max_size)
{
    size_t position = 0;


    while (position < max_size - 1)
    {
        char ch;

        ssize_t received =
            recv(socket_fd,
                 &ch,
                 1,
                 0);

        if (received == 0)
        {
            return 0;
        }

        if (received < 0)
        {
            return -1;
        }

        buffer[position++] =
            ch;

        if (ch == '\n')
        {
            break;
        }
    }


    buffer[position] =
        '\0';

    return (ssize_t)position;
}


/* =========================================================
   CLIENT MANAGEMENT
   ========================================================= */

int find_free_client_slot()
{
    for (int i = 0;
         i < MAX_CLIENTS;
         i++)
    {
        if (!clients[i].active)
        {
            return i;
        }
    }

    return -1;
}


int username_exists(const char *username)
{
    for (int i = 0;
         i < MAX_CLIENTS;
         i++)
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


int get_user_socket(const char *username)
{
    int socket_fd = -1;


    pthread_mutex_lock(&clients_mutex);


    for (int i = 0;
         i < MAX_CLIENTS;
         i++)
    {
        if (clients[i].active &&
            strcmp(clients[i].username,
                   username) == 0)
        {
            socket_fd =
                clients[i].socket;

            break;
        }
    }


    pthread_mutex_unlock(&clients_mutex);

    return socket_fd;
}


void remove_client(int socket)
{
    pthread_mutex_lock(&clients_mutex);


    for (int i = 0;
         i < MAX_CLIENTS;
         i++)
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


/* =========================================================
   ROOM MANAGEMENT
   ========================================================= */

int find_room(const char *room_name)
{
    for (int i = 0;
         i < MAX_ROOMS;
         i++)
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
    for (int i = 0;
         i < MAX_ROOMS;
         i++)
    {
        if (!rooms[i].active)
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


    for (int r = 0;
         r < MAX_ROOMS;
         r++)
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
                     j <
                     rooms[r].member_count - 1;
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


/* =========================================================
   LIST USERS
   ========================================================= */

void handle_list(int client_socket)
{
    char response[1024];


    strcpy(response,
           "OK USERS ");


    pthread_mutex_lock(&clients_mutex);


    int first = 1;


    for (int i = 0;
         i < MAX_CLIENTS;
         i++)
    {
        if (clients[i].active)
        {
            if (!first)
            {
                strcat(response,
                       ",");
            }

            strcat(response,
                   clients[i].username);

            first = 0;
        }
    }


    pthread_mutex_unlock(&clients_mutex);


    strcat(response,
           " NID:6344\n");


    send_all(client_socket,
             response,
             strlen(response));
}


/* =========================================================
   BROADCAST
   ========================================================= */

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


    for (int i = 0;
         i < MAX_CLIENTS;
         i++)
    {
        if (clients[i].active &&
            clients[i].socket != sender_socket)
        {
            send_all(clients[i].socket,
                     outgoing,
                     strlen(outgoing));
        }
    }


    pthread_mutex_unlock(&clients_mutex);


    char *response =
        "OK SENT NID:6344\n";


    send_all(sender_socket,
             response,
             strlen(response));


    log_event("BCAST sender=%s message=%s",
              sender_username,
              message);
}


/* =========================================================
   PRIVATE MESSAGE
   ========================================================= */

void handle_pmsg(int sender_socket,
                 const char *sender_username,
                 const char *target_username,
                 const char *message)
{
    int target_socket =
        get_user_socket(target_username);


    if (target_socket == -1)
    {
        char *error =
            "ERR 002 USER_NOT_FOUND NID:6344\n";


        send_all(sender_socket,
                 error,
                 strlen(error));


        log_event("PMSG_FAILED sender=%s target=%s reason=USER_NOT_FOUND",
                  sender_username,
                  target_username);


        return;
    }


    char outgoing[1200];


    snprintf(outgoing,
             sizeof(outgoing),
             "MSG PRIV %s %s\n",
             sender_username,
             message);


    send_all(target_socket,
             outgoing,
             strlen(outgoing));


    char *response =
        "OK SENT NID:6344\n";


    send_all(sender_socket,
             response,
             strlen(response));


    log_event("PMSG sender=%s target=%s message=%s",
              sender_username,
              target_username,
              message);
}


/* =========================================================
   JOIN ROOM
   ========================================================= */

void handle_join(int client_socket,
                 const char *username,
                 const char *room_name)
{
    pthread_mutex_lock(&rooms_mutex);


    int room_index =
        find_room(room_name);


    if (room_index == -1)
    {
        room_index =
            find_free_room_slot();


        if (room_index == -1)
        {
            pthread_mutex_unlock(&rooms_mutex);


            char *error =
                "ERR 009 ROOM_LIMIT_REACHED NID:6344\n";


            send_all(client_socket,
                     error,
                     strlen(error));


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


    send_all(client_socket,
             response,
             strlen(response));


    log_event("JOIN username=%s room=%s",
              username,
              room_name);
}


/* =========================================================
   LEAVE ROOM
   ========================================================= */

void handle_leave(int client_socket,
                  const char *username,
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


        send_all(client_socket,
                 error,
                 strlen(error));


        log_event("LEAVE_FAILED username=%s room=%s reason=ROOM_NOT_FOUND",
                  username,
                  room_name);


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


    send_all(client_socket,
             response,
             strlen(response));


    log_event("LEAVE username=%s room=%s",
              username,
              room_name);
}


/* =========================================================
   LIST ROOMS
   ========================================================= */

void handle_rooms(int client_socket)
{
    char response[1024];


    strcpy(response,
           "OK ROOMS ");


    pthread_mutex_lock(&rooms_mutex);


    int first = 1;


    for (int i = 0;
         i < MAX_ROOMS;
         i++)
    {
        if (rooms[i].active)
        {
            if (!first)
            {
                strcat(response,
                       ",");
            }


            strcat(response,
                   rooms[i].name);


            first = 0;
        }
    }


    pthread_mutex_unlock(&rooms_mutex);


    strcat(response,
           " NID:6344\n");


    send_all(client_socket,
             response,
             strlen(response));
}


/* =========================================================
   ROOM MESSAGE
   ========================================================= */

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


        send_all(sender_socket,
                 error,
                 strlen(error));


        log_event("RMSG_FAILED sender=%s room=%s reason=ROOM_NOT_FOUND",
                  sender_username,
                  room_name);


        return;
    }


    char outgoing[1200];


    snprintf(outgoing,
             sizeof(outgoing),
             "MSG ROOM %s %s %s\n",
             room_name,
             sender_username,
             message);


    for (int i = 0;
         i < rooms[room_index].member_count;
         i++)
    {
        int target_socket =
            rooms[room_index]
                .member_sockets[i];


        send_all(target_socket,
                 outgoing,
                 strlen(outgoing));
    }


    pthread_mutex_unlock(&rooms_mutex);


    char *response =
        "OK SENT NID:6344\n";


    send_all(sender_socket,
             response,
             strlen(response));


    log_event("RMSG sender=%s room=%s message=%s",
              sender_username,
              room_name,
              message);
}


/* =========================================================
   FILE STORAGE
   ========================================================= */

int create_storage_path(const char *sender_username,
                        const char *filename,
                        char *output,
                        size_t output_size)
{
    if (strstr(filename,
               "..") != NULL ||
        strchr(filename,
               '/') != NULL)
    {
        return -1;
    }


    mkdir("storage",
          0755);


    mkdir("storage/IT23634480",
          0755);


    char sender_directory[512];


    snprintf(sender_directory,
             sizeof(sender_directory),
             "storage/IT23634480/%s",
             sender_username);


    mkdir(sender_directory,
          0755);


    snprintf(output,
             output_size,
             "%s/%s",
             sender_directory,
             filename);


    return 0;
}


/* =========================================================
   SENDFILE
   ========================================================= */

void handle_sendfile(int sender_socket,
                     const char *sender_username,
                     const char *header)
{
    char target[100];
    char filename[256];
    long filesize;


    if (sscanf(header,
               "SENDFILE %99s %255s %ld",
               target,
               filename,
               &filesize) != 3)
    {
        char *error =
            "ERR 007 INVALID_FORMAT NID:6344\n";


        send_all(sender_socket,
                 error,
                 strlen(error));


        return;
    }


    if (filesize < 0 ||
        filesize > MAX_FILE_SIZE)
    {
        char *error =
            "ERR 004 FILE_TOO_LARGE NID:6344\n";


        send_all(sender_socket,
                 error,
                 strlen(error));


        log_event("FILE_FAILED sender=%s target=%s filename=%s reason=FILE_TOO_LARGE",
                  sender_username,
                  target,
                  filename);


        return;
    }


    int target_socket =
        get_user_socket(target);


    int room_index =
        -1;


    if (target_socket == -1)
    {
        pthread_mutex_lock(&rooms_mutex);

        room_index =
            find_room(target);

        pthread_mutex_unlock(&rooms_mutex);
    }


    if (target_socket == -1 &&
        room_index == -1)
    {
        char *error =
            "ERR 002 USER_NOT_FOUND NID:6344\n";


        send_all(sender_socket,
                 error,
                 strlen(error));


        log_event("FILE_FAILED sender=%s target=%s filename=%s reason=TARGET_NOT_FOUND",
                  sender_username,
                  target,
                  filename);


        return;
    }


    char *file_data = NULL;


    if (filesize > 0)
    {
        file_data =
            malloc((size_t)filesize);


        if (file_data == NULL)
        {
            char *error =
                "ERR 010 SERVER_ERROR NID:6344\n";


            send_all(sender_socket,
                     error,
                     strlen(error));


            return;
        }


        ssize_t received =
            recv_exact(sender_socket,
                       file_data,
                       (size_t)filesize);


        if (received != filesize)
        {
            printf("Incomplete file received from %s.\n",
                   sender_username);


            log_event("FILE_FAILED sender=%s target=%s filename=%s reason=INCOMPLETE_TRANSFER",
                      sender_username,
                      target,
                      filename);


            free(file_data);


            return;
        }
    }


    char storage_path[1024];


    if (create_storage_path(sender_username,
                            filename,
                            storage_path,
                            sizeof(storage_path)) != 0)
    {
        char *error =
            "ERR 007 INVALID_FILENAME NID:6344\n";


        send_all(sender_socket,
                 error,
                 strlen(error));


        free(file_data);


        return;
    }


    FILE *fp =
        fopen(storage_path,
              "wb");


    if (fp == NULL)
    {
        perror("fopen");


        char *error =
            "ERR 010 SERVER_ERROR NID:6344\n";


        send_all(sender_socket,
                 error,
                 strlen(error));


        free(file_data);


        return;
    }


    if (filesize > 0)
    {
        fwrite(file_data,
               1,
               (size_t)filesize,
               fp);
    }


    fclose(fp);


    printf("Stored file: %s (%ld bytes)\n",
           storage_path,
           filesize);


    char outgoing_header[512];


    snprintf(outgoing_header,
             sizeof(outgoing_header),
             "SENDFILE %s %s %ld\n",
             target,
             filename,
             filesize);


    if (target_socket != -1)
    {
        send_all(target_socket,
                 outgoing_header,
                 strlen(outgoing_header));


        if (filesize > 0)
        {
            send_all(target_socket,
                     file_data,
                     (size_t)filesize);
        }
    }
    else
    {
        int member_sockets[MAX_CLIENTS];

        int member_count = 0;


        pthread_mutex_lock(&rooms_mutex);


        for (int i = 0;
             i < rooms[room_index].member_count;
             i++)
        {
            int member_socket =
                rooms[room_index]
                    .member_sockets[i];


            if (member_socket !=
                sender_socket)
            {
                member_sockets[member_count++] =
                    member_socket;
            }
        }


        pthread_mutex_unlock(&rooms_mutex);


        for (int i = 0;
             i < member_count;
             i++)
        {
            send_all(member_sockets[i],
                     outgoing_header,
                     strlen(outgoing_header));


            if (filesize > 0)
            {
                send_all(member_sockets[i],
                         file_data,
                         (size_t)filesize);
            }
        }
    }


    char response[512];


    snprintf(response,
             sizeof(response),
             "OK FILE_RECEIVED %s NID:6344\n",
             filename);


    send_all(sender_socket,
             response,
             strlen(response));


    log_event("FILE sender=%s target=%s filename=%s size=%ld stored=%s",
              sender_username,
              target,
              filename,
              filesize,
              storage_path);


    free(file_data);
}


/* =========================================================
   CLIENT THREAD
   ========================================================= */

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


    ssize_t bytes_received =
        recv_line(client_socket,
                  buffer,
                  sizeof(buffer));


    if (bytes_received <= 0)
    {
        printf("Client disconnected before registration.\n");


        log_event("DISCONNECT_BEFORE_REGISTER socket=%d",
                  client_socket);


        close(client_socket);


        return NULL;
    }


    printf("Received: %s",
           buffer);


    if (sscanf(buffer,
               "REGISTER %49s",
               username) != 1)
    {
        char *error =
            "ERR 005 INVALID_COMMAND NID:6344\n";


        send_all(client_socket,
                 error,
                 strlen(error));


        log_event("REGISTER_FAILED socket=%d reason=INVALID_COMMAND",
                  client_socket);


        close(client_socket);


        return NULL;
    }


    pthread_mutex_lock(&clients_mutex);


    if (username_exists(username))
    {
        pthread_mutex_unlock(&clients_mutex);


        char *error =
            "ERR 001 USERNAME_TAKEN NID:6344\n";


        send_all(client_socket,
                 error,
                 strlen(error));


        printf("Registration rejected. Username already exists: %s\n",
               username);


        log_event("REGISTER_FAILED username=%s reason=USERNAME_TAKEN",
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


        send_all(client_socket,
                 error,
                 strlen(error));


        log_event("REGISTER_FAILED username=%s reason=SERVER_FULL",
                  username);


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


    send_all(client_socket,
             response,
             strlen(response));


    printf("User registered: %s\n",
           username);


    printf("Client %s is now connected.\n",
           username);


    log_event("REGISTER username=%s socket=%d",
              username,
              client_socket);


    /* ---------------- COMMAND LOOP ---------------- */

    while (1)
    {
        memset(buffer,
               0,
               sizeof(buffer));


        bytes_received =
            recv_line(client_socket,
                      buffer,
                      sizeof(buffer));


        if (bytes_received == 0)
        {
            printf("Client %s disconnected.\n",
                   username);


            log_event("DISCONNECT username=%s",
                      username);


            break;
        }


        if (bytes_received < 0)
        {
            perror("recv_line");


            log_event("DISCONNECT username=%s reason=RECV_ERROR",
                      username);


            break;
        }


        printf("Command from %s: %s",
               username,
               buffer);


        if (strcmp(buffer,
                   "LIST\n") == 0)
        {
            handle_list(client_socket);
        }


        else if (strcmp(buffer,
                        "ROOMS\n") == 0)
        {
            handle_rooms(client_socket);
        }


        else if (strncmp(buffer,
                         "BCAST ",
                         6) == 0)
        {
            char message[900];


            strcpy(message,
                   buffer + 6);


            message[
                strcspn(message,
                        "\n")
            ] = '\0';


            if (strlen(message) == 0)
            {
                char *error =
                    "ERR 007 INVALID_FORMAT NID:6344\n";


                send_all(client_socket,
                         error,
                         strlen(error));
            }
            else
            {
                handle_bcast(client_socket,
                             username,
                             message);
            }
        }


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


                send_all(client_socket,
                         error,
                         strlen(error));
            }
        }


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
                            username,
                            room_name);
            }
            else
            {
                char *error =
                    "ERR 007 INVALID_FORMAT NID:6344\n";


                send_all(client_socket,
                         error,
                         strlen(error));
            }
        }


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
                             username,
                             room_name);
            }
            else
            {
                char *error =
                    "ERR 007 INVALID_FORMAT NID:6344\n";


                send_all(client_socket,
                         error,
                         strlen(error));
            }
        }


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


                send_all(client_socket,
                         error,
                         strlen(error));
            }
        }


        else if (strncmp(buffer,
                         "SENDFILE ",
                         9) == 0)
        {
            handle_sendfile(client_socket,
                            username,
                            buffer);
        }


        else if (strcmp(buffer,
                        "QUIT\n") == 0)
        {
            char *bye =
                "OK BYE NID:6344\n";


            send_all(client_socket,
                     bye,
                     strlen(bye));


            printf("Client %s requested QUIT.\n",
                   username);


            log_event("QUIT username=%s",
                      username);


            break;
        }


        else
        {
            char *error =
                "ERR 005 INVALID_COMMAND NID:6344\n";


            send_all(client_socket,
                     error,
                     strlen(error));


            log_event("INVALID_COMMAND username=%s command=%s",
                      username,
                      buffer);
        }
    }


    remove_client_from_all_rooms(
        client_socket);


    remove_client(
        client_socket);


    close(
        client_socket);


    return NULL;
}


/* =========================================================
   MAIN
   ========================================================= */

int main()
{
    int server_socket;

    struct sockaddr_in server_addr;


    for (int i = 0;
         i < MAX_CLIENTS;
         i++)
    {
        clients[i].socket = -1;

        clients[i].active = 0;

        clients[i].username[0] = '\0';
    }


    for (int i = 0;
         i < MAX_ROOMS;
         i++)
    {
        rooms[i].active = 0;

        rooms[i].member_count = 0;

        rooms[i].name[0] = '\0';
    }


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


    memset(&server_addr,
           0,
           sizeof(server_addr));


    server_addr.sin_family =
        AF_INET;


    server_addr.sin_addr.s_addr =
        INADDR_ANY;


    server_addr.sin_port =
        htons(PORT);


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
    printf(" Port         : %d\n",
           PORT);
    printf(" NID          : 6344\n");
    printf(" Log File     : %s\n",
           LOG_FILE);
    printf("==============================\n");
    printf("Waiting for clients...\n");


    log_event("SERVER_START port=%d NID=6344",
              PORT);


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


        log_event("CONNECT socket=%d",
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


        pthread_detach(
            thread_id);
    }


    close(
        server_socket);


    return 0;
}
