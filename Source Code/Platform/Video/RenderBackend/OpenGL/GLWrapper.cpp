﻿#include "stdafx.h"

#include "Headers/GLWrapper.h"
#include "Headers/glIMPrimitive.h"

#include "Platform/Headers/PlatformRuntime.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/File/Headers/FileManagement.h"

#include "Core/Headers/Kernel.h"
#include "Core/Headers/Configuration.h"
#include "Core/Time/Headers/ProfileTimer.h"

#include "Utility/Headers/Localization.h"

#include "GUI/Headers/GUI.h"

#include "CEGUIOpenGLRenderer/include/Texture.h"

#include "Platform/Video/RenderBackend/OpenGL/CEGUIOpenGLRenderer/include/GL3Renderer.h"

#include "Platform/Video/RenderBackend/OpenGL/Shaders/Headers/glShaderProgram.h"

#include "Platform/Video/RenderBackend/OpenGL/Textures/Headers/glTexture.h"
#include "Platform/Video/RenderBackend/OpenGL/Textures/Headers/glSamplerObject.h"

#include "Platform/Video/RenderBackend/OpenGL/Buffers/Headers/glBufferImpl.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/RenderTarget/Headers/glFramebuffer.h"
#include "Platform/Video/RenderBackend/OpenGL/Buffers/VertexBuffer/Headers/glGenericVertexData.h"

#include "Platform/Video/GLIM/glim.h"

#ifndef GLFONTSTASH_IMPLEMENTATION
#define GLFONTSTASH_IMPLEMENTATION
#define FONTSTASH_IMPLEMENTATION
#include "Text/Headers/fontstash.h"
#include "Text/Headers/glfontstash.h"
#endif

#include <glbinding-aux/Meta.h>
#include <glbinding-aux/ContextInfo.h>
#include <glbinding/Binding.h>

namespace Divide {

namespace {
    // Weird stuff happens if this is enabled (i.e. certain draw calls hang forever)
    constexpr bool g_runAllQueriesInSameFrame = false;
    // Keep resident textures in memory for a max of 30 frames
    constexpr U8 g_maxTextureResidencyFrameCount = Config::TARGET_FRAME_RATE / 2;
}

eastl::unique_ptr<GLStateTracker> GL_API::s_stateTracker = nullptr;
GL_API::VAOMap GL_API::s_vaoCache;
std::atomic_bool GL_API::s_glFlushQueued;
GLUtil::glTextureViewCache GL_API::s_textureViewCache{};
GL_API::IMPrimitivePool GL_API::s_IMPrimitivePool{};
U32 GL_API::s_fenceSyncCounter[GL_API::s_LockFrameLifetime]{};
vector<GL_API::ResidentTexture> GL_API::s_residentTextures{};
SharedMutex GL_API::s_samplerMapLock;
GL_API::SamplerObjectMap GL_API::s_samplerMap{};
glHardwareQueryPool* GL_API::s_hardwareQueryPool = nullptr;

std::array<GLUtil::GLMemory::DeviceAllocator, to_base(GLUtil::GLMemory::GLMemoryType::COUNT)> GL_API::s_memoryAllocators = {
    GLUtil::GLMemory::DeviceAllocator(GLUtil::GLMemory::GLMemoryType::SHADER_BUFFER),
    GLUtil::GLMemory::DeviceAllocator(GLUtil::GLMemory::GLMemoryType::VERTEX_BUFFER),
    GLUtil::GLMemory::DeviceAllocator(GLUtil::GLMemory::GLMemoryType::INDEX_BUFFER),
    GLUtil::GLMemory::DeviceAllocator(GLUtil::GLMemory::GLMemoryType::OTHER)
};

#define TO_MEGABYTES(X) (X * 1024u * 1024u)
std::array<size_t, to_base(GLUtil::GLMemory::GLMemoryType::COUNT)> GL_API::s_memoryAllocatorSizes {
    TO_MEGABYTES(512),
    TO_MEGABYTES(1024),
    TO_MEGABYTES(256),
    TO_MEGABYTES(256)
};

namespace {
    struct SDLContextEntry {
        SDL_GLContext _context = nullptr;
        bool _inUse = false;
    };

    struct ContextPool {
        bool init(const size_t size, const DisplayWindow& window) {
            SDL_Window* raw = window.getRawWindow();
            _contexts.resize(size);
            for (SDLContextEntry& contextEntry : _contexts) {
                contextEntry._context = SDL_GL_CreateContext(raw);
            }
            return true;
        }

        bool destroy() noexcept {
            for (const SDLContextEntry& contextEntry : _contexts) {
                SDL_GL_DeleteContext(contextEntry._context);
            }
            _contexts.clear();
            return true;
        }

        bool getAvailableContext(SDL_GLContext& ctx) noexcept {
            assert(!_contexts.empty());
            for (SDLContextEntry& contextEntry : _contexts) {
                if (!contextEntry._inUse) {
                    ctx = contextEntry._context;
                    contextEntry._inUse = true;
                    return true;
                }
            }

            return false;
        }

        vector<SDLContextEntry> _contexts;
    } g_ContextPool;
};

GL_API::GL_API(GFXDevice& context)
    : RenderAPIWrapper(),
    _context(context),
    _swapBufferTimer(Time::ADD_TIMER("Swap Buffer Timer"))
{
    std::atomic_init(&s_glFlushQueued, false);
}

/// Try and create a valid OpenGL context taking in account the specified resolution and command line arguments
ErrorCode GL_API::initRenderingAPI([[maybe_unused]] GLint argc, [[maybe_unused]] char** argv, Configuration& config) {
    // Fill our (abstract API <-> openGL) enum translation tables with proper values
    GLUtil::fillEnumTables();

    const DisplayWindow& window = *_context.context().app().windowManager().mainWindow();
    g_ContextPool.init(_context.parent().totalThreadCount(), window);

    SDL_GL_MakeCurrent(window.getRawWindow(), window.userData()->_glContext);
    GLUtil::s_glMainRenderWindow = &window;
    _currentContext._windowGUID = window.getGUID();
    _currentContext._context = window.userData()->_glContext;

    glbinding::Binding::initialize([](const char *proc) noexcept  {
                                        return (glbinding::ProcAddress)SDL_GL_GetProcAddress(proc);
                                  }, true);

    if (SDL_GL_GetCurrentContext() == nullptr) {
        return ErrorCode::GLBINGING_INIT_ERROR;
    }

    glbinding::Binding::useCurrentContext();

    // Query GPU vendor to enable/disable vendor specific features
    GPUVendor vendor = GPUVendor::COUNT;
    const char* gpuVendorStr = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    if (gpuVendorStr != nullptr) {
        if (strstr(gpuVendorStr, "Intel") != nullptr) {
            vendor = GPUVendor::INTEL;
        } else if (strstr(gpuVendorStr, "NVIDIA") != nullptr) {
            vendor = GPUVendor::NVIDIA;
        } else if (strstr(gpuVendorStr, "ATI") != nullptr || strstr(gpuVendorStr, "AMD") != nullptr) {
            vendor = GPUVendor::AMD;
        } else if (strstr(gpuVendorStr, "Microsoft") != nullptr) {
            vendor = GPUVendor::MICROSOFT;
        } else {
            vendor = GPUVendor::OTHER;
        }
    } else {
        gpuVendorStr = "Unknown GPU Vendor";
        vendor = GPUVendor::OTHER;
    }
    GPURenderer renderer = GPURenderer::COUNT;
    const char* gpuRendererStr = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    if (gpuRendererStr != nullptr) {
        if (strstr(gpuRendererStr, "Tegra") || strstr(gpuRendererStr, "GeForce") || strstr(gpuRendererStr, "NV")) {
            renderer = GPURenderer::GEFORCE;
        } else if (strstr(gpuRendererStr, "PowerVR") || strstr(gpuRendererStr, "Apple")) {
            renderer = GPURenderer::POWERVR;
            vendor = GPUVendor::IMAGINATION_TECH;
        } else if (strstr(gpuRendererStr, "Mali")) {
            renderer = GPURenderer::MALI;
            vendor = GPUVendor::ARM;
        } else if (strstr(gpuRendererStr, "Adreno")) {
            renderer = GPURenderer::ADRENO;
            vendor = GPUVendor::QUALCOMM;
        } else if (strstr(gpuRendererStr, "AMD") || strstr(gpuRendererStr, "ATI")) {
            renderer = GPURenderer::RADEON;
        } else if (strstr(gpuRendererStr, "Intel")) {
            renderer = GPURenderer::INTEL;
        } else if (strstr(gpuRendererStr, "Vivante")) {
            renderer = GPURenderer::VIVANTE;
            vendor = GPUVendor::VIVANTE;
        } else if (strstr(gpuRendererStr, "VideoCore")) {
            renderer = GPURenderer::VIDEOCORE;
            vendor = GPUVendor::ALPHAMOSAIC;
        } else if (strstr(gpuRendererStr, "WebKit") || strstr(gpuRendererStr, "Mozilla") || strstr(gpuRendererStr, "ANGLE")) {
            renderer = GPURenderer::WEBGL;
            vendor = GPUVendor::WEBGL;
        } else if (strstr(gpuRendererStr, "GDI Generic")) {
            renderer = GPURenderer::GDI;
        } else {
            renderer = GPURenderer::UNKNOWN;
        }
    } else {
        gpuRendererStr = "Unknown GPU Renderer";
        renderer = GPURenderer::UNKNOWN;
    }
    // GPU info, including vendor, gpu and driver
    Console::printfn(Locale::Get(_ID("GL_VENDOR_STRING")), gpuVendorStr, gpuRendererStr, glGetString(GL_VERSION));

    DeviceInformation deviceInformation{};
    deviceInformation._vendor = vendor;
    deviceInformation._renderer = renderer;

    // Not supported in RenderDoc (as of 2021). Will always return false when using it to debug the app
    deviceInformation._bindlessTexturesSupported = glbinding::aux::ContextInfo::supported({ GLextension::GL_ARB_bindless_texture });
    Console::printfn(Locale::Get(_ID("GL_BINDLESS_TEXTURE_EXTENSION_STATE")), deviceInformation._bindlessTexturesSupported ? "True" : "False");

    if (s_hardwareQueryPool == nullptr) {
        s_hardwareQueryPool = MemoryManager_NEW glHardwareQueryPool(_context);
    }

    // OpenGL has a nifty error callback system, available in every build configuration if required
    if (Config::ENABLE_GPU_VALIDATION && config.debug.enableRenderAPIDebugging) {
        // GL_DEBUG_OUTPUT_SYNCHRONOUS is essential for debugging gl commands in the IDE
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        // hard-wire our debug callback function with OpenGL's implementation
        glDebugMessageCallback((GLDEBUGPROC)GLUtil::DebugCallback, nullptr);
    }

    // If we got here, let's figure out what capabilities we have available
    // Maximum addressable texture image units in the fragment shader
    deviceInformation._maxTextureUnits = to_U8(CLAMPED(GLUtil::getGLValue(GL_MAX_TEXTURE_IMAGE_UNITS), 16u, 255u));
    s_residentTextures.resize(to_size(deviceInformation._maxTextureUnits) * (1 << 4));

    GLUtil::getGLValue(GL_MAX_VERTEX_ATTRIB_BINDINGS, deviceInformation._maxVertAttributeBindings);
    GLUtil::getGLValue(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, deviceInformation._maxAtomicBufferBindingIndices);
    Console::printfn(Locale::Get(_ID("GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS")), deviceInformation._maxAtomicBufferBindingIndices);

    if (to_base(TextureUsage::COUNT) >= deviceInformation._maxTextureUnits) {
        Console::errorfn(Locale::Get(_ID("ERROR_INSUFFICIENT_TEXTURE_UNITS")));
        return ErrorCode::GFX_NOT_SUPPORTED;
    }

    if (to_base(AttribLocation::COUNT) >= deviceInformation._maxVertAttributeBindings) {
        Console::errorfn(Locale::Get(_ID("ERROR_INSUFFICIENT_ATTRIB_BINDS")));
        return ErrorCode::GFX_NOT_SUPPORTED;
    }

    deviceInformation._versionInfo._major = to_U8(GLUtil::getGLValue(GL_MAJOR_VERSION));
    deviceInformation._versionInfo._minor = to_U8(GLUtil::getGLValue(GL_MINOR_VERSION));
    Console::printfn(Locale::Get(_ID("GL_MAX_VERSION")), deviceInformation._versionInfo._major, deviceInformation._versionInfo._minor);

    if (deviceInformation._versionInfo._major < 4 || (deviceInformation._versionInfo._major == 4 && deviceInformation._versionInfo._minor < 6)) {
        Console::errorfn(Locale::Get(_ID("ERROR_OPENGL_VERSION_TO_OLD")));
        return ErrorCode::GFX_NOT_SUPPORTED;
    }

    // Maximum number of colour attachments per framebuffer
    GLUtil::getGLValue(GL_MAX_COLOR_ATTACHMENTS, deviceInformation._maxRTColourAttachments);

    s_stateTracker = eastl::make_unique<GLStateTracker>();

    glMaxShaderCompilerThreadsARB(0xFFFFFFFF);
    deviceInformation._shaderCompilerThreads = GLUtil::getGLValue(GL_MAX_SHADER_COMPILER_THREADS_ARB);
    Console::printfn(Locale::Get(_ID("GL_SHADER_THREADS")), deviceInformation._shaderCompilerThreads);

    glEnable(GL_MULTISAMPLE);
    // Line smoothing should almost always be used
    glEnable(GL_LINE_SMOOTH);

    // GL_FALSE causes a conflict here. Thanks glbinding ...
    glClampColor(GL_CLAMP_READ_COLOR, GL_NONE);

    // Cap max anisotropic level to what the hardware supports
    CLAMP(config.rendering.maxAnisotropicFilteringLevel,
          to_U8(0),
          to_U8(GLUtil::getGLValue(GL_MAX_TEXTURE_MAX_ANISOTROPY)));

    deviceInformation._maxAnisotropy = config.rendering.maxAnisotropicFilteringLevel;

    // Number of sample buffers associated with the framebuffer & MSAA sample count
    const U8 maxGLSamples = to_U8(std::min(254, GLUtil::getGLValue(GL_MAX_SAMPLES)));
    // If we do not support MSAA on a hardware level for whatever reason, override user set MSAA levels
    config.rendering.MSAASamples = std::min(config.rendering.MSAASamples, maxGLSamples);

    config.rendering.shadowMapping.csm.MSAASamples = std::min(config.rendering.shadowMapping.csm.MSAASamples, maxGLSamples);
    config.rendering.shadowMapping.spot.MSAASamples = std::min(config.rendering.shadowMapping.spot.MSAASamples, maxGLSamples);
    _context.gpuState().maxMSAASampleCount(maxGLSamples);

    // Print all of the OpenGL functionality info to the console and log
    // How many uniforms can we send to fragment shaders
    Console::printfn(Locale::Get(_ID("GL_MAX_UNIFORM")), GLUtil::getGLValue(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS));
    // How many uniforms can we send to vertex shaders
    Console::printfn(Locale::Get(_ID("GL_MAX_VERT_UNIFORM")), GLUtil::getGLValue(GL_MAX_VERTEX_UNIFORM_COMPONENTS));
    // How many uniforms can we send to vertex + fragment shaders at the same time
    Console::printfn(Locale::Get(_ID("GL_MAX_FRAG_AND_VERT_UNIFORM")), GLUtil::getGLValue(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS));
    // How many attributes can we send to a vertex shader
    deviceInformation._maxVertAttributes = GLUtil::getGLValue(GL_MAX_VERTEX_ATTRIBS);
    Console::printfn(Locale::Get(_ID("GL_MAX_VERT_ATTRIB")), deviceInformation._maxVertAttributes);
        
    // How many workgroups can we have per compute dispatch
    for (U8 i = 0u; i < 3; ++i) {
        GLUtil::getGLValue(GL_MAX_COMPUTE_WORK_GROUP_COUNT, deviceInformation._maxWorgroupCount[i], i);
        GLUtil::getGLValue(GL_MAX_COMPUTE_WORK_GROUP_SIZE,  deviceInformation._maxWorgroupSize[i], i);
    }

    deviceInformation._maxWorgroupInvocations = GLUtil::getGLValue(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS);
    deviceInformation._maxComputeSharedMemoryBytes = GLUtil::getGLValue(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE);

    Console::printfn(Locale::Get(_ID("GL_MAX_COMPUTE_WORK_GROUP_INFO")),
                     deviceInformation._maxWorgroupCount[0], deviceInformation._maxWorgroupCount[1], deviceInformation._maxWorgroupCount[2],
                     deviceInformation._maxWorgroupSize[0],  deviceInformation._maxWorgroupSize[1],  deviceInformation._maxWorgroupSize[2],
                     deviceInformation._maxWorgroupInvocations);
    Console::printfn(Locale::Get(_ID("GL_MAX_COMPUTE_SHARED_MEMORY_SIZE")), deviceInformation._maxComputeSharedMemoryBytes / 1024);
    
    // Maximum number of texture units we can address in shaders
    Console::printfn(Locale::Get(_ID("GL_MAX_TEX_UNITS")),
                     GLUtil::getGLValue(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS),
                     deviceInformation._maxTextureUnits);
    // Maximum number of varying components supported as outputs in the vertex shader
    deviceInformation._maxVertOutputComponents = GLUtil::getGLValue(GL_MAX_VERTEX_OUTPUT_COMPONENTS);
    Console::printfn(Locale::Get(_ID("GL_MAX_VERTEX_OUTPUT_COMPONENTS")), deviceInformation._maxVertOutputComponents);

    // Query shading language version support
    Console::printfn(Locale::Get(_ID("GL_GLSL_SUPPORT")),
                     glGetString(GL_SHADING_LANGUAGE_VERSION));
    // In order: Maximum number of uniform buffer binding points,
    //           maximum size in basic machine units of a uniform block and
    //           minimum required alignment for uniform buffer sizes and offset
    GLUtil::getGLValue(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, deviceInformation._UBOffsetAlignmentBytes);
    GLUtil::getGLValue(GL_MAX_UNIFORM_BLOCK_SIZE, deviceInformation._UBOMaxSizeBytes);
    const bool UBOSizeOver1Mb = deviceInformation._UBOMaxSizeBytes / 1024 > 1024;
    Console::printfn(Locale::Get(_ID("GL_UBO_INFO")),
                     GLUtil::getGLValue(GL_MAX_UNIFORM_BUFFER_BINDINGS),
                     (deviceInformation._UBOMaxSizeBytes / 1024) / (UBOSizeOver1Mb ? 1024 : 1),
                     UBOSizeOver1Mb ? "Mb" : "Kb",
                     deviceInformation._UBOffsetAlignmentBytes);

    // In order: Maximum number of shader storage buffer binding points,
    //           maximum size in basic machine units of a shader storage block,
    //           maximum total number of active shader storage blocks that may
    //           be accessed by all active shaders and
    //           minimum required alignment for shader storage buffer sizes and
    //           offset.
    GLUtil::getGLValue(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, deviceInformation._SSBOffsetAlignmentBytes);
    GLUtil::getGLValue(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, deviceInformation._SSBOMaxSizeBytes);
    deviceInformation._maxSSBOBufferBindings = GLUtil::getGLValue(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS);
    Console::printfn(
        Locale::Get(_ID("GL_SSBO_INFO")),
        deviceInformation._maxSSBOBufferBindings,
        deviceInformation._SSBOMaxSizeBytes / 1024 / 1024,
        GLUtil::getGLValue(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS),
        deviceInformation._SSBOffsetAlignmentBytes);

    // Maximum number of subroutines and maximum number of subroutine uniform
    // locations usable in a shader
    Console::printfn(Locale::Get(_ID("GL_SUBROUTINE_INFO")),
                     GLUtil::getGLValue(GL_MAX_SUBROUTINES),
                     GLUtil::getGLValue(GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS));

    GLint range[2];
    GLUtil::getGLValue(GL_SMOOTH_LINE_WIDTH_RANGE, range);
    Console::printfn(Locale::Get(_ID("GL_LINE_WIDTH_INFO")), range[0], range[1]);

    const I32 clipDistanceCount = std::max(GLUtil::getGLValue(GL_MAX_CLIP_DISTANCES), 0);
    const I32 cullDistanceCount = std::max(GLUtil::getGLValue(GL_MAX_CULL_DISTANCES), 0);

    deviceInformation._maxClipAndCullDistances = to_U8(GLUtil::getGLValue(GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES));
    deviceInformation._maxClipDistances = to_U8(clipDistanceCount);
    deviceInformation._maxCullDistances = to_U8(cullDistanceCount);
    DIVIDE_ASSERT(Config::MAX_CLIP_DISTANCES <= deviceInformation._maxClipDistances, "SDLWindowWrapper error: incorrect combination of clip and cull distance counts");
    DIVIDE_ASSERT(Config::MAX_CULL_DISTANCES <= deviceInformation._maxCullDistances, "SDLWindowWrapper error: incorrect combination of clip and cull distance counts");
    DIVIDE_ASSERT(Config::MAX_CULL_DISTANCES + Config::MAX_CLIP_DISTANCES <= deviceInformation._maxClipAndCullDistances, "SDLWindowWrapper error: incorrect combination of clip and cull distance counts");

    DIVIDE_ASSERT(Config::Lighting::ClusteredForward::CLUSTERS_X_THREADS < deviceInformation._maxWorgroupSize[0] &&
                  Config::Lighting::ClusteredForward::CLUSTERS_Y_THREADS < deviceInformation._maxWorgroupSize[1] &&
                  Config::Lighting::ClusteredForward::CLUSTERS_Z_THREADS < deviceInformation._maxWorgroupSize[2]);

    DIVIDE_ASSERT(to_U32(Config::Lighting::ClusteredForward::CLUSTERS_X_THREADS) *
                         Config::Lighting::ClusteredForward::CLUSTERS_Y_THREADS *
                         Config::Lighting::ClusteredForward::CLUSTERS_Z_THREADS < deviceInformation._maxWorgroupInvocations);

    GFXDevice::OverrideDeviceInformation(deviceInformation);
    // Seamless cubemaps are a nice feature to have enabled (core since 3.2)
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    //glEnable(GL_FRAMEBUFFER_SRGB);
    // Culling is enabled by default, but RenderStateBlocks can toggle it on a per-draw call basis
    glEnable(GL_CULL_FACE);

    // Enable all clip planes, I guess
    for (U8 i = 0u; i < Config::MAX_CLIP_DISTANCES; ++i) {
        glEnable(static_cast<GLenum>(static_cast<U32>(GL_CLIP_DISTANCE0) + i));
    }

    for (U8 i = 0u; i < to_base(GLUtil::GLMemory::GLMemoryType::COUNT); ++i) {
        s_memoryAllocators[i].init(s_memoryAllocatorSizes[i]);
    }

    s_textureViewCache.init(256);

    glShaderProgram::InitStaticData();

    // FontStash library initialization
    // 512x512 atlas with bottom-left origin
    _fonsContext = glfonsCreate(512, 512, FONS_ZERO_BOTTOMLEFT);
    if (_fonsContext == nullptr) {
        Console::errorfn(Locale::Get(_ID("ERROR_FONT_INIT")));
        return ErrorCode::FONT_INIT_ERROR;
    }

    // Prepare immediate mode emulation rendering
    NS_GLIM::glim.SetVertexAttribLocation(to_base(AttribLocation::POSITION));

    // Initialize our query pool
    s_hardwareQueryPool->init(
        {
            { GL_TIME_ELAPSED, 9 },
            { GL_TRANSFORM_FEEDBACK_OVERFLOW, 6 },
            { GL_VERTICES_SUBMITTED, 6 },
            { GL_PRIMITIVES_SUBMITTED, 6 },
            { GL_VERTEX_SHADER_INVOCATIONS, 6 },
            { GL_SAMPLES_PASSED, 6 },
            { GL_ANY_SAMPLES_PASSED, 6 },
            { GL_PRIMITIVES_GENERATED, 6 },
            { GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, 6 },
            { GL_ANY_SAMPLES_PASSED_CONSERVATIVE, 6 },
            { GL_TESS_CONTROL_SHADER_PATCHES, 6},
            { GL_TESS_EVALUATION_SHADER_INVOCATIONS, 6}
        }
    );

    // Once OpenGL is ready for rendering, init CEGUI
    if (config.gui.cegui.enabled) {
        _GUIGLrenderer = &CEGUI::OpenGL3Renderer::create();
        _GUIGLrenderer->enableExtraStateSettings(false);
        _context.context().gui().setRenderer(*_GUIGLrenderer);
    }

    glClearColor(DefaultColours::BLACK.r,
                 DefaultColours::BLACK.g,
                 DefaultColours::BLACK.b,
                 DefaultColours::BLACK.a);

    _performanceQueries[to_base(QueryType::GPU_TIME)] = eastl::make_unique<glHardwareQueryRing>(_context, GL_TIME_ELAPSED, 6);
    _performanceQueries[to_base(QueryType::VERTICES_SUBMITTED)] = eastl::make_unique<glHardwareQueryRing>(_context, GL_VERTICES_SUBMITTED, 6);
    _performanceQueries[to_base(QueryType::PRIMITIVES_GENERATED)] = eastl::make_unique<glHardwareQueryRing>(_context, GL_PRIMITIVES_GENERATED, 6);
    _performanceQueries[to_base(QueryType::TESSELLATION_PATCHES)] = eastl::make_unique<glHardwareQueryRing>(_context, GL_TESS_CONTROL_SHADER_PATCHES, 6);
    _performanceQueries[to_base(QueryType::TESSELLATION_CTRL_INVOCATIONS)] = eastl::make_unique<glHardwareQueryRing>(_context, GL_TESS_EVALUATION_SHADER_INVOCATIONS, 6);

    // That's it. Everything should be ready for draw calls
    Console::printfn(Locale::Get(_ID("START_OGL_API_OK")));
    return ErrorCode::NO_ERR;
}

/// Clear everything that was setup in initRenderingAPI()
void GL_API::closeRenderingAPI() {
    if (_GUIGLrenderer) {
        CEGUI::OpenGL3Renderer::destroy(*_GUIGLrenderer);
        _GUIGLrenderer = nullptr;
    }

    glShaderProgram::DestroyStaticData();

    // Destroy sampler objects
    {
        for (auto &sampler : s_samplerMap) {
            glSamplerObject::destruct(sampler.second);
        }
        s_samplerMap.clear();
    }
    // Destroy the text rendering system
    glfonsDelete(_fonsContext);
    _fonsContext = nullptr;

    _fonts.clear();
    s_textureViewCache.destroy();
    if (s_hardwareQueryPool != nullptr) {
        s_hardwareQueryPool->destroy();
        MemoryManager::DELETE(s_hardwareQueryPool);
    }
    for (GLUtil::GLMemory::DeviceAllocator& allocator : s_memoryAllocators) {
        allocator.deallocate();
    }
    g_ContextPool.destroy();

    for (VAOMap::value_type& value : s_vaoCache) {
        if (value.second != GLUtil::k_invalidObjectID) {
            GL_API::DeleteVAOs(1, &value.second);
        }
    }
    s_vaoCache.clear();
    s_stateTracker.reset();
    glLockManager::Clear();
}

/// Prepare the GPU for rendering a frame
bool GL_API::beginFrame(DisplayWindow& window, const bool global) {
    OPTICK_EVENT();
    // Start a duration query in debug builds
    if (global && _runQueries) {
        if_constexpr(g_runAllQueriesInSameFrame) {
            for (U8 i = 0u; i < to_base(QueryType::COUNT); ++i) {
                _performanceQueries[i]->begin();
            }
        } else {
            _performanceQueries[_queryIdxForCurrentFrame]->begin();
        }
    }

    GLStateTracker* stateTracker = GetStateTracker();

    SDL_GLContext glContext = window.userData()->_glContext;
    const I64 windowGUID = window.getGUID();

    if (glContext != nullptr && (_currentContext._windowGUID != windowGUID || _currentContext._context != glContext)) {
        SDL_GL_MakeCurrent(window.getRawWindow(), glContext);
        _currentContext._windowGUID = windowGUID;
        _currentContext._context = glContext;
    }

    // Clear our buffers
    if (!window.minimized() && !window.hidden()) {
        bool shouldClearColour = false, shouldClearDepth = false, shouldClearStencil = false;
        stateTracker->setClearColour(window.clearColour(shouldClearColour, shouldClearDepth));
        ClearBufferMask mask = ClearBufferMask::GL_NONE_BIT;
        if (shouldClearColour) {
            mask |= ClearBufferMask::GL_COLOR_BUFFER_BIT;
        }
        if (shouldClearDepth) {
            mask |= ClearBufferMask::GL_DEPTH_BUFFER_BIT;
        }
        if (shouldClearStencil) {
            mask |= ClearBufferMask::GL_STENCIL_BUFFER_BIT;
        }
        if (mask != ClearBufferMask::GL_NONE_BIT) {
            glClear(mask);
        }
    }
    // Clears are registered as draw calls by most software, so we do the same
    // to stay in sync with third party software
    _context.registerDrawCall();

    clearStates(window, stateTracker, global);

    const vec2<U16>& drawableSize = window.getDrawableSize();
    _context.setViewport(0, 0, drawableSize.width, drawableSize.height);

    return true;
}

void GL_API::endFrameLocal(const DisplayWindow& window) {
    OPTICK_EVENT();

    // Swap buffers    
    SDL_GLContext glContext = window.userData()->_glContext;
    const I64 windowGUID = window.getGUID();

    if (glContext != nullptr && (_currentContext._windowGUID != windowGUID || _currentContext._context != glContext)) {
        OPTICK_EVENT("GL_API: Swap Context");
        SDL_GL_MakeCurrent(window.getRawWindow(), glContext);
        _currentContext._windowGUID = windowGUID;
        _currentContext._context = glContext;
    }
    {
        OPTICK_EVENT("GL_API: Swap Buffers");
        SDL_GL_SwapWindow(window.getRawWindow());
    }
}

void GL_API::endFrameGlobal(const DisplayWindow& window) {
    OPTICK_EVENT();

    if (_runQueries) {
        OPTICK_EVENT("End GPU Queries");
        // End the timing query started in beginFrame() in debug builds

        if_constexpr(g_runAllQueriesInSameFrame) {
            for (U8 i = 0; i < to_base(QueryType::COUNT); ++i) {
                _performanceQueries[i]->end();
            }
        } else {
            _performanceQueries[_queryIdxForCurrentFrame]->end();
        }
    }

    if (glGetGraphicsResetStatus() != GL_NO_ERROR) {
        DIVIDE_UNEXPECTED_CALL_MSG("OpenGL Reset Status raised!");
    }

    _swapBufferTimer.start();
    endFrameLocal(window);
    {
        //OPTICK_EVENT("Post-swap delay");
        //SDL_Delay(1);
    }
    _swapBufferTimer.stop();

    for (U32 i = 0u; i < GL_API::s_LockFrameLifetime - 1; ++i) {
        s_fenceSyncCounter[i] = s_fenceSyncCounter[i + 1];
    }

    OPTICK_EVENT("GL_API: Post-Swap cleanup");
    s_textureViewCache.onFrameEnd();
    s_glFlushQueued.store(false);
    if (ShaderProgram::s_UseBindlessTextures) {
        for (ResidentTexture& texture : s_residentTextures) {
            if (texture._address == 0u) {
                // Most common case
                continue;
            }

            if (++texture._frameCount > g_maxTextureResidencyFrameCount) {
                glMakeTextureHandleNonResidentARB(texture._address);
                texture = {};
            }
        }
    }

    if (_runQueries) {
        OPTICK_EVENT("GL_API: Time Query");
        static std::array<I64, to_base(QueryType::COUNT)> results{};
        if_constexpr(g_runAllQueriesInSameFrame) {
            for (U8 i = 0; i < to_base(QueryType::COUNT); ++i) {
                results[i] = _performanceQueries[i]->getResultNoWait();
                _performanceQueries[i]->incQueue();
            }
        } else {
            results[_queryIdxForCurrentFrame] = _performanceQueries[_queryIdxForCurrentFrame]->getResultNoWait();
            _performanceQueries[_queryIdxForCurrentFrame]->incQueue();
        }

        _queryIdxForCurrentFrame = (_queryIdxForCurrentFrame + 1) % to_base(QueryType::COUNT);

        if (g_runAllQueriesInSameFrame || _queryIdxForCurrentFrame == 0) {
            _perfMetrics._gpuTimeInMS = Time::NanosecondsToMilliseconds<F32>(results[to_base(QueryType::GPU_TIME)]);
            _perfMetrics._verticesSubmitted = to_U64(results[to_base(QueryType::VERTICES_SUBMITTED)]);
            _perfMetrics._primitivesGenerated = to_U64(results[to_base(QueryType::PRIMITIVES_GENERATED)]);
            _perfMetrics._tessellationPatches = to_U64(results[to_base(QueryType::TESSELLATION_PATCHES)]);
            _perfMetrics._tessellationInvocations = to_U64(results[to_base(QueryType::TESSELLATION_CTRL_INVOCATIONS)]);
        }
    }

    const size_t fenceSize = std::size(s_fenceSyncCounter);

    for (size_t i = 0u; i < std::size(_perfMetrics._syncObjectsInFlight); ++i) {
        _perfMetrics._syncObjectsInFlight[i] = i < fenceSize ? s_fenceSyncCounter[i] : 0u;
    }

    _perfMetrics._generatedRenderTargetCount = to_U32(_context.renderTargetPool().getRenderTargets().size());
    _runQueries = _context.queryPerformanceStats();
}

/// Finish rendering the current frame
void GL_API::endFrame(DisplayWindow& window, const bool global) {
    OPTICK_EVENT();

    if (global) {
        endFrameGlobal(window);
    } else {
        endFrameLocal(window);
    }
}

void GL_API::idle([[maybe_unused]] const bool fast) {
    glShaderProgram::Idle(_context.context());
}


/// Text rendering is handled exclusively by Mikko Mononen's FontStash library (https://github.com/memononen/fontstash)
/// with his OpenGL frontend adapted for core context profiles
void GL_API::drawText(const TextElementBatch& batch) {
    OPTICK_EVENT();

    BlendingSettings textBlend{};
    textBlend.blendSrc(BlendProperty::SRC_ALPHA);
    textBlend.blendDest(BlendProperty::INV_SRC_ALPHA);
    textBlend.blendOp(BlendOperation::ADD);
    textBlend.blendSrcAlpha(BlendProperty::ONE);
    textBlend.blendDestAlpha(BlendProperty::ZERO);
    textBlend.blendOpAlpha(BlendOperation::COUNT);
    textBlend.enabled(true);

    GetStateTracker()->setBlending(0, textBlend);
    GetStateTracker()->setBlendColour(DefaultColours::BLACK_U8);

    const I32 width = _context.renderingResolution().width;
    const I32 height = _context.renderingResolution().height;
        
    size_t drawCount = 0;
    size_t previousStyle = 0;

    fonsClearState(_fonsContext);
    for (const TextElement& entry : batch.data())
    {
        if (previousStyle != entry.textLabelStyleHash()) {
            const TextLabelStyle& textLabelStyle = TextLabelStyle::get(entry.textLabelStyleHash());
            const UColour4& colour = textLabelStyle.colour();
            // Retrieve the font from the font cache
            const I32 font = getFont(TextLabelStyle::fontName(textLabelStyle.font()));
            // The font may be invalid, so skip this text label
            if (font != FONS_INVALID) {
                fonsSetFont(_fonsContext, font);
            }
            fonsSetBlur(_fonsContext, textLabelStyle.blurAmount());
            fonsSetBlur(_fonsContext, textLabelStyle.spacing());
            fonsSetAlign(_fonsContext, textLabelStyle.alignFlag());
            fonsSetSize(_fonsContext, to_F32(textLabelStyle.fontSize()));
            fonsSetColour(_fonsContext, colour.r, colour.g, colour.b, colour.a);
            previousStyle = entry.textLabelStyleHash();
        }

        const F32 textX = entry.position().d_x.d_scale * width + entry.position().d_x.d_offset;
        const F32 textY = height - (entry.position().d_y.d_scale * height + entry.position().d_y.d_offset);

        F32 lh = 0;
        fonsVertMetrics(_fonsContext, nullptr, nullptr, &lh);
        
        const TextElement::TextType& text = entry.text();
        const size_t lineCount = text.size();
        for (size_t i = 0; i < lineCount; ++i) {
            fonsDrawText(_fonsContext,
                         textX,
                         textY - lh * i,
                         text[i].c_str(),
                         nullptr);
        }
        drawCount += lineCount;
        

        // Register each label rendered as a draw call
        _context.registerDrawCalls(to_U32(drawCount));
    }
}

void GL_API::drawIMGUI(const ImDrawData* data, I64 windowGUID) {
    static I32 s_maxCommandCount = 8u;

    constexpr U32 MaxVertices = (1 << 16);
    constexpr U32 MaxIndices = MaxVertices * 3u;
    static ImDrawVert vertices[MaxVertices];
    static ImDrawIdx indices[MaxIndices];

    OPTICK_EVENT();

    assert(data != nullptr);
    if (!data->Valid) {
        return;
    }

    s_maxCommandCount = std::max(s_maxCommandCount, data->CmdListsCount);
    GenericVertexData* buffer = _context.getOrCreateIMGUIBuffer(windowGUID, s_maxCommandCount, MaxVertices);
    assert(buffer != nullptr);
    buffer->incQueue();

    //ref: https://gist.github.com/floooh/10388a0afbe08fce9e617d8aefa7d302
    I32 numVertices = 0, numIndices = 0;
    for (I32 n = 0; n < data->CmdListsCount; ++n) {
        const ImDrawList* cl = data->CmdLists[n];
        const I32 clNumVertices = cl->VtxBuffer.size();
        const int clNumIndices  = cl->IdxBuffer.size();

        if ((numVertices + clNumVertices) > MaxVertices ||
            (numIndices  + clNumIndices)  > MaxIndices)
        {
            break;
        }

        memcpy(&vertices[numVertices], cl->VtxBuffer.Data, clNumVertices * sizeof(ImDrawVert));
        memcpy(&indices[numIndices],   cl->IdxBuffer.Data, clNumIndices  * sizeof(ImDrawIdx));

        numVertices += clNumVertices;
        numIndices  += clNumIndices;
    }

    buffer->updateBuffer(0u, 0u, numVertices, vertices);

    GenericVertexData::IndexBuffer idxBuffer{};
    idxBuffer.smallIndices = sizeof(ImDrawIdx) == sizeof(U16);
    idxBuffer.dynamic = true;
    idxBuffer.count = numIndices;
    idxBuffer.data = indices;
    buffer->setIndexBuffer(idxBuffer);

    GLStateTracker* stateTracker = GetStateTracker();

    U32 baseVertex = 0u;
    U32 indexOffset = 0u;
    for (I32 n = 0; n < data->CmdListsCount; ++n) {
        const ImDrawList* cmd_list = data->CmdLists[n];
        for (const ImDrawCmd& pcmd : cmd_list->CmdBuffer) {

            if (pcmd.UserCallback) {
                // User callback (registered via ImDrawList::AddCallback)
                pcmd.UserCallback(cmd_list, &pcmd);
            } else {
                Rect<I32> clipRect{
                    pcmd.ClipRect.x - data->DisplayPos.x,
                    pcmd.ClipRect.y - data->DisplayPos.y,
                    pcmd.ClipRect.z - data->DisplayPos.x,
                    pcmd.ClipRect.w - data->DisplayPos.y
                };

                const Rect<I32>& viewport = stateTracker->_activeViewport;
                if (clipRect.x < viewport.z &&
                    clipRect.y < viewport.w &&
                    clipRect.z >= 0 &&
                    clipRect.w >= 0)
                {
                    const I32 tempW = clipRect.w;
                    clipRect.z -= clipRect.x;
                    clipRect.w -= clipRect.y;
                    clipRect.y  = viewport.w - tempW;

                    stateTracker->setScissor(clipRect);
                    if (stateTracker->bindTexture(to_U8(TextureUsage::UNIT0),
                                                    TextureType::TEXTURE_2D,
                                                    static_cast<GLuint>(reinterpret_cast<intptr_t>(pcmd.TextureId))) == GLStateTracker::BindResult::FAILED) {
                        DIVIDE_UNEXPECTED_CALL();
                    }

                    GenericDrawCommand cmd{};
                    cmd._cmd.indexCount = pcmd.ElemCount;
                    cmd._cmd.firstIndex = indexOffset + pcmd.IdxOffset;
                    cmd._cmd.baseVertex = baseVertex + pcmd.VtxOffset;
                    buffer->draw(cmd);
                }
            }
        }
        indexOffset += cmd_list->IdxBuffer.size();
        baseVertex += cmd_list->VtxBuffer.size();
    }

    buffer->insertFencesIfNeeded();
}

bool GL_API::draw(const GenericDrawCommand& cmd) const {
    OPTICK_EVENT();

    if (cmd._sourceBuffer._id == 0) {
        U32 indexCount = 0u;
        switch (GL_API::GetStateTracker()->_activeTopology) {
            case PrimitiveTopology::COUNT     : DIVIDE_UNEXPECTED_CALL();         break;
            case PrimitiveTopology::TRIANGLES : indexCount = cmd._drawCount * 3;  break;
            case PrimitiveTopology::POINTS    : indexCount = cmd._drawCount * 1;  break;
            default                           : indexCount = cmd._cmd.indexCount; break;
        }

        glDrawArrays(GLUtil::glPrimitiveTypeTable[to_base(GL_API::GetStateTracker()->_activeTopology)], cmd._cmd.firstIndex, indexCount);
    } else {
        // Because this can only happen on the main thread, try and avoid costly lookups for hot-loop drawing
        static VertexDataInterface::Handle s_lastID = { U16_MAX, 0u };
        static VertexDataInterface* s_lastBuffer = nullptr;

        if (s_lastID != cmd._sourceBuffer) {
            s_lastID = cmd._sourceBuffer;
            s_lastBuffer = VertexDataInterface::s_VDIPool.find(s_lastID);
        }

        DIVIDE_ASSERT(s_lastBuffer != nullptr);
        s_lastBuffer->draw(cmd);
    }

    return true;
}

void GL_API::preFlushCommandBuffer([[maybe_unused]] const GFX::CommandBuffer& commandBuffer) {
    NOP();
}

void GL_API::flushCommand(GFX::CommandBase* cmd) {
    static GFX::MemoryBarrierCommand pushConstantsMemCommand{};
    static bool pushConstantsNeedLock = false;

    OPTICK_EVENT();

    OPTICK_TAG("Type", to_base(cmd->Type()));

    const auto lockPushConstants = [&]() {
        flushCommand(&pushConstantsMemCommand);
        pushConstantsMemCommand._bufferLocks.resize(0);
    };

    switch (cmd->Type()) {
        case GFX::CommandType::BEGIN_RENDER_PASS: {
            OPTICK_EVENT("BEGIN_RENDER_PASS");

            const GFX::BeginRenderPassCommand* crtCmd = cmd->As<GFX::BeginRenderPassCommand>();

            glFramebuffer* rt = static_cast<glFramebuffer*>(_context.renderTargetPool().getRenderTarget(crtCmd->_target));
            Attorney::GLAPIRenderTarget::begin(*rt, crtCmd->_descriptor);
            GetStateTracker()->_activeRenderTarget = rt;
            PushDebugMessage(crtCmd->_name.c_str());
        }break;
        case GFX::CommandType::END_RENDER_PASS: {
            OPTICK_EVENT("END_RENDER_PASS");
            PopDebugMessage();

            assert(GL_API::GetStateTracker()->_activeRenderTarget != nullptr);
            Attorney::GLAPIRenderTarget::end(
                *GetStateTracker()->_activeRenderTarget,
                cmd->As<GFX::EndRenderPassCommand>()->_setDefaultRTState
            );
        }break;
        case GFX::CommandType::BEGIN_RENDER_SUB_PASS: {
            OPTICK_EVENT("BEGIN_RENDER_SUB_PASS");

            const GFX::BeginRenderSubPassCommand* crtCmd = cmd->As<GFX::BeginRenderSubPassCommand>();

            assert(GL_API::GetStateTracker()->_activeRenderTarget != nullptr);
            for (const RenderTarget::DrawLayerParams& params : crtCmd->_writeLayers) {
                GetStateTracker()->_activeRenderTarget->drawToLayer(params);
            }

            GetStateTracker()->_activeRenderTarget->setMipLevel(crtCmd->_mipWriteLevel);
        }break;
        case GFX::CommandType::END_RENDER_SUB_PASS: {
            OPTICK_EVENT("END_RENDER_SUB_PASS");
        }break;
        case GFX::CommandType::COPY_TEXTURE: {
            OPTICK_EVENT("COPY_TEXTURE");

            const GFX::CopyTextureCommand* crtCmd = cmd->As<GFX::CopyTextureCommand>();
            glTexture::copy(crtCmd->_source, crtCmd->_destination, crtCmd->_params);
        }break;
        case GFX::CommandType::BIND_DESCRIPTOR_SETS: {
            OPTICK_EVENT("BIND_DESCRIPTOR_SETS");

            GFX::BindDescriptorSetsCommand* crtCmd = cmd->As<GFX::BindDescriptorSetsCommand>();
            if (!crtCmd->_set._textureViews.empty() &&
                makeTextureViewsResidentInternal(crtCmd->_set._textureViews, 0u, U8_MAX) == GLStateTracker::BindResult::FAILED)
            {
                DIVIDE_UNEXPECTED_CALL();
            }
            if (!crtCmd->_set._textureData.empty() &&
                makeTexturesResidentInternal(crtCmd->_set._textureData, 0u, U8_MAX) == GLStateTracker::BindResult::FAILED)
            {
                DIVIDE_UNEXPECTED_CALL();
            }
        }break;
        case GFX::CommandType::BIND_PIPELINE: {
            if (pushConstantsNeedLock) {
                lockPushConstants();
                pushConstantsNeedLock = false;
            }

            OPTICK_EVENT("BIND_PIPELINE");

            const Pipeline* pipeline = cmd->As<GFX::BindPipelineCommand>()->_pipeline;
            assert(pipeline != nullptr);
            if (bindPipeline(*pipeline) == ShaderResult::Failed) {
                Console::errorfn(Locale::Get(_ID("ERROR_GLSL_INVALID_BIND")), pipeline->descriptor()._shaderProgramHandle);
            }
        } break;
        case GFX::CommandType::SEND_PUSH_CONSTANTS: {
            OPTICK_EVENT("SEND_PUSH_CONSTANTS");

            const auto dumpLogs = [this]() {
                Console::d_errorfn(Locale::Get(_ID("ERROR_GLSL_INVALID_PUSH_CONSTANTS")));
                if (Config::ENABLE_GPU_VALIDATION) {
                    // Shader failed to compile probably. Dump all shader caches for inspection.
                    glShaderProgram::Idle(_context.context());
                    Console::flush();
                }
            };

            const Pipeline* activePipeline = GetStateTracker()->_activePipeline;
            if (activePipeline == nullptr) {
                dumpLogs();
                break;
            }

            ShaderProgram* program = ShaderProgram::FindShaderProgram(activePipeline->descriptor()._shaderProgramHandle);
            if (program == nullptr) {
                // Should we skip the upload?
                dumpLogs();
                break;
            }

            const PushConstants& pushConstants = cmd->As<GFX::SendPushConstantsCommand>()->_constants;
            program->uploadPushConstants(pushConstants, pushConstantsMemCommand);
            pushConstantsNeedLock = !pushConstantsMemCommand._bufferLocks.empty();
        } break;
        case GFX::CommandType::SET_SCISSOR: {
            OPTICK_EVENT("SET_SCISSOR");

            GetStateTracker()->setScissor(cmd->As<GFX::SetScissorCommand>()->_rect);
        }break;
        case GFX::CommandType::SET_TEXTURE_RESIDENCY: {
            OPTICK_EVENT("SET_TEXTURE_RESIDENCY");

            const GFX::SetTexturesResidencyCommand* crtCmd = cmd->As<GFX::SetTexturesResidencyCommand>();
            if (crtCmd->_state) {
                for (const SamplerAddress address : crtCmd->_addresses) {
                    MakeTexturesResidentInternal(address);
                }
            } else {
                for (const SamplerAddress address : crtCmd->_addresses) {
                    MakeTexturesNonResidentInternal(address);
                }
            }
        }break;
        case GFX::CommandType::BEGIN_DEBUG_SCOPE: {
            OPTICK_EVENT("BEGIN_DEBUG_SCOPE");
            PushDebugMessage(cmd->As<GFX::BeginDebugScopeCommand>()->_scopeName.c_str());
        } break;
        case GFX::CommandType::END_DEBUG_SCOPE: {
            OPTICK_EVENT("END_DEBUG_SCOPE");
            PopDebugMessage();
        } break;
        case GFX::CommandType::ADD_DEBUG_MESSAGE: {
            OPTICK_EVENT("ADD_DEBUG_MESSAGE");
            PushDebugMessage(cmd->As<GFX::AddDebugMessageCommand>()->_msg.c_str());
            PopDebugMessage();
        }break;
        case GFX::CommandType::COMPUTE_MIPMAPS: {
            OPTICK_EVENT("COMPUTE_MIPMAPS");

            const GFX::ComputeMipMapsCommand* crtCmd = cmd->As<GFX::ComputeMipMapsCommand>();

            if (crtCmd->_layerRange.x == 0 && crtCmd->_layerRange.y == crtCmd->_texture->descriptor().layerCount()) {
                OPTICK_EVENT("GL: In-place computation - Full");
                glGenerateTextureMipmap(crtCmd->_texture->data()._textureHandle);
            } else {
                OPTICK_EVENT("GL: View-based computation");

                const TextureDescriptor& descriptor = crtCmd->_texture->descriptor();
                const GLenum glInternalFormat = GLUtil::internalFormat(descriptor.baseFormat(), descriptor.dataType(), descriptor.srgb(), descriptor.normalized());

                TextureView view = {};
                view._textureData = crtCmd->_texture->data();
                view._layerRange.set(crtCmd->_layerRange);
                view._targetType = view._textureData._textureType;

                if (crtCmd->_mipRange.max == 0u) {
                    view._mipLevels.set(0, crtCmd->_texture->mipCount());
                } else {
                    view._mipLevels.set(crtCmd->_mipRange);
                }
                assert(IsValid(view._textureData));

                if (IsArrayTexture(view._targetType) && view._layerRange.max == 1) {
                    switch (view._targetType) {
                        case TextureType::TEXTURE_2D_ARRAY:
                            view._targetType = TextureType::TEXTURE_2D;
                            break;
                        case TextureType::TEXTURE_2D_ARRAY_MS:
                            view._targetType = TextureType::TEXTURE_2D_MS;
                            break;
                        case TextureType::TEXTURE_CUBE_ARRAY:
                            view._targetType = TextureType::TEXTURE_CUBE_MAP;
                            break;
                        default: break;
                    }
                }

                if (IsCubeTexture(view._targetType)) {
                    view._layerRange *= 6; //offset and count
                }

                auto[handle, cacheHit] = s_textureViewCache.allocate(view.getHash());

                if (!cacheHit)
                {
                    OPTICK_EVENT("GL: cache miss  - Image");
                    glTextureView(handle,
                                  GLUtil::glTextureTypeTable[to_base(view._targetType)],
                                  view._textureData._textureHandle,
                                  glInternalFormat,
                                  static_cast<GLuint>(view._mipLevels.x),
                                  static_cast<GLuint>(view._mipLevels.y),
                                  static_cast<GLuint>(view._layerRange.x),
                                  static_cast<GLuint>(view._layerRange.y));
                }
                if (view._mipLevels.x != 0u || view._mipLevels.y != 0u) {
                    OPTICK_EVENT("GL: In-place computation - Image");
                    glGenerateTextureMipmap(handle);
                }
                s_textureViewCache.deallocate(handle, 3);
            }
        }break;
        case GFX::CommandType::DRAW_TEXT: {
            OPTICK_EVENT("DRAW_TEXT");

            if (GetStateTracker()->_activePipeline != nullptr) {
                drawText(cmd->As<GFX::DrawTextCommand>()->_batch);
            }
        }break;
        case GFX::CommandType::DRAW_IMGUI: {
            OPTICK_EVENT("DRAW_IMGUI");

            if (GetStateTracker()->_activePipeline != nullptr) {
                const GFX::DrawIMGUICommand* crtCmd = cmd->As<GFX::DrawIMGUICommand>();
                drawIMGUI(crtCmd->_data, crtCmd->_windowGUID);
            }
        }break;
        case GFX::CommandType::DRAW_COMMANDS : {
            OPTICK_EVENT("DRAW_COMMANDS");
            DIVIDE_ASSERT(GetStateTracker()->_activePipeline != nullptr);

            U32 drawCount = 0u;
            const GFX::DrawCommand::CommandContainer& drawCommands = cmd->As<GFX::DrawCommand>()->_drawCommands;
            for (const GenericDrawCommand& currentDrawCommand : drawCommands) {
                if (draw(currentDrawCommand)) {
                    drawCount += isEnabledOption(currentDrawCommand, CmdRenderOptions::RENDER_WIREFRAME) 
                                       ? 2 
                                       : isEnabledOption(currentDrawCommand, CmdRenderOptions::RENDER_GEOMETRY) ? 1 : 0;
                }
            }
            _context.registerDrawCalls(drawCount);
        }break;
        case GFX::CommandType::DISPATCH_COMPUTE: {
            OPTICK_EVENT("DISPATCH_COMPUTE");

            if (GetStateTracker()->_activePipeline != nullptr) {
                const GFX::DispatchComputeCommand* crtCmd = cmd->As<GFX::DispatchComputeCommand>();
                const vec3<U32>& workGroupCount = crtCmd->_computeGroupSize;
                DIVIDE_ASSERT(workGroupCount.x < GFXDevice::GetDeviceInformation()._maxWorgroupCount[0] &&
                              workGroupCount.y < GFXDevice::GetDeviceInformation()._maxWorgroupCount[1] &&
                              workGroupCount.z < GFXDevice::GetDeviceInformation()._maxWorgroupCount[2]);
                glDispatchCompute(crtCmd->_computeGroupSize.x, crtCmd->_computeGroupSize.y, crtCmd->_computeGroupSize.z);
            }
        }break;
        case GFX::CommandType::SET_CLIPING_STATE: {
            OPTICK_EVENT("SET_CLIPING_STATE");

            const GFX::SetClippingStateCommand* crtCmd = cmd->As<GFX::SetClippingStateCommand>();
            GetStateTracker()->setClippingPlaneState(crtCmd->_lowerLeftOrigin, crtCmd->_negativeOneToOneDepth);
        } break;
        case GFX::CommandType::MEMORY_BARRIER: {
            OPTICK_EVENT("MEMORY_BARRIER");

            const GFX::MemoryBarrierCommand* crtCmd = cmd->As<GFX::MemoryBarrierCommand>();
            const U32 barrierMask = crtCmd->_barrierMask;
            if (barrierMask != 0) {
                if (BitCompare(barrierMask, to_base(MemoryBarrierType::TEXTURE_BARRIER))) {
                    glTextureBarrier();
                } 
                if (barrierMask == to_base(MemoryBarrierType::ALL_MEM_BARRIERS)) {
                    glMemoryBarrier(MemoryBarrierMask::GL_ALL_BARRIER_BITS);
                } else {
                    MemoryBarrierMask glMask = MemoryBarrierMask::GL_NONE_BIT;
                    for (U8 i = 0; i < to_U8(MemoryBarrierType::COUNT) + 1; ++i) {
                        if (BitCompare(barrierMask, 1u << i)) {
                            switch (static_cast<MemoryBarrierType>(1 << i)) {
                            case MemoryBarrierType::BUFFER_UPDATE:
                                glMask |= MemoryBarrierMask::GL_BUFFER_UPDATE_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::SHADER_STORAGE:
                                glMask |= MemoryBarrierMask::GL_SHADER_STORAGE_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::COMMAND_BUFFER:
                                glMask |= MemoryBarrierMask::GL_COMMAND_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::ATOMIC_COUNTER:
                                glMask |= MemoryBarrierMask::GL_ATOMIC_COUNTER_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::QUERY:
                                glMask |= MemoryBarrierMask::GL_QUERY_BUFFER_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::RENDER_TARGET:
                                glMask |= MemoryBarrierMask::GL_FRAMEBUFFER_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::TEXTURE_UPDATE:
                                glMask |= MemoryBarrierMask::GL_TEXTURE_UPDATE_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::TEXTURE_FETCH:
                                glMask |= MemoryBarrierMask::GL_TEXTURE_FETCH_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::SHADER_IMAGE:
                                glMask |= MemoryBarrierMask::GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::TRANSFORM_FEEDBACK:
                                glMask |= MemoryBarrierMask::GL_TRANSFORM_FEEDBACK_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::VERTEX_ATTRIB_ARRAY:
                                glMask |= MemoryBarrierMask::GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::INDEX_ARRAY:
                                glMask |= MemoryBarrierMask::GL_ELEMENT_ARRAY_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::UNIFORM_DATA:
                                glMask |= MemoryBarrierMask::GL_UNIFORM_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::PIXEL_BUFFER:
                                glMask |= MemoryBarrierMask::GL_PIXEL_BUFFER_BARRIER_BIT;
                                break;
                            case MemoryBarrierType::PERSISTENT_BUFFER:
                                glMask |= MemoryBarrierMask::GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT;
                                break;
                            default:
                                NOP();
                                break;
                            }
                        }
                    }
                    glMemoryBarrier(glMask);
                }

                if (!crtCmd->_bufferLocks.empty()) {
                    SyncObject* sync = glLockManager::CreateSyncObject();
                    for (const BufferLock& lock : crtCmd->_bufferLocks) {
                        if (!lock._targetBuffer->lockByteRange(lock._range, sync)) {
                            DIVIDE_UNEXPECTED_CALL();
                        }
                    }
                }
            }
        } break;
        default: break;
    }

    switch (cmd->Type()) {
        case GFX::CommandType::EXTERNAL:
        case GFX::CommandType::DISPATCH_COMPUTE:
        case GFX::CommandType::DRAW_IMGUI:
        case GFX::CommandType::DRAW_TEXT:
        case GFX::CommandType::DRAW_COMMANDS: {
            if (pushConstantsNeedLock) {
                lockPushConstants();
                pushConstantsNeedLock = false;
            }
        } break;
        default: break;
    }
}

void GL_API::postFlushCommandBuffer([[maybe_unused]] const GFX::CommandBuffer& commandBuffer) {
    OPTICK_EVENT();

    bool expected = true;
    if (s_glFlushQueued.compare_exchange_strong(expected, false)) {
        OPTICK_EVENT("GL_FLUSH");
        glFlush();
    }
}

vec2<U16> GL_API::getDrawableSize(const DisplayWindow& window) const noexcept {
    int w = 1, h = 1;
    SDL_GL_GetDrawableSize(window.getRawWindow(), &w, &h);
    return vec2<U16>(w, h);
}

U32 GL_API::getHandleFromCEGUITexture(const CEGUI::Texture& textureIn) const {
    return to_U32(static_cast<const CEGUI::OpenGLTexture&>(textureIn).getOpenGLTexture());
}

void GL_API::onThreadCreated([[maybe_unused]] const std::thread::id& threadID) {
    // Double check so that we don't run into a race condition!
    ScopedLock<Mutex> lock(GLUtil::s_glSecondaryContextMutex);
    assert(SDL_GL_GetCurrentContext() == NULL);

    // This also makes the context current
    assert(GLUtil::s_glSecondaryContext == nullptr && "GL_API::syncToThread: double init context for current thread!");
    [[maybe_unused]] const bool ctxFound = g_ContextPool.getAvailableContext(GLUtil::s_glSecondaryContext);
    assert(ctxFound && "GL_API::syncToThread: context not found for current thread!");

    SDL_GL_MakeCurrent(GLUtil::s_glMainRenderWindow->getRawWindow(), GLUtil::s_glSecondaryContext);
    glbinding::Binding::initialize([](const char* proc) noexcept {
        return (glbinding::ProcAddress)SDL_GL_GetProcAddress(proc);
    });
    
    // Enable OpenGL debug callbacks for this context as well
    if_constexpr(Config::ENABLE_GPU_VALIDATION) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        // Debug callback in a separate thread requires a flag to distinguish it from the main thread's callbacks
        glDebugMessageCallback((GLDEBUGPROC)GLUtil::DebugCallback, GLUtil::s_glSecondaryContext);
    }

    glMaxShaderCompilerThreadsARB(0xFFFFFFFF);
}

/// Try to find the requested font in the font cache. Load on cache miss.
I32 GL_API::getFont(const Str64& fontName) {
    if (_fontCache.first.compare(fontName) != 0) {
        _fontCache.first = fontName;
        const U64 fontNameHash = _ID(fontName.c_str());
        // Search for the requested font by name
        const auto& it = _fonts.find(fontNameHash);
        // If we failed to find it, it wasn't loaded yet
        if (it == std::cend(_fonts)) {
            // Fonts are stored in the general asset directory -> in the GUI
            // subfolder -> in the fonts subfolder
            ResourcePath fontPath(Paths::g_assetsLocation + Paths::g_GUILocation + Paths::g_fontsPath);
            fontPath += fontName.c_str();
            // We use FontStash to load the font file
            _fontCache.second = fonsAddFont(_fonsContext, fontName.c_str(), fontPath.c_str());
            // If the font is invalid, inform the user, but map it anyway, to avoid
            // loading an invalid font file on every request
            if (_fontCache.second == FONS_INVALID) {
                Console::errorfn(Locale::Get(_ID("ERROR_FONT_FILE")), fontName.c_str());
            }
            // Save the font in the font cache
            hashAlg::insert(_fonts, fontNameHash, _fontCache.second);
            
        } else {
            _fontCache.second = it->second;
        }

    }

    // Return the font
    return _fontCache.second;
}

/// Reset as much of the GL default state as possible within the limitations given
void GL_API::clearStates(const DisplayWindow& window, GLStateTracker* stateTracker, const bool global) const {
    if (global) {
        if (stateTracker->bindTextures(0, GFXDevice::GetDeviceInformation()._maxTextureUnits - 1, TextureType::COUNT, nullptr, nullptr) == GLStateTracker::BindResult::FAILED) {
            DIVIDE_UNEXPECTED_CALL();
        }
        stateTracker->setPixelPackUnpackAlignment();
    }

    if (stateTracker->setActiveVAO(0) == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }
    if (stateTracker->setActiveBuffer(GL_ELEMENT_ARRAY_BUFFER, 0) == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }
    if (stateTracker->setActiveFB(RenderTarget::RenderTargetUsage::RT_READ_WRITE, 0) == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }
    stateTracker->_activeClearColour.set(window.clearColour());
    const U8 blendCount = to_U8(stateTracker->_blendEnabled.size());
    for (U8 i = 0u; i < blendCount; ++i) {
        stateTracker->setBlending(i, {});
    }
    stateTracker->setBlendColour({ 0u, 0u, 0u, 0u });

    const vec2<U16>& drawableSize = _context.getDrawableSize(window);
    stateTracker->setScissor({ 0, 0, drawableSize.width, drawableSize.height });

    stateTracker->_activePipeline = nullptr;
    stateTracker->_activeRenderTarget = nullptr;
    if (stateTracker->setActiveProgram(0u) == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }
    if (stateTracker->setActiveShaderPipeline(0u) == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }
    if (stateTracker->setStateBlock(RenderStateBlock::DefaultHash()) == GLStateTracker::BindResult::FAILED) {
        DIVIDE_UNEXPECTED_CALL();
    }

    stateTracker->setDepthWrite(true);
}

GLStateTracker::BindResult GL_API::makeTexturesResidentInternal(TextureDataContainer& textureData, const U8 offset, U8 count) const {
    // All of the complicate and fragile code bellow does actually provide a measurable performance increase 
    // (micro second range for a typical scene, nothing amazing, but still ...)
    // CPU cost is comparable to the multiple glBind calls on some specific driver + GPU combos.

    constexpr GLuint k_textureThreshold = 3;
    GLStateTracker* stateTracker = GetStateTracker();

    const size_t totalTextureCount = textureData.count();

    count = std::min(count, to_U8(totalTextureCount - offset));
    assert(to_size(offset) + count <= totalTextureCount);
    const auto& textures = textureData._entries;

    GLStateTracker::BindResult result = GLStateTracker::BindResult::FAILED;
    if (count > 1) {
        // If we have 3 or more textures, there's a chance we might get a binding gap, so just sort
        if (totalTextureCount > 2) {
            textureData.sortByBinding();
        }

        U8 prevBinding = textures.front()._binding;
        const TextureType targetType = textures.front()._data._textureType;

        U8 matchingTexCount = 0u;
        U8 startBinding = U8_MAX;
        U8 endBinding = 0u; 

        for (U8 idx = offset; idx < offset + count; ++idx) {
            const TextureEntry& entry = textures[idx];
            assert(IsValid(entry._data));
            if (entry._binding != INVALID_TEXTURE_BINDING && targetType != entry._data._textureType) {
                break;
            }
            // Avoid large gaps between bindings. It's faster to just bind them individually.
            if (matchingTexCount > 0 && entry._binding - prevBinding > k_textureThreshold) {
                break;
            }
            // We mainly want to handle ONLY consecutive units
            prevBinding = entry._binding;
            startBinding = std::min(startBinding, entry._binding);
            endBinding = std::max(endBinding, entry._binding);
            ++matchingTexCount;
        }

        if (matchingTexCount >= k_textureThreshold) {
            static vector<GLuint> handles{};
            static vector<GLuint> samplers{};
            static bool init = false;
            if (!init) {
                init = true;
                handles.resize(GFXDevice::GetDeviceInformation()._maxTextureUnits, GLUtil::k_invalidObjectID);
                samplers.resize(GFXDevice::GetDeviceInformation()._maxTextureUnits, GLUtil::k_invalidObjectID);
            } else {
                std::memset(&handles[startBinding], GLUtil::k_invalidObjectID, (to_size(endBinding - startBinding) + 1) * sizeof(GLuint));
            }

            for (U8 idx = offset; idx < offset + matchingTexCount; ++idx) {
                const TextureEntry& entry = textures[idx];
                if (entry._binding != INVALID_TEXTURE_BINDING) {
                    handles[entry._binding]  = entry._data._textureHandle;
                    samplers[entry._binding] = GetSamplerHandle(entry._sampler);
                }
            }

            
            for (U8 binding = startBinding; binding < endBinding; ++binding) {
                if (handles[binding] == GLUtil::k_invalidObjectID) {
                    const TextureType crtType = stateTracker->getBoundTextureType(binding);
                    samplers[binding] = stateTracker->getBoundSamplerHandle(binding);
                    handles[binding] = stateTracker->getBoundTextureHandle(binding, crtType);
                }
            }

            result = stateTracker->bindTextures(startBinding, endBinding - startBinding + 1, targetType, &handles[startBinding], &samplers[startBinding]);
        } else {
            matchingTexCount = 1;
            result = makeTexturesResidentInternal(textureData, offset, 1);
            if (result == GLStateTracker::BindResult::FAILED) {
                DIVIDE_UNEXPECTED_CALL();
            }
        }

        // Recurse to try and get more matches
        result = makeTexturesResidentInternal(textureData, offset + matchingTexCount, count - matchingTexCount);
    } else if (count == 1) {
        // Normal usage. Bind a single texture at a time
        const TextureEntry& entry = textures[offset];
        if (entry._binding != INVALID_TEXTURE_BINDING) {
            assert(IsValid(entry._data));
            const GLuint handle = entry._data._textureHandle;
            const GLuint sampler = GetSamplerHandle(entry._sampler);
            result = stateTracker->bindTextures(entry._binding, 1, entry._data._textureType, &handle, &sampler);
        }
    } else {
        result = GLStateTracker::BindResult::ALREADY_BOUND;
    }

    return result;
}

GLStateTracker::BindResult GL_API::makeTextureViewsResidentInternal(const TextureViews& textureViews, const U8 offset, U8 count) const {
    count = std::min(count, to_U8(textureViews.count()));

    GLStateTracker::BindResult result = GLStateTracker::BindResult::FAILED;
    for (U8 i = offset; i < count + offset; ++i) {
        const auto& it = textureViews._entries[i];
        const size_t viewHash = it.getHash();
        TextureView view = it._view;
        if (view._targetType == TextureType::COUNT) {
            view._targetType = view._textureData._textureType;
        }
        const TextureData& data = view._textureData;
        assert(IsValid(data));

        auto [textureID, cacheHit] = s_textureViewCache.allocate(viewHash);
        DIVIDE_ASSERT(textureID != 0u);

        if (!cacheHit)
        {
            const GLenum glInternalFormat = GLUtil::internalFormat(it._descriptor.baseFormat(), it._descriptor.dataType(), it._descriptor.srgb(), it._descriptor.normalized());

            if (IsCubeTexture(view._targetType)) {
                view._layerRange *= 6;
            }

            glTextureView(textureID,
                GLUtil::glTextureTypeTable[to_base(view._targetType)],
                data._textureHandle,
                glInternalFormat,
                static_cast<GLuint>(it._view._mipLevels.x),
                static_cast<GLuint>(it._view._mipLevels.y),
                view._layerRange.min,
                view._layerRange.max);
        }

        const GLuint samplerHandle = GetSamplerHandle(it._view._samplerHash);
        result = GL_API::GetStateTracker()->bindTextures(static_cast<GLushort>(it._binding), 1, view._targetType, &textureID, &samplerHandle);
        if (result == GLStateTracker::BindResult::FAILED) {
            DIVIDE_UNEXPECTED_CALL();
        }
        // Self delete after 3 frames unless we use it again
        s_textureViewCache.deallocate(textureID, 3u);
    }

    return result;
}

bool GL_API::setViewport(const Rect<I32>& viewport) {
    return GetStateTracker()->setViewport(viewport);
}

ShaderResult GL_API::bindPipeline(const Pipeline& pipeline) const {
    OPTICK_EVENT();
    GLStateTracker* stateTracker = GetStateTracker();

    if (stateTracker->_activePipeline && *stateTracker->_activePipeline == pipeline) {
        return ShaderResult::OK;
    }

    stateTracker->_activePipeline = &pipeline;

    const PipelineDescriptor& pipelineDescriptor = pipeline.descriptor();
    {
        OPTICK_EVENT("Set Raster State");
        // Set the proper render states
        const size_t stateBlockHash = pipelineDescriptor._stateHash == 0u ? _context.getDefaultStateBlock(false) : pipelineDescriptor._stateHash;
        // Passing 0 is a perfectly acceptable way of enabling the default render state block
        if (stateTracker->setStateBlock(stateBlockHash) == GLStateTracker::BindResult::FAILED) {
            DIVIDE_UNEXPECTED_CALL();
        }
    }
    {
        OPTICK_EVENT("Set Blending");
        U16 i = 0u;
        stateTracker->setBlendColour(pipelineDescriptor._blendStates._blendColour);
        for (const BlendingSettings& blendState : pipelineDescriptor._blendStates._settings) {
            stateTracker->setBlending(i++, blendState);
        }
    }

    ShaderResult ret = ShaderResult::Failed;
    ShaderProgram* program = ShaderProgram::FindShaderProgram(pipelineDescriptor._shaderProgramHandle);
    if (program != nullptr)
    {
        {
            OPTICK_EVENT("Set Vertex Format");
            stateTracker->setVertexFormat(program->descriptor()._primitiveTopology,
                                          program->descriptor()._vertexFormat,
                                          program->vertexFormatHash());
        }
        {
            OPTICK_EVENT("Set Shader Program");
            glShaderProgram& glProgram = static_cast<glShaderProgram&>(*program);
            // We need a valid shader as no fixed function pipeline is available
            // Try to bind the shader program. If it failed to load, or isn't loaded yet, cancel the draw request for this frame
            ret = Attorney::GLAPIShaderProgram::bind(glProgram);
        }

        if (ret != ShaderResult::OK) {
            if (stateTracker->setActiveProgram(0u) == GLStateTracker::BindResult::FAILED) {
                DIVIDE_UNEXPECTED_CALL();
            }
            if (stateTracker->setActiveShaderPipeline(0u) == GLStateTracker::BindResult::FAILED) {
                DIVIDE_UNEXPECTED_CALL();
            }
            stateTracker->_activePipeline = nullptr;
        }
    }

    return ret;
}

/******************************************************************************************************************************/
/****************************************************STATIC METHODS************************************************************/
/******************************************************************************************************************************/

GLStateTracker* GL_API::GetStateTracker() noexcept {
    DIVIDE_ASSERT(s_stateTracker != nullptr);

    return s_stateTracker.get();
}

GLUtil::GLMemory::GLMemoryType GL_API::GetMemoryTypeForUsage(const GLenum usage) noexcept {
    assert(usage != GL_NONE);
    switch (usage) {
        case GL_UNIFORM_BUFFER:
        case GL_SHADER_STORAGE_BUFFER:
            return GLUtil::GLMemory::GLMemoryType::SHADER_BUFFER;
        case GL_ELEMENT_ARRAY_BUFFER:
            return GLUtil::GLMemory::GLMemoryType::INDEX_BUFFER;
        case GL_ARRAY_BUFFER:
            return GLUtil::GLMemory::GLMemoryType::VERTEX_BUFFER;
    };

    return GLUtil::GLMemory::GLMemoryType::OTHER;
}

GLUtil::GLMemory::DeviceAllocator& GL_API::GetMemoryAllocator(const GLUtil::GLMemory::GLMemoryType memoryType) noexcept {
    return s_memoryAllocators[to_base(memoryType)];
}

bool GL_API::MakeTexturesResidentInternal(const SamplerAddress address) {
    if (!ShaderProgram::s_UseBindlessTextures) {
        return true;
    }

    if (address > 0u) {
        bool valid = false;
        // Check for existing resident textures
        for (ResidentTexture& texture : s_residentTextures) {
            if (texture._address == address) {
                texture._frameCount = 0u;
                valid = true;
                break;
            }
        }

        if (!valid) {
            // Register a new resident texture
            for (ResidentTexture& texture : s_residentTextures) {
                if (texture._address == 0u) {
                    texture._address = address;
                    texture._frameCount = 0u;
                    glMakeTextureHandleResidentARB(address);
                    valid = true;
                    break;
                }
            }
        }

        return valid;
    }

    return true;
}

bool GL_API::MakeTexturesNonResidentInternal(const SamplerAddress address) {
    if (!ShaderProgram::s_UseBindlessTextures) {
        return true;
    }

    if (address > 0u) {
        for (ResidentTexture& texture : s_residentTextures) {
            if (texture._address == address) {
                texture._address = 0u;
                texture._frameCount = 0u;
                glMakeTextureHandleNonResidentARB(address);
                return true;
            }
        }
        return false;
    }

    return true;
}
void GL_API::QueueFlush() noexcept {
    s_glFlushQueued.store(true);
}

void GL_API::PushDebugMessage(const char* message) {
    OPTICK_EVENT();

    if_constexpr(Config::ENABLE_GPU_VALIDATION) {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, static_cast<GLuint>(_ID(message)), -1, message);
    }
    assert(GetStateTracker()->_debugScopeDepth < GetStateTracker()->_debugScope.size());
    GetStateTracker()->_debugScope[GetStateTracker()->_debugScopeDepth++] = message;
}

void GL_API::PopDebugMessage() {
    OPTICK_EVENT();

    if_constexpr(Config::ENABLE_GPU_VALIDATION) {
        glPopDebugGroup();
    }
    GetStateTracker()->_debugScope[GetStateTracker()->_debugScopeDepth--] = "";
}

bool GL_API::DeleteShaderPrograms(const GLuint count, GLuint* programs) {
    if (count > 0 && programs != nullptr) {
        for (GLuint i = 0; i < count; ++i) {
            if (GetStateTracker()->_activeShaderProgram == programs[i]) {
                if (GetStateTracker()->setActiveProgram(0u) == GLStateTracker::BindResult::FAILED) {
                    DIVIDE_UNEXPECTED_CALL();
                }
            }
            glDeleteProgram(programs[i]);
        }
        
        memset(programs, 0, count * sizeof(GLuint));
        return true;
    }
    return false;
}

bool GL_API::DeleteTextures(const GLuint count, GLuint* textures, const TextureType texType) {
    if (count > 0 && textures != nullptr) {
        
        for (GLuint i = 0; i < count; ++i) {
            const GLuint crtTex = textures[i];
            if (crtTex != 0) {
                GLStateTracker* stateTracker = GetStateTracker();

                auto bindingIt = stateTracker->_textureBoundMap[to_base(texType)];
                for (GLuint& handle : bindingIt) {
                    if (handle == crtTex) {
                        handle = 0u;
                    }
                }

                for (ImageBindSettings& settings : stateTracker->_imageBoundMap) {
                    if (settings._texture == crtTex) {
                        settings.reset();
                    }
                }
            }
        }
        glDeleteTextures(count, textures);
        memset(textures, 0, count * sizeof(GLuint));
        return true;
    }

    return false;
}

bool GL_API::DeleteSamplers(const GLuint count, GLuint* samplers) {
    if (count > 0 && samplers != nullptr) {

        for (GLuint i = 0; i < count; ++i) {
            const GLuint crtSampler = samplers[i];
            if (crtSampler != 0) {
                for (GLuint& boundSampler : GetStateTracker()->_samplerBoundMap) {
                    if (boundSampler == crtSampler) {
                        boundSampler = 0;
                    }
                }
            }
        }
        glDeleteSamplers(count, samplers);
        memset(samplers, 0, count * sizeof(GLuint));
        return true;
    }

    return false;
}


bool GL_API::DeleteBuffers(const GLuint count, GLuint* buffers) {
    if (count > 0 && buffers != nullptr) {
        for (GLuint i = 0; i < count; ++i) {
            const GLuint crtBuffer = buffers[i];
            GLStateTracker* stateTracker = GetStateTracker();
            for (GLuint& boundBuffer : stateTracker->_activeBufferID) {
                if (boundBuffer == crtBuffer) {
                    boundBuffer = GLUtil::k_invalidObjectID;
                }
            }
            for (auto& boundBuffer : stateTracker->_activeVAOIB) {
                if (boundBuffer.second == crtBuffer) {
                    boundBuffer.second = GLUtil::k_invalidObjectID;
                }
            }
        }

        glDeleteBuffers(count, buffers);
        memset(buffers, 0, count * sizeof(GLuint));
        return true;
    }

    return false;
}

bool GL_API::DeleteVAOs(const GLuint count, GLuint* vaos) {
    if (count > 0u && vaos != nullptr) {
        for (GLuint i = 0u; i < count; ++i) {
            if (GetStateTracker()->_activeVAOID == vaos[i]) {
                GetStateTracker()->_activeVAOID = GLUtil::k_invalidObjectID;
                break;
            }
        }

        glDeleteVertexArrays(count, vaos);
        memset(vaos, 0, count * sizeof(GLuint));
        return true;
    }
    return false;
}

bool GL_API::DeleteFramebuffers(const GLuint count, GLuint* framebuffers) {
    if (count > 0 && framebuffers != nullptr) {
        for (GLuint i = 0; i < count; ++i) {
            const GLuint crtFB = framebuffers[i];
            for (GLuint& activeFB : GetStateTracker()->_activeFBID) {
                if (activeFB == crtFB) {
                    activeFB = GLUtil::k_invalidObjectID;
                }
            }
        }
        glDeleteFramebuffers(count, framebuffers);
        memset(framebuffers, 0, count * sizeof(GLuint));
        return true;
    }
    return false;
}

IMPrimitive* GL_API::NewIMP(Mutex& lock, GFXDevice& parent) {
    ScopedLock<Mutex> w_lock(lock);
    return s_IMPrimitivePool.newElement(parent);
}

bool GL_API::DestroyIMP(Mutex& lock, IMPrimitive*& primitive) {
    if (primitive != nullptr) {
        ScopedLock<Mutex> w_lock(lock);
        s_IMPrimitivePool.deleteElement(static_cast<glIMPrimitive*>(primitive));
        primitive = nullptr;
        return true;
    }

    return false;
}

/// Return the OpenGL sampler object's handle for the given hash value
GLuint GL_API::GetSamplerHandle(const size_t samplerHash) {
    // If the hash value is 0, we assume the code is trying to unbind a sampler object
    if (samplerHash > 0) {
        {
            SharedLock<SharedMutex> r_lock(s_samplerMapLock);
            // If we fail to find the sampler object for the given hash, we print an error and return the default OpenGL handle
            const SamplerObjectMap::const_iterator it = s_samplerMap.find(samplerHash);
            if (it != std::cend(s_samplerMap)) {
                // Return the OpenGL handle for the sampler object matching the specified hash value
                return it->second;
            }
        }
        {

            ScopedLock<SharedMutex> w_lock(s_samplerMapLock);
            // Check again
            const SamplerObjectMap::const_iterator it = s_samplerMap.find(samplerHash);
            if (it == std::cend(s_samplerMap)) {
                // Cache miss. Create the sampler object now.
                // Create and store the newly created sample object. GL_API is responsible for deleting these!
                const GLuint sampler = glSamplerObject::construct(SamplerDescriptor::Get(samplerHash));
                emplace(s_samplerMap, samplerHash, sampler);
                return sampler;
            }
        }
    }

    return 0u;
}

glHardwareQueryPool* GL_API::GetHardwareQueryPool() noexcept {
    return s_hardwareQueryPool;
}

GLsync GL_API::CreateFenceSync() {
    OPTICK_EVENT("Create Sync");

    DIVIDE_ASSERT(s_fenceSyncCounter[s_LockFrameLifetime - 1u] < std::numeric_limits<U32>::max());

    ++s_fenceSyncCounter[s_LockFrameLifetime - 1u];
    return glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void GL_API::DestroyFenceSync(GLsync& sync) {
    OPTICK_EVENT("Delete Sync");

    DIVIDE_ASSERT(s_fenceSyncCounter[s_LockFrameLifetime - 1u] > 0u);

    --s_fenceSyncCounter[s_LockFrameLifetime - 1u];
    glDeleteSync(sync);
    sync = nullptr;
}
};
