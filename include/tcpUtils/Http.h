#pragma once
#include <functional>
#include <map>
#include <optional>
#include <tcpUtils/Socket.h>
#include <tcpUtils/NetworkException.h>
#include <tcpUtils/Url.h>

namespace tcp
{
	/**
	 * @brief
	 * The protocol used by a HTTP request.
	 */
	enum class HttpProtocol
	{
		/// Always use the HTTP protocol.
		http,
		/// Always use the HTTPS protocol
		https,
		/// Use the protocol type specified in the URL, but try using HTTP if it was not specified.
		fromUrlPreferHttp,
		/// Use the protocol type specified in the URL, but try using HTTPS if it was not specified.
		fromUrlPreferHttps,
	};

	class HttpRequest;

	/**
	 * @brief
	 * An exception that occurred because of a HTTP request.
	 */
	class HttpException : public NetworkException
	{
	public:
		HttpException(std::string message)
			: NetworkException(message)
		{

		}
	};

	/**
	 * @brief
	 * A response of HTTP requests.
	 */
	class HttpResponse
	{
	public:
		/**
		 * @brief
		 * The HTTP status received as part of this HTTP request.
		 */
		int statusCode = 0;
		/**
		 * @brief
		 * A brief description of the request status in text form.
		 *
		 * @see statusCode
		 */
		std::string statusDescription;
		/**
		 * @brief
		 * The HTTP version that the response used.
		 * Only HTTP 1.1 and 1.0 are supported right now.
		 */
		std::string httpVersion;

		/**
		 * @brief
		 * The response headers sent from the server.
		 */
		std::map<std::string, std::string> headers;

		/**
		 * @brief
		 * Used internally to parse HTTP headers and body from raw bytes.
		 */
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

	/**
	 * @brief
	 * A class that lets you easily do non-streaming HTTP requests, where the incoming and outgoing data
	 * all already in memory.
	 */
	class SimpleHttpResponse : public HttpResponse
	{
	public:
		/**
		 * @brief
		 * The bytes received after the HTTP request is complete.
		 */
		std::vector<uint8_t> body;

		/**
		 * @brief
		 * Gets the body of this request as a string_view.
		 */
		std::string_view GetBodyString() const noexcept
		{
			return { reinterpret_cast<const char*>(body.data()), body.size() };
		}

		/**
		 * @brief
		 * Runs the HTTP request and returns a simple HTTP response that contains all the bytes
		 * received.
		 *
		 * This function overrides the sendBytesFunction and receiveBytesFunction of the given request
		 * to perform a simple HTTP request.
		 *
		 * @param request
		 * The request to run.
		 * @return
		 * The response gotten from that request.
		 *
		 * @throw SocketException
		 * An exception is thrown if a connection error occurs while sending or receiving data.
		 *
		 * @throw HttpException
		 * An exception is thrown if the request configuration is invalid or the server responds
		 * with a malformed response.
		 */
		static SimpleHttpResponse FromRequest(HttpRequest* request);
		/**
		 * @brief
		 * Runs the HTTP request with the given payload and returns a simple HTTP response that contains all the bytes
		 * received.
		 *
		 * This function overrides the sendBytesFunction and receiveBytesFunction of the given request
		 * to perform a simple HTTP request.
		 *
		 * @param request
		 * The request to run.
		 * @param data
		 * The data to send as the body of this HTTP request.
		 * @param dataLength
		 * The length of the data to send as the body of this HTTP request.
		 * @return
		 * The response gotten from that request.
		 *
		 * @throw SocketException
		 * An exception is thrown if a connection error occurs while sending or receiving data.
		 *
		 * @throw HttpException
		 * An exception is thrown if the request configuration is invalid or the server responds
		 * with a malformed response.
		 */
		static SimpleHttpResponse FromRequestWithData(HttpRequest* request, const void* data, size_t dataLength);
	private:
		SimpleHttpResponse(HttpResponse&& fromResponse)
			: HttpResponse(fromResponse)
		{

		}
	};

	/**
	 * @brief
	 * A class describing a HTTP request.
	 */
	class HttpRequest
	{
	public:
		/**
		 * @brief
		 * Creates a request object from the given URL and HTTP method.
		 * @param url
		 * The URL to use for this request.
		 * @param method
		 * The method for the URL. Example: "GET", "POST"
		 */
		HttpRequest(Url url, std::string method);

		/**
		 * @brief
		 * Runs this request.
		 *
		 * Runs this request by sending the headers and all bytes returned by sendBytesFunction (if present)
		 * to the endpoint.
		 *
		 * @return
		 * The response of the request, once it has finished.
		 *
		 * @throw SocketException
		 * An exception is thrown if a connection error occurs while sending or receiving data.
		 *
		 * @throw HttpException
		 * An exception is thrown if the request configuration is invalid or the server responds
		 * with a malformed response.
		 */
		HttpResponse Run();

		/**
		 * @brief
		 * Function used to process bytes received by this request.
		 *
		 * The function gets a void* argument that points to the chunk of bytes that were received
		 * and a size_t argument that is the length of bytes of that chunk.
		 */
		std::function<void(void*, size_t)> receiveBytesFunction;
		/**
		 * @brief
		 * Function used to send a chunk of bytes as the body of this request.
		 * The function returns a const void* that points to the chunk of memory to send.
		 * It is also given a size_t reference, which should be set to the number of bytes to write.
		 *
		 * Once the number of bytes to write is 0 or the number of bytes to write specified by bodySize is reached,
		 * the request is completed.
		 */
		std::function<const void* (size_t&)> sendBytesFunction;

		uint32_t allowedRedirects = 10;

		/**
		 * @brief
		 * Follow redirects if the server responds with a HTTP 301 or 308 status code.
		 */
		bool followRedirects = true;

		/**
		 * @brief
		 * Keep the socket connection alive after the request runs.
		 *
		 * If this is true, httpSocket will not be null after the request completes and the socket
		 * can be used to send more data over the connection.
		 *
		 * @warning The socket will be owned by the user of this class when this value is true. You will have to
		 * delete it yourself once you are done with it.
		 */
		bool keepAlive = false;
		/**
		 * @brief
		 * Stop the request after parsing headers.
		 *
		 * This is used in combination with keepAlive to initiate a WebSocket request by the HttpWebSocket class.
		 */
		bool stopAfterHeaders = false;
		/**
		 * @brief
		 * The HTTP method used for this request.
		 */
		std::string method;
		/**
		 * @brief
		 * The URL to send this message to.
		 */
		Url url;
		/**
		 * @brief
		 * The protocol to use for this request.
		 */
		HttpProtocol protocol = HttpProtocol::fromUrlPreferHttp;
		/**
		 * @brief
		 * The size of the body of the request in bytes.
		 *
		 * If this is std::nullopt and sendBytesFunction has a value, this is a chunked HTTP message.
		 * If this is std::nullopt and sendBytesFunction doesn't have a value, this request has no body.
		 */
		std::optional<size_t> bodySize;
		/**
		 * @brief
		 * The socket used to run this request.
		 *
		 * @see keepAlive
		 */
		Socket* httpSocket = nullptr;

		/**
		 * @brief
		 * Headers to include in the request
		 */
		std::map<std::string, std::string> headers;
	};
}