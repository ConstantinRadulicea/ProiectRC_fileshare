#include "fileshare_client.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#define SERVER_PORT 8081
#define SERVER_IP "127.0.0.1"

int client_main() {
    FILE* fptr;
    int valread, client_fd;
    struct sockaddr_in serv_addr;
    char* hello = "Hello from client";
    char user_inbut_buf[1024] = { 0 };
    int32_t status, file_size, bytes_readden;
    client_command_t command;
    ssize_t status_2;

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    // Convert IPv4 and IPv6 addresses from text to binary
    // form
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if ((status = connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    while (1)
    {
        printf(">");
        fgets(user_inbut_buf, 1024, stdin);
        user_inbut_buf[strlen(user_inbut_buf) - 1] = '\0';
        command_parser(&command, user_inbut_buf, strlen(user_inbut_buf));


        if (strncasecmp(command.name_ptr, CLIENT_COMMAND_GET_STR, command.name_size) == 0) {
            send(client_fd, user_inbut_buf, strlen(user_inbut_buf), 0);
            send(client_fd, CLIENT_COMMAND_TERMINATOR, strlen(CLIENT_COMMAND_TERMINATOR), 0);

            valread = read(client_fd, &status, sizeof(int32_t));
            status = ntohl(status);
            if (status == FILESHARE_SUCCESS)
            {
                valread = read(client_fd, &file_size, sizeof(uint32_t));
                file_size = ntohl(file_size);

                fptr = fopen(command.parameters_ptr, "w");
                if (fptr == NULL) {
                    printf("cant create file\n");
                    exit(1);
                }

                bytes_readden = 0;
                while (bytes_readden < file_size) {
                    valread = read(client_fd, user_inbut_buf, 1024);
                    if (valread < 0) {
                        exit(2);
                    }
                    bytes_readden += valread;
                    status_2 = fwrite(user_inbut_buf, sizeof(user_inbut_buf[0]), valread, fptr);
                    //status_2 = write(fptr, user_inbut_buf, valread);
                    if (status_2 < 0)
                    {
                        printf("ERROR file write: [%d] %s\n", errno, strerror(errno));
                    }
                }
                fclose(fptr);
            }
            else {
                valread = read(client_fd, &file_size, sizeof(uint32_t));
                file_size = ntohl(file_size);
                valread = read(client_fd, user_inbut_buf, 1024);
                user_inbut_buf[valread] = '\0';
                printf("Server error: %s\n", user_inbut_buf);
            }
        }
        else if (strncasecmp(command.name_ptr, CLIENT_COMMAND_LS_STR, command.name_size) == 0) {
            strcat(user_inbut_buf, CLIENT_COMMAND_TERMINATOR);
            status = send(client_fd, user_inbut_buf, strlen(user_inbut_buf), 0);

                while (1) {
                    valread = read(client_fd, user_inbut_buf, 1024);
                    if (valread < 0) {
                        exit(2);
                    }
                    user_inbut_buf[valread] == '\0';
                    printf("%s", user_inbut_buf);
                    if (user_inbut_buf[valread - 1] == '\0') {
                        break;
                    }
                }
            }
            else {
                printf("Invalid command!\n");
            }
        }
        // closing the connected socket
        close(client_fd);
        return 0;
}