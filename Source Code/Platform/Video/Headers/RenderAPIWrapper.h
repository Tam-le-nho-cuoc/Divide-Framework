/*
   Copyright (c) 2018 DIVIDE-Studio
   Copyright (c) 2009 Ionut Cava

   This file is part of DIVIDE Framework.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software
   and associated documentation files (the "Software"), to deal in the Software
   without restriction,
   including without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
   PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
   DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
   IN CONNECTION WITH THE SOFTWARE
   OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#pragma once
#ifndef _RENDER_API_H_
#define _RENDER_API_H_

#include "CommandBuffer.h"
#include "Platform/Video/Buffers/RenderTarget/Headers/RenderTarget.h"

namespace CEGUI {
    class Texture;
};

namespace Divide {

enum class ErrorCode : I8;

template <typename T>
class vec4;

class DisplayWindow;

struct Configuration;
struct TextElementBatch;

FWD_DECLARE_MANAGED_CLASS(ShaderProgram);

struct VideoModes {
    // Video resolution
    I32 Width, Height;
    // Red bits per pixel
    I32 RedBits;
    // Green bits per pixel
    I32 GreenBits;
    // Blue bits per pixel
    I32 BlueBits;
};

/// Queries are expensive, so this result MAY BE SEVERAL frames out of date!
struct PerformanceMetrics
{
    F32 _gpuTimeInMS = 0.f;
    /// Returns the time in milliseconds that it took to render one frame
    U64 _verticesSubmitted = 0u;
    /// Returns the total number of vertices submitted between frame start and end (before swap buffers)
    /// Includes all vertices, including GUI and debug stuff (but the delta should still be useful)
    U64 _primitivesGenerated = 0u;
    ///  Number of patches processed by the tessellation control shader
    U64 _tessellationPatches = 0u;
    /// Number of times the tessellation control shader has been invoked
    U64 _tessellationInvocations = 0u;
    U32 _generatedRenderTargetCount = 0u;
    /// Number of active sync objects
    U32 _syncObjectsInFlight[3] = {};
    /// Scratch buffer queue usage
    U32 _scratchBufferQueueUsage[2] = {};
};

struct DeviceInformation
{
    struct VersionInformation {
        U8 _major = 0u;
        U8 _minor = 0u;
        U8 _patch = 0u;
        U8 _variant = 0u;
    };
    U32 _maxWorgroupCount[3] = {65535u, 65535u, 65535u};
    U16 _maxWorgroupSize[3] = {1024u, 1024u, 64u};
    size_t _UBOMaxSizeBytes = 64 * 1024;
    size_t _SSBOMaxSizeBytes = 1024 * 1024 * 1024u;
    size_t _maxComputeSharedMemoryBytes = 1024 * 1024 * 1024;
    U32 _maxWorgroupInvocations = 1024u;
    U32 _maxVertAttributeBindings = 16u;
    U32 _maxVertAttributes = 16u;
    U32 _maxVertOutputComponents = 16u;
    U32 _maxAtomicBufferBindingIndices = 4u;
    U32 _maxSSBOBufferBindings = 32u;
    U32 _shaderCompilerThreads = 1u;
    VersionInformation _versionInfo = { 4u, 6u };
    U16 _UBOffsetAlignmentBytes = 256u;
    U16 _SSBOffsetAlignmentBytes = 16u;
    U8 _maxTextureUnits = 32u;
    U8 _maxRTColourAttachments = 4u;
    U8 _maxAnisotropy = 0u;
    U8 _maxClipDistances = 2u;
    U8 _maxCullDistances = 6u;
    U8 _maxClipAndCullDistances = 8u;
    GPUVendor _vendor = GPUVendor::COUNT;
    GPURenderer _renderer = GPURenderer::COUNT;
    bool _bindlessTexturesSupported = false;
};

using AttributeFlags = std::array<bool, to_base(AttribLocation::COUNT)>;
using AttributeOffsets = std::array<size_t, to_base(AttribLocation::COUNT)>;

/// Renderer Programming Interface
class NOINITVTABLE RenderAPIWrapper : NonCopyable {
public:
    virtual ~RenderAPIWrapper() = default;

protected:
    friend class GFXDevice;

    /// Clear buffers, set default states, etc
    virtual [[nodiscard]] bool beginFrame(DisplayWindow& window, bool global = false) = 0;
    /// Clear shaders, restore active texture units, etc
    virtual void endFrame(DisplayWindow& window, bool global = false) = 0;
    virtual void idle(bool fast) = 0;

    virtual ErrorCode initRenderingAPI(I32 argc, char** argv, Configuration& config) = 0;
    virtual void closeRenderingAPI() = 0;

    [[nodiscard]] virtual const PerformanceMetrics& getPerformanceMetrics() const noexcept = 0;

    virtual void preFlushCommandBuffer(const GFX::CommandBuffer& commandBuffer) = 0;
    virtual void flushCommand(GFX::CommandBase* cmd) = 0;
    virtual void postFlushCommandBuffer(const GFX::CommandBuffer& commandBuffer) = 0;

    [[nodiscard]] virtual vec2<U16> getDrawableSize(const DisplayWindow& window) const = 0;

    // The definition of a hack. Feel free to quote this. -Ionut
    /// Convert a CEGUI texture handle to something that our current rendering API can use
    [[nodiscard]] virtual U32 getHandleFromCEGUITexture(const CEGUI::Texture& textureIn) const = 0;

    virtual bool setViewport(const Rect<I32>& newViewport) = 0;

    virtual void onThreadCreated(const std::thread::id& threadID) = 0;
};

};  // namespace Divide

#endif
