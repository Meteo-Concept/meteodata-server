#ifndef RANGE_H
#define RANGE_H

#include <iostream>
#include <limits>
#include <optional>

template<typename Numeric>
struct Range
{
	std::optional<Numeric> _begin;
	std::optional<Numeric> _end;
	bool _beginIncluded;
	bool _endIncluded;

	static std::optional<Numeric> unbound() {
		return std::optional<Numeric>();
	}
};

#endif /* RANGE_H */
