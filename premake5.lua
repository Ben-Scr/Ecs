workspace "Ecs"
	architecture "x86_64"
	configurations { "Debug", "Release", "Dist" }
	startproject "EcsTests"
	location "."

newoption {
	trigger = "with-demo",
	description = "Generate the optional IndexSDK demo project"
}

local outputDir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
local indexSdkRoot = "Thirdparty/IndexSDK-Dist-windows-x86_64"
local indexSdkRootAbs = path.getabsolute(indexSdkRoot)

project "Ecs"
	location "."
	kind "None"
	language "C++"
	cppdialect "C++20"

	files {
		"Src/**.hpp"
	}

	includedirs {
		"Src"
	}

project "EcsTests"
	location "."
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"

	targetdir ("Bin/" .. outputDir .. "/%{prj.name}")
	objdir ("Bin-Int/" .. outputDir .. "/%{prj.name}")

	files {
		"Tests/**.cpp",
		"Src/**.hpp"
	}

	includedirs {
		"Src"
	}

	filter "system:windows"
		systemversion "latest"
		warnings "Extra"

	filter "configurations:Debug"
		symbols "On"
		optimize "Off"

	filter "configurations:Release"
		symbols "On"
		optimize "Speed"

	filter "configurations:Dist"
		symbols "Off"
		optimize "Full"

	filter {}

if _OPTIONS["with-demo"] then
	if not os.isdir(indexSdkRootAbs .. "/include") or
		not os.isdir(indexSdkRootAbs .. "/lib") or
		not os.isdir(indexSdkRootAbs .. "/runtime") then
		error(
			"--with-demo requires a complete IndexSDK at " ..
			indexSdkRoot
		)
	end

	project "EcsDemo"
		location "."
		kind "WindowedApp"
		system "windows"
		configurations { "Dist" }
		language "C++"
		cppdialect "C++20"

		targetdir ("Bin/" .. outputDir .. "/%{prj.name}")
		objdir ("Bin-Int/" .. outputDir .. "/%{prj.name}")

		files {
			"Demo/**.cpp",
			"Demo/**.hpp",
			"Src/**.hpp"
		}

		includedirs {
			"Src",
			indexSdkRoot .. "/include"
		}

		libdirs {
			indexSdkRoot .. "/lib"
		}

		links {
			"Index-Engine",
			"Tracy"
		}

		defines {
			"IDX_IMPORT_DLL",
			"IDX_PLATFORM_WINDOWS",
			"IDX_DIST",
			"NDEBUG",
			"GLFW_DLL",
			"IDX_RHI_WEBGPU",
			"WEBGPU_CPP_HAS_ENUM_CLASS_BITMASKS=1",
			"IMGUI_IMPL_WEBGPU_BACKEND_DAWN",
			"INDEX_WITH_RENDER=1",
			"INDEX_WITH_AUDIO=1",
			"INDEX_WITH_PHYSICS=1",
			"INDEX_WITH_SCRIPTING=1",
			"INDEX_WITH_APPLICATION=1",
			"INDEX_WITH_EDITOR=0",
			"INDEX_ENTITY_BITS=20",
			"MAGIC_ENUM_RANGE_MIN=-1",
			"MAGIC_ENUM_RANGE_MAX=32",
			"INDEX_ALL_MODULES=1",
			"INDEX_PROFILER_ENABLED",
			"TRACY_ENABLE",
			"TRACY_IMPORTS",
			"TRACY_ON_DEMAND"
		}

		filter "system:windows"
			systemversion "latest"
			staticruntime "Off"
			links {
				"shell32",
				"gdi32",
				"user32"
			}
			buildoptions {
				"/utf-8",
				"/FS",
				"/Zc:preprocessor"
			}
			postbuildcommands {
				'{COPYDIR} %[' .. indexSdkRoot .. '/runtime] %[%{cfg.targetdir}]'
			}

		filter "configurations:Dist"
			symbols "Off"
			optimize "Full"

		filter {}
end