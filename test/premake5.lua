workspace "test-DLRaster"
    configurations { "Debug", "Release" }

project "test"
    kind "ConsoleApp"
    language "C++"

    files { "*.h", "*.cpp" }
    includedirs { "..", "external/SDL2/include" }

    filter { "system:macosx" }
        buildoptions { "-std=c++11" }
        frameworkdirs { "external" }
        linkoptions { "-framework SDL2", "-framework OpenGL", "-rpath @executable_path/../../external" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        flags { "Symbols" }

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "Full"
