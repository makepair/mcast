#ifndef CAST_UTIL_H
#define CAST_UTIL_H

#include <stddef.h>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <iterator>
#include <type_traits>

#include "Logging.h"

static_assert(sizeof(size_t) == 8, "sizeof(size_t) != 8");

inline constexpr size_t PowOfTwo(size_t x) {
  x -= 1;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x |= x >> 32;

  return x + 1;
}

inline constexpr bool IsPowOfTwo(size_t x) {  // exclude zero
  assert(x > 0);
  x |= x << 1;
  x |= x << 2;
  x |= x << 4;
  x |= x << 8;
  x |= x << 16;
  x |= x << 32;
  x -= 1;

  return !(x & x);
}

template <typename InputIt, typename OutputIt>
inline OutputIt move_n(InputIt first, size_t n, OutputIt d_first) {
  return std::move(first, first + n, d_first);
}

///@host_port_str "127.0.0.1:80", "*:80"
inline bool ParseIPAddress(const std::string &host_port_str, std::string *host,
                           uint16_t *port) {
  std::string host_port;
  host_port.reserve(host_port_str.size());
  std::remove_copy(host_port_str.begin(), host_port_str.end(),
                   std::back_inserter(host_port), ' ');

  if (host_port.empty())
    return false;

  auto d = host_port.find_last_of(':');
  if (d == host_port.npos || d == 0 || d == host_port.size() - 1)
    return false;

  *host = host_port.substr(0, d);
  *port = static_cast<uint16_t>(std::atoi(host_port.c_str() + (d + 1)));
  return true;
}

#define CHECK(x)                          \
  do {                                    \
    if (!(x)) {                           \
      LOG_FATA << "CHECK FAILED: " << #x; \
    }                                     \
  } while (false);

#define CHECK_EQ(x, y)                                                         \
  do {                                                                         \
    if ((x) != (y)) {                                                          \
      LOG_FATA << "CHECK_EQ FAILED: " << (x) << "!=" << (y) << ", expr:" << #x \
               << ", " << #y;                                                  \
    }                                                                          \
  } while (false);

#define CHECK_NE(x, y)                                                         \
  do {                                                                         \
    if ((x) == (y)) {                                                          \
      LOG_FATA << "CHECK_NE FAILED: " << (x) << "==" << (y) << ", expr:" << #x \
               << ", " << #y;                                                  \
    }                                                                          \
  } while (false);

#define CHECK_LT(x, y)                                                        \
  do {                                                                        \
    if ((x) >= (y)) {                                                         \
      LOG_FATA << "CHECK_LE FAILED: " << (x) << "<" << (y) << ", expr:" << #x \
               << ", " << #y;                                                 \
    }                                                                         \
  } while (false);

#define CHECK_LE(x, y)                                                        \
  do {                                                                        \
    if ((x) > (y)) {                                                          \
      LOG_FATA << "CHECK_LE FAILED: " << (x) << ">" << (y) << ", expr:" << #x \
               << ", " << #y;                                                 \
    }                                                                         \
  } while (false);

#define CHECK_GT(x, y)                                      \
  do {                                                      \
    if ((x) <= (y)) {                                       \
      LOG_FATA << "CHECK_GT FAILED: " << (x) << "<=" << (y) \
               << ", expr:" << #x;                          \
    }                                                       \
  } while (false);

#define CHECK_GE(x, y)                                                         \
  do {                                                                         \
    if ((x) < (y)) {                                                           \
      LOG_FATA << "CHECK_GE FAILED: " << (x) << "<" << (y) << ", expr:" << #x; \
    }                                                                          \
  } while (false);

#endif  // CAST_UTIL_H
