
#ifndef NCLOUD_UTIL_H
#define NCLOUD_UTIL_H

#include "stdincludes.h"
#include "json/json.h"

template<typename ... Args>
std::string string_format(const std::string& format, Args ... args){
    size_t size = 1 + snprintf(nullptr, 0, format.c_str(), args ...);
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1);
}

// C++11 does not support make_unique(), have to wait for C++14

template<typename T, typename ... Args>
    std::unique_ptr<T> make_unique_helper(std::false_type, Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<typename T, typename ... Args>
std::unique_ptr<T> make_unique_helper(std::true_type, Args&&... args) {
    static_assert(std::extent<T>::value == 0,
            "make_unique<T[N]>() is forbidden, please use make_unique<T[]>(),");
    typedef typename std::remove_extent<T>::type U;
    return std::unique_ptr<T>(new U[sizeof...(Args)]{std::forward<Args>(args)...});
}

template<typename T, typename ... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return make_unique_helper<T>(
        std::is_array<T>(),std::forward<Args>(args)... );
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


struct RawMessage;
class Address;

// Takes a json structure, serializes it, and then returns a RawMessage.
std::unique_ptr<RawMessage> rawMessageFromJson(const Address &fromAddress,
                                               const Address &toAddress,
                                               Json::Value root);

// Takes a RawMessage, unserializes the message, and creates a Json value.
Json::Value jsonFromRawMessage(const RawMessage *raw);

// Takes a binary stream, serializes it, and then returns a RawMessage
std::unique_ptr<RawMessage> rawMessageFromStream(const Address &from,
                                                 const Address &to,
                                                 stringstream &os);

// Functions to write various data types to a binary stream.
template<class T>
void write_raw(std::ostream& os, const T& value)
{
    os.write(reinterpret_cast<const char *>(&value), sizeof(value));
}

template<class T>
void read_raw(std::istream& is, T& value)
{
    is.read(reinterpret_cast<char *>(&value), sizeof(value));
}

template<class T>
T read_raw(std::istream& is)
{
    T value;
    read_raw(is, value);
    return value;
}

void write_chars_raw(std::ostream& os, const char *p, size_t length);
void read_chars_raw(std::istream& is, char *p, size_t length);

#endif  /* NCLOUD_UTIL_H */
