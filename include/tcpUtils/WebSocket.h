#pragma once
#include <tcpUtils/Http.h>
#include <tcpUtils/NetworkException.h>

namespace tcp
{
	class WebSocketException : NetworkException
	{
	public:
		WebSocketException(std::string message)
			: NetworkException(message)
		{

		}
	};

	enum class WebSocketOp
	{
		continueLast = 0,
		sendText = 1,
		sendBinary = 2,
		close = 8,
	};

	struct WebSocketMessage
	{
		WebSocketOp operation = WebSocketOp::sendText;
		void* payload = nullptr;
		size_t payloadLength = 0;
	};

	class HttpWebSocket
	{
	public:
		HttpWebSocket(HttpRequest* withRequest, std::function<int32_t()> randomIntFunction = nullptr);
		~HttpWebSocket();

		Socket* connection = nullptr;

		void SendMessage(bool isText, void* data, size_t length);
		void SendMessage(WebSocketOp op, void* data, size_t length);

		std::function<int32_t()> randomIntFunction;
		std::function<void(WebSocketMessage)> receiveMessage;
		void ReadConnection();
		void CloseWithError(std::string error);
	};
}