#define _DEFAULT_SOURCE
#include "fileshare_server.h"

int setnonblocking(int sockfd) {
	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags == -1) {
		return -1;
	}

	if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
		return -1;
	}
	return 0;
}

int listen_inet_socket(int portnum, int accept_backlog) {
	int sockfd, opt, res;
	struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		return -1;
	}

	// This helps avoid spurious EADDRINUSE when the previous instance of this
	// server died.
	opt = 1;
	res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (res < 0) {
		return -1;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portnum);

	if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		return -1;
	}

	if (listen(sockfd, accept_backlog) < 0) {
		return -1;
	}

	return sockfd;
}

int server_init(fileshare_server_t *server, char* shared_directory_path) {
	size_t filepath_len;
	char* filepath;

	server->shared_directory_path = NULL;
	server->socket = -1;

	filepath_len = strlen(shared_directory_path);
	filepath = (char*)malloc(filepath_len+1);
	if (filepath == NULL) {
		return FILESHARE_ERROR_MALLOC;
	}
	strncpy(filepath, shared_directory_path, filepath_len);
	filepath[filepath_len] = '\0';

	server->shared_directory_path = filepath;
	return FILESHARE_SUCCESS;
}

int server_start(fileshare_server_t* server, int port, int accept_backlog) {
	int temp_socket;

	temp_socket = listen_inet_socket(port, accept_backlog);
	if (temp_socket < 0) {
		return FILESHARE_ERROR_INVALID_SOCKET;
	}
	server->socket = temp_socket;
	return FILESHARE_SUCCESS;
}

int server_accept(fileshare_server_t* server, fileshare_client_t *client) {
	int newsockfd;
	struct sockaddr_in peer_addr;
	socklen_t peer_addr_len;

	memset(client, 0, sizeof(fileshare_client_t));

	newsockfd = accept(server->socket, (struct sockaddr*)&peer_addr, &peer_addr_len);
	if (newsockfd < 0) {
		return FILESHARE_ERROR_INVALID_SOCKET;
	}

	client->socket = newsockfd;
	client->sockaddr = peer_addr;

	return FILESHARE_SUCCESS;
}

void close_client(fileshare_client_t* client) {
	close(client->socket);
}

int server_recv_client_command(fileshare_server_t* server, fileshare_client_t* client, client_command_t *command) {
	char* buffer;
	char* terminator_ptr;
	size_t recv_size;
	ssize_t recv_size_last;

	recv_size = 0;

	buffer = (char*)malloc(CLIENT_BUF_SIZE);
	if (buffer == NULL) {
		close_client(client);
		return FILESHARE_ERROR_MALLOC;
	}

	for (;;)
	{
		recv_size_last = recv(client->socket, &(buffer[recv_size]), CLIENT_BUF_SIZE - recv_size, 0);
		if (recv_size < 0) {
			free(buffer);
			close_client(client);
			return FILESHARE_ERROR_RECV;
		}
		else if (recv_size_last == 0 && recv_size_last < CLIENT_BUF_SIZE) {
			free(buffer);
			close_client(client);
			return FILESHARE_ERROR_CONNECTION_CLOSED;
		}
		else if (recv_size_last == 0 && recv_size_last >= CLIENT_BUF_SIZE) {
			free(buffer);
			close_client(client);
			return FILESHARE_ERROR_RECV_BUFFER_FULL;
		}
		terminator_ptr = strstr(&(buffer[recv_size]), CLIENT_COMMAND_TERMINATOR);
		if (terminator_ptr != NULL) {
			*terminator_ptr = '\0';
			break;
		}
		recv_size += recv_size_last;
	}

	printf("%s", buffer);

	command_parser(command, buffer, (size_t)terminator_ptr - (size_t)buffer);

	return FILESHARE_SUCCESS;
}


void command_parser(client_command_t* command, char* msg, size_t msg_len) {
	command->msg = msg;
	command->msg_size = msg_len;
	command->name_ptr = msg;
	command->parameters_ptr = strchr(msg, ' ');

	if (command->parameters_ptr != NULL) {
		command->name_size = (size_t)(command->parameters_ptr) - (size_t)command->msg;
		command->parameters_size = command->msg_size - (size_t)1;

		command->parameters_ptr += 1;
		if (command->parameters_ptr == &(command->msg[msg_len])) {
			command->parameters_ptr = NULL;
		}
	}
	else {
		command->name_size = command->msg_size;
		command->parameters_size = 0;
	}
}

int command_ls_handle(fileshare_server_t* server, fileshare_client_t* client, client_command_t* command) {
	struct dirent** namelist;
	int n;

	n = scandir(server->shared_directory_path, &namelist, NULL, alphasort);
	if (n == -1) {
		return FILESHARE_ERROR_SCANDIR;
	}

	while (n--) {
		send(client->socket, namelist[n]->d_name, strlen(namelist[n]->d_name), 0);
		send(client->socket, CLIENT_COMMAND_TERMINATOR, strlen(CLIENT_COMMAND_TERMINATOR), 0);
		free(namelist[n]);
	}
	send(client->socket, "\0", 1, 0);
	free(namelist);
	return FILESHARE_SUCCESS;
}

int command_get_handle(fileshare_server_t* server, fileshare_client_t* client, client_command_t* command) {
	unsigned char* f;
	int size;
	struct stat s;
	int fd;
	int status;
	uint8_t tempStatus;

	char filepath[512];

	strcpy(filepath, server->shared_directory_path);
	strcat(filepath, "/");
	if (command->parameters_ptr == NULL) {
		send_to_client(client->socket, FILESHARE_ERROR_INVALID_COMMAND_PARAMETER, "Invalid parameters!\n", 21);
		return FILESHARE_ERROR_INVALID_COMMAND_PARAMETER;
	}
	strcat(filepath, command->parameters_ptr);


	fd = open(filepath, O_RDONLY);
	if (fd == -1) {
		send_to_client(client->socket, FILESHARE_ERROR_FILE, "File error!\n", 13);
		return FILESHARE_ERROR_FILE;
	}


	/* Get the size of the file. */
	status = fstat(fd, &s);
	size = s.st_size;

	f = (char*)mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (f == MAP_FAILED) {
		send_to_client(client->socket, FILESHARE_ERROR_MMAP, "Server error!\n", 15);
		return FILESHARE_ERROR_MMAP;
	}
	send_to_client(client->socket, FILESHARE_SUCCESS, f, s.st_size);
	munmap(f, s.st_size);
	return FILESHARE_SUCCESS;
}

int server_process_client(fileshare_server_t* server, fileshare_client_t* client) {
	client_command_t command;
	int returnValue;

	for (;;)
	{
		returnValue = server_recv_client_command(server, client, &command);
		if (returnValue != FILESHARE_SUCCESS) {
			if (returnValue == FILESHARE_ERROR_CONNECTION_CLOSED)
			{
				close_client(client);
				return FILESHARE_ERROR_CONNECTION_CLOSED;
			}
		}

		if (strncasecmp(command.name_ptr, CLIENT_COMMAND_GET_STR, command.name_size) == 0) {
			command_get_handle(server, client, &command);
		}
		else if (strncasecmp(command.name_ptr, CLIENT_COMMAND_LS_STR, command.name_size) == 0) {
			command_ls_handle(server, client, &command);
		}
		else {
			send(client->socket, "Invalid command!\n", 18, 0);
		}

		free(command.msg);
	}
}


int send_to_client(int socket, int32_t status, void* buffer, uint32_t buffer_size) {
	uint32_t temp_u32;
	status = htonl(status);
	send(socket, &status, sizeof(status), 0);
	temp_u32 = htonl(buffer_size);
	send(socket, &temp_u32, sizeof(temp_u32), 0);
	send(socket, buffer, buffer_size, 0);
}

void server_clear(fileshare_server_t* server) {
	free(server->shared_directory_path);
}

void server_run(fileshare_server_t* server) {
	fileshare_client_t new_client;


	for (;;) {
		if (server_accept(server, &new_client) != FILESHARE_SUCCESS) {
			printf("ERROR: error in server_accept()\n");
			continue;
		}

		server_process_client(server, &new_client);
	}
}


int server_main() {
	fileshare_server_t server;
	char fileshare_path[] = "/";
	
	server_init(&server, fileshare_path);
	server_start(&server, 8081, 2);
	
	server_run(&server);

	server_clear(&server);
	return 0;
}