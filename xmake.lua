add_rules("mode.debug", "mode.release")

-- Single version constant; keep tooth.json in sync.
local urde_version = "0.1.3"
set_version(urde_version)

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

option("target_type")
    set_default("server")
    set_showmenu(true)
    set_values("server", "client")
option_end()

-- add_requires("levilamina x.x.x") for a specific version
-- add_requires("levilamina develop") to use develop version
-- please note that you should add bdslibrary yourself if using dev version
add_requires("levilamina 26.51.9", {configs = {target_type = get_config("target_type")}})

add_requires("levibuildscript")
add_requires("nlohmann_json v3.12.0")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("UtilityRenderDistanceExpansion")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker", {modVersion = urde_version})
    if is_plat("windows") then
        add_defines("NOMINMAX", "UNICODE", "_HAS_CXX23=1")
        set_exceptions("none") -- To avoid conflicts with /EHa.
        add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
        add_cxflags(
            "/EHs",
            "-Wno-microsoft-cast",
            "-Wno-invalid-offsetof",
            "-Wno-c++2b-extensions",
            "-Wno-microsoft-include",
            "-Wno-overloaded-virtual",
            "-Wno-ignored-qualifiers",
            "-Wno-missing-field-initializers",
            "-Wno-potentially-evaluated-expression",
            "-Wno-pragma-system-header-outside-header",
            {tools = {"clang_cl"}}
        )
        set_toolchains("clang-cl")
    end
    add_packages("levilamina")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_includedirs("src")
    if is_config("target_type", "server") then
        add_defines("URDE_SERVER")
        add_includedirs("src-server")
        add_files("src-server/**.cpp")
    else
        add_defines("URDE_CLIENT")
        add_includedirs("src-client")
        add_files("src-client/**.cpp")
    end

-- Tiny game-independent checks; not part of the default mod/package build.
target("urde-checks")
    set_default(false)
    set_kind("binary")
    set_languages("c++20")
    set_toolchains("clang-cl")
    add_cxflags("/EHsc", "/utf-8", "/UNDEBUG")
    add_includedirs("src")
    add_packages("nlohmann_json")
    add_defines("NOMINMAX", "UNICODE")
    add_files("tests/RangeChecks.cpp", "src/range/Config.cpp")
