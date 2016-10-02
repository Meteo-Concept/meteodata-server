#ifndef MESSAGE_H
#define MESSAGE_H

#include <cassandra.h>

namespace meteodata {
class Message
{
public:
	virtual ~Message() = default;
	/**
	 * @brief Fills in the blanks in a Cassandra insertion prepared
	 * statement
	 *
	 * @param station The station identifier for which the measure was taken
	 * @param statement The prepared statement in which to add the
	 * measured values stored in the current Message
	 */
	virtual void populateDataPoint(const CassUuid station, CassStatement* const statement) const = 0;
};

}

#endif
