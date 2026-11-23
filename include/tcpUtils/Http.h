#pragma once
#include <functional>
#include <map>
#include <optional>
#include <tcpUtils/Socket.h>
#include <tcpUtils/NetworkException.h>
#include <tcpUtils/Url.h>

namespace tcp
{
	enum class HttpProtocol
	{
		http,
		https,
		fromUrlPreferHttp,
		fromUrlPreferHttps,
	};

	class HttpRequest;

	class HttpException : public NetworkException
	{
	public:
		HttpException(std::string message)
			: NetworkException(message)
		{

		}
	};

	class HttpResponse
	{
	public:
		int statusCode = 0;
		std::string statusDescription;
		std::string httpVersion;

		std::vector<uint8_t> body;
		std::map<std::string, std::string> headers;

		std::string_view GetBodyString() const
		{
			return { reinterpret_cast<const char*>(body.data()), body.size() };
		}

		static HttpResponse FromRequest(HttpRequest* request);
		static HttpResponse FromRequestWithData(HttpRequest* request, const void* data, size_t dataLength);

		bool ParseRequest(void* bytes, size_t count, const std::function<void(void*, size_t)>& callback,
			bool stopAfterHeaders);

	private:
		void ParseHeader();
		void ProcessInitialHeaders();
		bool ParseChunk(void* bytes, size_t count, const std::function<void(void*, size_t)>& callback);

		bool parsedInitialLine = false;
		bool isParsingHeaders = true;
		bool isChunkedEncoding = false;
		bool lastWasCr = false;
		bool chunkEnded = false;
		size_t bodySize = 0;
		size_t chunkSize = 0;
		std::vector<uint8_t> currentHeaderChunk;
	};

	class HttpRequest
	{
	public:
		HttpRequest(Url url, std::string method);

		HttpResponse Run();

		std::function<void(void*, size_t)> receiveBytesFunction;
		std::function<const void* (size_t&)> sendBytesFunction;

		uint32_t allowedRedirects = 10;
		bool followRedirects = true;
		bool keepAlive = false;
		bool stopAfterHeaders = false;
		std::string method;
		Url url;
		HttpProtocol protocol = HttpProtocol::fromUrlPreferHttp;
		std::optional<size_t> bodySize;
		Socket* httpSocket = nullptr;

		std::map<std::string, std::string> headers;
	};
}