#if __unix__
#pragma once
#include <tcpUtils/Socket.h>
#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

namespace tcp::impl
{
	class SocketImpl
	{
	public:
		SocketImpl();
		~SocketImpl();

		void Connect(std::string host, uint16_t port, SocketOptions options);

		void Send(const void* data, size_t dataLength);
		size_t Receive(void* toData, size_t dataLength);

	private:

		void ConnectTls(std::string host, uint16_t port, SocketOptions options);
		void SendTls(const void* data, size_t dataLength);
		size_t ReceiveTls(void* toData, size_t dataLength);

		SSL_CTX* sslContext = nullptr;
		SSL* ssl = nullptr;
		BIO* sslBasicIO = nullptr;
		int socketFd = 0;
		bool useTls = false;
	};
}
#endif
