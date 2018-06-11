#ifndef CAST_UTIL_CONFIG_H
#define CAST_UTIL_CONFIG_H

namespace mcast {

static constexpr int kCacheLineSize = 64;  // bytes
static constexpr int kPageSize = 4096;     // bytes
static constexpr int kWordSize = sizeof(void*);     // bytes
static constexpr int kPointerSize = sizeof(void*);     // bytes


}

#endif  // CAST_UTIL_CONFIG_H
