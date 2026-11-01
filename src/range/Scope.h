#pragma once
#include <utility>

namespace urde {
// Stack-owned thread-local context, including null scopes: nested unrelated work
// cannot accidentally inherit its caller's permission. Restores on every exit.
template <class T>
class Scope {
    T*& slot;
    T* previous;
public:
    Scope(T*& slot, T* value) : slot(slot), previous(std::exchange(slot, value)) {}
    ~Scope() { slot = previous; }
    Scope(Scope const&) = delete;
    Scope& operator=(Scope const&) = delete;
};
}
