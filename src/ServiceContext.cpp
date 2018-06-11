#include "ServiceContext.h"

#include <utility>

#include "Service.h"
#include "ServiceHandle.h"
#include "System.h"
#include "util/Logging.h"

#ifdef VALGRIND
extern "C" {
#include "valgrind.h"
}
#endif

namespace mcast {

static_assert(sizeof(void*) == 8, "must be LP64");

ServiceContext::ServiceContext() : srv(nullptr), stack(nullptr), stack_size(0) {}

ServiceContext::~ServiceContext() {
#ifdef VALGRIND
  VALGRIND_STACK_DEREGISTER(valg_ret);
#endif

  if (stack)
    munmap(stack, static_cast<size_t>(stack_size));
}

ServiceContext::ServiceContextPtr ServiceContext::Create(Service* srv, System* /*sys*/,
                                                         int stacksize,
                                                         void (*ServiceMain)(intptr_t)) {
  stacksize = (stacksize + kStackAlignmentMask) & ~kStackAlignmentMask;

  void* page_addr = mmap(NULL, static_cast<size_t>(stacksize), PROT_READ | PROT_WRITE,
                         MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (MAP_FAILED == page_addr) {
    throw std::bad_alloc();
  }

  std::shared_ptr<ServiceContext> sc(new ServiceContext);
  sc->srv = srv;
  sc->stack = static_cast<uint8_t*>(page_addr);
  sc->stack_size = stacksize;

#ifdef VALGRIND
  sc->valg_ret = VALGRIND_STACK_REGISTER(sc->stack, sc->stack + sc->stack_size);
#endif

  uint8_t* sp = sc->stack + sc->stack_size;
  auto const rstack_size = sp - sc->stack;

  LOG_TRACE << " ServiceContext::Create " << srv->name()
            << " sp:" << static_cast<void*>(sp) << ", stack size:" << rstack_size;

  sc->ucontext = make_fcontext(sp, static_cast<size_t>(rstack_size), ServiceMain);
  CHECK(sc->ucontext);
  return sc;
}

}  // namespace mcast
