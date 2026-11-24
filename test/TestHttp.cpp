#include <tcpUtils/Http.h>
#include <tcpUtils/WebSocket.h>
#include <print>

using namespace tcp;

int main()
{
	try
	{
		size_t sentCount = 0;
		HttpRequest request = HttpRequest(Url("http://localhost:5170/post"), "POST");
		request.sendBytesFunction = [&sentCount](size_t& sent) -> const void* {
			if (sentCount++ < 10)
			{
				sent = 11;
				return "hello world";
			}
			sent = 0;
			return nullptr;

		};
		auto response = request.Run();

		std::println("{} {}", response.statusCode, response.statusDescription);
	}
	catch (NetworkException& e)
	{
		std::println("{}", e.what());
		return 1;
	}
}
