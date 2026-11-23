#include <tcpUtils/Url.h>
#include <vector>

tcp::Url::Url(std::string FromString)
{
	const std::vector<std::string> possibleSchemes = { SCHEME_HTTP, SCHEME_HTTPS,
		SCHEME_WEBSOCKET, SCHEME_SECURE_WEBSOCKET };
	std::string urlType;

	for (auto& i : possibleSchemes)
	{
		if (FromString.substr(0, i.size()) == i)
		{
			urlType = i;
			FromString = FromString.substr(i.size());
			break;
		}
	}

	if (urlType.empty())
	{
		urlType = SCHEME_HTTP;
	}

	size_t slash = FromString.find_first_of("/");
	if (slash != std::string::npos)
	{
		this->path = FromString.substr(slash);
		FromString = FromString.substr(0, slash);
	}
	else
	{
		this->path = "/";
	}

	size_t Colon = FromString.find_first_of(":");
	if (Colon != std::string::npos)
	{
		this->port = std::stoi(FromString.substr(Colon + 1));
		FromString = FromString.substr(0, Colon);
	}
	this->scheme = urlType;
	this->hostName = FromString;
}

tcp::Url::Url()
{
}
