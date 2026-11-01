#pragma once
#include "mod/UrdeMod.h"
#include "ll/api/memory/Symbol.h"

namespace urde {
// Resolve the named native symbols before touching delay-imported hook pointers.
// Missing symbols fail enable, with no address/signature-scanning fallback.
inline bool requireSymbol(char const* symbol) {
    if (ll::memory::SymbolView{symbol}.resolve()) return true;
    UrdeMod::getInstance().getSelf().getLogger().error("Required native symbol unavailable: {}. Stopping enable; no fallback.",symbol);
    return false;
}
template <class Hook>
bool installHook(char const* name) {
    int result = Hook::hook();
    if (result == 0) return true;
    UrdeMod::getInstance().getSelf().getLogger().error("Required hook unavailable: {} ({}). Stopping enable; no fallback.",name,result);
    return false;
}
}
