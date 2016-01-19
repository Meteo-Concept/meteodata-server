#include <iostream>
#include <vector>
#include <algorithm>

struct Message : public std::vector<char>
{
	Message(const char* msg, std::size_t size)
	{
		std::generate_n(begin(), size, [&msg]() { return *(msg++); });
	}
};
