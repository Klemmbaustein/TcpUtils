#include <tcpUtils/Http.h>
#include <tcpUtils/WebSocket.h>
#include <print>

using namespace tcp;

int main()
{
	try
	{
		HttpRequest s = HttpRequest(Url("ws://localhost:5170/ws"), "GET");
		s.protocol = HttpProtocol::http;
		HttpWebSocket socket = HttpWebSocket(&s);

		socket.receiveMessage = [&socket](WebSocketMessage message) {
			std::string messageString = std::string(reinterpret_cast<char*>(message.payload), message.payloadLength);

			std::println("{}", messageString);

			messageString += " WebSockets!";
			socket.SendMessage(WebSocketOp::sendText, messageString.data(), messageString.size());

			socket.CloseWithError(WebSocketCloseReason::normalClosure, messageString);
		};

		while (socket.IsConnected())
		{
			socket.ReadConnection();
		}
	}
	catch (NetworkException& e)
	{
		std::println("{}", e.what());
		return 1;
	}
}
