#ifndef MESSAGE_H
#define MESSAGE_H

#include <cassandra.h>

namespace meteodata {
class Message
{
public:
	virtual ~Message() = default;
	virtual void populateDataPoint(const CassUuid station, CassStatement* const statement) const = 0;
};

}

#endif
