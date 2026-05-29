set(WINTERS_THIRD_PARTY_DIR "${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib")

function(WintersRequireFile FilePath Description)
    if(NOT EXISTS "${FilePath}")
        message(FATAL_ERROR "Missing ${Description}: ${FilePath}")
    endif()
endfunction()

function(WintersAddImportedConfigLib TargetName DebugLib ReleaseLib)
    WintersRequireFile("${DebugLib}" "${TargetName} Debug library")
    WintersRequireFile("${ReleaseLib}" "${TargetName} Release library")

    add_library(${TargetName} UNKNOWN IMPORTED GLOBAL)
    set_target_properties(${TargetName} PROPERTIES
        IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
        IMPORTED_LOCATION_DEBUG "${DebugLib}"
        IMPORTED_LOCATION_RELEASE "${ReleaseLib}"
    )
endfunction()

WintersAddImportedConfigLib(Winters::Assimp
    "${WINTERS_THIRD_PARTY_DIR}/Assimp/Lib/Debug/assimp-vc143-mtd.lib"
    "${WINTERS_THIRD_PARTY_DIR}/Assimp/Lib/Release/assimp-vc143-mt.lib"
)

WintersAddImportedConfigLib(Winters::DirectXTK
    "${WINTERS_THIRD_PARTY_DIR}/DirectXTK/Lib/Debug/DirectXTK.lib"
    "${WINTERS_THIRD_PARTY_DIR}/DirectXTK/Lib/Release/DirectXTK.lib"
)

set(WINTERS_FMOD_LIB "${WINTERS_THIRD_PARTY_DIR}/FMOD/Lib/fmod_vc.lib")
WintersRequireFile("${WINTERS_FMOD_LIB}" "FMOD library")

add_library(Winters::FMOD UNKNOWN IMPORTED GLOBAL)
set_target_properties(Winters::FMOD PROPERTIES
    IMPORTED_LOCATION "${WINTERS_FMOD_LIB}"
)

add_library(Winters::WindowsGraphicsDX11 INTERFACE IMPORTED GLOBAL)
target_link_libraries(Winters::WindowsGraphicsDX11 INTERFACE
    d3d11
    dxgi
    d3dcompiler
    dxguid
    windowscodecs
    ole32
)
