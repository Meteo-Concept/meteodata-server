#include <iostream>
#include <vector>
#include <algorithm>

class Message : public std::vector<char>
{
public:
	Message(const char* msg, std::size_t size)
	{
		std::generate_n(begin(), size, [&msg]() { return *(msg++); });
	}

	enum class MessageType {
		LOOP1,
		LOOP2
	};

	MessageType getType() { return _type; }


private:
	MessageType _type;
};
