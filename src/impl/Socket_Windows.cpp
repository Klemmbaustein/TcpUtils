#ifdef _WIN32
#include "Socket_Windows.h"
#include <tcpUtils/Socket.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <shlwapi.h>
#include <schannel.h>
#include <assert.h>
#include <format>
#include <vector>
#include "Socket_Unix.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment (lib, "secur32.lib")
#pragma comment (lib, "shlwapi.lib")

static std::wstring strToWstr(const std::string& str)
{
	int WideLength = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
	wchar_t* WideString = new wchar_t[WideLength + 1];
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, WideString, WideLength);
	WideString[WideLength] = 0;
	std::wstring OutStr = WideString;
	delete[] WideString;
	return OutStr;
}

static std::string wstrToStr(const std::wstring& wstr)
{
	int ResultLength = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), int(wstr.size()), nullptr, 0, 0, 0);
	char* ToBuffer = new char[ResultLength];
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, ToBuffer, ResultLength, NULL, NULL);
	std::string Result = std::string(ToBuffer, ResultLength);
	delete[] ToBuffer;
	return Result;
}

static std::string getErrorString(DWORD ErrorMessageID)
{
	if (ErrorMessageID == 0)
	{
		return "No error";
	}

	LPWSTR MessageBuffer = nullptr;
	size_t Size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
		| FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, ErrorMessageID, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
		(LPWSTR)&MessageBuffer, 0, NULL);

	std::wstring Message = std::wstring(MessageBuffer, Size);

	LocalFree(MessageBuffer);

	return wstrToStr(Message);
}


static std::string getLastErrorString()
{
	return getErrorString(GetLastError());
}

tcp::impl::SocketImpl::SocketImpl()
{
	WSADATA winsockData;
	int result = WSAStartup(MAKEWORD(2, 2), &winsockData);

	ZeroMemory(incoming, sizeof(incoming));

	if (result != 0)
	{
		throw SocketException(std::format("WSAStartup failed: {}", result));
	}
}

void tcp::impl::SocketImpl::Connect(std::string host, uint16_t port, SocketOptions options)
{
	connectionSocket = socket(AF_INET6, SOCK_STREAM, 0);

	DWORD ipv6OnlyValue = 0;
	setsockopt(connectionSocket, IPPROTO_IPV6, IPV6_V6ONLY,
		reinterpret_cast<char*>(&ipv6OnlyValue), sizeof(ipv6OnlyValue));

	// Connect to server
	if (!WSAConnectByNameW(connectionSocket, strToWstr(host).data(), std::to_wstring(port).data(),
		NULL, NULL, NULL, NULL, NULL, NULL))
	{
		throw SocketException(getErrorString(WSAGetLastError()));
	}

	if (options.useTls)
	{
		InitializeTls(host);
		useTls = true;
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

	int errorCode = ::send(connectionSocket, reinterpret_cast<const char*>(data), int(dataLength), 0);
	if (errorCode == SOCKET_ERROR)
	{
		errorCode = WSAGetLastError();
		closesocket(connectionSocket);
		connectionSocket = INVALID_SOCKET;
		throw SocketException(std::format("send failed. {}", errorCode));
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

	int read = recv(connectionSocket, reinterpret_cast<char*>(toData), int(dataLength), 0);

	if (read == SOCKET_ERROR)
	{
		throw SocketException(getErrorString(WSAGetLastError()));
	}
	return size_t(read);
}

void tcp::impl::SocketImpl::InitializeTls(std::string host)
{
	SCHANNEL_CRED cred =
	{
		.dwVersion = SCHANNEL_CRED_VERSION,
		.grbitEnabledProtocols = SP_PROT_TLS1_2,
		.dwFlags = SCH_USE_STRONG_CRYPTO | SCH_CRED_AUTO_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS,
	};

	std::string name = UNISP_NAME_A;

	if (AcquireCredentialsHandleA(NULL, name.data(), SECPKG_CRED_OUTBOUND, NULL,
		&cred, NULL, NULL, &credentials, NULL) != SEC_E_OK)
	{
		closesocket(connectionSocket);
		WSACleanup();
		throw SocketException("Failed to acquire TLS credentials");
	}

	std::vector<char> incoming;
	incoming.resize(TLS_MAX_PACKET_SIZE);

	int received = 0;

	CtxtHandle* newContext = NULL;
	int result = 0;
	while (true)
	{
		SecBuffer inBuffers[2] = { 0 };
		inBuffers[0].BufferType = SECBUFFER_TOKEN;
		inBuffers[0].pvBuffer = incoming.data();
		inBuffers[0].cbBuffer = received;
		inBuffers[1].BufferType = SECBUFFER_EMPTY;

		SecBuffer outBuffers[1] = { 0 };
		outBuffers[0].BufferType = SECBUFFER_TOKEN;

		SecBufferDesc inDesc = { SECBUFFER_VERSION, ARRAYSIZE(inBuffers), inBuffers };
		SecBufferDesc outDesc = { SECBUFFER_VERSION, ARRAYSIZE(outBuffers), outBuffers };

		DWORD flags = ISC_REQ_USE_SUPPLIED_CREDS | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY |
			ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_STREAM;
		SECURITY_STATUS sec = InitializeSecurityContextA(
			&credentials,
			newContext,
			newContext ? NULL : host.data(),
			flags,
			0,
			0,
			newContext ? &inDesc : NULL,
			0,
			newContext ? NULL : &context,
			&outDesc,
			&flags,
			NULL);

		int err = GetLastError();

		// after first call to InitializeSecurityContext context is available and should be reused for next calls
		newContext = &context;

		if (inBuffers[1].BufferType == SECBUFFER_EXTRA)
		{
			MoveMemory(incoming.data(), incoming.data() + (received - inBuffers[1].cbBuffer), inBuffers[1].cbBuffer);
			received = inBuffers[1].cbBuffer;
		}
		else
		{
			received = 0;
		}

		if (sec == SEC_E_OK)
		{
			// tls handshake completed
			break;
		}
		else if (sec == SEC_I_INCOMPLETE_CREDENTIALS)
		{
			// server asked for client certificate, not supported here
			result = -1;
			break;
		}
		else if (sec == SEC_I_CONTINUE_NEEDED)
		{
			// need to send data to server
			const char* buffer = reinterpret_cast<char*>(outBuffers[0].pvBuffer);
			int size = outBuffers[0].cbBuffer;

			while (size != 0)
			{
				int d = ::send(connectionSocket, buffer, size, 0);
				if (d <= 0)
				{
					break;
				}
				size -= d;
				buffer += d;
			}
			if (outBuffers[0].pvBuffer)
			{
				FreeContextBuffer(outBuffers[0].pvBuffer);
			}
			if (size != 0)
			{
				// failed to fully send data to server
				result = -1;
				break;
			}
		}
		else if (sec != SEC_E_INCOMPLETE_MESSAGE)
		{
			result = -1;
			switch (sec)
			{
			case SEC_E_CERT_EXPIRED:
				throw SocketException("Certificate expired");
			case SEC_E_WRONG_PRINCIPAL:
				throw SocketException("Bad hostname");
			case SEC_E_UNTRUSTED_ROOT:
				throw SocketException("Untrusted root");
			case SEC_E_ALGORITHM_MISMATCH:
			case SEC_E_ILLEGAL_MESSAGE:
				throw SocketException("cannot negotiate crypto algorithms");
			default:
				throw SocketException("Unknown connection error");
			}
		}

		// read more data from server when possible
		if (received == TLS_MAX_PACKET_SIZE)
		{
			throw SocketException("Too much data");
		}

		int r = recv(connectionSocket, incoming.data() + received, TLS_MAX_PACKET_SIZE - received, 0);
		if (r == 0)
		{
			throw SocketException("Server disconnected");
		}
		else if (r < 0)
		{
			throw SocketException(getErrorString(WSAGetLastError()));
		}
		received += r;
	}

	QueryContextAttributes(&context, SECPKG_ATTR_STREAM_SIZES, &sizes);
}

void tcp::impl::SocketImpl::SendTls(const void* data, size_t dataLength)
{
	while (dataLength != 0)
	{
		uint32_t sendSize = uint32_t(min(dataLength, sizes.cbMaximumMessage));

		char* writeBuffer = new char[TLS_MAX_PACKET_SIZE];
		assert(sizes.cbHeader + sizes.cbMaximumMessage + sizes.cbTrailer <= TLS_MAX_PACKET_SIZE);

		SecBuffer buffers[3]{};
		buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
		buffers[0].pvBuffer = writeBuffer;
		buffers[0].cbBuffer = sizes.cbHeader;
		buffers[1].BufferType = SECBUFFER_DATA;
		buffers[1].pvBuffer = writeBuffer + sizes.cbHeader;
		buffers[1].cbBuffer = sendSize;
		buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
		buffers[2].pvBuffer = writeBuffer + sizes.cbHeader + sendSize;
		buffers[2].cbBuffer = sizes.cbTrailer;

		CopyMemory(buffers[1].pvBuffer, data, dataLength);

		SecBufferDesc desc = { SECBUFFER_VERSION, ARRAYSIZE(buffers), buffers };
		SECURITY_STATUS sec = EncryptMessage(&context, 0, &desc, 0);
		if (sec != SEC_E_OK)
		{
			// This should not happen, but just in case check it
			throw SocketException(getErrorString(WSAGetLastError()));
		}

		int total = buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer;
		int sent = 0;
		while (sent != total)
		{
			int d = ::send(connectionSocket, writeBuffer + sent, total - sent, 0);
			if (d <= 0)
			{
				// error sending data to socket, or server disconnected
				delete writeBuffer;
				throw SocketException(getErrorString(WSAGetLastError()));
			}
			sent += d;
		}

		data = reinterpret_cast<const uint8_t*>(data) + sendSize;
		dataLength -= sendSize;
		delete writeBuffer;
	}
}

size_t tcp::impl::SocketImpl::ReceiveTls(void* data, size_t dataLength)
{
	uint32_t result = 0;

	while (dataLength != 0)
	{
		if (decryptedBuffer)
		{
			// if there is decrypted data available, then use it as much as possible
			uint32_t used = uint32_t(min(dataLength, decryptedLength));
			CopyMemory(data, decryptedBuffer, used);
			data = reinterpret_cast<uint8_t*>(data) + used;
			dataLength -= used;
			result += used;

			if (used == decryptedLength)
			{
				// all decrypted data is used, remove ciphertext from incoming buffer so next time it starts from beginning
				MoveMemory(incoming, incoming + usedSize, received - usedSize);
				received -= usedSize;
				usedSize = 0;
				decryptedLength = 0;
				decryptedBuffer = nullptr;
			}
			else
			{
				decryptedLength -= used;
				decryptedBuffer += used;
			}
			continue;
		}
		// if any ciphertext data available then try to decrypt it
		if (received != 0)
		{
			SecBuffer buffers[4]{};
			assert(sizes.cBuffers == ARRAYSIZE(buffers));

			buffers[0].BufferType = SECBUFFER_DATA;
			buffers[0].pvBuffer = incoming;
			buffers[0].cbBuffer = uint32_t(received);
			buffers[1].BufferType = SECBUFFER_EMPTY;
			buffers[2].BufferType = SECBUFFER_EMPTY;
			buffers[3].BufferType = SECBUFFER_EMPTY;

			SecBufferDesc desc = { SECBUFFER_VERSION, ARRAYSIZE(buffers), buffers };

			SECURITY_STATUS sec = DecryptMessage(&context, &desc, 0, NULL);
			if (sec == SEC_E_OK)
			{
				assert(buffers[0].BufferType == SECBUFFER_STREAM_HEADER);
				assert(buffers[1].BufferType == SECBUFFER_DATA);
				assert(buffers[2].BufferType == SECBUFFER_STREAM_TRAILER);

				decryptedBuffer = reinterpret_cast<uint8_t*>(buffers[1].pvBuffer);
				decryptedLength = buffers[1].cbBuffer;
				usedSize = received - (buffers[3].BufferType == SECBUFFER_EXTRA ? buffers[3].cbBuffer : 0);

				// data is now decrypted, go back to beginning of loop to copy memory to output buffer
				continue;
			}
			else if (sec == SEC_I_CONTEXT_EXPIRED)
			{
				// server closed TLS connection (but socket is still open)
				received = 0;
				return result;
			}
			else if (sec == SEC_I_RENEGOTIATE)
			{
				throw SocketException("Unimplemented");
			}
			else if (sec != SEC_E_INCOMPLETE_MESSAGE)
			{
				throw SocketException("TLS protocol error SEC_I_RENEGOTIATE");
			}
			// otherwise sec == SEC_E_INCOMPLETE_MESSAGE which means need to read more data
		}
		// otherwise not enough data received to decrypt

		if (result != 0)
		{
			// some data is already copied to output buffer, so return that before blocking with recv
			break;
		}

		if (received == sizeof(incoming))
		{
			throw SocketException("Too much data for TLS handshake");
		}

		// wait for more ciphertext data from server
		int r = recv(connectionSocket, reinterpret_cast<char*>(incoming) + received, int32_t(sizeof(incoming) - received), 0);
		if (r == 0)
		{
			// server disconnected socket
			return 0;
		}
		else if (r < 0)
		{
			int errorCode = WSAGetLastError();
			throw SocketException(getErrorString(WSAGetLastError()));
		}
		received += r;
	}

	return result;
}

tcp::impl::SocketImpl::~SocketImpl()
{
	if (useTls)
	{
		DWORD type = SCHANNEL_SHUTDOWN;

		SecBuffer inBuffers[1];
		inBuffers[0].BufferType = SECBUFFER_TOKEN;
		inBuffers[0].pvBuffer = &type;
		inBuffers[0].cbBuffer = sizeof(type);

		SecBufferDesc inDesc = { SECBUFFER_VERSION, ARRAYSIZE(inBuffers), inBuffers };
		ApplyControlToken(&context, &inDesc);

		SecBuffer outBuffers[1];
		outBuffers[0].BufferType = SECBUFFER_TOKEN;

		SecBufferDesc outDesc = { SECBUFFER_VERSION, ARRAYSIZE(outBuffers), outBuffers };
		DWORD flags = ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_STREAM;
		if (InitializeSecurityContextA(&credentials, &context, NULL, flags, 0, 0, &outDesc, 0, NULL, &outDesc, &flags, NULL) == SEC_E_OK)
		{
			char* buffer = (char*)outBuffers[0].pvBuffer;
			int size = outBuffers[0].cbBuffer;
			while (size != 0)
			{
				int d = ::send(connectionSocket, buffer, size, 0);
				if (d <= 0)
				{
					// ignore any failures socket will be closed anyway
					break;
				}
				buffer += d;
				size -= d;
			}
			FreeContextBuffer(outBuffers[0].pvBuffer);
		}

		DeleteSecurityContext(&context);
		FreeCredentialsHandle(&credentials);
	}
	closesocket(connectionSocket);
	WSACleanup();
}
#endif
