#pragma once
#include <string>
#include <cstdint>
#include <optional>

namespace tcp
{
	/**
	 * @brief
	 * Internet Uniform Resource Location. Used for HTTP requests.
	 */
	class Url
	{
	public:
		/**
		 * @brief
		 * Creates an URL from a full string.
		 * @param fromString
		 * The string to parse as an URL.
		 *
		 * If no path is included in the URL, it will be the default value of "/"
		 */
		Url(std::string fromString) noexcept;

		/**
		 * @brief
		 * Creates an empty URL.
		 */
		Url();

		/**
		 * @brief
		 * The host of the URL. Example: www.w3c.org
		 */
		std::string hostName;
		/**
		 * @brief
		 * The path of the URL.
		 */
		std::string path = "/";
		/**
		 * @brief
		 * The protocol scheme of the URL, including the following ://
		 *
		 * Currently, this can only be http://, https://, ws:// and wss://
		 *
		 * If empty, the scheme will be inferred by HTTP connections.
		 */
		std::optional<std::string> scheme;
		/**
		 * @brief
		 * The port specified in the URL.
		 *
		 * If this is std::nullopt, the default port will be assumed.
		 */
		std::optional<uint16_t> port;

		/**
		 * @brief
		 * HTTPS URL scheme.
		 */
		static constexpr auto SCHEME_HTTPS = "https://";
		/**
		 * @brief
		 * HTTP URL scheme.
		 */
		static constexpr auto SCHEME_HTTP = "http://";
		/**
		 * @brief
		 * Websocket URL scheme.
		 */
		static constexpr auto SCHEME_WEBSOCKET = "ws://";
		/**
		 * @brief
		 * Secure Websocket URL scheme, for WebSockets using TLS
		 */
		static constexpr auto SCHEME_SECURE_WEBSOCKET = "wss://";
	};
}