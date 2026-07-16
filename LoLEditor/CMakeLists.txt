set(WINTERS_LOL_EDITOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(WINTERS_LOL_EDITOR_OUTPUT_DIR "${WINTERS_LOL_EDITOR_DIR}/Bin")

set(WINTERS_LOL_EDITOR_SOURCES
	"${WINTERS_LOL_EDITOR_DIR}/Private/main.cpp"
	"${WINTERS_LOL_EDITOR_DIR}/Private/LoLEditorApp.cpp"
	"${WINTERS_LOL_EDITOR_DIR}/Private/LoLMapEditorScene.cpp"
	"${WINTERS_LOL_EDITOR_DIR}/Private/World/LoLStageDocument.cpp"
)

set(WINTERS_LOL_EDITOR_HEADERS
	"${WINTERS_LOL_EDITOR_DIR}/Public/LoLEditorApp.h"
	"${WINTERS_LOL_EDITOR_DIR}/Public/LoLMapEditorScene.h"
	"${WINTERS_LOL_EDITOR_DIR}/Public/World/LoLStageDocument.h"
)

add_executable(WintersLoLEditor WIN32
	${WINTERS_LOL_EDITOR_SOURCES}
	${WINTERS_LOL_EDITOR_HEADERS}
)

set_target_properties(WintersLoLEditor PROPERTIES
	OUTPUT_NAME "WintersLoLEditor"
	FOLDER "Editor/LoL"
	VS_DEBUGGER_WORKING_DIRECTORY "${WINTERS_ROOT_DIR}"
)

WintersApplyMsvcCommonOptions(WintersLoLEditor)
WintersSetOutputDirectories(WintersLoLEditor "${WINTERS_LOL_EDITOR_OUTPUT_DIR}")

target_compile_definitions(WintersLoLEditor PRIVATE
	WIN32
	UNICODE
	_UNICODE
	_WINDOWS
	NOMINMAX
	$<$<CONFIG:Debug>:WINTERS_PROFILING>
	WINTERS_LOL_EDITOR
	$<$<CONFIG:Debug>:_DEBUG>
	$<$<CONFIG:Debug>:WINTERS_ENABLE_NON_AI_DEBUG_STRING=1>
	$<$<CONFIG:Release>:NDEBUG>
)

target_include_directories(WintersLoLEditor PRIVATE
	"${WINTERS_ROOT_DIR}"
	"${WINTERS_LOL_EDITOR_DIR}/Public"
	"${WINTERS_ROOT_DIR}/Engine/External/imgui"
)

target_link_libraries(WintersLoLEditor PRIVATE
	Winters::Engine
)

source_group(TREE "${WINTERS_LOL_EDITOR_DIR}" FILES
	${WINTERS_LOL_EDITOR_SOURCES}
	${WINTERS_LOL_EDITOR_HEADERS}
)

add_custom_command(TARGET WintersLoLEditor POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"$<TARGET_FILE:WintersEngine>"
		"$<TARGET_FILE_DIR:WintersLoLEditor>"
	COMMENT "Copy WintersEngine runtime DLL to LoLEditor output"
	VERBATIM
)

add_custom_command(TARGET WintersLoLEditor POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy_directory
		"$<$<CONFIG:Debug>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/Assimp/Bin/Debug>$<$<CONFIG:Release>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/Assimp/Bin/Release>"
		"$<TARGET_FILE_DIR:WintersLoLEditor>"
	COMMAND "${CMAKE_COMMAND}" -E copy_directory
		"$<$<CONFIG:Debug>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/DirectXTK/Bin/Debug>$<$<CONFIG:Release>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/DirectXTK/Bin/Release>"
		"$<TARGET_FILE_DIR:WintersLoLEditor>"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/FMOD/Bin/fmod.dll"
		"$<TARGET_FILE_DIR:WintersLoLEditor>"
	COMMENT "Copy third-party runtime DLLs to LoLEditor output"
	VERBATIM
)

add_custom_command(TARGET WintersLoLEditor POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy_directory
		"${WINTERS_ROOT_DIR}/Shaders"
		"$<TARGET_FILE_DIR:WintersLoLEditor>/Shaders"
	COMMENT "Copy shaders to LoLEditor output"
	VERBATIM
)
