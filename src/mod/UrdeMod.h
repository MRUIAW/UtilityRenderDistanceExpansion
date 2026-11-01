#pragma once

#include "ll/api/mod/NativeMod.h"

namespace urde {
class UrdeMod {
public:
    static UrdeMod& getInstance();
    UrdeMod() : mSelf(*ll::mod::NativeMod::current()) {}
    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }
    bool load();
    bool enable();
    bool disable();
    bool unload();
private:
    ll::mod::NativeMod& mSelf;
};
}
