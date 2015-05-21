#include <memory>
#include <iostream>
#include <string>
#include <cstdio>

//using namespace std; //Don't if you're in a header-file

template<typename ... Args>
std::string string_format(const std::string& format, Args ... args){
    size_t size = 1 + snprintf(nullptr, 0, format.c_str(), args ...);
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1);
}

bool isLittleEndian();

template<typename T, typename ...Args>
std::unique_ptr<T> make_unique( Args&& ...args )
{
    return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) );
}

// Generic exception
class AppException : public std::exception
{
public:
	AppException(const char *description)
	{
		desc = description;
	}

	virtual const char *what() const throw()
	{
		return desc.c_str();
	}

protected:
	std::string desc;
};

