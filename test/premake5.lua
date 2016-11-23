workspace "test-DL_Raster"
    configurations { "Debug", "Release" }

externalproject "SDL"
    location "external/SDL2/VisualC/SDL"
    uuid "81CE8DAF-EBB2-4761-8E45-B71ABCCA8C68"
    language "C"
    kind "SharedLib"

project "test"
    kind "ConsoleApp"
    language "C++"

    files { "*.h", "*.cpp", "../DL_Raster.h", "DL_Raster-MSVC_Debug_Helpers.natvis" }
    includedirs { "..", "external/SDL2/include" }

    filter { "system:macosx" }
        buildoptions { "-std=c++11" }
        frameworkdirs { "external" }
        linkoptions { "-framework SDL2", "-framework OpenGL", "-rpath @executable_path/../../external" }

    filter { "system:windows" }
        links { "SDL" }
        postbuildcommands { "copy /Y Win32\\%{cfg.buildcfg}\\SDL2.dll bin\\%{cfg.buildcfg}\\" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "Full"
