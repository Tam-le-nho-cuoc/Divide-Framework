/*
Copyright (c) 2018 DIVIDE-Studio
Copyright (c) 2009 Ionut Cava

This file is part of DIVIDE Framework.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software
and associated documentation files (the "Software"), to deal in the Software
without restriction,
including without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software
is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#pragma once
#ifndef _SHADER_BUFFER_H_
#define _SHADER_BUFFER_H_

#include "Core/Headers/RingBuffer.h"
#include "Platform/Video/Buffers/VertexBuffer/Headers/VertexDataInterface.h"
#include "Platform/Video/Headers/GraphicsResource.h"
#include "Platform/Video/Headers/RenderAPIWrapper.h"

namespace Divide {

struct ShaderBufferDescriptor;

namespace Attorney {
    class ShaderBufferBind;
};

class ShaderProgram;
class NOINITVTABLE ShaderBuffer : public LockableDataRangeBuffer,
                                  public GraphicsResource,
                                  public RingBufferSeparateWrite
                                  
{
    friend class Attorney::ShaderBufferBind;

   public:
       enum class Usage : U8 {
           CONSTANT_BUFFER = 0,
           UNBOUND_BUFFER,
           COMMAND_BUFFER,
           ATOMIC_COUNTER,
           COUNT
       };

   public:
    explicit ShaderBuffer(GFXDevice& context, const ShaderBufferDescriptor& descriptor);

    virtual ~ShaderBuffer() = default;

    virtual BufferLock clearData(BufferRange range);

    virtual BufferLock writeData(BufferRange range, bufferPtr data);

    virtual void readData(BufferRange range, bufferPtr result) const;

    virtual BufferLock clearBytes(BufferRange range) = 0;

    virtual BufferLock writeBytes(BufferRange range, bufferPtr data) = 0;

    virtual void readBytes(BufferRange range, bufferPtr result) const = 0;

    [[nodiscard]] FORCE_INLINE U32    getPrimitiveCount() const noexcept { return _params._elementCount; }
    [[nodiscard]] FORCE_INLINE size_t getPrimitiveSize()  const noexcept { return _params._elementSize; }
    [[nodiscard]] FORCE_INLINE Usage  getUsage()          const noexcept { return _usage;  }

    FORCE_INLINE BufferLock writeData(bufferPtr data) { return writeData({ 0u, _params._elementCount }, data); }
    FORCE_INLINE BufferLock clearData() { return clearData({ 0u, _params._elementCount }); }
    FORCE_INLINE bool bind(ShaderBufferLocation bindIndex) { return bind(to_U8(bindIndex)); }
    FORCE_INLINE bool bindRange(ShaderBufferLocation bindIndex, BufferRange range) { return bindRange(to_U8(bindIndex), range); }

    [[nodiscard]] static size_t AlignmentRequirement(Usage usage) noexcept;

    PROPERTY_R(string, name);

  protected:
    virtual bool bindByteRange(U8 bindIndex, BufferRange range) = 0;

    bool bindRange(U8 bindIndex, BufferRange range);

    /// Bind return false if the buffer was already bound
    FORCE_INLINE bool bind(U8 bindIndex) { return bindRange(bindIndex, { 0u, _params._elementCount }); }

   protected:
    BufferParams _params;
    size_t _maxSize = 0u;
    const Usage _usage;
};

/// If initialData is NULL, the buffer contents are undefined (good for CPU -> GPU transfers),
/// however for GPU->GPU buffers, we may want a sane initial state to work with.
/// If _initialData is not NULL, we zero out whatever empty space is left available
/// determined by comparing the data size to the buffer size
struct ShaderBufferDescriptor {
    BufferParams _bufferParams;
    string _name = "";
    U32 _ringBufferLength = 1u;
    ShaderBuffer::Usage _usage = ShaderBuffer::Usage::COUNT;
    bool _separateReadWrite = false; ///< Use a separate read/write index based on queue length
};

FWD_DECLARE_MANAGED_CLASS(ShaderBuffer);

namespace Attorney {
    class ShaderBufferBind {
        static bool bindByteRange(ShaderBuffer& buf, U8 bindIndex, BufferRange range) {
            return buf.bindByteRange(bindIndex, range);
        }

        static bool bindRange(ShaderBuffer& buf, U8 bindIndex, const BufferRange range) {
            return buf.bindRange(bindIndex, range);
        }

        /// Bind return false if the buffer was already bound
        static bool bind(ShaderBuffer& buf, U8 bindIndex) {
            return buf.bind(bindIndex);
        }

        friend class GFXDevice;
        friend class UniformBlockUploader;
    };
};

};  // namespace Divide
#endif //_SHADER_BUFFER_H_
