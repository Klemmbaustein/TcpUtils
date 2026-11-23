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
	std::string Lower(std::string Input)
	{
		std::transform(Input.begin(), Input.end(), Input.begin(),
			[](unsigned char c) { return std::tolower(c); });
		return Input;
	}

	/**
	* @brief
	* Removes whitespace from the start and end of the string.
	*/
	[[nodiscard]]
	std::string Trim(std::string Input)
	{
		Input.erase(Input.begin(), std::find_if(Input.begin(), Input.end(), [](unsigned char ch) {
			return !std::isspace(ch) && ch != '\r';
		}));

		Input.erase(std::find_if(Input.rbegin(), Input.rend(), [](unsigned char ch) {
			return !std::isspace(ch) && ch != '\r';
		}).base(), Input.end());
		return Input;
	}
}