set(WINTERS_ENGINE_DIR "${WINTERS_ROOT_DIR}/Engine")
set(WINTERS_ENGINE_OUTPUT_DIR "${WINTERS_ENGINE_DIR}/Bin")

set(WINTERS_IMGUI_SOURCES
    "${WINTERS_ENGINE_DIR}/External/imgui/backends/imgui_impl_dx11.cpp"
    "${WINTERS_ENGINE_DIR}/External/imgui/backends/imgui_impl_win32.cpp"
    "${WINTERS_ENGINE_DIR}/External/imgui/imgui.cpp"
    "${WINTERS_ENGINE_DIR}/External/imgui/imgui_demo.cpp"
    "${WINTERS_ENGINE_DIR}/External/imgui/imgui_draw.cpp"
    "${WINTERS_ENGINE_DIR}/External/imgui/imgui_tables.cpp"
    "${WINTERS_ENGINE_DIR}/External/imgui/imgui_widgets.cpp"
)

set(WINTERS_LUA_SOURCES
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lapi.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lauxlib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lbaselib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lcode.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lcorolib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lctype.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/ldblib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/ldebug.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/ldo.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/ldump.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lfunc.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lgc.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/linit.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/liolib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/llex.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lmathlib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lmem.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/loadlib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lobject.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lopcodes.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/loslib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lparser.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lstate.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lstring.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lstrlib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/ltable.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/ltablib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/ltm.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lundump.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lutf8lib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lvm.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lzio.c"
)

file(GLOB_RECURSE WINTERS_ENGINE_PRIVATE_SOURCES CONFIGURE_DEPENDS
    "${WINTERS_ENGINE_DIR}/Private/*.cpp"
)
list(FILTER WINTERS_ENGINE_PRIVATE_SOURCES EXCLUDE REGEX "/Private/Tools/AssetConverter/main\\.cpp$")

file(GLOB_RECURSE WINTERS_ENGINE_HEADERS CONFIGURE_DEPENDS
    "${WINTERS_ENGINE_DIR}/Include/*.h"
    "${WINTERS_ENGINE_DIR}/Public/*.h"
    "${WINTERS_ENGINE_DIR}/Private/*.h"
)

add_library(WintersEngine SHARED
    ${WINTERS_IMGUI_SOURCES}
    ${WINTERS_LUA_SOURCES}
    ${WINTERS_ENGINE_PRIVATE_SOURCES}
    ${WINTERS_ENGINE_HEADERS}
)

add_library(Winters::Engine ALIAS WintersEngine)

set_target_properties(WintersEngine PROPERTIES
    OUTPUT_NAME "WintersEngine"
    FOLDER "Engine"
)

WintersApplyMsvcCommonOptions(WintersEngine)
WintersSetOutputDirectories(WintersEngine "${WINTERS_ENGINE_OUTPUT_DIR}")

target_precompile_headers(WintersEngine PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${WINTERS_ENGINE_DIR}/Public/WintersPCH.h>"
)

set_source_files_properties(${WINTERS_LUA_SOURCES} PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
)

target_compile_definitions(WintersEngine PRIVATE
    WINTERS_ENGINE_EXPORTS
    WIN32
    UNICODE
    _UNICODE
    _WINDOWS
    _USRDLL
    WINTERS_PROFILING
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_include_directories(WintersEngine
    PUBLIC
        "${WINTERS_ENGINE_DIR}/Include"
        "${WINTERS_ENGINE_DIR}/Public"
    PRIVATE
        "${WINTERS_ENGINE_DIR}/External/imgui"
        "${WINTERS_ENGINE_DIR}/External/imgui/backends"
        "${WINTERS_ENGINE_DIR}/Private"
        "${WINTERS_ENGINE_DIR}/Public/ECS"
        "${WINTERS_ENGINE_DIR}/Public/ECS/Components"
        "${WINTERS_ENGINE_DIR}/Public/ECS/Systems"
        "${WINTERS_ENGINE_DIR}/Public/Sound"
        "${WINTERS_ENGINE_DIR}/ThirdPartyLib/Assimp/Inc"
        "${WINTERS_ENGINE_DIR}/ThirdPartyLib/DirectXTK/Inc"
        "${WINTERS_ENGINE_DIR}/ThirdPartyLib/FMOD/Inc"
)

target_link_libraries(WintersEngine PRIVATE
    Winters::Assimp
    Winters::DirectXTK
    Winters::FMOD
    Winters::WindowsGraphicsDX11
)

set(WINTERS_ENGINE_SOURCE_GROUP_FILES
    ${WINTERS_IMGUI_SOURCES}
    ${WINTERS_LUA_SOURCES}
    ${WINTERS_ENGINE_PRIVATE_SOURCES}
    ${WINTERS_ENGINE_HEADERS}
)

function(WintersEngineSourceGroup group_name)
    set(group_files)
    foreach(pattern IN LISTS ARGN)
        foreach(source_file IN LISTS WINTERS_ENGINE_SOURCE_GROUP_FILES)
            if(source_file MATCHES "${pattern}")
                list(APPEND group_files "${source_file}")
            endif()
        endforeach()
    endforeach()

    if(group_files)
        list(REMOVE_DUPLICATES group_files)
        source_group("${group_name}" FILES ${group_files})
    endif()
endfunction()

# Keep this domain map in sync with Engine/Include/Engine.vcxproj.filters.
WintersEngineSourceGroup("Include"
    "/Engine/Include/(EngineConfig|IWintersApp|WintersAPI|WintersEngine|WintersMath|WintersPaths|WintersTypes)\\.h$"
    "/Engine/Public/Engine_(Defines|Enum|Function|Macro|Struct|Typedef)\\.h$"
)

WintersEngineSourceGroup("00. Core"
    "/Engine/Public/WintersCore\\.h$"
)
WintersEngineSourceGroup("00. Core\\00. Timer"
    "/Engine/Private/Core/CTimer\\.cpp$"
    "/Engine/Public/Core/CTimer\\.h$"
)
WintersEngineSourceGroup("00. Core\\00. Timer\\Manager"
    "/Engine/Private/Timer_Manager\\.cpp$"
    "/Engine/Public/Core/Timer_Manager\\.h$"
)
WintersEngineSourceGroup("00. Core\\01. Transform"
    "/Engine/Private/Core/CTransform\\.cpp$"
    "/Engine/Public/Core/CTransform\\.h$"
)
WintersEngineSourceGroup("00. Core\\02. Platform"
    "/Engine/Public/Platform/(IPlatformSurface|IPlatformWindow|PlatformTypes)\\.h$"
)
WintersEngineSourceGroup("00. Core\\02. Platform\\00. Window"
    "/Engine/Private/Platform/CWin32Window\\.cpp$"
    "/Engine/Public/Platform/CWin32Window\\.h$"
)
WintersEngineSourceGroup("00. Core\\03. Paths"
    "/Engine/Private/Core/WintersPaths\\.cpp$"
)
WintersEngineSourceGroup("00. Core\\04. Input"
    "/Engine/Private/Platform/CInput\\.cpp$"
    "/Engine/Public/Core/CInput\\.h$"
)
WintersEngineSourceGroup("00. Core\\05. PCH"
    "/Engine/Private/Core/WintersPCH\\.cpp$"
    "/Engine/Public/WintersPCH\\.h$"
)
WintersEngineSourceGroup("00. Core\\06. Profiler"
    "/Engine/Include/ProfilerAPI\\.h$"
    "/Engine/Private/Core/Profiler/ProfilerStableView\\.cpp$"
    "/Engine/Public/Core/Profiler/(ProfilerStableView|ProfilerTypes)\\.h$"
)
WintersEngineSourceGroup("00. Core\\06. Profiler\\00. CPU"
    "/Engine/Private/Core/Profiler/CPUProfiler\\.cpp$"
    "/Engine/Public/Core/Profiler/CPUProfiler\\.h$"
)

WintersEngineSourceGroup("01. Runtime\\00. EngineApp"
    "/Engine/Private/Framework/CEngineApp\\.cpp$"
    "/Engine/Public/Framework/CEngineApp\\.h$"
)
WintersEngineSourceGroup("01. Runtime\\01. WintersEngine"
    "/Engine/Private/WintersEngine\\.cpp$"
)
WintersEngineSourceGroup("01. Runtime\\02. GameInstance"
    "/Engine/Include/GameInstance\\.h$"
    "/Engine/Private/GameInstance\\.cpp$"
)
WintersEngineSourceGroup("01. Runtime\\03. GameContext"
    "/Engine/Include/GameContext\\.h$"
)
WintersEngineSourceGroup("01. Runtime\\04. Scene"
    "/Engine/Include/IScene\\.h$"
    "/Engine/Private/Scene/Scene_Manager\\.cpp$"
    "/Engine/Public/Scene/Scene_Manager\\.h$"
)

WintersEngineSourceGroup("02. RHI\\00. Interface"
    "/Engine/Public/RHI/(CRHIResourceTable|IRHIBindGroup|IRHIBindGroupLayout|IRHICommandList|IRHIDevice|IRHIPipelineState|IRHIQueue|IRHIRenderPass|IRHISwapChain|RHICapabilities|RHIDescriptors|RHIHandles|RHISurface|RHITypes)\\.h$"
)
WintersEngineSourceGroup("02. RHI\\01. DX11\\00. Device"
    "/Engine/Private/RHI/DX11/CDX11Device\\.(cpp|h)$"
)
WintersEngineSourceGroup("02. RHI\\01. DX11\\01. Pipeline"
    "/Engine/Private/RHI/DX11/DX11Pipeline\\.(cpp|h)$"
)
WintersEngineSourceGroup("02. RHI\\01. DX11\\02. Shader"
    "/Engine/Private/RHI/DX11/DX11Shader\\.(cpp|h)$"
)
WintersEngineSourceGroup("02. RHI\\01. DX11\\03. Buffer"
    "/Engine/Private/RHI/DX11/(DX11Buffer|DX11ConstantBuffer)\\.(cpp|h)$"
)
WintersEngineSourceGroup("02. RHI\\01. DX11\\04. StateCache"
    "/Engine/Private/RHI/DX11/(BlendStateCache|SamplerStateCache)\\.(cpp|h)$"
)
WintersEngineSourceGroup("02. RHI\\02. Texture"
    "/Engine/Private/RHI/RHITextureLoader\\.cpp$"
    "/Engine/Public/RHI/RHITextureLoader\\.h$"
)
WintersEngineSourceGroup("02. RHI\\03. Geometry"
    "/Engine/Public/RHI/Geometry/CubeGeometry\\.h$"
)

WintersEngineSourceGroup("03. Renderer\\00. Camera"
    "/Engine/Private/Renderer/CCamera\\.cpp$"
    "/Engine/Public/Renderer/CCamera\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\02. Cube"
    "/Engine/Private/Renderer/CubeRenderer\\.cpp$"
    "/Engine/Public/Renderer/CubeRenderer\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\03. Model\\00. Renderer"
    "/Engine/Private/Renderer/ModelRenderer\\.cpp$"
    "/Engine/Public/Renderer/ModelRenderer\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\03. Model\\01. NormalPass"
    "/Engine/Private/Renderer/NormalPass\\.cpp$"
    "/Engine/Public/Renderer/NormalPass\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\03. Model\\02. SSAOPass"
    "/Engine/Private/Renderer/SSAOPass\\.cpp$"
    "/Engine/Public/Renderer/SSAOPass\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\04. Plane"
    "/Engine/Private/Renderer/PlaneRenderer\\.cpp$"
    "/Engine/Public/Renderer/PlaneRenderer\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\05. FX\\00. Asset"
    "/Engine/Private/FX/FxAsset\\.cpp$"
    "/Engine/Public/FX/(FxAsset|FxLifeCycle)\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\05. FX\\01. DepthMode"
    "/Engine/Public/FX/FxDepthMode\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\05. FX\\02. DeterministicRandom"
    "/Engine/Private/FX/DeterministicRandom\\.cpp$"
    "/Engine/Public/FX/DeterministicRandom\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\05. FX\\03. StaticMeshRenderer"
    "/Engine/Private/Renderer/FxStaticMeshRenderer\\.cpp$"
    "/Engine/Public/Renderer/FxStaticMeshRenderer\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\05. FX\\04. MaterialDesc"
    "/Engine/Public/FX/FxMaterialDesc\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\05. FX\\05. ShaderConstant"
    "/Engine/Public/Renderer/FxShaderConstants\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\05. FX\\06. ParticlePool"
    "/Engine/Private/FX/ParticlePool\\.cpp$"
    "/Engine/Public/FX/ParticlePool\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\05. FX\\07. ParameterMap"
    "/Engine/Public/FX/ParameterMap\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\05. FX\\08. RHI"
    "/Engine/Private/Renderer/RHIFx(MeshResource|SpriteRenderer)\\.cpp$"
    "/Engine/Public/Renderer/RHIFx(MeshResource|SpriteRenderer)\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\05. FX\\09. UIRenderer"
    "/Engine/Private/Renderer/UIRenderer\\.cpp$"
    "/Engine/Public/Renderer/UIRenderer\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\07. Material"
    "/Engine/Private/Renderer/CMaterialPBR\\.cpp$"
    "/Engine/Public/Renderer/(BlendTypes|CBPerMaterial|CMaterialPBR|LightData)\\.h$"
)
WintersEngineSourceGroup("03. Renderer\\08. FogOfWar"
    "/Engine/Private/Renderer/FogOfWarRenderer\\.cpp$"
    "/Engine/Public/Renderer/FogOfWarRenderer\\.h$"
)

WintersEngineSourceGroup("04. Resource\\00. Mesh"
    "/Engine/Private/Resource/Mesh\\.cpp$"
    "/Engine/Public/Resource/Mesh\\.h$"
)
WintersEngineSourceGroup("04. Resource\\01. Texture"
    "/Engine/Private/Resource/Texture\\.cpp$"
    "/Engine/Public/Resource/Texture\\.h$"
)
WintersEngineSourceGroup("04. Resource\\02. Model"
    "/Engine/Private/Resource/Model\\.cpp$"
    "/Engine/Public/Resource/Model\\.h$"
)
WintersEngineSourceGroup("04. Resource\\03. Bone"
    "/Engine/Private/Resource/Bone\\.cpp$"
    "/Engine/Public/Resource/Bone\\.h$"
)
WintersEngineSourceGroup("04. Resource\\04. Skeleton"
    "/Engine/Private/Resource/Skeleton\\.cpp$"
    "/Engine/Public/Resource/Skeleton\\.h$"
)
WintersEngineSourceGroup("04. Resource\\05. Animation"
    "/Engine/Private/Resource/Animation\\.cpp$"
    "/Engine/Public/Resource/Animation\\.h$"
)
WintersEngineSourceGroup("04. Resource\\06. Animator"
    "/Engine/Private/Resource/Animator\\.cpp$"
    "/Engine/Public/Resource/Animator\\.h$"
)
WintersEngineSourceGroup("04. Resource\\07. ResourceCache"
    "/Engine/Private/Resource/ResourceCache\\.cpp$"
    "/Engine/Public/Resource/ResourceCache\\.h$"
)
WintersEngineSourceGroup("04. Resource\\08. AssetFormat\\Anim"
    "/Engine/(Private|Public)/AssetFormat/Anim/"
)
WintersEngineSourceGroup("04. Resource\\08. AssetFormat\\Common"
    "/Engine/(Private|Public)/AssetFormat/Common/"
)
WintersEngineSourceGroup("04. Resource\\08. AssetFormat\\Material"
    "/Engine/(Private|Public)/AssetFormat/Material/"
)
WintersEngineSourceGroup("04. Resource\\08. AssetFormat\\Mesh"
    "/Engine/(Private|Public)/AssetFormat/Mesh/"
)

WintersEngineSourceGroup("05. ECS\\00. Core\\00. World"
    "/Engine/Private/ECS/World\\.cpp$"
    "/Engine/Public/ECS/World\\.h$"
)
WintersEngineSourceGroup("05. ECS\\00. Core\\01. ComponentStore"
    "/Engine/Public/ECS/ComponentStore\\.h$"
)
WintersEngineSourceGroup("05. ECS\\00. Core\\02. Entity"
    "/Engine/Public/ECS/Entity\\.h$"
)
WintersEngineSourceGroup("05. ECS\\00. Core\\10. JobSystem"
    "/Engine/Private/Core/JobSystem\\.cpp$"
    "/Engine/Public/Core/JobSystem\\.h$"
)
WintersEngineSourceGroup("05. ECS\\00. Core\\10. JobSystem\\00. Core"
    "/Engine/Public/Core/Fiber/"
    "/Engine/Public/Core/JobCounter\\.h$"
    "/Engine/Public/Core/JobSystem/"
)
WintersEngineSourceGroup("05. ECS\\01. System"
    "/Engine/Public/ECS/(ISystem|SystemAccess)\\.h$"
)
WintersEngineSourceGroup("05. ECS\\01. System\\00. Navigation"
    "/Engine/Private/ECS/Systems/NavigationSystem\\.cpp$"
    "/Engine/Public/ECS/Systems/NavigationSystem\\.h$"
)
WintersEngineSourceGroup("05. ECS\\01. System\\01. CommandBuffer"
    "/Engine/Private/ECS/CommandBuffer\\.cpp$"
    "/Engine/Public/ECS/CCommandBuffer\\.h$"
)
WintersEngineSourceGroup("05. ECS\\01. System\\02. SystemScheduler"
    "/Engine/Private/ECS/SystemScheduler\\.cpp$"
    "/Engine/Public/ECS/SystemScheduler\\.h$"
)
WintersEngineSourceGroup("05. ECS\\01. System\\03. TransformSystem"
    "/Engine/Private/ECS/Systems/TransformSystem\\.cpp$"
    "/Engine/Public/ECS/Systems/TransformSystem\\.h$"
)
WintersEngineSourceGroup("05. ECS\\02. Components\\00. Core"
    "/Engine/Public/ECS/Components/CoreComponents\\.h$"
)
WintersEngineSourceGroup("05. ECS\\02. Components\\01. Transform"
    "/Engine/Private/ECS/Components/TransformComponent\\.cpp$"
    "/Engine/Public/ECS/Components/TransformComponent\\.h$"
)
WintersEngineSourceGroup("05. ECS\\02. Components\\02. Gameplay"
    "/Engine/Public/ECS/Components/GameplayComponents\\.h$"
)
WintersEngineSourceGroup("05. ECS\\02. Components\\03. MeshGroupVisibility"
    "/Engine/Public/ECS/Components/MeshGroupVisibilityComponent\\.h$"
)
WintersEngineSourceGroup("05. ECS\\02. Components\\04. Navigation"
    "/Engine/Private/ECS/SpatialIndex\\.cpp$"
    "/Engine/Public/ECS/SpatialIndex\\.h$"
    "/Engine/Public/ECS/Components/NavAgentComponent\\.h$"
)
WintersEngineSourceGroup("05. ECS\\02. Components\\05. Minion"
    "/Engine/Public/ECS/Components/MinionPerformanceComponents\\.h$"
)
WintersEngineSourceGroup("05. ECS\\02. Components\\06. FX"
    "/Engine/Public/ECS/Components/FxInstanceComponent\\.h$"
)
WintersEngineSourceGroup("05. ECS\\02. Components\\07. Renderer"
    "/Engine/Public/ECS/Components/RenderComponent\\.h$"
)
WintersEngineSourceGroup("05. ECS\\02. Components\\08. SpatialAgent"
    "/Engine/Public/ECS/Components/SpatialAgentComponent\\.h$"
)
WintersEngineSourceGroup("05. ECS\\02. Components\\09. Vision"
    "/Engine/Public/ECS/Components/VisionComponents\\.h$"
)
WintersEngineSourceGroup("05. ECS\\02. Components\\10. Bush"
    "/Engine/Private/ECS/BushVolumeIndex\\.cpp$"
    "/Engine/Public/ECS/BushVolumeIndex\\.h$"
)
WintersEngineSourceGroup("05. ECS\\03. Systems\\00. Core"
    "/Engine/Private/ECS/Systems/CoreSystems\\.cpp$"
    "/Engine/Public/ECS/Systems/CoreSystems\\.h$"
)
WintersEngineSourceGroup("05. ECS\\03. Systems\\01. Minion\\00. AI"
    "/Engine/Private/ECS/Systems/MinionAISystem\\.cpp$"
    "/Engine/Public/ECS/Systems/MinionAISystem\\.h$"
)
WintersEngineSourceGroup("05. ECS\\03. Systems\\01. Minion\\01. Performance"
    "/Engine/Private/ECS/Systems/MinionPerformanceSystem\\.cpp$"
    "/Engine/Public/ECS/Systems/MinionPerformanceSystem\\.h$"
)
WintersEngineSourceGroup("05. ECS\\03. Systems\\01. Minion\\02. Separation"
    "/Engine/Private/ECS/Systems/MinionSeparationSystem\\.cpp$"
    "/Engine/Public/ECS/Systems/MinionSeparationSystem\\.h$"
)
WintersEngineSourceGroup("05. ECS\\03. Systems\\02. Entity"
    "/Engine/Private/ECS/Systems/EntityBlueprint(Registry)?\\.cpp$"
    "/Engine/Public/ECS/Systems/EntityBlueprint(Registry)?\\.h$"
)
WintersEngineSourceGroup("05. ECS\\03. Systems\\03. Collision"
    "/Engine/Private/ECS/Systems/GameplayCollisionSystem\\.cpp$"
    "/Engine/Public/ECS/Systems/GameplayCollisionSystem\\.h$"
)
WintersEngineSourceGroup("05. ECS\\03. Systems\\04. Vision"
    "/Engine/Private/ECS/Systems/VisionSystem\\.cpp$"
    "/Engine/Public/ECS/Systems/VisionSystem\\.h$"
)
WintersEngineSourceGroup("05. ECS\\03. Systems\\06. Navigation"
    "/Engine/Private/ECS/Systems/SpatialHashSystem\\.cpp$"
    "/Engine/Public/ECS/Systems/SpatialHashSystem\\.h$"
)
WintersEngineSourceGroup("05. ECS\\03. Systems\\07. Turret\\00. AI"
    "/Engine/Private/ECS/Systems/TurretAISystem\\.cpp$"
    "/Engine/Public/ECS/Systems/TurretAISystem\\.h$"
)
WintersEngineSourceGroup("05. ECS\\03. Systems\\07. Turret\\01. Projectile"
    "/Engine/Private/ECS/Systems/TurretProjectileSystem\\.cpp$"
    "/Engine/Public/ECS/Systems/TurretProjectileSystem\\.h$"
)
WintersEngineSourceGroup("05. ECS\\03. Systems\\08. StatusEffect"
    "/Engine/Private/ECS/Systems/StatusEffectSystem\\.cpp$"
    "/Engine/Public/ECS/Systems/StatusEffectSystem\\.h$"
)

WintersEngineSourceGroup("06. Navigation\\MapSurface"
    "/Engine/Private/Manager/Navigation/Map(SurfaceSampler|WalkableBaker)\\.cpp$"
    "/Engine/Public/Manager/Navigation/Map(SurfaceSampler|WalkableBaker)\\.h$"
)
WintersEngineSourceGroup("06. Navigation\\NavGrid"
    "/Engine/Private/Manager/Navigation/NavGrid\\.cpp$"
    "/Engine/Public/Manager/Navigation/NavGrid\\.h$"
)
WintersEngineSourceGroup("06. Navigation\\Pathfinder"
    "/Engine/Private/Manager/Navigation/Pathfinder\\.cpp$"
    "/Engine/Public/Manager/Navigation/Pathfinder\\.h$"
)

WintersEngineSourceGroup("07. UI"
    "/Engine/Private/Manager/UI/UI_Manager\\.cpp$"
    "/Engine/Public/Manager/UI/UI_Manager\\.h$"
)
WintersEngineSourceGroup("07. UI\\00. AtlasManifest"
    "/Engine/Private/Manager/UI/UIAtlasManifest\\.cpp$"
    "/Engine/Public/Manager/UI/UIAtlasManifest\\.h$"
)
WintersEngineSourceGroup("07. UI\\01. HUD"
    "/Engine/Private/Manager/UI/ChampionHUDPanel\\.(cpp|h)$"
    "/Engine/Public/Manager/UI/ChampionHUDState\\.h$"
)
WintersEngineSourceGroup("07. UI\\02. Font"
    "/Engine/Private/Manager/UI/Font_Manager\\.cpp$"
    "/Engine/Public/Manager/UI/Font_Manager\\.h$"
)
WintersEngineSourceGroup("07. UI\\03. Lua"
    "/Engine/Private/Manager/UI/LuaUIHost\\.cpp$"
    "/Engine/Public/Manager/UI/LuaUIHost\\.h$"
)

WintersEngineSourceGroup("08. Sound"
    "/Engine/Private/Sound/Sound_Manager\\.cpp$"
    "/Engine/Public/Sound/(SoundChannel|Sound_Manager)\\.h$"
)

WintersEngineSourceGroup("09. AI\\00. BT"
    "/Engine/Private/AI/(BehaviorTree|BTNodes_Champion)\\.cpp$"
    "/Engine/Public/AI/(BehaviorTree|Blackboard|BTNodes_Champion)\\.h$"
    "/Engine/Private/ECS/Systems/BehaviorTreeSystem\\.cpp$"
    "/Engine/Public/ECS/Systems/BehaviorTreeSystem\\.h$"
)
WintersEngineSourceGroup("09. AI\\01. MCTS"
    "/Engine/Private/AI/MCTSPlanner\\.cpp$"
    "/Engine/Public/AI/MCTSPlanner\\.h$"
    "/Engine/Private/ECS/Systems/MCTSSystem\\.cpp$"
    "/Engine/Public/ECS/Systems/MCTSSystem\\.h$"
)
WintersEngineSourceGroup("09. AI\\02. RL"
    "/Engine/Private/AI/RLBridge\\.cpp$"
    "/Engine/Public/AI/RLBridge\\.h$"
)

WintersEngineSourceGroup("10. Editor"
    "/Engine/Private/Editor/ImGuiLayer\\.cpp$"
    "/Engine/Public/Editor/ImGuiLayer\\.h$"
)
WintersEngineSourceGroup("10. Editor\\00. ProfilerOverlay"
    "/Engine/Private/Manager/Profiler/ProfilerOverlay\\.cpp$"
    "/Engine/Public/Manager/Profiler/ProfilerOverlay\\.h$"
)
WintersEngineSourceGroup("10. Editor\\Imgui"
    "/Engine/External/imgui/"
)

WintersEngineSourceGroup("11. Scripting"
    "/Engine/Private/Scripting/LuaRuntime\\.cpp$"
    "/Engine/Public/Scripting/LuaRuntime\\.h$"
)
WintersEngineSourceGroup("11. Scripting\\00. Lua VM"
    "/Engine/External/lua-5\\.4\\.8/src/"
)

if(WINTERS_ENABLE_ENGINE_POST_BUILD_DEPLOY)
    add_custom_command(TARGET WintersEngine POST_BUILD
        COMMAND "${WINTERS_ROOT_DIR}/UpdateLib.bat"
        WORKING_DIRECTORY "${WINTERS_ROOT_DIR}"
        COMMENT "Deploying WintersEngine artifacts through UpdateLib.bat"
        VERBATIM
    )
endif()
