function(WintersApplyMsvcCommonOptions TargetName)
    target_compile_features(${TargetName} PUBLIC cxx_std_20)

    if(MSVC)
        target_compile_options(${TargetName} PRIVATE
            /W3
            /sdl
            /permissive-
            /utf-8
            /FS
        )

        target_compile_options(${TargetName} PRIVATE
            $<$<CONFIG:Debug>:/Od>
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:Release>:/Gy>
        )

        target_link_options(${TargetName} PRIVATE
            $<$<CONFIG:Debug>:/DEBUG>
            $<$<CONFIG:Release>:/OPT:REF>
            $<$<CONFIG:Release>:/OPT:ICF>
        )

        set_property(TARGET ${TargetName} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        )
    endif()
endfunction()

function(WintersSetOutputDirectories TargetName OutputRoot)
    set_target_properties(${TargetName} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${OutputRoot}/$<CONFIG>"
        LIBRARY_OUTPUT_DIRECTORY "${OutputRoot}/$<CONFIG>"
        ARCHIVE_OUTPUT_DIRECTORY "${OutputRoot}/$<CONFIG>"
    )
endfunction()
