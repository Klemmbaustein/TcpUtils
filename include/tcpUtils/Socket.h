#pragma once
#include <tcpUtils/NetworkException.h>
#include <string>
#include <cstdint>

namespace tcp
{
	namespace impl
	{
		class SocketImpl;
	}

	struct SocketOptions
	{
		bool useTls : 1 = true;
	};

	class Socket
	{
	public:
		Socket(std::string host, uint16_t port, SocketOptions options);

		~Socket();

		void Send(const void* data, size_t dataLength);
		size_t Receive(void* toData, size_t dataLength);

	private:
		impl::SocketImpl* implementation = nullptr;
	};

	class SocketException : public NetworkException
	{
	public:
		SocketException(std::string message)
			: NetworkException(message)
		{

		}
	};
}
