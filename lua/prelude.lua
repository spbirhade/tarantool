function preload(modname)
        print("Loading " .. modname)
        local filename = "lua/" .. modname .. ".lua"
        package.preload[modname] = assert(loadfile(filename))
        package.preload[modname](modname)
end

preload("stat")

print("Lua prelude initialized.")
