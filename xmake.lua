-- set minimum xmake version
set_xmakever("3.0.0")

-- include CommonLibF4 (Dear-Modding-FO4 fork, 3 runtimes: OG 1.10.163 / NG 1.10.984 / AE 1.11.x)
includes("extern/CommonLibF4")

-- set project constants
set_project("R-CATS")
set_version("0.1.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- MO2 mods folder; the plugin installs to <mods_dir>/R-CATS/F4SE/Plugins after each build.
-- Override with: xmake f --mods_dir="..."   (the XSE_FO4_MODS_PATH env var also works)
option("mods_dir", function()
    set_default("D:/Naked Commonwealth/mods")
    set_showmenu(true)
    set_description("MO2 mods folder to deploy the plugin into")
end)

-- engine-free code: config, logging front, event bus, layer registry, Input layer.
-- Never includes CommonLibF4, so it builds and unit-tests without the game.
target("rcats_core", function()
    set_kind("static")
    set_default(true)

    add_files("src/core/*.cpp", "src/input/*.cpp")
    add_headerfiles("src/core/*.h", "src/input/*.h")
    add_includedirs("src", { public = true })

    add_cxxflags("cl::/EHsc", "cl::/permissive-", "cl::/Zc:preprocessor")

    on_install(function() end)
end)

-- the F4SE plugin: one DLL for all three runtimes
target("R-CATS", function()
    add_rules("commonlibf4.plugin", {
        name = "R-CATS",
        author = "keiro",
        description = "Rider Combat And Traversal System"
    })

    add_deps("rcats_core")

    add_files("src/plugin/*.cpp")
    add_headerfiles("src/plugin/*.h")
    add_includedirs("src")
    set_pcxxheader("src/plugin/PCH.h")

    add_extrafiles("res/R-CATS.ini")

    if not os.getenv("XSE_FO4_MODS_PATH") and get_config("mods_dir") then
        set_installdir(path.join(get_config("mods_dir"), "R-CATS"))
    end

    -- The INI is seeded once and never overwritten, so in-game tuning survives rebuilds.
    after_install(function(target)
        local dst = path.join(target:installdir(), "F4SE/Plugins/R-CATS.ini")
        if not os.isfile(dst) then
            os.cp(path.join(os.projectdir(), "res/R-CATS.ini"), dst)
            cprint("${dim}seeded %s", dst)
        end
    end)
end)

-- unit tests for the engine-free core:  xmake test
target("rcats_tests", function()
    set_kind("binary")
    set_default(true)

    add_deps("rcats_core")
    add_files("tests/*.cpp")
    add_cxxflags("cl::/EHsc", "cl::/permissive-", "cl::/Zc:preprocessor")
    add_tests("default")

    on_install(function() end)
end)
