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

	/**
	 * @brief
	 * Options used when connecting a TCP socket.
	 */
	struct SocketOptions
	{
		/**
		 * @brief
		 * Should the connection use transport layer security, using schannel on Windows and OpenSSL on Unix.
		 */
		bool useTls : 1 = true;
	};

	/**
	 * @brief
	 * TCP/IP socket implementation.
	 *
	 * This is the base of both the HTTP and WebSockets implementation.
	 */
	class Socket
	{
	public:
		/**
		 * @brief
		 * Connects to the given address.
		 * @param host
		 * The host to connect to. Example: localhost, www.w3c.org
		 * @param port
		 * The port to connect to. Example: 80, 443
		 * @param options
		 * Options for the socket connection.
		 *
		 * @throw SocketException
		 * Exceptions are thrown if any part of the connection fails.
		 */
		Socket(std::string host, uint16_t port, SocketOptions options);

		/**
		 * @brief
		 * Socket cannot be copied.
		 */
		Socket(const Socket&) = delete;
		/**
		 * @brief
		 * Socket can be moved.
		 */
		Socket(Socket&&) = default;

		~Socket();

		/**
		 * @brief
		 * Sends data over the socket.
		 *
		 * This function is blocking.
		 *
		 * @param data
		 * The data to send.
		 * @param dataLength
		 * The length of the data, in bytes.
		 *
		 * @throw SocketException
		 * Exceptions are thrown if sending the data fails.
		 */
		void Send(const void* data, size_t dataLength);
		/**
		 * @brief
		 * Receives data over the socket.
		 *
		 * @param toData
		 * The buffer to write the new data to.
		 * @param dataLength
		 * The length in bytes of the buffer the data is written to. The function will write this many bytes to this buffer,
		 * or less if the server sent less bytes.
		 * @return
		 * The number of bytes actually read.
		 *
		 * @throw SocketException
		 * Exceptions are thrown if an error occurs while attempting to send data.
		 */
		size_t Receive(void* toData, size_t dataLength);

	private:
		impl::SocketImpl* implementation = nullptr;
	};

	/**
	 * @brief
	 * An exception that occurred because of an error in a TCP socket.
	 */
	class SocketException : public NetworkException
	{
	public:
		SocketException(std::string message)
			: NetworkException(message)
		{

		}
	};
}
