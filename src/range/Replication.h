#pragma once
#include <memory>
#include <vector>
#include "mc/world/level/ChunkPos.h"

namespace urde::replication {
// Game-pointer-free immutable policy, shared by dedicated and hosting consumers.
struct Range {
    int actorInterestChunks = 0;
    std::vector<ChunkPos> offsets;
    void rebuild(int radius);
};
using Provider = std::shared_ptr<Range const> (*)();
bool enable(Provider provider);
void invalidate();
void disable();
bool active();
int effectiveRadius();
}
