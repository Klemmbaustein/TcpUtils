#include <tcpUtils/WebSocket.h>
#include <tcpUtils/Base64.h>
#include <random>
#include <cstring>

static unsigned char reverse(unsigned char b)
{
	b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
	b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
	b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
	return b;
}

tcp::HttpWebSocket::HttpWebSocket(HttpRequest* withRequest, std::function<uint32_t()> randomIntFunction)
{
	std::string keyBinary;
	keyBinary.resize(16, 0);

	if (!randomIntFunction)
	{
		static std::random_device rd;
		static std::mt19937 defaultRandomSource{ rd() };

		randomIntFunction = [] -> uint32_t {
			std::uniform_int_distribution<uint32_t> generator{ 0, UINT32_MAX };
			return generator(defaultRandomSource);
		};
	}

	this->randomIntFunction = randomIntFunction;

	for (size_t i = 0; i < keyBinary.size(); i += sizeof(uint32_t))
	{
		auto result = randomIntFunction();

		keyBinary[i] = result & 0xFF;
		keyBinary[i + 1] = (result >> 8) & 0xFF;
		keyBinary[i + 2] = (result >> 16) & 0xFF;
		keyBinary[i + 3] = (result >> 24) & 0xFF;
	}

	withRequest->method = "GET";
	withRequest->keepAlive = true;
	withRequest->stopAfterHeaders = true;
	withRequest->headers.insert({ "Sec-WebSocket-Version", "13" });
	withRequest->headers.insert({ "Connection", "Upgrade" });
	withRequest->headers.insert({ "Upgrade", "websocket" });
	withRequest->headers.insert({ "User-Agent", "websocket" });

	withRequest->headers.insert({ "Sec-WebSocket-Key", base64_encode(keyBinary) });

	auto response = withRequest->Run();
	connection = withRequest->httpSocket;

	if (response.statusCode != 101)
	{
		throw WebSocketException("URL did not reply with a WebSocket handshake");
	}
	isConnectionActive = true;
}

tcp::HttpWebSocket::~HttpWebSocket()
{
	if (!isConnectionActive || sentClose)
	{
		SendMessage(WebSocketOp::close, nullptr, 0);
	}
	delete connection;
}

void tcp::HttpWebSocket::SendMessage(bool isText, void* data, size_t length)
{
	SendMessage(WebSocketOp(isText ? 1 : 2), data, length);
}

void tcp::HttpWebSocket::SendMessage(WebSocketOp op, void* data, size_t length)
{
	std::vector<uint8_t> result;

	result.push_back(1 << 7 | uint8_t(op));
	if (length <= 125)
	{
		result.push_back(1 << 7 | uint8_t(length));
	}
	else if (length < UINT16_MAX)
	{
		result.push_back(1 << 7 | 126);
		uint16_t size16 = uint16_t(length);
		result.push_back((size16 >> 8) & 0xFF);
		result.push_back(size16 & 0xFF);
	}
	else if (length < INT64_MAX)
	{
		result.push_back(1 << 7 | 127);
		uint64_t size64 = uint64_t(length);
		result.push_back((size64 >> 56) & 0xFF);
		result.push_back((size64 >> 48) & 0xFF);
		result.push_back((size64 >> 40) & 0xFF);
		result.push_back((size64 >> 32) & 0xFF);
		result.push_back((size64 >> 24) & 0xFF);
		result.push_back((size64 >> 16) & 0xFF);
		result.push_back((size64 >> 8) & 0xFF);
		result.push_back(size64 & 0xFF);
	}
	else
	{
		throw WebSocketException("Message too long");
	}

	auto headerEnd = result.size();

	result.resize(headerEnd + 4 + length);

	uint32_t maskKey = 0;
	memcpy(result.data() + headerEnd, &maskKey, sizeof(maskKey));
	if (length)
	{
		memcpy(result.data() + headerEnd + 4, data, length);
	}
	auto bytes = result.data();

	connection->Send(bytes, result.size());
}

void tcp::HttpWebSocket::ReadConnection()
{
	uint8_t headerBytes[2]{ 0 };

	if (!connection->Receive(headerBytes, sizeof(headerBytes)))
	{
		throw WebSocketException("Connection closed without a closing handshake");
	}
	WebSocketMessage message;
	message.operation = WebSocketOp(headerBytes[0] & 0x0F);

	bool isClose = false;

	switch (message.operation)
	{
	case WebSocketOp::continueLast:
	case WebSocketOp::sendText:
	case WebSocketOp::sendBinary:
		break;
	case WebSocketOp::close:
		isClose = true;
		break;
	default:
		throw WebSocketException("Unknown Websocket message type");
	}

	uint8_t sizeAndHasMask = headerBytes[1];

	bool hasMask = bool(sizeAndHasMask & 1 << 7);
	uint8_t byteSize = sizeAndHasMask & 0x7F;

	uint64_t totalSize = byteSize;

	if (byteSize == 126)
	{
		uint8_t sizeBytes[2];
		connection->Receive(sizeBytes, sizeof(sizeBytes));
		totalSize = sizeBytes[0] << 8 | sizeBytes[1];
	}
	else if (byteSize == 127)
	{
		uint8_t sizeBytes[8];
		connection->Receive(sizeBytes, sizeof(sizeBytes));
		totalSize = 0;
		totalSize |= static_cast<int64_t>(sizeBytes[0]) << 56;
		totalSize |= static_cast<int64_t>(sizeBytes[1]) << 48;
		totalSize |= static_cast<int64_t>(sizeBytes[2]) << 40;
		totalSize |= static_cast<int64_t>(sizeBytes[3]) << 32;
		totalSize |= static_cast<int64_t>(sizeBytes[4]) << 24;
		totalSize |= static_cast<int64_t>(sizeBytes[4]) << 16;
		totalSize |= static_cast<int64_t>(sizeBytes[4]) << 8;
		totalSize |= static_cast<int64_t>(sizeBytes[4]) << 0;
	}

	std::string result;
	result.resize(totalSize);
	connection->Receive(result.data(), result.size());
	message.payload = result.data();
	message.payloadLength = result.size();

	if (!isClose)
	{
		if (receiveMessage)
		{
			receiveMessage(message);
		}
	}
	else
	{
		if (sentClose)
		{
			isConnectionActive = false;
			return;
		}

		WebSocketCloseMessage closeMessage;
		if (message.payloadLength < 2)
		{
			closeMessage.payload = nullptr;
			closeMessage.payloadLength = 0;
			closeMessage.closeReason = WebSocketCloseReason::undefined;
		}
		else if (message.payloadLength >= 2)
		{
			closeMessage.payload = nullptr;
			closeMessage.payloadLength = 0;
			memcpy(&closeMessage.closeReason, result.data(), sizeof(closeMessage.closeReason));
			closeMessage.closeReason = WebSocketCloseReason(uint16_t(closeMessage.closeReason) >> 8
				| uint16_t(closeMessage.closeReason) << 8);
		}
		closeMessage.operation = WebSocketOp::close;
		if (receiveClose)
		{
			receiveClose(closeMessage);
		}
		SendMessage(WebSocketOp::close, result.data(), result.size());
		isConnectionActive = false;
	}
}

void tcp::HttpWebSocket::CloseWithError(WebSocketCloseReason reason, std::string error)
{
	if (reason == WebSocketCloseReason::undefined)
	{
		SendMessage(WebSocketOp::close, nullptr, 0);
		sentClose = true;
		return;
	}

	reason = WebSocketCloseReason(uint16_t(reason) >> 8
		| uint16_t(reason) << 8);
	error.insert(error.begin(), 2, '\0');
	memcpy(error.data(), &reason, 2);

	if (error.size() > 123)
	{
		throw WebSocketException("WebSockets tried to send a close reason longer than the limit of 123 bytes.");
	}

	SendMessage(WebSocketOp::close, error.data(), error.size());
	sentClose = true;
}
