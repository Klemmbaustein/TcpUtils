#pragma once
#include <string>
#include <algorithm>

namespace tcp::util
{

	/**
	* @brief
	* Returns the given string where all characters have been converted to lower case.
	*/
	[[nodiscard]]
	inline std::string Lower(std::string input)
	{
		std::transform(input.begin(), input.end(), input.begin(),
			[](unsigned char c) { return std::tolower(c); });
		return input;
	}

	/**
	* @brief
	* Removes whitespace from the start and end of the string.
	*/
	[[nodiscard]]
	inline std::string Trim(std::string input)
	{
		input.erase(input.begin(), std::find_if(input.begin(), input.end(), [](unsigned char ch) {
			return !std::isspace(ch) && ch != '\r';
		}));

		input.erase(std::find_if(input.rbegin(), input.rend(), [](unsigned char ch) {
			return !std::isspace(ch) && ch != '\r';
		}).base(), input.end());
		return input;
	}
}