set(WINTERS_ELDEN_CLIENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(WINTERS_ELDEN_CLIENT_OUTPUT_DIR "${WINTERS_ELDEN_CLIENT_DIR}/Bin")

set(WINTERS_ELDEN_CLIENT_SOURCES
    "${WINTERS_ELDEN_CLIENT_DIR}/Private/main.cpp"
    "${WINTERS_ELDEN_CLIENT_DIR}/Private/EldenRingApp.cpp"
    "${WINTERS_ELDEN_CLIENT_DIR}/Private/EldenAssetProbeScene.cpp"
    "${WINTERS_ELDEN_CLIENT_DIR}/Private/EldenLimgraveShowcaseScene.cpp"
    "${WINTERS_ELDEN_CLIENT_DIR}/Private/EldenRingRHITestCubeRenderer.cpp"
)

set(WINTERS_ELDEN_CLIENT_HEADERS
    "${WINTERS_ELDEN_CLIENT_DIR}/Public/EldenRingApp.h"
    "${WINTERS_ELDEN_CLIENT_DIR}/Public/EldenLimgraveShowcaseScene.h"
    "${WINTERS_ELDEN_CLIENT_DIR}/Public/EldenRingProbeScene.h"
    "${WINTERS_ELDEN_CLIENT_DIR}/Public/EldenRingRenderSettings.h"
    "${WINTERS_ELDEN_CLIENT_DIR}/Public/EldenRingRHITestCubeRenderer.h"
)

add_executable(WintersElden WIN32
    ${WINTERS_ELDEN_CLIENT_SOURCES}
    ${WINTERS_ELDEN_CLIENT_HEADERS}
)

add_executable(Winters::EldenRingClient ALIAS WintersElden)

set_target_properties(WintersElden PROPERTIES
    OUTPUT_NAME "WintersElden"
    FOLDER "Client/EldenRing"
    VS_DEBUGGER_WORKING_DIRECTORY "${WINTERS_ROOT_DIR}"
)

WintersApplyMsvcCommonOptions(WintersElden)
WintersSetOutputDirectories(WintersElden "${WINTERS_ELDEN_CLIENT_OUTPUT_DIR}")

target_compile_definitions(WintersElden PRIVATE
    WIN32
    UNICODE
    _UNICODE
    _WINDOWS
    NOMINMAX
    WINTERS_PROFILING
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Debug>:WINTERS_ENABLE_NON_AI_DEBUG_STRING=1>
    $<$<CONFIG:Release>:NDEBUG>
)

target_include_directories(WintersElden PRIVATE
    "${WINTERS_ROOT_DIR}"
    "${WINTERS_ELDEN_CLIENT_DIR}/Public"
    "${WINTERS_ROOT_DIR}/Engine/External/imgui"
    "${WINTERS_ROOT_DIR}/Engine/External/imgui/backends"
    "${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/FlatBuffers/Inc"
)

target_link_libraries(WintersElden PRIVATE
    Winters::Engine
)

source_group(TREE "${WINTERS_ELDEN_CLIENT_DIR}" FILES
    ${WINTERS_ELDEN_CLIENT_SOURCES}
    ${WINTERS_ELDEN_CLIENT_HEADERS}
)

add_custom_command(TARGET WintersElden POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:WintersEngine>"
        "$<TARGET_FILE_DIR:WintersElden>"
    COMMENT "Copy WintersEngine runtime DLL to EldenRingClient output"
    VERBATIM
)

add_custom_command(TARGET WintersElden POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "$<$<CONFIG:Debug>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/Assimp/Bin/Debug>$<$<CONFIG:Release>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/Assimp/Bin/Release>"
        "$<TARGET_FILE_DIR:WintersElden>"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "$<$<CONFIG:Debug>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/DirectXTK/Bin/Debug>$<$<CONFIG:Release>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/DirectXTK/Bin/Release>"
        "$<TARGET_FILE_DIR:WintersElden>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/FMOD/Bin/fmod.dll"
        "$<TARGET_FILE_DIR:WintersElden>"
    COMMENT "Copy third-party runtime DLLs to EldenRingClient output"
    VERBATIM
)

add_custom_command(TARGET WintersElden POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${WINTERS_ROOT_DIR}/Shaders"
        "$<TARGET_FILE_DIR:WintersElden>/Shaders"
    COMMENT "Copy shaders to EldenRingClient output"
    VERBATIM
)
