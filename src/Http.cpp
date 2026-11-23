#include <tcpUtils/Http.h>
#include "StringUtil.h"
#include <format>

using namespace tcp;

tcp::HttpRequest::HttpRequest(std::string url, std::string path, std::string method)
{
	this->url = url;
	this->method = method;
	this->path = path.empty() ? "/" : path;
}

HttpResponse tcp::HttpRequest::Run()
{
	uint16_t requestPort;

	if (port)
	{
		requestPort = *port;
	}
	else
	{
		requestPort = this->protocol == HttpProtocol::http ? 80 : 443;
	}

	httpSocket = new Socket(this->url, requestPort, SocketOptions{
		.useTls = this->protocol == HttpProtocol::https,
	});

	auto sendHeaderPart = [this](std::string part) {
		part.append("\r\n");
		httpSocket->Send(reinterpret_cast<uint8_t*>(part.data()), part.size());
	};

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

	this->headers.insert({ "Host", url });

	sendHeaderPart(std::format("{} {} HTTP/1.1", method, path));

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
			this->statusCode = std::stoi(line.substr(firstSpace, secondSpace - firstSpace).data());
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
		auto contentSize = headers.find("content-size");

		if (contentSize != headers.end())
		{
			this->bodySize = std::stoul(contentSize->second);
		}
	}
	catch (std::exception& e)
	{
		throw HttpException("Bad Content-Size value: " + std::string(e.what()));
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
