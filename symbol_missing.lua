rule("symbol_missing")
    add_orders("@levibuildscript/linkrule", "symbol_missing")

    before_link(function (target)
        import("lib.detect.find_file")
        import("core.project.config")

        local envs = os.joinenvs(target:pkgenvs(), os.getenvs())
        local searchpaths = path.splitenv(envs.PATH or envs.Path or envs.path or "")
        local tool
        for _, name in ipairs({"symbol_missing.exe", "symbol_missing"}) do
            tool = find_file(name, searchpaths) or find_file(name, {os.projectdir()})
            if tool then
                break
            end
        end
        assert(tool, "Cannot find symbol_missing")

        local linkdirs = {}
        local sysdirs = {}
        local seen_dirs = {}
        local function add_dir(list, dir)
            if not dir or dir == "" then return end
            local key = dir:lower()
            if seen_dirs[key] then return end
            seen_dirs[key] = true
            table.insert(list, dir)
        end

        for _, dir in ipairs(table.wrap(target:get("linkdirs"))) do
            add_dir(linkdirs, dir)
        end
        for _, pkg in ipairs(target:orderpkgs()) do
            for _, dir in ipairs(table.wrap(pkg:get("linkdirs"))) do
                add_dir(linkdirs, dir)
            end
        end

        local tc = target:toolchain("clang-cl") or target:toolchain("msvc")
            or target:toolchain("clang") or target:toolchain()
        if tc then
            local vcvars = tc:config("vcvars")
            if vcvars then
                for _, name in ipairs({"LIB", "LIBPATH"}) do
                    for _, dir in ipairs(path.splitenv(vcvars[name] or "")) do
                        add_dir(sysdirs, dir)
                    end
                end
            end
        end

        local inputs = {}
        local seen_inputs = {}
        local function add_input(file)
            if not file or file == "" then return end
            local key = file:lower()
            if seen_inputs[key] then return end
            seen_inputs[key] = true
            table.insert(inputs, file)
        end

        for _, file in ipairs(target:objectfiles()) do
            if not file:lower():endswith(".rc.obj") then
                add_input(file)
            end
        end
        for _, dep in ipairs(target:orderdeps()) do
            if dep:kind() == "static" then
                local file = dep:targetfile()
                if file and os.isfile(file) then
                    add_input(file)
                end
            end
        end

        local links = {}
        local seen_links = {}
        local function add_link(name)
            if not name or name == "" or name:startswith("-") or name:startswith("/") then return end
            local key = name:lower()
            if seen_links[key] then return end
            seen_links[key] = true
            table.insert(links, name)
        end
        for _, name in ipairs(table.wrap(target:get("links"))) do
            add_link(name)
        end
        for _, name in ipairs(table.wrap(target:get("syslinks"))) do
            add_link(name)
        end
        for _, pkg in ipairs(target:orderpkgs()) do
            for _, name in ipairs(table.wrap(pkg:get("links"))) do
                add_link(name)
            end
            for _, name in ipairs(table.wrap(pkg:get("syslinks"))) do
                add_link(name)
            end
        end

        local function resolve_library(name, dirs)
            if not name or name == "" then return nil end
            if os.isfile(name) then return name end
            local lower = name:lower()
            local candidates
            if lower:endswith(".lib") or lower:endswith(".a") then
                candidates = {name}
            else
                candidates = {name .. ".lib", name .. ".a", "lib" .. name .. ".a"}
            end
            for _, dir in ipairs(dirs) do
                if dir and dir ~= "" then
                    for _, candidate in ipairs(candidates) do
                        local file = path.join(dir, candidate)
                        if os.isfile(file) then
                            return file
                        end
                    end
                end
            end
            return nil
        end

        for _, name in ipairs(links) do
            local file = resolve_library(name, linkdirs)
                or resolve_library(name, sysdirs)
            if file then
                add_input(file)
            end
        end

        if #inputs == 0 then
            return
        end

        local report = path.join(os.projectdir(), "symbol_missing")
        local rsp = path.join(config.builddir(), "symbol_missing.rsp")
        os.mkdir(path.directory(rsp))
        io.writefile(rsp, table.concat(inputs, "\n"))

        local argv = {"--no-fail", "--demangle", "--report=" .. report, "@" .. rsp}
        for _, dir in ipairs(sysdirs) do
            table.insert(argv, "--libdir=" .. dir)
        end
        for _, dir in ipairs(linkdirs) do
            table.insert(argv, "--libdir=" .. dir)
        end
        os.execv(tool, argv)
    end)
rule_end()
