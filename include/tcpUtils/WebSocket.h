#pragma once
#include <tcpUtils/Http.h>
#include <tcpUtils/NetworkException.h>

namespace tcp
{
	/**
	 * @brief
	 * Error that occurred because of a WebSocket connection.
	 */
	class WebSocketException : public NetworkException
	{
	public:
		WebSocketException(std::string message)
			: NetworkException(message)
		{

		}
	};

	enum class WebSocketOp
	{
		/// Message is a continuation to the last message.
		continueLast = 0,
		/// Message sends UTF-8 encoded text.
		sendText = 1,
		/// Message sends raw binary data.
		sendBinary = 2,
		/// Message is a close request.
		close = 8,
		/// Message is a ping message
		ping = 9,
		/// Message is a pong message
		pong = 10,
	};

	/**
	 * @brief
	 * The reason why a WebSocket connection is closed.
	 *
	 * This value is used in HttpWebSocket::Close and HttpWebSocket::receiveClose
	 */
	enum class WebSocketCloseReason : uint16_t
	{
		/// No reason is specified for the close operation.
		undefined = 0,
		normalClosure = 1000,
		goingAway = 1001,
		protocolError = 1002,
		unsupportedData = 1003,
		invalidPayloadData = 1007,
		policyViolated = 1008,
		messageTooBig = 1009,
		unsupportedExtension = 1010,
		internalServerError = 1011,
	};

	/**
	 * @brief
	 * A message received over a WebSocket connection.
	 */
	struct WebSocketMessage
	{
		/**
		 * @brief
		 * The opcode that was sent with this message.
		 */
		WebSocketOp operation = WebSocketOp::sendText;
		/**
		 * @brief
		 * The payload of this message.
		 */
		void* payload = nullptr;
		/**
		 * @brief
		 * The length of the payload, in bytes.
		 */
		size_t payloadLength = 0;
	};

	/**
	 * @brief
	 * A close message received over a WebSocket connection.
	 */
	struct WebSocketCloseMessage : public WebSocketMessage
	{
		/**
		 * @brief
		 * The reason why the WebSocket was closed.
		 *
		 * If no reason was specified, this will be WebSocketCloseReason::undefined
		 */
		WebSocketCloseReason closeReason = WebSocketCloseReason::undefined;
	};

	/**
	 * @brief
	 * WebSocket connection, established over a HTTP web request.
	 *
	 * To keep the connection alive, the user should repeatedly call HttpWebSocket::ReadConnection()
	 * while IsConnected() is true. This blocks and should ideally be done on another thread.
	 *
	 * Calls to ReadConnection() should not happen repeatedly, but otherwise this class is thread-safe.
	 */
	class HttpWebSocket
	{
	public:
		/**
		 * @brief
		 * Starts a WebSocket connection from the given HTTP request.
		 * @param withRequest
		 * The request to use for the connection.
		 * @param randomIntFunction
		 * A function to use for generating random numbers. If left empty,
		 * the standard library std::mt19937 will be used.
		 *
		 * This function runs the given request and attempts to establish a WebSocket connection off of it
		 * by sending the appropriate headers and by using the socket used for the request.
		 *
		 * @throw HttpException
		 * This throws an exception if the given request does not get a valid response.
		 *
		 * @throw WebSocketException
		 * This throws an exception if the remote server did not accept the WebSocket handshake.
		 *
		 * @throw SocketException
		 * This throws an exception if the TCP connection used for the HTTP request fails.
		 */
		HttpWebSocket(HttpRequest* withRequest, std::function<uint32_t()> randomIntFunction = nullptr);

		/**
		 * @brief
		 * Destroys the WebSocket and closes
		 */
		~HttpWebSocket();

		/**
		 * @brief
		 * WebSocket cannot be copied.
		 */
		HttpWebSocket(const HttpWebSocket&) = delete;
		/**
		 * @brief
		 * WebSocket can be moved.
		 */
		HttpWebSocket(HttpWebSocket&&) = default;

		/**
		 * @brief
		 * Sends a message over the WebSocket connection.
		 * @param isText
		 * True if this message should be treated UTF-8 text data, false if it should be treated as raw binary content.
		 * @param data
		 * A pointer to the data to send.
		 * @param length
		 * The length of the data to send, in bytes.
		 */
		void SendMessage(bool isText, void* data, size_t length);

		/**
		 * @brief
		 * Sends a message over the WebSocket connection.
		 * @param op
		 * The WebSocket opcode to use.
		 * @param data
		 * A pointer to the data to send.
		 * @param length
		 * The length of the data to send, in bytes.
		 *
		 * Do not use WebSocketOp::close with this function directly. Use Close or CloseWithError() instead,
		 * as they use the correct message format required by the WebSocket standard.
		 */
		void SendMessage(WebSocketOp op, void* data, size_t length);

		/**
		 * @brief
		 * Returns true if the connection this object represents is still alive.
		 * @return
		 * True if the connection is alive, false if the connection was closed.
		 */
		bool IsConnected() const noexcept
		{
			return isConnectionActive;
		}

		/**
		 * @brief
		 * Set this callback to handle messages sent to this WebSocket.
		 */
		std::function<void(WebSocketMessage)> receiveMessage;
		/**
		 * @brief
		 * Set this callback to handle close messages sent to this WebSocket.
		 *
		 * After a close is received, the client should no longer send any other messages through the socket.
		 */
		std::function<void(WebSocketCloseMessage)> receiveClose;

		/**
		 * @brief
		 * Polls this connection.
		 *
		 * This operation blocks and should ideally be done on another thread.
		 *
		 * @throw WebSocketException
		 * This throws an exception if the remote server sends a malformed WebSocket request or if the
		 * remote connection is closed without a proper close handshake.
		 *
		 * @throw SocketException
		 * This throws an exception if sending the close request over the network fails.
		 */
		void ReadConnection();

		/**
		 * @brief
		 * Closes this connection for the given reason.
		 * @param reason
		 * The standard WebSocket close reason.
		 * @param error
		 * A user-defined message explaining the close reason further.
		 * This string may not be longer than 123 characters.
		 *
		 * @throw WebSocketException
		 * This throws an exception if error is longer than 123 characters.
		 *
		 * @throw SocketException
		 * This throws an exception if sending the close request over the network fails.
		 */
		void CloseWithError(WebSocketCloseReason reason, std::string error);

		/**
		 * @brief
		 * Closes this connection for the given reason.
		 * @param reason
		 * The standard WebSocket close reason.
		 *
		 * @throw SocketException
		 * This throws an exception if sending the close request over the network fails.
		 */
		void Close(WebSocketCloseReason reason)
		{
			CloseWithError(reason, "");
		}

	private:
		Socket* connection = nullptr;
		std::function<uint32_t()> randomIntFunction;
		bool sentClose = false;
		bool isConnectionActive = false;
	};
}