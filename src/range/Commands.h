#pragma once
#include <functional>
#include <string>
#include "ll/api/command/CommandHandle.h"
#include "mc/server/commands/CommandOutput.h"

namespace urde {
// External linkage required by command reflection.
struct SetParams { std::string key; std::string value; };
inline void registerControls(ll::command::CommandHandle& command,
                             std::function<std::string()> status,
                             std::function<std::string(std::string const&, std::string const&)> set,
                             std::function<std::string()> reload) {
    command.overload().text("status").execute([status](CommandOrigin const&, CommandOutput& output) {
        output.success(status());
    });
    command.overload<SetParams>().text("set").required("key").required("value")
        .execute([set](CommandOrigin const&, CommandOutput& output, SetParams const& params) {
            try { output.success(set(params.key, params.value)); }
            catch (std::exception const& error) { output.error(error.what()); }
        });
    command.overload().text("reload").execute([reload](CommandOrigin const&, CommandOutput& output) {
        try { output.success(reload()); }
        catch (std::exception const& error) { output.error(error.what()); }
    });
}
}
