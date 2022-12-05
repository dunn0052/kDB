workspace "DB"

    require "cmake" -- needed for cmake extension

    platforms
    {
        "rpi" -- Needed for arm64 architecture
        --[[
        "Linux",
        "Windows",
        "MacOS"
        ]]
    }

    configurations
    {
        "Debug", -- no opt w/ logging
        "Performance", -- opt w/ performance logging
        "Release", -- opt w/ logging
        "Distribution" -- opt w/o logging (fastest)
    }

-- PATH MACROS
outputdir = "%{cfg.buildcfg}"
projectsrc = "%{prj.name}/src/**.cpp"
projectinc ="%{prj.name}/inc/*.hh" -- private headers
projincpath ="%{prj.name}/inc/" -- private header path
commoninc = "common_inc/"
targetbuilddir = outputdir .. "/bin/"
sharedbuildlibs = outputdir .. "/lib/"
intermediatedir = outputdir .. "bin-intermediates/%{prj.name}"
dbincdir = "db/inc/"

project "Logger"

    location "Logger"
    kind "SharedLib"
    language "C++"

    targetdir(sharedbuildlibs)
    objdir(intermediatedir)

    files
    {
        projectsrc,
        projectinc
    }

    includedirs
    {
        projincpath,
        commoninc
    }

    cppdialect "C++17"
    staticruntime "On" -- static linking .so or .dll
    systemversion "latest" -- compiler version

    defines
    {

    }

project "CLI"

    location "CLI"
    kind "ConsoleApp"
    language "C++"

    targetdir(targetbuilddir)
    objdir(intermediatedir)
    libdirs(sharedbuildlibs)

    files
    {
        projectsrc,
        projectinc
    }

    includedirs
    {
        commoninc,
        projincpath,
        dbincdir
    }

    cppdialect "C++17"
    systemversion "latest" -- compiler version

    links
    {
        "Logger"
    }

    defines
    {

    }

project "Schema"

    location "Schema"
    kind "ConsoleApp"
    language "C++"

    targetdir(targetbuilddir)
    objdir(intermediatedir)
    libdirs(sharedbuildlibs)

    files
    {
        projectsrc,
        projectinc
    }

    includedirs
    {
        commoninc,
        projincpath
    }

    cppdialect "C++17"
    systemversion "latest" -- compiler version

    links
    {
        "Logger"
    }

    defines
    {
        "__ENABLE_LOGGING",
        "__LOG_SHOW_LINE"
    }

    postbuildcommands
    {
        "Schema -a"
    }

project "DBMapper"

    location "DBMapper"
    kind "SharedLib"
    language "C++"

    targetdir(sharedbuildlibs)
    objdir(intermediatedir)
    libdirs(sharedbuildlibs)

    files
    {
        projectsrc,
        projectinc
    }

    includedirs
    {
        commoninc,
        projincpath,
        dbincdir
    }

    cppdialect "C++17"
    systemversion "latest" -- compiler version

    links
    {
        "Logger"
    }

    defines
    {

    }

    dependson
    {
        "Schema"
    }

project "Talker"

    location "Talker"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    systemversion "latest" -- compiler version

    targetdir(targetbuilddir)
    objdir(intermediatedir)
    libdirs(sharedbuildlibs)

    files
    {
        projectsrc,
        projectinc
    }

    includedirs
    {
        commoninc,
        projincpath
    }


    links
    {
        "Logger",
        "DBMapper",
        "Threads::Threads"
    }


    defines
    {
    }

project "Listener"

    location "Listener"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    systemversion "latest" -- compiler version

    targetdir(targetbuilddir)
    objdir(intermediatedir)
    libdirs(sharedbuildlibs)

    files
    {
        projectsrc,
    }

    includedirs
    {
        commoninc,
    }


    links
    {
        "Logger",
        "DBMapper",
        "Threads::Threads"
    }


    defines
    {
    }

project "Profiler"

    location "Profiler"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    systemversion "latest" -- compiler version

    targetdir(targetbuilddir)
    objdir(intermediatedir)
    libdirs(sharedbuildlibs)

    files
    {
        projectsrc,
        projectinc
    }

    includedirs
    {
        commoninc,
        projincpath
    }

    links
    {
        "Threads::Threads"
    }
    defines
    {
        "__ENABLE_PROFILING"
    }


project "InstantiateDB"
    location "InstantiateDB"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    systemversion "latest" -- compiler version

    targetdir(targetbuilddir)
    objdir(intermediatedir)
    libdirs(sharedbuildlibs)

    files
    {
        projectsrc,
        projectinc
    }

    includedirs
    {
        commoninc,
        projincpath,
        dbincdir
    }

    links
    {
        "Logger"
    }

    defines
    {
        "__ENABLE_LOGGING",
        "__LOG_SHOW_LINE"
    }

    dependson
    {
        "Schema"
    }

project "DBSet"
    location "DBSet"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    systemversion "latest" -- compiler version

    targetdir(targetbuilddir)
    objdir(intermediatedir)
    libdirs(sharedbuildlibs)

    files
    {
        projectsrc,
        projectinc
    }

    includedirs
    {
        commoninc,
        projincpath,
        dbincdir
    }

    links
    {
        "Logger"
    }

    defines
    {
        "__ENABLE_LOGGING",
        "__LOG_SHOW_LINE"
    }

    dependson
    {
        "Schema",
        "InstantiateDB"
    }

project "UpdateDaemon"
    location "UpdateDaemon"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    systemversion "latest" -- compiler version

    targetdir(targetbuilddir)
    objdir(intermediatedir)
    libdirs(sharedbuildlibs)

    files
    {
        projectsrc,
        projectinc
    }

    includedirs
    {
        commoninc,
        projincpath,
        dbincdir
    }

    links
    {
        "Logger"
    }

    defines
    {
        "__ENABLE_LOGGING",
        "__LOG_SHOW_LINE"
    }

    dependson
    {
        "Schema",
    }

filter "configurations:Debug"
    defines "__ENABLE_LOGGING"
    symbols "On"

filter "configurations:Release"
    defines "__ENABLE_LOGGING"
    optimize "Speed"

filter "configurations:Performance"
    defines "__ENABLE_PROFILING"
    optimize "Speed"

filter "configurations:Distribution"
    optimize "Speed"

filter "platforms:rpi"
    architecture "arm64"
    system "linux"