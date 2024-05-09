#ifndef __FILESHARE_SERVER_H__
#define __FILESHARE_SERVER_H__

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <string.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>


#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define CLIENT_BUF_SIZE 512

#define FILESHARE_SUCCESS 0
#define FILESHARE_ERROR_MALLOC 1
#define FILESHARE_ERROR_INVALID_SOCKET -1
#define FILESHARE_ERROR_RECV 2
#define FILESHARE_ERROR_CONNECTION_CLOSED 3
#define FILESHARE_ERROR_RECV_BUFFER_FULL 4
#define FILESHARE_ERROR_INVALID_COMMAND 5
#define FILESHARE_ERROR_INVALID_COMMAND_PARAMETER 6
#define FILESHARE_ERROR_SCANDIR 7
#define FILESHARE_ERROR_MMAP 8
#define FILESHARE_ERROR_FILE 9


#define CLIENT_COMMAND_GET_STR "GET"
#define CLIENT_COMMAND_LS_STR "LS"
#define CLIENT_COMMAND_TERMINATOR "\r\n"


typedef struct fileshare_server_s {
	int socket;
	char* shared_directory_path;
}fileshare_server_t;

typedef struct fileshare_client_s {
	int socket;
	struct sockaddr_in sockaddr;
}fileshare_client_t;

typedef struct client_command_s {
	char* msg;
	size_t msg_size;
	char* name_ptr;
	size_t name_size;
	char* parameters_ptr;
	size_t parameters_size;
}client_command_t;

int send_to_client(int socket, int status, void* buffer, uint32_t buffer_size);

int setnonblocking(int sockfd);

int listen_inet_socket(int portnum, int accept_backlog);

int server_init(fileshare_server_t* server, char* shared_directory_path);

int server_start(fileshare_server_t* server, int port, int accept_backlog);

int server_accept(fileshare_server_t* server, fileshare_client_t* client);

void close_client(fileshare_client_t* client);

void command_parser(client_command_t* command, char* msg, size_t msg_len);

int server_recv_client_command(fileshare_server_t* server, fileshare_client_t* client, client_command_t* command);

int command_ls_handle(fileshare_server_t* server, fileshare_client_t* client, client_command_t* command);


int command_get_handle(fileshare_server_t* server, fileshare_client_t* client, client_command_t* command);

int server_process_client(fileshare_server_t* server, fileshare_client_t* client);

void server_clear(fileshare_server_t* server);

void server_run(fileshare_server_t* server);


int server_main();


#endif // !__FILESHARE_SERVER_H__
