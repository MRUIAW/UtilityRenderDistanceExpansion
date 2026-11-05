#include "range/Limits.h"
#include "range/Scope.h"
#include "range/Config.h"
#include "range/CommandValues.h"
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
    auto policy = movement::Settings::decode({{"full_rate_types",{"minecraft:villager"}},
                                              {"full_rate_tags",{"smooth"}}},4);
    assert(policy.updateInterval == 4 && policy.fullRatePlayers && policy.typeIds.contains("minecraft:villager"));
    assert(client::Settings::decode({{"entity_range",128},{"block_entity_range",128},{"terrain_detail",128}}).movement.updateInterval == 1);
    for (uint64_t native = 0; native <= 12; ++native) {
        for (bool player : {false,true}) for (bool listed : {false,true}) for (bool tagged : {false,true}) {
            int queries = 0;
            auto result = movement::decide(native,policy,true,[&] { ++queries; return player; },
                [&]() -> std::string const& { ++queries; static std::string yes="minecraft:villager",no="minecraft:pig"; return listed ? yes : no; },
                [&](auto const&) { ++queries; return tagged; });
            assert(result.interval == std::min(native,uint64_t(player || listed || tagged ? 1 : 4)));
            if (native <= 1) assert(queries == 0);
        }
    }
    auto noQuery = []() -> bool { assert(false); return false; };
    auto noTypeQuery = []() -> std::string { assert(false); return {}; };
    auto noTagQuery = [](auto const&) -> bool { assert(false); return false; };
    policy.set("update_interval","vanilla");
    assert(movement::decide(10,policy,true,noQuery,noTypeQuery,noTagQuery).interval == 10);
    assert(!movement::decide(std::nullopt,policy,true,noQuery,noTypeQuery,noTagQuery).interval);
    policy.set("update_interval","4");
    assert(movement::decide(10,policy,false,noQuery,noTypeQuery,noTagQuery).interval == 10);
    auto ordinary = movement::Settings::decode({{"full_rate_players",false}},4);
    assert(movement::decide(4,ordinary,true,noQuery,noTypeQuery,noTagQuery).interval == 4);
    for (auto text : {"0","-1","11","4.5","nan","1junk"}) {
        bool rejected=false;
        try { auto copy=policy; copy.set("update_interval",text); } catch (std::exception const&) { rejected=true; }
        assert(rejected);
    }
    for (auto invalid : {nlohmann::json{{"update_interval",0}}, {{"update_interval",1.5}},
                         {{"full_rate_players","true"}}, {{"full_rate_types","minecraft:pig"}},
                         {{"full_rate_tags",{""}}}, {{"full_rate_tags",{1}}}}) {
        bool rejected=false;
        try { (void)movement::Settings::decode(invalid,4); } catch (std::exception const&) { rejected=true; }
        assert(rejected);
    }
    assert(movement::Settings::decode(policy.encode(),1).typeIds == policy.typeIds);
    for (int cap : {1,4,10}) {
        ordinary.set("update_interval",std::to_string(cap));
        for (uint64_t native : {0ull,1ull,2ull,4ull,10ull,UINT64_MAX}) {
            auto result = movement::decide(native,ordinary,true,noQuery,noTypeQuery,noTagQuery);
            assert(result.interval == std::min(native,uint64_t(cap)));
            assert(result.outcome == (native > uint64_t(cap) ? movement::Outcome::capped : movement::Outcome::unchanged));
        }
    }
    auto classify = [&](bool player, std::string type, bool tag) {
        return movement::decide(10,policy,true,[&] { return player; },[&] { return type; },[&](auto const&) { return tag; });
    };
    assert(classify(true,"minecraft:villager",true).outcome == movement::Outcome::player);
    assert(classify(false,"minecraft:villager",true).outcome == movement::Outcome::type);
    assert(classify(false,"minecraft:pig",true).outcome == movement::Outcome::tag);
    assert(classify(false,"minecraft:pig",false).outcome == movement::Outcome::capped);
    policy.set("full_rate_players","false");
    assert(classify(true,"minecraft:pig",false).interval == 4);
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
