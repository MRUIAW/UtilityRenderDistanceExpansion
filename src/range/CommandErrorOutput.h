#pragma once
#include "range/CommandErrors.h"
#include "ll/api/event/command/ExecuteCommandEvent.h"
#include "ll/api/service/Bedrock.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"

namespace urde {
// Keep the old explanatory errors after native rejection without advertising
// error-only overloads in the compact real grammar. Permissions and parsing have
// already run; this cannot execute a command or change its result/configuration.
inline void explainCommandMisuse(ll::event::ExecutedCommandEvent& event, bool client) {
    if (event.result().mSuccess || event.suppressOutput()) return;
    auto registry = ll::service::getCommandRegistry(client);
    if (!registry || event.minecraftCommands().mRegistry.get() != &*registry) return;
    auto const& context = event.commandContext();
    if (!context.mOrigin || (!client && context.mOrigin->getPermissionsLevel() < CommandPermissionLevel::GameDirectors)) return;
    auto message = commandMisuse(context.mCommand,client);
    if (message.empty()) return;
    CommandOutput output(CommandOutputType::AllOutput);
    output.error(message);
    event.minecraftCommands().handleOutput(*context.mOrigin,output);
}
}
