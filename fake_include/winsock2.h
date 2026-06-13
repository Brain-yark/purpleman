#pragma once

// Minimal winsock2 stub for static checks
typedef int SOCKET;
#define INVALID_SOCKET (-1)

struct timeval { long tv_sec; long tv_usec; };

// fd_set stub
struct fd_set { int fd_count; SOCKET fd_array[64]; };

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, timeval* timeout);

int closesocket(SOCKET s);
int send(SOCKET s, const char* buf, int len, int flags);
int recv(SOCKET s, char* buf, int len, int flags);

