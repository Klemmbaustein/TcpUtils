#pragma once
#include <string>
#include <cstdint>
#include <optional>

namespace tcp
{
	class Url
	{
	public:
		Url(std::string FromString);
		Url();

		std::string hostName;
		std::string path;
		std::optional<std::string> scheme;
		std::optional<uint16_t> port;

		static constexpr auto SCHEME_HTTPS = "https://";
		static constexpr auto SCHEME_HTTP = "http://";
		static constexpr auto SCHEME_WEBSOCKET = "ws://";
		static constexpr auto SCHEME_SECURE_WEBSOCKET = "wss://";
	};
}