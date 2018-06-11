#ifndef CAST_LOGGING_H_
#define CAST_LOGGING_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <array>
#include <cassert>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

#include "Noncopyable.h"

namespace mcast {

enum class LogLevel { kTrace = 1, kInfo, kWarning, kFata, kNum };

template <std::size_t Size, typename Char = char>
class LogStreamBuffer : public std::basic_streambuf<Char> {
 public:
  using Base = std::basic_streambuf<Char>;
  using int_type = typename Base::int_type;

  static_assert(Size > 1, "");

  LogStreamBuffer() { Base::setp(buffer_.begin(), buffer_.end() - 1); }

  const Char *c_string() {
    auto const s = avail();
    if (s > Size - 1) {
      buffer_[Size - 1] = '\0';
    } else {
      buffer_[s] = '\0';
    }
    return buffer_.data();
  }

  // ignores overflow.
  virtual int_type overflow(int_type ch) override { return ch; }

  size_t avail() const {
    assert(Base::pptr() >= Base::pbase());
    return static_cast<size_t>(Base::pptr() - Base::pbase());
  }

 private:
  std::array<Char, Size> buffer_;
};

template <std::size_t Size, typename Char = char>
class LogStream : public std::basic_ostream<Char> {
 public:
  typedef std::basic_ostream<Char> Base;

  LogStream() : Base(NULL) { Base::rdbuf(&streambuf_); }

  const Char *c_string() { return streambuf_.c_string(); }
  size_t avail() const { return streambuf_.avail(); }

 private:
  LogStreamBuffer<Size, Char> streambuf_;
};

template <typename Char = char>
class NullStreamBuffer : public std::basic_streambuf<Char> {
 public:
  using Base = std::basic_streambuf<Char>;
  using int_type = typename Base::int_type;

  // ignores overflow.
  int_type overflow(int_type ch) override { return ch; }
  std::streamsize xsputn(const Char *s, std::streamsize count) override {
    return count;
  }
};

template <typename Char = char>
class LogNullStream : public std::basic_ostream<Char> {
 public:
  typedef std::basic_ostream<Char> Base;

  LogNullStream() : Base(NULL) { Base::rdbuf(&streambuf_); }

 private:
  NullStreamBuffer<Char> streambuf_;
};

class Logger {
 public:
  typedef void (*Printer)(LogLevel, const char *filename, const char *line,
                          const char *msg, size_t msg_size);

  Logger(LogLevel lev, const char *file_name, const char *line)
      : level_(lev), file_name_(file_name), line_(line) {}

  ~Logger() {
    try {
      if (level_ == LogLevel::kFata || s_level <= level_) {
        s_printer(level_, file_name_, line_, stream_.c_string(),
                  stream_.avail());
      }
    } catch (...) {
    }
  }

  std::ostream &stream() {
    if (level_ == LogLevel::kFata || s_level <= level_) {
      return stream_;
    } else {
      return s_null_stream;
    }
  }

  static LogLevel s_level;
  static Printer s_printer;

 private:
  static LogNullStream<char> s_null_stream;
  LogStream<1024> stream_;
  LogLevel level_;
  const char *file_name_;
  const char *line_;
};

inline Logger::Printer SetLogPrinter(Logger::Printer p) {
  auto oldp = Logger::s_printer;
  Logger::s_printer = p;
  return oldp;
}

inline LogLevel SetLogLevel(LogLevel lev) {
  auto oldl = Logger::s_level;
  Logger::s_level = lev;
  return oldl;
}

inline std::string ErrnoText() {
  char buf[512];
  buf[0] = '\0';
  return strerror_r(errno, buf, sizeof buf);
}

}  // namespace mcast

#define LINE_STR_CAT(x) #x
#define LINE_STR(x) LINE_STR_CAT(x)

#define ERRNO_TEXT \
  (mcast::ErrnoText() + '(' + __FILE__ + ':' + LINE_STR(__LINE__) + ')')

#define LOG_ERRNO()                                                      \
  {                                                                      \
    char buf[512];                                                       \
    buf[0] = '\0';                                                       \
    mcast::Logger(mcast::LogLevel::kWarning, __FILE__, LINE_STR(__LINE__)) \
            .stream()                                                    \
        << "errno : " << strerror_r(errno, buf, sizeof buf);             \
  }

#define LOG_TRACE \
  mcast::Logger(mcast::LogLevel::kTrace, __FILE__, LINE_STR(__LINE__)).stream()

#define LOG_INFO \
  mcast::Logger(mcast::LogLevel::kInfo, __FILE__, LINE_STR(__LINE__)).stream()

#define LOG_WARN \
  mcast::Logger(mcast::LogLevel::kWarning, __FILE__, LINE_STR(__LINE__)).stream()

#define LOG_FATA \
  mcast::Logger(mcast::LogLevel::kFata, __FILE__, LINE_STR(__LINE__)).stream()

#endif  // CAST_LOGGING_H_
