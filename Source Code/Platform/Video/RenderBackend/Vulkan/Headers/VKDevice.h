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
#ifndef _VK_DEVICE_H_
#define _VK_DEVICE_H_

#include "VKCommon.h"

namespace Divide {
    class VKDevice final : NonCopyable, NonMovable {
    public:
        struct Queue {
            VkQueue _queue{};
            U32 _queueFamily{ 0u };
        };
    public:
        VKDevice(vkb::Instance& instance, VkSurfaceKHR targetSurface);
        ~VKDevice();

        void waitIdle() const;

        [[nodiscard]] Queue getQueue(vkb::QueueType type) const noexcept;

        [[nodiscard]] VkDevice getVKDevice() const noexcept;
        [[nodiscard]] VkPhysicalDevice getVKPhysicalDevice() const noexcept;

        [[nodiscard]] const vkb::Device& getDevice() const noexcept;
        [[nodiscard]] const vkb::PhysicalDevice& getPhysicalDevice() const noexcept;
    private:
        vkb::Device _device{}; // Vulkan device for commands
        vkb::PhysicalDevice _physicalDevice{}; // GPU chosen as the default device
    };
}; //namespace Divide
#endif //_VK_DEVICE_H_
