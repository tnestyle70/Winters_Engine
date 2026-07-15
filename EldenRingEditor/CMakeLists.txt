set(WINTERS_ELDENRING_EDITOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(WINTERS_ELDENRING_EDITOR_OUTPUT_DIR "${WINTERS_ELDENRING_EDITOR_DIR}/Bin")

set(WINTERS_ELDENRING_EDITOR_SOURCES
	"${WINTERS_ELDENRING_EDITOR_DIR}/Private/main.cpp"
	"${WINTERS_ELDENRING_EDITOR_DIR}/Private/EldenRingEditorApp.cpp"
	"${WINTERS_ELDENRING_EDITOR_DIR}/Private/EldenRingEditorScene.cpp"
	"${WINTERS_ELDENRING_EDITOR_DIR}/Private/World/EditorTransaction.cpp"
	"${WINTERS_ELDENRING_EDITOR_DIR}/Private/World/WorldCellDocument.cpp"
)

set(WINTERS_ELDENRING_EDITOR_HEADERS
	"${WINTERS_ELDENRING_EDITOR_DIR}/Public/EldenRingEditorApp.h"
	"${WINTERS_ELDENRING_EDITOR_DIR}/Public/EldenRingEditorScene.h"
	"${WINTERS_ELDENRING_EDITOR_DIR}/Public/World/EditorTransaction.h"
	"${WINTERS_ELDENRING_EDITOR_DIR}/Public/World/WorldCellDocument.h"
)

add_executable(WintersEldenRingEditor WIN32
	${WINTERS_ELDENRING_EDITOR_SOURCES}
	${WINTERS_ELDENRING_EDITOR_HEADERS}
)

set_target_properties(WintersEldenRingEditor PROPERTIES
	OUTPUT_NAME "WintersEldenRingEditor"
	FOLDER "Editor/EldenRing"
	VS_DEBUGGER_WORKING_DIRECTORY "${WINTERS_ROOT_DIR}"
)

WintersApplyMsvcCommonOptions(WintersEldenRingEditor)
WintersSetOutputDirectories(WintersEldenRingEditor "${WINTERS_ELDENRING_EDITOR_OUTPUT_DIR}")

target_compile_definitions(WintersEldenRingEditor PRIVATE
	WIN32
	UNICODE
	_UNICODE
	_WINDOWS
	NOMINMAX
	$<$<CONFIG:Debug>:WINTERS_PROFILING>
	WINTERS_ELDENRING_EDITOR
	$<$<CONFIG:Debug>:_DEBUG>
	$<$<CONFIG:Debug>:WINTERS_ENABLE_NON_AI_DEBUG_STRING=1>
	$<$<CONFIG:Release>:NDEBUG>
)

target_include_directories(WintersEldenRingEditor PRIVATE
	"${WINTERS_ROOT_DIR}"
	"${WINTERS_ELDENRING_EDITOR_DIR}/Public"
	"${WINTERS_ROOT_DIR}/Engine/External/imgui"
	"${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/FlatBuffers/Inc"
)

target_link_libraries(WintersEldenRingEditor PRIVATE
	Winters::Engine
)

source_group(TREE "${WINTERS_ELDENRING_EDITOR_DIR}" FILES
	${WINTERS_ELDENRING_EDITOR_SOURCES}
	${WINTERS_ELDENRING_EDITOR_HEADERS}
)

add_custom_command(TARGET WintersEldenRingEditor POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"$<TARGET_FILE:WintersEngine>"
		"$<TARGET_FILE_DIR:WintersEldenRingEditor>"
	COMMENT "Copy WintersEngine runtime DLL to EldenRingEditor output"
	VERBATIM
)

add_custom_command(TARGET WintersEldenRingEditor POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy_directory
		"$<$<CONFIG:Debug>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/Assimp/Bin/Debug>$<$<CONFIG:Release>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/Assimp/Bin/Release>"
		"$<TARGET_FILE_DIR:WintersEldenRingEditor>"
	COMMAND "${CMAKE_COMMAND}" -E copy_directory
		"$<$<CONFIG:Debug>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/DirectXTK/Bin/Debug>$<$<CONFIG:Release>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/DirectXTK/Bin/Release>"
		"$<TARGET_FILE_DIR:WintersEldenRingEditor>"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/FMOD/Bin/fmod.dll"
		"$<TARGET_FILE_DIR:WintersEldenRingEditor>"
	COMMENT "Copy third-party runtime DLLs to EldenRingEditor output"
	VERBATIM
)

add_custom_command(TARGET WintersEldenRingEditor POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy_directory
		"${WINTERS_ROOT_DIR}/Shaders"
		"$<TARGET_FILE_DIR:WintersEldenRingEditor>/Shaders"
	COMMENT "Copy shaders to EldenRingEditor output"
	VERBATIM
)
