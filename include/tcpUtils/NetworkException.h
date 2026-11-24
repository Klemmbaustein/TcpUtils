#pragma once
#include <stdexcept>
#include <string>

namespace tcp
{
	/**
	 * @brief
	 * Base class for every kind of exception that occurred through network operations.
	 */
	class NetworkException : std::exception
	{
	public:
		NetworkException(std::string message)
			: error(message)
		{

		}

		const char* what() const noexcept override
		{
			return error.c_str();
		}

	private:
		std::string error;
	};
}