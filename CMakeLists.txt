cmake_minimum_required(VERSION 3.12)
add_definitions (-DDEBUG)

project(OpenGL)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
	add_compile_options(/utf-8 /wd4828 /wd4819)
endif()

include_directories(
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/include
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/application/miniz
)

link_directories(
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/lib
)

add_subdirectory(wrapper)
add_subdirectory(application)
add_subdirectory(glframework)
add_subdirectory(imgui)

add_executable(openglStudy "main.cpp" "glad.c" )

if(MSVC)
	# Embed Per-Monitor V2 awareness before GLFW creates the native window.
	target_sources(openglStudy PRIVATE "application.manifest")
endif()

target_link_libraries(openglStudy glfw3.lib  assimp-vc143-mtd.lib zlibstaticd.lib   wrapper app fw imguilib)

set(OPENGL_RUNTIME_DLLS
	"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/bin/assimp-vc143-mt.dll"
	"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/bin/kubazip.dll"
	"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/bin/minizip.dll"
	"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/bin/poly2tri.dll"
	"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/bin/pugixml.dll"
	"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/bin/zlib1.dll"
)

# Keep runtime shaders, textures, and showcase models in sync after every build.
add_custom_command(
	TARGET openglStudy POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/assets"
		"${CMAKE_BINARY_DIR}/assets"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${OPENGL_RUNTIME_DLLS}
		"$<TARGET_FILE_DIR:openglStudy>"
	COMMENT "Syncing runtime assets and DLLs"
	COMMAND_EXPAND_LISTS
)

# Visual Studio may run app-local dependency handling after POST_BUILD during an
# incremental build. This target makes runtime staging the final ALL_BUILD step.
add_custom_target(stage_opengl_runtime ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/assets"
		"${CMAKE_BINARY_DIR}/assets"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${OPENGL_RUNTIME_DLLS}
		"$<TARGET_FILE_DIR:openglStudy>"
	DEPENDS openglStudy
	COMMENT "Staging OpenGL runtime after target build"
	COMMAND_EXPAND_LISTS
)
