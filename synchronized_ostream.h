#pragma once
#include "locked_ostream.h"

class synchronized_ostream
{
	std::mutex cout_mu;
public:
	synchronized_ostream() = default;
	synchronized_ostream(const synchronized_ostream& other) = delete;
	synchronized_ostream& operator=(const synchronized_ostream& other) = delete;

	template <typename T>
	locked_ostream operator<<(const T& lhs)
	{
		std::unique_lock<std::mutex> ul(cout_mu);
		std::cout << lhs;
		return locked_ostream(std::move(ul));
	}

	synchronized_ostream& operator<<(std::ostream& (*f)(std::ostream &))
	{
		f(std::cout);
		return *this;
	}

	synchronized_ostream& operator<<(std::ostream& (*f)(std::ios &))
	{
		f(std::cout);
		return *this;
	}

	synchronized_ostream& operator<<(std::ostream& (*f)(std::ios_base &))
	{
		f(std::cout);
		return *this;
	}
};