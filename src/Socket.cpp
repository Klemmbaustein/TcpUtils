#include "tcpUtils/Socket.h"
#include "impl/Socket_Windows.h"
#include "impl/Socket_Unix.h"

tcp::Socket::Socket(std::string host, uint16_t port, SocketOptions options)
{
	implementation = new impl::SocketImpl();

	implementation->Connect(host, port, options);
}

tcp::Socket::~Socket()
{
	delete implementation;
}

void tcp::Socket::Send(const void* data, size_t dataLength)
{
	implementation->Send(data, dataLength);
}

size_t tcp::Socket::Receive(void* toData, size_t dataLength)
{
	return implementation->Receive(toData, dataLength);
}
