#pragma once
#include <iostream>
#include <mutex>

// locked_ostream.h: thread-safe stdin

class locked_ostream
{
	friend class synchronized_ostream;

	std::unique_lock<std::mutex> ul;

	explicit locked_ostream(std::unique_lock<std::mutex>&& ul_)
	{
		ul = std::move(ul_);
	}
public:

	template <typename T>
	locked_ostream& operator<<(const T& lhs)
	{
		std::cout << lhs;
		return *this;
	}

	// Stuff like std::endl
	locked_ostream& operator<<(std::ostream& (*f)(std::ostream &))
	{
		f(std::cout);
		return *this;
	}

	// Stuff like good, eof, bad, clear?
	locked_ostream& operator<<(std::ostream& (*f)(std::ios &))
	{
		f(std::cout);
		return *this;
	}


	// Flags (std::hex, std::boolalpha, ...)
	locked_ostream& operator<<(std::ostream& (*f)(std::ios_base &))
	{
		f(std::cout);
		return *this;
	}
};