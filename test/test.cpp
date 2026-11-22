#include <tcpUtils/Socket.h>
#include <print>

using namespace tcp;

int main()
{
	try
	{
		Socket s = Socket("www.google.com", 443, SocketOptions{ .useTls = true });

		std::string message = "GET / HTTP/1.1\r\nHost: www.google.com\r\n\r\n";
		char received[8192];

		s.Send(message.data(), message.size());

		size_t count = s.Receive(received, sizeof(received));
		std::println("{}", std::string(received, count));
	}
	catch (SocketException& e)
	{
		std::println("{}", e.what());
	}
}
