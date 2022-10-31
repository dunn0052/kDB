workspace "DB"

    require "cmake" -- needed for cmake extension

    platforms
    {
        "rpi", -- Needed for arm64 architecture
        "Linux",
        "Windows",
        "MacOS"
    }

    configurations
    {
        "Debug", -- no opt w/ logging
        "Performance", -- opt w/ performance logging
        "Release", -- opt w/ logging
        "Distribution" -- opt w/o logging (fastest)
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
projectsrc = "%{prj.name}/src/**.cpp"
projectinc ="%{prj.name}/inc/*.hh" -- private headers
projincpath ="%{prj.name}/inc/" -- private header path
commoninc = "common_inc/"
targetbuilddir = "bin/" .. outputdir .. "/%{prj.name}"
intermediatedir = "bin-intermediates/" ..outputdir .. "/%{prj.name}"

project "Logger"

    location "Logger"
    kind "SharedLib"
    language "C++"

    targetdir(targetbuilddir)
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

--[[
    postbuildCommands
    {
        -- Move .so to common lib folder??
        ("{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir )
    }
]]

filter "configurations:Debug"
    --defines "LOGGING_DEFINES??"
    symbols "On"

filter "configurations:Release"
    --defines "LOGGING??"
    optimize "On"

filter "configurations:Performance"
    --defines "Performance logging define"
    optimize "On"

filter "configurations:Distribution"
    optimize "On"

filter "platforms:rpi"
    architecture "arm64"

filter "platforms:Windows"
    architecture "x64"