#ifndef RBX_SIGNAL_HPP
#define RBX_SIGNAL_HPP

#include "util/thread.hpp"

#include <list>

namespace rubinius {
  class VM;
  class CallFrame;

  class SignalHandler {
    VM* vm_;
    std::list<int> pending_signals_;
    thread::SpinLock lock_;

  public:
    SignalHandler(VM* vm);
    void add_signal(int sig);
    void handle_signal(int sig);
    static void signal_tramp(int sig);

    void deliver_signals(CallFrame* call_frame);
  };
}

#endif
