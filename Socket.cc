#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>

#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

Socket::~Socket()
{
    close(sockfd_);
}

void Socket::bindAddress(const InetAddress& localaddr)
{
    if (0 != ::bind(sockfd_, (sockaddr*)localaddr.getSocketAddr(), sizeof(sockaddr_in)))
    {
        LOG_FATAL("%s %s %d bind socket fd %d failed.\n", __FILENAME__, __FUNCTION__, __LINE__, sockfd_);
    }
}

void Socket::listen()
{
    if (0 != ::listen(sockfd_, 1024))
    {
        LOG_FATAL("%s %s %d listen socket fd %d failed.\n", __FILENAME__, __FUNCTION__, __LINE__, sockfd_);
    }
}

int Socket::accept(InetAddress* peerAddr)
{
    struct sockaddr_in addr;
    socklen_t len;
    bzero(&addr, sizeof(addr));
    int connfd = ::accept(sockfd_, (sockaddr*)&addr, &len);
    if (connfd >= 0)
    {
        peerAddr->setSocketAddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    if(::shutdown(sockfd_, SHUT_WR))
    {
        LOG_ERROR("%s %s %d shutdown socket fd %d failed.\n", __FILENAME__, __FUNCTION__, __LINE__, sockfd_);
    }
}

void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}