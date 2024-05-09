#ifndef __FILESHARE_CLIENT_H__
#define __FILESHARE_CLIENT_H__

#include "fileshare_server.h"

// returns a socket
int client_connect(const char* address, int port);
int client_main();


#endif // !__FILESHARE_CLIENT_H__

