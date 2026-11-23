#include <tcpUtils/WebSocket.h>
#include <tcpUtils/Base64.h>

unsigned char reverse(unsigned char b)
{
	b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
	b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
	b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
	return b;
}

tcp::HttpWebSocket::HttpWebSocket(HttpRequest* withRequest, std::function<int32_t()> randomIntFunction)
{
	std::string keyBinary;
	// TODO: Definitely do not keep it like this
	keyBinary.resize(16, 42);

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
}

tcp::HttpWebSocket::~HttpWebSocket()
{
	SendMessage(WebSocketOp::close, nullptr, 0);
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
		throw WebSocketException("Message too long.");
	}

	auto headerEnd = result.size();

	result.resize(headerEnd + 4 + length);

	int32_t maskKey = 0;
	memcpy(result.data() + headerEnd, &maskKey, sizeof(maskKey));
	memcpy(result.data() + headerEnd + 4, data, length);
	auto bytes = result.data();

	connection->Send(bytes, result.size());
}

void tcp::HttpWebSocket::ReadConnection()
{
	uint8_t headerBytes[2];

	connection->Receive(headerBytes, sizeof(headerBytes));

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
	WebSocketMessage message;
	message.payload = result.data();
	message.payloadLength = result.size();
	message.operation = WebSocketOp(headerBytes[0] & 0x0F);
	receiveMessage(message);
}

void tcp::HttpWebSocket::CloseWithError(std::string error)
{
}
