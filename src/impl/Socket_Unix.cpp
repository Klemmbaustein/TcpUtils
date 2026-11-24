#if __unix__
#include "Socket_Unix.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cstring>
#include <format>
#include <climits>

tcp::impl::SocketImpl::SocketImpl()
{
}

tcp::impl::SocketImpl::~SocketImpl()
{
	if (useTls)
	{
		if (sslBasicIO)
		{
			BIO_free_all(sslBasicIO);
		}
		if (sslContext)
		{
			SSL_CTX_free(sslContext);
		}
	}
	else if (socketFd)
	{
		close(socketFd);
	}
}

void tcp::impl::SocketImpl::Connect(std::string host, uint16_t port, SocketOptions options)
{
	if (options.useTls)
	{
		ConnectTls(host, port, options);
		return;
	}

	addrinfo* address;
	if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), nullptr, &address) != 0)
	{
		SocketException(std::format("Failed to get address information: {}:{}", host, port));
	}

	for (auto ptr = address; ptr != nullptr; ptr = ptr->ai_next)
	{
		int result = -2;

		switch (ptr->ai_family)
		{
		case AF_INET:
		case AF_INET6:
			socketFd = socket(ptr->ai_family, SOCK_STREAM, 0);
			result = connect(socketFd, ptr->ai_addr, ptr->ai_addrlen);
			break;
		default:
			continue;
		}
		if (result != -2)
		{
			break;
		}
	}

	freeaddrinfo(address);

	if (socketFd == 0)
	{
		throw SocketException(std::format("Could not find valid address for host: {}", host.c_str()));
	}

	if (socketFd < 0)
	{
		throw SocketException(std::format("Could not create socket: {}", strerror(errno)));
	}
}

void tcp::impl::SocketImpl::ConnectTls(std::string host, uint16_t port, SocketOptions options)
{
	useTls = true;

	sslContext = SSL_CTX_new(SSLv23_client_method());

	SSL_CTX_set_default_verify_paths(sslContext);

	if (!sslContext)
	{
		throw SocketException("Failed to create OpenSSL context");
	}

	sslBasicIO = BIO_new_ssl_connect(sslContext);
	BIO_get_ssl(sslBasicIO, &ssl);
	SSL_set_tlsext_host_name(ssl, host.c_str());
	BIO_set_conn_hostname(sslBasicIO, (host + ":" + std::to_string(port)).c_str());
	if (!BIO_do_handshake(sslBasicIO))
	{
		throw SocketException("SSL Handshake failed");
	}

	int err = SSL_get_verify_result(ssl);
	if (err != X509_V_OK)
	{
		const char* message = X509_verify_cert_error_string(err);
		throw SocketException(std::format("Certificate error: {} ({})", message, err));
	}
	X509* cert = SSL_get0_peer_certificate(ssl);
	if (cert == nullptr)
	{
		throw SocketException("Peer missing certificate");
	}
	if (X509_check_host(cert, host.c_str(), host.size(), 0, nullptr) != 1)
	{
		throw SocketException("Hostname mismatch");
	}

	int64_t result = BIO_do_connect(sslBasicIO);
	if (result <= 0)
	{
		throw SocketException(std::format("Failed connection: {}", ERR_error_string(ERR_get_error(), nullptr)));
	}
}

void tcp::impl::SocketImpl::Send(const void* data, size_t dataLength)
{
	if (dataLength > INT_MAX)
	{
		throw SocketException(std::format("Message length of {} exceeds maximum length of {}", dataLength, INT_MAX));
	}

	if (useTls)
	{
		SendTls(data, dataLength);
		return;
	}

	int errorCode = ::send(socketFd, reinterpret_cast<const char*>(data), int(dataLength), 0);
	if (errorCode < 0)
	{
		throw SocketException(std::format("Send failed: {}", strerror(errno)));
	}
}

size_t tcp::impl::SocketImpl::Receive(void* toData, size_t dataLength)
{
	if (dataLength > INT_MAX)
	{
		throw SocketException(std::format("Buffer length of {} exceeds maximum length of {}", dataLength, INT_MAX));
	}

	if (useTls)
	{
		return ReceiveTls(toData, dataLength);
	}

	int read = recv(socketFd, reinterpret_cast<char*>(toData), int(dataLength), 0);

	if (read < 0)
	{
		throw SocketException(strerror(errno));
	}
	return size_t(read);
}

void tcp::impl::SocketImpl::SendTls(const void* data, size_t dataLength)
{
	if (BIO_write(sslBasicIO, data, dataLength) <= 0)
	{
		throw SocketException("Failed write");
	}
}

size_t tcp::impl::SocketImpl::ReceiveTls(void* toData, size_t dataLength)
{
	auto read = BIO_read(sslBasicIO, toData, dataLength);
	if (read < 0)
	{
		throw SocketException("Failed read");
	}
	return read;
}

#endif
