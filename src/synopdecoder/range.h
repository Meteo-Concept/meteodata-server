#ifndef RANGE_H
#define RANGE_H

#include <iostream>
#include <limits>
#include <experimental/optional>

template<typename Numeric>
struct Range
{
	std::experimental::optional<Numeric> _begin;
	std::experimental::optional<Numeric> _end;
	bool _beginIncluded;
	bool _endIncluded;

	static std::experimental::optional<Numeric> unbound() {
		return std::experimental::optional<Numeric>();
	}
};

#endif /* RANGE_H */
