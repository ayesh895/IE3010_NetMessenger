#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 10480
#define SERVER_IP "127.0.0.1"

#define BUFFER_SIZE 4096
#define MAX_FILE_SIZE (10 * 1024 * 1024)


/* =========================================================
   TCP HELPERS
   ========================================================= */

ssize_t send_all(int socket_fd,
                 const void *buffer,
                 size_t length)
{
    size_t total = 0;
    const char *data = buffer;

    while (total < length)
    {
        ssize_t sent =
            send(socket_fd,
                 data + total,
                 length - total,
                 0);

        if (sent <= 0)
        {
            return -1;
        }

        total += (size_t)sent;
    }

    return (ssize_t)total;
}


ssize_t recv_exact(int socket_fd,
                   void *buffer,
                   size_t length)
{
    size_t total = 0;
    char *data = buffer;

    while (total < length)
    {
        ssize_t received =
            recv(socket_fd,
                 data + total,
                 length - total,
                 0);

        if (received <= 0)
        {
            return received;
        }

        total += (size_t)received;
    }

    return (ssize_t)total;
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

        buffer[position++] = ch;

        if (ch == '\n')
        {
            break;
        }
    }

    buffer[position] = '\0';

    return (ssize_t)position;
}


/* =========================================================
   RECEIVE FILE
   ========================================================= */

void receive_file(int client_socket,
                  const char *filename,
                  long filesize)
{
    if (filesize < 0 ||
        filesize > MAX_FILE_SIZE)
    {
        printf("\nInvalid incoming file size.\n");
        return;
    }

    char *file_data = NULL;

    if (filesize > 0)
    {
        file_data =
            malloc((size_t)filesize);

        if (file_data == NULL)
        {
            printf("\nMemory allocation failed.\n");
            return;
        }

        ssize_t received =
            recv_exact(client_socket,
                       file_data,
                       (size_t)filesize);

        if (received != filesize)
        {
            printf("\nFile receive failed.\n");

            free(file_data);

            return;
        }
    }

    mkdir("received",
          0755);

    char path[512];

    snprintf(path,
             sizeof(path),
             "received/%s",
             filename);

    FILE *fp =
        fopen(path,
              "wb");

    if (fp == NULL)
    {
        perror("fopen");

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

    free(file_data);

    printf("\nFile received successfully: %s (%ld bytes)\n",
           path,
           filesize);
}


/* =========================================================
   RECEIVER THREAD
   ========================================================= */

void *receiver_thread(void *arg)
{
    int client_socket =
        *(int *)arg;

    char buffer[BUFFER_SIZE];

    while (1)
    {
        memset(buffer,
               0,
               sizeof(buffer));

        ssize_t bytes_received =
            recv_line(client_socket,
                      buffer,
                      sizeof(buffer));

        if (bytes_received <= 0)
        {
            printf("\nDisconnected from server.\n");
            break;
        }

        /*
         * Incoming file header
         */
        if (strncmp(buffer,
                    "SENDFILE ",
                    9) == 0)
        {
            char target[100];
            char filename[256];
            long filesize;

            if (sscanf(buffer,
                       "SENDFILE %99s %255s %ld",
                       target,
                       filename,
                       &filesize) == 3)
            {
                printf("\nIncoming file: %s (%ld bytes)\n",
                       filename,
                       filesize);

                receive_file(client_socket,
                             filename,
                             filesize);
            }
            else
            {
                printf("\nInvalid file header received.\n");
            }
        }
        else
        {
            printf("\n%s",
                   buffer);
        }

        printf("> ");
        fflush(stdout);
    }

    return NULL;
}


/* =========================================================
   SEND FILE
   ========================================================= */

int send_file_command(int client_socket,
                      const char *command)
{
    char target[100];
    char filename[256];
    long declared_size;

    if (sscanf(command,
               "SENDFILE %99s %255s %ld",
               target,
               filename,
               &declared_size) != 3)
    {
        printf("Usage: SENDFILE <target> <filename> <filesize>\n");

        return -1;
    }

    FILE *fp =
        fopen(filename,
              "rb");

    if (fp == NULL)
    {
        perror("fopen");

        return -1;
    }

    if (fseek(fp,
              0,
              SEEK_END) != 0)
    {
        perror("fseek");

        fclose(fp);

        return -1;
    }

    long actual_size =
        ftell(fp);

    rewind(fp);

    if (actual_size < 0)
    {
        printf("Unable to determine file size.\n");

        fclose(fp);

        return -1;
    }

    if (actual_size != declared_size)
    {
        printf("File size mismatch.\n");
        printf("Actual size   : %ld bytes\n",
               actual_size);
        printf("Declared size : %ld bytes\n",
               declared_size);

        fclose(fp);

        return -1;
    }

    if (actual_size > MAX_FILE_SIZE)
    {
        printf("File is too large.\n");

        fclose(fp);

        return -1;
    }

    char header[512];

    snprintf(header,
             sizeof(header),
             "SENDFILE %s %s %ld\n",
             target,
             filename,
             actual_size);

    if (send_all(client_socket,
                 header,
                 strlen(header)) < 0)
    {
        perror("send");

        fclose(fp);

        return -1;
    }

    char buffer[BUFFER_SIZE];

    long total_sent = 0;

    while (total_sent < actual_size)
    {
        size_t remaining =
            (size_t)(actual_size - total_sent);

        size_t chunk_size =
            remaining > sizeof(buffer)
                ? sizeof(buffer)
                : remaining;

        size_t read_count =
            fread(buffer,
                  1,
                  chunk_size,
                  fp);

        if (read_count == 0)
        {
            break;
        }

        if (send_all(client_socket,
                     buffer,
                     read_count) < 0)
        {
            perror("send");

            fclose(fp);

            return -1;
        }

        total_sent +=
            (long)read_count;
    }

    fclose(fp);

    if (total_sent != actual_size)
    {
        printf("File send incomplete.\n");

        return -1;
    }

    printf("File bytes sent: %ld\n",
           total_sent);

    return 0;
}


/* =========================================================
   MAIN
   ========================================================= */

int main()
{
    int client_socket;

    struct sockaddr_in server_addr;

    char username[50];
    char register_message[100];
    char response[512];


    /* Create TCP socket */

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


    /* Configure server address */

    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family =
        AF_INET;

    server_addr.sin_port =
        htons(PORT);


    if (inet_pton(AF_INET,
                  SERVER_IP,
                  &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");

        close(client_socket);

        return 1;
    }


    /* Connect */

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


    /* Registration */

    printf("Enter username: ");

    scanf("%49s",
          username);

    snprintf(register_message,
             sizeof(register_message),
             "REGISTER %s\n",
             username);

    if (send_all(client_socket,
                 register_message,
                 strlen(register_message)) < 0)
    {
        perror("send");

        close(client_socket);

        return 1;
    }

    memset(response,
           0,
           sizeof(response));

    ssize_t bytes_received =
        recv_line(client_socket,
                  response,
                  sizeof(response));

    if (bytes_received <= 0)
    {
        printf("No registration response.\n");

        close(client_socket);

        return 1;
    }

    printf("Server response: %s",
           response);

    if (strncmp(response,
                "ERR",
                3) == 0)
    {
        printf("Registration failed. Closing connection.\n");

        close(client_socket);

        return 1;
    }


    /* Start receiver thread */

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
     * Remove newline left by scanf()
     */
    getchar();


    /* Command help */

    char command[1024];

    printf("\nConnection active.\n");
    printf("Available commands:\n");
    printf("  LIST\n");
    printf("  BCAST <message>\n");
    printf("  PMSG <username> <message>\n");
    printf("  JOIN <room>\n");
    printf("  LEAVE <room>\n");
    printf("  ROOMS\n");
    printf("  RMSG <room> <message>\n");
    printf("  SENDFILE <target> <filename> <filesize>\n");
    printf("  QUIT\n");


    /* Main keyboard command loop */

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
         * File transfer command
         */
        if (strncmp(command,
                    "SENDFILE ",
                    9) == 0)
        {
            send_file_command(client_socket,
                              command);

            continue;
        }


        /*
         * Send normal protocol command
         */
        if (send_all(client_socket,
                     command,
                     strlen(command)) < 0)
        {
            perror("send");

            break;
        }


        /*
         * QUIT is a real server protocol command.
         * After sending it, do not accept more
         * keyboard commands.
         *
         * Give receiver thread a short moment
         * to display OK BYE before shutdown.
         */
        if (strcmp(command,
                   "QUIT\n") == 0)
        {
            sleep(1);

            break;
        }
    }


    /*
     * Close socket.
     */
    shutdown(client_socket,
             SHUT_RDWR);

    close(client_socket);


    printf("Client program ended.\n");


    return 0;
}
