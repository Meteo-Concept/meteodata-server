#ifndef PARSER_H
#define PARSER_H

#include "synop_message.h"

#include <vector>

class Parser
{
private:
	SynopMessage _message;
	std::vector<std::string> _groups;

	bool parseSection0(decltype(_groups)::iterator& it);
	bool parseSection1(decltype(_groups)::iterator& it);
	bool parseSection2(decltype(_groups)::iterator& it);
	bool parseSection3(decltype(_groups)::iterator& it);
	bool parseSection4(decltype(_groups)::iterator& it);
	bool parseSection5(decltype(_groups)::iterator& it);

public:
	Parser();

	bool parse(std::istream& in);
	const SynopMessage& getDecodedMessage() {
		return _message;
	}
};

#endif /* PARSER_H */
