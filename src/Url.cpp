#include <tcpUtils/Url.h>
#include <vector>

tcp::Url::Url(std::string fromString) noexcept
{
	const std::vector<std::string> possibleSchemes = { SCHEME_HTTP, SCHEME_HTTPS,
		SCHEME_WEBSOCKET, SCHEME_SECURE_WEBSOCKET };
	std::string urlType;

	for (auto& i : possibleSchemes)
	{
		if (fromString.substr(0, i.size()) == i)
		{
			urlType = i;
			fromString = fromString.substr(i.size());
			break;
		}
	}

	if (urlType.empty())
	{
		urlType = SCHEME_HTTP;
	}

	size_t slash = fromString.find_first_of("/");
	if (slash != std::string::npos)
	{
		this->path = fromString.substr(slash);
		fromString = fromString.substr(0, slash);
	}
	else
	{
		this->path = "/";
	}

	size_t Colon = fromString.find_first_of(":");
	if (Colon != std::string::npos)
	{
		this->port = std::stoi(fromString.substr(Colon + 1));
		fromString = fromString.substr(0, Colon);
	}
	this->scheme = urlType;
	this->hostName = fromString;
}

tcp::Url::Url()
{
}
