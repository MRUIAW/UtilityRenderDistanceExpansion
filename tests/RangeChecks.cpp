#include "range/Limits.h"
#include "range/Scope.h"
#include "range/Config.h"
#include "range/CommandValues.h"
#include "range/CommandErrors.h"
#include "range/MovementPairs.h"
#include "client/ClientConfig.h"
#include <cassert>
#include <iostream>
#include <limits>

struct CheckConfig {
    float range = 128;
    void validate() const { urde::validateRange(range,urde::MaxBlocks); }
    void set(std::string const& key, std::string const& value) {
        if (key != "range") throw std::invalid_argument("unknown key");
        range = urde::parseRange(value,urde::MaxBlocks);
    }
    nlohmann::json encode() const { return {{"range",urde::encodeRange(range)}}; }
    static CheckConfig decode(nlohmann::json const& json) {
        return {urde::readRange(json,"range",urde::MaxBlocks)};
    }
};

int main() {
    using namespace urde;
    assert(commandMisuse("/urde set",false) == "set requires a key and value: set <key> <value>");
    assert(commandMisuse(" urde reload update_interval 4 ",false) == "status/reload take no key or value");
    assert(commandMisuse("urdec set entity_shape 4",true) == "entity_shape requires circle or square");
    assert(commandMisuse("urdec set entity_shape",true) == "set requires a value: set <key> <value>");
    assert(commandMisuse("urdec set entity_shape vanilla",true) == "vanilla is not supported for entity_shape");
    assert(commandMisuse("urdec set entity_range square",true) == "circle/square require entity_shape");
    for (auto line : {"urde status","urde reload","urde set update_interval 4","urde set update_interval vanilla",
                      "urdec reload alpha 4","other set"})
        assert(commandMisuse(line,false).empty());
    assert(commandMisuse("urdec set entity_shape circle",true).empty());
    assert(commandMisuse("urdec set entity_shape square",true).empty());
    auto policy = movement::Settings::decode({{"full_rate_types",{"minecraft:villager"}},
                                              {"full_rate_tags",{"smooth"}}},4);
    assert(policy.updateInterval == 4 && policy.typeIds.contains("minecraft:villager"));
    assert(!policy.typeIds.contains(movement::PlayerType));
    assert(movement::Settings::decode({},4).typeIds.contains("minecraft:player"));
    assert(movement::Settings::decode({{"full_rate_types",nlohmann::json::array()}},4).typeIds.empty());
    assert(movement::Settings::decode({{"full_rate_players","ignored"}},4).typeIds.contains("minecraft:player"));
    assert(!policy.encode().contains("full_rate_players"));
    auto defaults = movement::Settings::decode({},4);
    assert(defaults.select([] { return std::string("minecraft:player"); },[](auto const&) { return false; }).interval == 1);
    assert(defaults.select([] { return std::string("Minecraft:player"); },[](auto const&) { return false; }).interval == 4);
    assert(client::Settings::decode({{"entity_range",128},{"block_entity_range",128},{"terrain_detail",128}}).movement.updateInterval == 1);
    for (uint64_t native = 0; native <= 12; ++native) for (unsigned n = 1; n <= 10; ++n) {
        auto result = movement::decide(native,100,99,n);
        assert(result.interval == std::min(native,uint64_t(n)));
        assert(result.outcome == (native > n ? movement::Outcome::capped : movement::Outcome::unchanged));
    }
    for (unsigned n = 1; n <= 10; ++n) {
        assert(movement::decide(std::nullopt,100,std::nullopt,n).interval == n);
        assert(!movement::decide(std::nullopt,100,100,n).interval);
        assert(!movement::decide(std::nullopt,100+n-1,100,n).interval);
        assert(movement::decide(std::nullopt,100+n,100,n).interval == n);
        assert(!movement::decide(std::nullopt,99,100,n).interval);
        auto late = movement::decide(std::nullopt,100+n+2,100,n);
        assert(late.interval == std::min(uint64_t(n+2),movement::OpenedPayloadCap) && late.late);
        assert(late.outcome == movement::Outcome::opened);
        assert(!movement::decide(std::nullopt,100+n,100,n).late);
    }
    assert(movement::decide(std::nullopt,1000,1,4).interval == 10); // unknown policy: sane payload cap
    assert(movement::decide(7,100,std::nullopt,4).interval == 4); // first send
    assert(movement::decide(7,104,100,4).interval == 4); // on time
    auto lateNative = movement::decide(7,106,100,4);
    assert(lateNative.interval == 6 && lateNative.late && lateNative.outcome == movement::Outcome::capped);
    lateNative = movement::decide(7,110,100,4);
    assert(lateNative.interval == 7 && lateNative.late && lateNative.outcome == movement::Outcome::unchanged);
    assert(movement::decide(0,110,100,4).interval == 0 && movement::decide(1,110,100,4).interval == 1);
    assert(movement::decide(3,110,100,4).interval == 3); // preserve all native V<=N
    assert(movement::decide(7,100,100,4).interval == 4); // repeated native engaged never delayed
    assert(movement::decide(7,99,100,4).interval == 4); // no unsigned elapsed underflow
    assert(movement::decide(std::nullopt,103,100,1).interval == 3); // full-rate but attempted late
    assert(movement::decide(7,103,100,1).interval == 3);
    assert(!movement::decide(7,200,100,0).late); // vanilla is fully unchanged/unobserved by lever
    for (unsigned n = 1; n <= 10; ++n) for (uint64_t v = 0; v <= 15; ++v)
        for (uint64_t gap = 0; gap <= 20; ++gap) {
            auto result = movement::decide(v,100+gap,100,n);
            assert(result.interval && *result.interval <= v); // native never slowed or withheld
            assert(result.late == (gap > n));
            assert((result.outcome == movement::Outcome::capped) == (*result.interval < v));
        }
    auto noTypeQuery = []() -> std::string { assert(false); return {}; };
    auto noTagQuery = [](auto const&) -> bool { assert(false); return false; };
    policy.set("update_interval","vanilla");
    assert(policy.select(noTypeQuery,noTagQuery).interval == 0);
    assert(movement::decide(10,100,std::nullopt,0).interval == 10);
    assert(!movement::decide(std::nullopt,100,std::nullopt,0).interval);
    policy.set("update_interval","4");
    policy.set("update_interval","4.0");
    for (auto text : {"0","-1","11","4.5","nan","1junk","nonsense","1e999"}) {
        bool rejected=false;
        try { auto copy=policy; copy.set("update_interval",text); }
        catch (std::exception const& error) { rejected=std::string(error.what()).find("whole ticks 1..10") != std::string::npos; }
        assert(rejected);
    }
    for (auto invalid : {nlohmann::json{{"update_interval",0}}, {{"update_interval",1.5}},
                         {{"full_rate_types","minecraft:pig"}},
                         {{"full_rate_tags",{""}}}, {{"full_rate_tags",{1}}}}) {
        bool rejected=false;
        try { (void)movement::Settings::decode(invalid,4); } catch (std::exception const&) { rejected=true; }
        assert(rejected);
    }
    assert(movement::Settings::decode(policy.encode(),1).typeIds == policy.typeIds);
    auto classify = [&](std::string type, bool tag) {
        return policy.select([&] { return type; },[&](auto const&) { return tag; });
    };
    assert(classify("minecraft:villager",false).interval == 1);
    assert(classify("minecraft:pig",true).fullRate);
    assert(classify("minecraft:pig",false).interval == 4);
    movement::PairTable<1> pairs;
    auto* pair = pairs.touch(1,2,100);
    assert(pair && !pair->lastEngaged());
    pair->sent(100);
    assert(pairs.touch(1,2,101)->lastEngaged() == 100);
    assert(!pairs.touch(2,1,101)->lastEngaged()); // ordered sender/recipient identity
    assert(pairs.touch(3,2,101) && pairs.touch(4,2,101));
    assert(!pairs.touch(5,2,101)); // saturation falls back, no live eviction
    pairs.prune(299);
    assert(pairs.size() == 4);
    pairs.prune(301);
    assert(pairs.size() == 0);
    pair = pairs.touch(1,2,400); pair->sent(400);
    assert(!pairs.touch(1,2,600)->lastEngaged()); // exact stale boundary
    pairs.clear(); assert(pairs.size() == 0);
    // Simulate native returning 7 only seven ticks after the last final send.
    // Each lever-opened result resets BOTH observed final cadence and the native
    // recipient timestamp; N=1/2/4 must not be delayed by the stale native gate.
    for (unsigned n : {1u,2u,4u,10u}) {
        std::optional<uint64_t> last;
        uint64_t previous = 0;
        for (uint64_t tick = 0; tick < 100; ++tick) {
            auto native = !last || tick-*last >= 7 ? std::optional<uint64_t>(7) : std::nullopt;
            auto result = movement::decide(native,tick,last,n);
            if (result.interval) {
                if (last) assert(tick-previous == std::min(n,7u));
                previous = tick; last = tick;
            }
        }
    }
    // Sparse attempts: touch tracks visibility, sent tracks only engaged results.
    // Payload follows the actual gap; the opening threshold is still N.
    movement::PairTable<1> sparsePairs;
    auto attempt = [&](uint64_t tick, std::optional<uint64_t> native) {
        auto* record = sparsePairs.touch(10,20,tick);
        assert(record);
        auto result = movement::decide(native,tick,record->lastEngaged(),4);
        if (result.interval) record->sent(tick);
        return result;
    };
    assert(attempt(100,std::nullopt).interval == 4);
    assert(!attempt(102,std::nullopt).interval);
    auto sparseLate = attempt(106,std::nullopt);
    assert(sparseLate.interval == 6 && sparseLate.late);
    assert(attempt(107,1).interval == 1); // native faster send still resets last
    assert(!attempt(110,std::nullopt).interval);
    assert(attempt(111,std::nullopt).interval == 4);
    assert(attempt(140,std::nullopt).interval == 10);
    // Publication pins the complete movement policy including prebuilt type IDs.
    auto movementFile = std::filesystem::current_path()/"build"/"checks"/"movement.json";
    auto clientJson = client::Settings{}.encode();
    clientJson.update(policy.encode());
    writeJson(movementFile,clientJson);
    ConfigStore<client::Settings> movementStore;
    movementStore.load(movementFile);
    auto oldMovement = movementStore.accept();
    clientJson["update_interval"] = 10;
    clientJson["full_rate_types"] = {"minecraft:pig"};
    writeJson(movementFile,clientJson);
    (void)movementStore.reload();
    assert(movementStore.desired()->movement.typeIds.contains("minecraft:pig"));
    assert(oldMovement->movement.typeIds.contains("minecraft:villager"));
    assert(oldMovement->movement.updateInterval == 4 && movementStore.accept()->movement.updateInterval == 10);
    // Loading legacy data is nonmutating, notices once, and saving drops the key
    // while retaining an explicitly empty type list.
    auto legacyFile = movementFile.parent_path()/"legacy-movement.json";
    auto legacyJson = client::Settings{}.encode();
    legacyJson["full_rate_players"] = false;
    legacyJson["full_rate_types"] = nlohmann::json::array();
    writeJson(legacyFile,legacyJson);
    int legacyNotices = 0;
    ConfigStore<client::Settings> legacyStore;
    legacyStore.load(legacyFile,[&] { ++legacyNotices; });
    assert(legacyNotices == 1 && readJson(legacyFile).contains("full_rate_players"));
    legacyStore.load(legacyFile,[&] { ++legacyNotices; });
    assert(legacyNotices == 1 && legacyStore.desired()->movement.typeIds.empty());
    (void)legacyStore.reload();
    assert(!readJson(legacyFile).contains("full_rate_players"));
    assert(readJson(legacyFile).at("full_rate_types").empty());
    assert(diskCount(8) == 197);
    assert(localReplicationChunks(0,512) == 0);
    assert(localReplicationChunks(0.01f,0) == 0);
    assert(localReplicationChunks(std::numeric_limits<float>::denorm_min(),0) == 0);
    assert(localReplicationChunks(16,16) == 0);
    assert(localReplicationChunks(64,0) == 5);
    assert(localReplicationChunks(std::nextafter(64.0f,128.0f),0) == 5);
    assert(localReplicationChunks(80,0) == 6);
    int nativeCount = 0;
    for (int x = -5; x <= 5; ++x) for (int z = -5; z <= 5; ++z) {
        if (x*x+z*z <= 17) { ++nativeCount; assert(x*x+z*z <= 5*5); }
    }
    assert(nativeCount == 57 && 4*4+1 > 4*4); // radius4 misses native (4,1)
    assert(localReplicationChunks(128,0) == 9);
    assert(localReplicationChunks(128,16) == 10);
    assert(diskCount(10) == 317);
    assert(localReplicationChunks(MaxBlocks,16) == MaxChunks);
    assert(replicationFits(512,16) && !replicationFits(512,512));
    for (float range : {16.0f,32.0f,64.0f,128.0f,144.0f,528.0f}) {
        int r = requiredLocalChunks(range,0);
        for (int x = 0; x <= int(std::ceil((range+8)/16)); ++x)
            for (int z = 0; z <= int(std::ceil((range+8)/16)); ++z) {
                double gx = std::max(16.0*x-8.0,0.0), gz = std::max(16.0*z-8.0,0.0);
                bool required = gx*gx+gz*gz <= double(range)*range;
                assert(!required || x*x+z*z <= r*r);
            }
    }
    auto oldClient = nlohmann::json{{"entityRange",512},{"block_entity_range",128},{"terrainDetail","vanilla"}};
    auto settings = client::Settings::decode(oldClient);
    assert(settings.entityShape == UrdeEntityShape::circle && settings.replicationMargin == 16);
    assert(settings.entityRange == 512 && settings.terrainDetail == 0);
    settings.set("replication_margin","0");
    settings.set("entity_shape","square");
    assert(settings.encode().at("replication_margin") == 0);
    assert(client::Settings::decode(settings.encode()).entityShape == UrdeEntityShape::square);
    for (auto [key,value] : {std::pair{"entity_shape","sphere"}, {"replication_margin","vanilla"},
                            {"replication_margin","nan"}, {"replication_margin","-1"}, {"replication_margin","30"}}) {
        bool rejected = false;
        try { auto candidate = settings; candidate.set(key,value); } catch (std::exception const&) { rejected = true; }
        assert(rejected);
    }
    for (auto field : {nlohmann::json("vanilla"),nlohmann::json(-1),nlohmann::json(nullptr),nlohmann::json(1e-100)}) {
        bool rejected = false;
        try { auto json = settings.encode(); json["replication_margin"] = field; (void)client::Settings::decode(json); }
        catch (std::exception const&) { rejected = true; }
        assert(rejected);
    }
    bool unknownRejected = false;
    try { auto json = oldClient; json["unexpected"] = 1; (void)client::Settings::decode(json); }
    catch (std::exception const&) { unknownRejected = true; }
    assert(unknownRejected);
    assert(diskCount(4) == 49);
    assert(diskCount(MaxChunks) <= (2*MaxChunks+1)*(2*MaxChunks+1));
    assert(parseRange("vanilla",MaxBlocks) == 0);
    assert(parseRange("128",MaxBlocks) == 128);
    assert(parseRange("32",MaxServerChunks,true) == 32);
    assert(commandNumberText(96.0f) == "96");
    assert(commandNumberText(8) == "8");
    for (float value : {0.0000001f, 0.01f, 16.01f, 96.5f, 128.0f, MaxBlocks,
                        std::numeric_limits<float>::min(), std::nextafter(MaxBlocks,0.0f)})
        assert(parseRange(commandNumberText(value),MaxBlocks) == value);
    for (float value : {0.0f, -1.0f, std::nextafter(MaxBlocks,1024.0f),
                        std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        bool rejected = false;
        try { (void)parseRange(commandNumberText(value),MaxBlocks); }
        catch (std::exception const&) { rejected = true; }
        assert(rejected);
    }
    for (int value : {1,8,32}) assert(parseRange(commandNumberText(value),MaxServerChunks,true) == value);
    for (int value : {0,-1,33}) {
        bool rejected = false;
        try { (void)parseRange(commandNumberText(value),MaxServerChunks,true); }
        catch (std::exception const&) { rejected = true; }
        assert(rejected);
    }
    for (auto text : {"nan","inf","-1","0","513","128oops","1e30"}) {
        bool rejected = false;
        try { (void)parseRange(text,MaxBlocks); } catch (std::exception const&) { rejected = true; }
        assert(rejected);
    }
    bool fractionalRejected = false;
    try { (void)parseRange("8.5",MaxChunks,true); } catch (std::exception const&) { fractionalRejected = true; }
    assert(fractionalRejected);
    int outer = 1, inner = 2;
    int* slot = nullptr;
    {
        Scope first(slot,&outer);
        try { Scope nested(slot,&inner); assert(slot == &inner); throw 1; } catch (int) {}
        assert(slot == &outer);
        { Scope inactive(slot,static_cast<int*>(nullptr)); assert(!slot); }
        assert(slot == &outer);
    }
    assert(!slot);
    // Queue and draw scopes never leak into one another, including nested
    // unrelated/native calls and exceptions. Both remain stack-owned.
    int* queue = &outer;
    int* draw = nullptr;
    try {
        Scope clearQueue(queue,static_cast<int*>(nullptr));
        Scope drawScope(draw,&inner);
        assert(!queue && draw == &inner);
        { Scope nativeDraw(draw,static_cast<int*>(nullptr)); assert(!draw); }
        assert(draw == &inner);
        throw 1;
    } catch (int) {}
    assert(queue == &outer && !draw);
    for (auto json : {nlohmann::json{{"range",0}}, nlohmann::json{{"range",-1}},
                      nlohmann::json{{"range",513}}, nlohmann::json{{"range",1e-100}},
                      nlohmann::json{{"range",nullptr}}, nlohmann::json::object()}) {
        bool rejected = false;
        try { (void)CheckConfig::decode(json); } catch (std::exception const&) { rejected = true; }
        assert(rejected);
    }
    assert(CheckConfig::decode({{"range","vanilla"}}).range == 0);
    auto aliases = {std::pair{"entity_range","entityRange"}, std::pair{"block_entity_range","blockEntityRange"},
                    std::pair{"terrain_detail","terrainDetail"}, std::pair{"actor_interest_chunks","actorInterestChunks"}};
    auto migrated = migrateKeys({{"entityRange",96},{"block_entity_range",128},{"terrainDetail","vanilla"}},aliases);
    assert(migrated == nlohmann::json({{"entity_range",96},{"block_entity_range",128},{"terrain_detail","vanilla"}}));
    assert(migrateKeys({{"actorInterestChunks",1}},aliases) == nlohmann::json({{"actor_interest_chunks",1}}));
    for (auto [key,old] : aliases) {
        bool rejected = false;
        try { (void)migrateKeys(nlohmann::json{{key,128},{old,128}},aliases); }
        catch (std::invalid_argument const& error) { rejected = std::string(error.what()).find("both") != std::string::npos; }
        assert(rejected);
    }
    // All files are confined to this test's ignored build subdirectory.
    auto file = std::filesystem::current_path() / "build" / "checks" / "config.json";
    writeJson(file,{{"range",128}});
    ConfigStore<CheckConfig> store;
    store.load(file);
    assert(!store.active());
    auto pinned = store.accept();
    assert(pinned->range == 128);
    (void)store.change("range","256");
    assert(store.desired()->range == 256 && store.active()->range == 128);
    assert(readJson(file).at("range") == 256);
    store.accept();
    assert(store.active()->range == 256 && pinned->range == 128);
    writeJson(file,{{"range",-1}});
    bool retained = false;
    // A fresh store bypasses the one-second command throttle for reload testing.
    ConfigStore<CheckConfig> reloader;
    writeJson(file,{{"range",128}});
    reloader.load(file);
    reloader.accept();
    writeJson(file,{{"range",-1}});
    try { (void)reloader.reload(); } catch (std::exception const&) { retained = true; }
    assert(retained && reloader.desired()->range == 128 && reloader.active()->range == 128);
    reloader.clearEffective();
    assert(!reloader.active());
    std::cout << "Movement interval decisions/lazy queries/publication, range validation, JSON/persistence/snapshot rejection, disk counts and nested/exception scopes passed\n";
}
