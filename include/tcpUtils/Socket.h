#pragma once
#include <stdexcept>
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

	class SocketException : public std::exception
	{
	public:

		SocketException(std::string message)
			: error(message)
		{

		}

		const char* what() const noexcept override
		{
			return error.c_str();
		}

	private:
		std::string error;
	};
}
