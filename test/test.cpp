#include <tcpUtils/Http.h>
#include <print>

using namespace tcp;

int main()
{
	try
	{
		HttpRequest s = HttpRequest("www.google.com", "/", "GET");

		s.protocol = HttpProtocol::https;

		HttpResponse response = HttpResponse::FromRequest(&s);

		std::println("{}", response.GetBodyString());
	}
	catch (NetworkException& e)
	{
		std::println("{}", e.what());
	}
}
