#include <tcpUtils/Http.h>
#include "impl/StringUtil.h"
#include <format>
#include <cstring>

using namespace tcp;

tcp::HttpRequest::HttpRequest(Url url, std::string method)
{
	this->url = url;
	this->method = method;
}

HttpResponse tcp::HttpRequest::Run()
{
	HttpProtocol initialProtocol = this->protocol;

	switch (this->protocol)
	{
	case HttpProtocol::fromUrlPreferHttps:
	case HttpProtocol::fromUrlPreferHttp:
		if (url.scheme.has_value())
		{
			if (*url.scheme == Url::SCHEME_WEBSOCKET || *url.scheme == Url::SCHEME_HTTP)
			{
				this->protocol = HttpProtocol::http;
				break;
			}
			else if (*url.scheme == Url::SCHEME_SECURE_WEBSOCKET || *url.scheme == Url::SCHEME_HTTPS)
			{
				this->protocol = HttpProtocol::https;
				break;
			}
			// Fall through if it matches none.
		}
		if (this->protocol == HttpProtocol::fromUrlPreferHttp)
		{
			this->protocol = HttpProtocol::http;
		}
		if (this->protocol == HttpProtocol::fromUrlPreferHttps)
		{
			this->protocol = HttpProtocol::https;
		}
		break;
	default:
		break;
	}

	if (this->protocol != HttpProtocol::http && this->protocol != HttpProtocol::https)
	{
		throw HttpException("Failed to convert HTTP scheme value");
	}

	// this->protocol may only have two values here because of the previous condition, so just select one
	// using the ternary operation.
	uint16_t port = url.port.has_value() ? *url.port : (this->protocol == HttpProtocol::https ? 443 : 80);

	httpSocket = new Socket(this->url.hostName, port, SocketOptions{
		.useTls = this->protocol == HttpProtocol::https,
	});

	auto sendHeaderPart = [this](std::string part) {
		part.append("\r\n");
		httpSocket->Send(reinterpret_cast<uint8_t*>(part.data()), part.size());
	};

	std::string requestOrigin;

	if (url.scheme)
	{
		requestOrigin += *url.scheme;
	}
	requestOrigin += url.hostName;
	if (url.port)
	{
		requestOrigin += ":" + std::to_string(*url.port);
	}

	headers.insert({ "Origin", requestOrigin });

	if (sendBytesFunction && bodySize)
	{
		this->headers.insert({ "Content-Length", std::to_string(*bodySize) });
	}
	else if (sendBytesFunction && !bodySize)
	{
		this->headers.insert({ "Transfer-Encoding", "chunked" });
	}
	else
	{
		this->headers.insert({ "Content-Length", "0" });
	}

	this->headers.insert({ "Host", url.hostName });

	sendHeaderPart(std::format("{} {} HTTP/1.1", method, url.path));

	for (auto& i : this->headers)
	{
		sendHeaderPart(std::format("{}: {}", i.first.c_str(), i.second.c_str()));
	}
	sendHeaderPart("");

	if (sendBytesFunction)
	{
		size_t count = 0;

		do
		{
			const void* bytes = sendBytesFunction(count);

			if (count)
			{
				httpSocket->Send(bytes, count);
			}
		} while (count != 0 && (!bodySize || bodySize != count));
	}

	HttpResponse response;

	size_t received = 0;

	do
	{
		uint8_t data[4096];

		received = httpSocket->Receive(data, sizeof(data));

		if (received && response.ParseRequest(data, received, receiveBytesFunction, stopAfterHeaders))
		{
			break;
		}
	} while (received != 0);

	if (!keepAlive)
	{
		delete httpSocket;
		httpSocket = nullptr;
	}

	if (followRedirects && allowedRedirects && (response.statusCode == 301 || response.statusCode == 308))
	{
		if (allowedRedirects == 0)
		{
			throw HttpException("Too many redirects");
		}
		// Don't let a server infinitely redirect the client around.
		allowedRedirects--;

		auto newLocation = response.headers.find("location");

		if (newLocation != response.headers.end())
		{
			auto initialPort = this->url.port;
			this->url = Url(newLocation->second);
			this->protocol = initialProtocol;

			// This is a bit weird but seems to match what browsers do.
			if (!this->url.port.has_value())
			{
				this->url.port = initialPort;
			}

			if (this->protocol == HttpProtocol::http
				&& this->url.scheme != Url::SCHEME_HTTP && this->url.scheme != Url::SCHEME_WEBSOCKET)
			{
				throw HttpException("Redirect to another URL that does not match the original scheme of HTTP");
			}
			if (this->protocol == HttpProtocol::https
				&& this->url.scheme != Url::SCHEME_HTTPS && this->url.scheme != Url::SCHEME_SECURE_WEBSOCKET)
			{
				throw HttpException("Redirect to another URL that does not match the original scheme of HTTPS");
			}

			return Run();
		}
		else
		{
			throw HttpException("Received redirect response, but no location to redirect to");
		}
	}

	return response;
}

HttpResponse tcp::HttpResponse::FromRequest(HttpRequest* request)
{
	std::vector<uint8_t> body;

	request->receiveBytesFunction = [&body](void* bytes, size_t count) {
		size_t oldSize = body.size();
		body.resize(body.size() + count);
		memcpy(body.data() + oldSize, bytes, count);
	};

	auto response = request->Run();

	response.body = std::move(body);

	return response;
}

HttpResponse tcp::HttpResponse::FromRequestWithData(HttpRequest* request, const void* data, size_t dataLength)
{
	request->bodySize = dataLength;

	std::vector<uint8_t> body;

	request->receiveBytesFunction = [&body](void* bytes, size_t count) {
		size_t oldSize = body.size();
		body.resize(body.size() + count);
		memcpy(body.data() + oldSize, bytes, count);
	};
	request->sendBytesFunction = [data, dataLength](size_t& count) -> const void* {
		count = dataLength;
		return data;
	};

	auto response = request->Run();

	response.body = std::move(body);

	return response;
}

bool tcp::HttpResponse::ParseRequest(void* bytes, size_t count, const std::function<void(void*, size_t)>& callback, bool stopAfterHeaders)
{
	for (size_t i = 0; i < count; i++)
	{
		if (!isParsingHeaders)
		{
			if (isChunkedEncoding)
			{
				return ParseChunk(reinterpret_cast<uint8_t*>(bytes) + i, count - i, callback);
			}

			if (count - i > bodySize)
			{
				return true;
			}
			callback(reinterpret_cast<uint8_t*>(bytes) + i, count - i);

			this->bodySize -= count - i;

			return this->bodySize == 0;
		}

		uint8_t& next = reinterpret_cast<uint8_t*>(bytes)[i];

		if (next == '\r')
		{
			lastWasCr = true;
			continue;
		}
		if (next == '\n' && lastWasCr)
		{
			ParseHeader();
			currentHeaderChunk.clear();
			lastWasCr = false;
			if (stopAfterHeaders && !isParsingHeaders)
			{
				return true;
			}
			continue;
		}
		else if (next == '\n')
		{
			throw HttpException("Unexpected newline in HTTP header");
		}
		if (lastWasCr)
		{
			currentHeaderChunk.push_back('\r');
			lastWasCr = false;
		}
		currentHeaderChunk.push_back(next);
	}
	return false;
}

void tcp::HttpResponse::ParseHeader()
{
	std::string_view line = { reinterpret_cast<const char*>(currentHeaderChunk.data()), currentHeaderChunk.size() };

	if (!parsedInitialLine)
	{
		try
		{
			size_t firstSpace = line.find_first_of(' ');

			this->httpVersion = line.substr(0, firstSpace);

			if (httpVersion != "HTTP/1.1" && httpVersion != "HTTP/1.0")
			{
				throw HttpException("Unknown or unsupported HTTP version");
			}

			size_t secondSpace = line.find_first_of(' ', firstSpace + 1);
			this->statusCode = std::stoi(std::string(line.substr(firstSpace, secondSpace - firstSpace)));
			this->statusDescription = line.substr(secondSpace + 1);
		}
		catch (std::exception&)
		{
			throw HttpException("Bad HTTP header");
		}
		parsedInitialLine = true;
		return;
	}

	if (currentHeaderChunk.empty())
	{
		ProcessInitialHeaders();
		this->isParsingHeaders = false;
		return;
	}

	size_t firstColon = line.find_first_of(':');

	if (firstColon != std::string::npos)
	{
		headers.insert(std::pair{ util::Lower(std::string(line.substr(0, firstColon))),
			util::Trim(std::string(line.substr(firstColon + 1))) });
	}
	else
	{
		throw HttpException("Bad HTTP header");
	}
}

void tcp::HttpResponse::ProcessInitialHeaders()
{
	try
	{
		auto contentSize = headers.find("content-length");

		if (contentSize != headers.end())
		{
			this->bodySize = std::stoul(contentSize->second);
		}
	}
	catch (std::exception& e)
	{
		throw HttpException("Bad Content-Length value: " + std::string(e.what()));
	}

	auto transferEncoding = headers.find("transfer-encoding");

	if (transferEncoding != headers.end())
	{
		isChunkedEncoding = util::Lower(transferEncoding->second) == "chunked";
	}
}

bool tcp::HttpResponse::ParseChunk(void* bytes, size_t count, const std::function<void(void*, size_t)>& callback)
{
	for (size_t i = 0; i < count; i++)
	{
		if (bodySize != 0)
		{
			if (count - i > bodySize)
			{
				callback(reinterpret_cast<uint8_t*>(bytes) + i, bodySize);
				i += bodySize;
				bodySize = 0;
			}
			else
			{
				callback(reinterpret_cast<uint8_t*>(bytes) + i, count - i);

				this->bodySize -= count - i;
				break;
			}
			if (i >= count)
			{
				continue;
			}
			chunkEnded = true;
		}

		uint8_t& next = reinterpret_cast<uint8_t*>(bytes)[i];

		if (next == '\r')
		{
			lastWasCr = true;
			continue;
		}
		if (next == '\n' && lastWasCr)
		{
			if (chunkEnded)
			{
				chunkEnded = false;
				lastWasCr = false;
				currentHeaderChunk.clear();
				continue;
			}

			try
			{
				bodySize = std::stoi(std::string(reinterpret_cast<char*>(currentHeaderChunk.data()),
					currentHeaderChunk.size()), nullptr, 16);
			}
			catch (std::exception& e)
			{
				throw HttpException("Failed to parse chunk size: " + std::string(e.what()));
			}
			currentHeaderChunk.clear();
			lastWasCr = false;
			if (bodySize == 0)
			{
				return true;
			}
			continue;
		}
		else if (next == '\n')
		{
			throw HttpException("Unexpected item in chunked encoding");
		}
		if (lastWasCr)
		{
			currentHeaderChunk.push_back('\r');
			lastWasCr = false;
		}
		currentHeaderChunk.push_back(next);
	}

	return false;
}
