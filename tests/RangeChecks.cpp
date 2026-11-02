#include "range/Limits.h"
#include "range/Scope.h"
#include "range/Config.h"
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
    assert(diskCount(8) == 197);
    assert(localReplicationChunks(0) == 0);
    assert(localReplicationChunks(0.01f) == 1);
    assert(localReplicationChunks(std::numeric_limits<float>::denorm_min()) == 1);
    assert(localReplicationChunks(16) == 1);
    assert(localReplicationChunks(16.01f) == 2);
    assert(localReplicationChunks(128) == 8);
    assert(localReplicationChunks(MaxBlocks) == MaxChunks);
    assert(diskCount(4) == 49);
    assert(diskCount(MaxChunks) <= (2*MaxChunks+1)*(2*MaxChunks+1));
    assert(parseRange("vanilla",MaxBlocks) == 0);
    assert(parseRange("128",MaxBlocks) == 128);
    assert(parseRange("32",MaxChunks,true) == 32);
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
    for (auto json : {nlohmann::json{{"range",0}}, nlohmann::json{{"range",-1}},
                      nlohmann::json{{"range",513}}, nlohmann::json{{"range",1e-100}},
                      nlohmann::json{{"range",nullptr}}, nlohmann::json::object()}) {
        bool rejected = false;
        try { (void)CheckConfig::decode(json); } catch (std::exception const&) { rejected = true; }
        assert(rejected);
    }
    assert(CheckConfig::decode({{"range","vanilla"}}).range == 0);
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
    std::cout << "Range validation, JSON/persistence/snapshot rejection, disk counts and nested/exception scopes passed\n";
}
