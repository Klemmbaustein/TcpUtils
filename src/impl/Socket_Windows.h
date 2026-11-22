#if _WIN32
#pragma once
#define SECURITY_WIN32
#include <tcpUtils/Socket.h>
#include <WinSock2.h>
#include <security.h>

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

		void InitializeTls(std::string host);

		void SendTls(const void* data, size_t dataLength);
		size_t ReceiveTls(void* data, size_t dataLength);

		SecPkgContext_StreamSizes sizes{};

		bool useTls = false;

		CredHandle credentials{};
		CtxtHandle context{};
		SOCKET connectionSocket = INVALID_SOCKET;

		size_t decryptedLength = 0;
		size_t received = 0;
		size_t usedSize = 0;
		uint8_t* decryptedBuffer = nullptr;

		static constexpr int TLS_MAX_PACKET_SIZE = 16384 + 512;

		uint8_t incoming[TLS_MAX_PACKET_SIZE];
	};
}
#endif