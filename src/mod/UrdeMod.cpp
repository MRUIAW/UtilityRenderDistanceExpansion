#include "mod/UrdeMod.h"
#include "range/Side.h"
#include "ll/api/mod/RegisterHelper.h"

namespace urde {
UrdeMod& UrdeMod::getInstance() {
    static UrdeMod instance;
    return instance;
}
bool UrdeMod::load() { return side::load(getSelf().getConfigDir()); }
bool UrdeMod::enable() { return side::enable(); }
bool UrdeMod::disable() { return side::disable(); }
bool UrdeMod::unload() { return side::disable(); }
}
LL_REGISTER_MOD(urde::UrdeMod, urde::UrdeMod::getInstance());
