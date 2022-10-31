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
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
projectsrc = "%{prj.name}/src/**.cpp"
projectinc ="%{prj.name}/inc/*.hh" -- private headers
projincpath ="%{prj.name}/inc/" -- private header path
commoninc = "common_inc/"
targetbuilddir = "bin/" .. outputdir .. "/%{prj.name}"
sharedbuildlibs = "bin/" .. outputdir .. "/lib/"
intermediatedir = "bin-intermediates/" ..outputdir .. "/%{prj.name}"
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


filter "configurations:Debug"
    defines "_ENABLE_LOGGING"
    symbols "On"

filter "configurations:Release"
    defines "_ENABLE_LOGGING"
    optimize "On"

filter "configurations:Performance"
    --defines "Performance logging define"
    optimize "On"

filter "configurations:Distribution"
    optimize "On"

filter "platforms:rpi"
    architecture "arm64"

filter "platforms:Linux"
    architecture "x64"

filter "platforms:Windows"
    architecture "x64"