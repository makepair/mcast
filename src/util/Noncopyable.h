#ifndef CAST_NOCCOPYABLE_H_
#define CAST_NOCCOPYABLE_H_

namespace mcast {

class Noncopyable {
 public:
  Noncopyable() = default;

 protected:
  Noncopyable(const Noncopyable &) = delete;
  Noncopyable &operator=(const Noncopyable &) = delete;
};


}

#endif