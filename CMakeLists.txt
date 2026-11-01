cmake_minimum_required(VERSION 3.24)

project(RemasteredEngineFixes VERSION 1.0.0 LANGUAGES CXX)

# OBSE64's source (https://github.com/ianpatt/obse64), for its plugin header, PluginAPI.h
set(OBSE64_DIR "" CACHE PATH "OBSE64 source folder")
if(NOT EXISTS "${OBSE64_DIR}/obse64/PluginAPI.h")
	message(FATAL_ERROR "Set OBSE64_DIR to a copy of OBSE64's source: the folder with obse64/PluginAPI.h in it.")
endif()

add_library(${PROJECT_NAME} SHARED
	src/Main.cpp
	src/Log.cpp
	src/Patch.cpp
	src/PickUpPhysics.cpp
	src/MagicMenuRefresh.cpp
	src/TokenRemoval.cpp
	src/WholeGoldBounty.cpp
	src/UntranslatedText.cpp
	src/SavedProjectiles.cpp
	src/MagicShaderRemoval.cpp
	src/ScriptEffectNames.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE "${OBSE64_DIR}/obse64")
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_compile_definitions(${PROJECT_NAME} PRIVATE
	WIN32_LEAN_AND_MEAN
	NOMINMAX
	PLUGIN_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
	PLUGIN_VERSION_MINOR=${PROJECT_VERSION_MINOR}
	PLUGIN_VERSION_PATCH=${PROJECT_VERSION_PATCH}
	PLUGIN_VERSION_STRING="${PROJECT_VERSION}"
)
target_compile_options(${PROJECT_NAME} PRIVATE /W4)

# the C++ runtime is linked in, so there are no runtime DLLs to install
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# The release zip, with the DLL in OBSE\Plugins, to extract into the game's Binaries\Win64 folder:
#   cmake --build build --config Release --target release_zip
set(RELEASE_DIR "${CMAKE_BINARY_DIR}/zip")
set(RELEASE_ZIP "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${PROJECT_VERSION}.zip")
add_custom_target(release_zip
	COMMAND ${CMAKE_COMMAND} -E rm -rf "${RELEASE_DIR}"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${RELEASE_DIR}/OBSE/Plugins"
	COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${RELEASE_DIR}/OBSE/Plugins/"
	COMMAND ${CMAKE_COMMAND} -E rm -f "${RELEASE_ZIP}"
	COMMAND ${CMAKE_COMMAND} -E chdir "${RELEASE_DIR}" ${CMAKE_COMMAND} -E tar cf "${RELEASE_ZIP}" --format=zip OBSE
	DEPENDS ${PROJECT_NAME}
	COMMENT "Packing ${RELEASE_ZIP}"
	VERBATIM
)
