#include "stdafx.h"

#include "Headers/glBufferImpl.h"
#include "Headers/glMemoryManager.h"

#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/Video/RenderBackend/OpenGL/Headers/GLWrapper.h"
#include "Platform/Video/RenderBackend/OpenGL/Headers/glLockManager.h"

#include "Utility/Headers/Localization.h"

namespace Divide {

glBufferImpl::glBufferImpl(GFXDevice& context, const BufferImplParams& params)
    : glObject(glObjectType::TYPE_BUFFER, context),
      GUIDWrapper(),
      _params(params),
      _context(context)
{
    assert(_params._bufferParams._updateFrequency != BufferUpdateFrequency::COUNT);

    // Create all buffers with zero mem and then write the actual data that we have (If we want to initialise all memory)
    if (_params._bufferParams._updateUsage == BufferUpdateUsage::GPU_R_GPU_W || _params._bufferParams._updateFrequency == BufferUpdateFrequency::ONCE) {
        GLenum usage = GL_NONE;
        if (_params._target == GL_ATOMIC_COUNTER_BUFFER) {
            usage = GL_STREAM_COPY;
        } else {
            switch (_params._bufferParams._updateFrequency) {
                case BufferUpdateFrequency::ONCE:
                    switch (_params._bufferParams._updateUsage) {
                        case BufferUpdateUsage::CPU_W_GPU_R: usage = GL_STATIC_DRAW; break;
                        case BufferUpdateUsage::CPU_R_GPU_W: usage = GL_STATIC_READ; break;
                        case BufferUpdateUsage::GPU_R_GPU_W: usage = GL_STATIC_COPY; break;
                        default: break;
                    }
                    break;
                case BufferUpdateFrequency::OCASSIONAL:
                    switch (_params._bufferParams._updateUsage) {
                        case BufferUpdateUsage::CPU_W_GPU_R: usage = GL_DYNAMIC_DRAW; break;
                        case BufferUpdateUsage::CPU_R_GPU_W: usage = GL_DYNAMIC_READ; break;
                        case BufferUpdateUsage::GPU_R_GPU_W: usage = GL_DYNAMIC_COPY; break;
                        default: break;
                    }
                    break;
                case BufferUpdateFrequency::OFTEN:
                    switch (_params._bufferParams._updateUsage) {
                        case BufferUpdateUsage::CPU_W_GPU_R: usage = GL_STREAM_DRAW; break;
                        case BufferUpdateUsage::CPU_R_GPU_W: usage = GL_STREAM_READ; break;
                        case BufferUpdateUsage::GPU_R_GPU_W: usage = GL_STREAM_COPY; break;
                        default: break;
                    }
                    break;
                default: break;
            }
        }

        DIVIDE_ASSERT(usage != GL_NONE);

        GLUtil::createAndAllocBuffer(_params._dataSize, 
                                     usage,
                                     _memoryBlock._bufferHandle,
                                     _params._bufferParams._initialData,
                                     _params._name);

        _memoryBlock._offset = 0u;
        _memoryBlock._size = _params._dataSize;
        _memoryBlock._free = false;
    } else {
        const BufferStorageMask storageMask = GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT;
        const MapBufferAccessMask accessMask = GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT;

        const size_t alignment = params._target == GL_UNIFORM_BUFFER 
                                                ? GFXDevice::GetDeviceInformation()._UBOffsetAlignmentBytes
                                                : _params._target == GL_SHADER_STORAGE_BUFFER
                                                                  ? GFXDevice::GetDeviceInformation()._SSBOffsetAlignmentBytes
                                                                  : sizeof(U32);

        GLUtil::GLMemory::DeviceAllocator& allocator = GL_API::GetMemoryAllocator(GL_API::GetMemoryTypeForUsage(_params._target));
        _memoryBlock = allocator.allocate(_params._useChunkAllocation,
                                          _params._dataSize,
                                          alignment,
                                          storageMask,
                                          accessMask,
                                          params._target,
                                          _params._name,
                                          _params._bufferParams._initialData);
        assert(_memoryBlock._ptr != nullptr && _memoryBlock._size >= _params._dataSize && "PersistentBuffer::Create error: Can't mapped persistent buffer!");
    }
}

glBufferImpl::~glBufferImpl()
{
    if (_memoryBlock._bufferHandle != GLUtil::k_invalidObjectID) {
        if (!waitByteRange(0u, _memoryBlock._size, true)) {
            DIVIDE_UNEXPECTED_CALL();
        }

        if (_memoryBlock._ptr != nullptr) {
            const GLUtil::GLMemory::DeviceAllocator& allocator = GL_API::GetMemoryAllocator(GL_API::GetMemoryTypeForUsage(_params._target));
            allocator.deallocate(_memoryBlock);
        } else {
            GLUtil::freeBuffer(_memoryBlock._bufferHandle);
            GLUtil::freeBuffer(_copyBufferTarget);
        }
    }
}

bool glBufferImpl::lockByteRange(const size_t offsetInBytes, const size_t rangeInBytes, SyncObject* sync) {
    if (_memoryBlock._ptr != nullptr) {
        return _lockManager.lockRange(_memoryBlock._offset + offsetInBytes, rangeInBytes, sync);
    }

    return true;
}

bool glBufferImpl::waitByteRange(const size_t offsetInBytes, const size_t rangeInBytes, const bool blockClient) {
    if (_memoryBlock._ptr != nullptr) {
        return _lockManager.waitForLockedRange(_memoryBlock._offset + offsetInBytes, rangeInBytes, blockClient);
    }

    return true;
}

[[nodiscard]] inline bool Overlaps(const BufferMapRange& lhs, const BufferMapRange& rhs) noexcept {
    return lhs._offset < (rhs._offset + rhs._range) && rhs._offset < (lhs._offset + lhs._range);
}

void glBufferImpl::writeOrClearBytes(const size_t offsetInBytes, const size_t rangeInBytes, const bufferPtr data, const bool zeroMem) {

    assert(rangeInBytes > 0u && offsetInBytes + rangeInBytes <= _memoryBlock._size);

    OPTICK_EVENT();
    OPTICK_TAG("Mapped", static_cast<bool>(_memoryBlock._ptr != nullptr));
    OPTICK_TAG("Offset", to_U32(offsetInBytes));
    OPTICK_TAG("Range", to_U32(rangeInBytes));

    if (!waitByteRange(offsetInBytes, rangeInBytes, true)) {
        Console::errorfn(Locale::Get(_ID("ERROR_BUFFER_LOCK_MANAGER_WAIT")));
    }

    if (_memoryBlock._ptr != nullptr) {
        if (zeroMem) {
            memset(&_memoryBlock._ptr[offsetInBytes], 0, rangeInBytes);
        } else {
            memcpy(&_memoryBlock._ptr[offsetInBytes], data, rangeInBytes);
        }
    } else {
        DIVIDE_ASSERT(zeroMem, "glBufferImpl: trying to write to a buffer create with BufferUpdateFrequency::ONCE");

        Byte* ptr = (Byte*)glMapNamedBufferRange(_memoryBlock._bufferHandle, _memoryBlock._offset + offsetInBytes, rangeInBytes, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        memset(ptr, 0, rangeInBytes);
        glUnmapNamedBuffer(_memoryBlock._bufferHandle);
    }
}

void glBufferImpl::readBytes(const size_t offsetInBytes, const size_t rangeInBytes, bufferPtr data) {

    if (!waitByteRange(offsetInBytes, rangeInBytes, true)) {
        DIVIDE_UNEXPECTED_CALL();
    }

    if (_memoryBlock._ptr != nullptr) {
        if (_params._target != GL_ATOMIC_COUNTER_BUFFER) {
            glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
        }
        memcpy(data, _memoryBlock._ptr + offsetInBytes, rangeInBytes);
    } else {
        if (_copyBufferTarget == GLUtil::k_invalidObjectID || _copyBufferSize < rangeInBytes) {
            GLUtil::freeBuffer(_copyBufferTarget);
            _copyBufferSize = rangeInBytes;
            const string copyBufferName = _params._name != nullptr ? string{ _params._name } + "_copy" 
                                                                   : Util::StringFormat("COPY_BUFFER_%d", _memoryBlock._bufferHandle);
            GLUtil::createAndAllocBuffer(_copyBufferSize,
                                         GL_STREAM_READ,
                                         _copyBufferTarget,
                                         {nullptr, 0u},
                                         copyBufferName.c_str());
        }
        glCopyNamedBufferSubData(_memoryBlock._bufferHandle, _copyBufferTarget, _memoryBlock._offset + offsetInBytes, 0u, rangeInBytes);

        const Byte* bufferData = (Byte*)glMapNamedBufferRange(_copyBufferTarget, 0u, rangeInBytes, MapBufferAccessMask::GL_MAP_READ_BIT);
        assert(bufferData != nullptr);
        memcpy(data, bufferData, rangeInBytes);
        glUnmapNamedBuffer(_copyBufferTarget);
    }
}

}; //namespace Divide