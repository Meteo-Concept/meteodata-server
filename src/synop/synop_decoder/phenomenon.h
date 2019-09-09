#ifndef PHENOMENON_H
#define PHENOMENON_H

#include <string>
#include <chrono>

struct Phenomenon
{
	using TimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::minutes>;

	std::string _description;
	std::experimental::optional<TimePoint> _begin;
	std::experimental::optional<TimePoint> _end;
};

template<typename Numeric>
struct Record : public Phenomenon
{
	Numeric value;
};

#endif /* PHENOMENON_H */
