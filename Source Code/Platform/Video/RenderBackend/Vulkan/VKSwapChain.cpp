#include "stdafx.h"

#include "Headers/VKSwapChain.h"
#include "Headers/VKDevice.h"

#include "Utility/Headers/Localization.h"

#include "Platform/Headers/DisplayWindow.h"

namespace Divide {

    VKSwapChain::VKSwapChain(const VKDevice& device, const DisplayWindow& window)
        : _device(device)
        , _window(window)
    {
    }

    VKSwapChain::~VKSwapChain()
    {
        destroy();
    }
    
    void VKSwapChain::destroy() {
        const VkDevice device = _device.getVKDevice();

        _device.waitIdle();
       
        vkb::destroy_swapchain(_swapChain);
        _swapChain.swapchain = VK_NULL_HANDLE;
        //destroy swapchain resources
        _swapChain.destroy_image_views(_swapchainImageViews);
        _swapchainImages.clear();
        _swapchainImageViews.clear();

        vkDestroyRenderPass(device, _renderPass, nullptr);
        for (size_t i = 0u; i < _framebuffers.size(); i++) {
            vkDestroyFramebuffer(device, _framebuffers[i], nullptr);
        }

        _framebuffers.clear();
        if (!_inFlightFences.empty()) {
            for (U8 i = 0u; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroySemaphore(device, _renderFinishedSemaphores[i], nullptr);
                vkDestroySemaphore(device, _imageAvailableSemaphores[i], nullptr);
                vkDestroyFence(device, _inFlightFences[i], nullptr);
            }
        }
        _imagesInFlight.clear();
    }

    ErrorCode VKSwapChain::create(const bool vSync, const bool adaptiveSync, VkSurfaceKHR targetSurface) {
        const auto& windowDimensions = _window.getDrawableSize();
        const VkExtent2D windowExtents{ windowDimensions.width, windowDimensions.height };

        const ErrorCode err = createSwapChainInternal(vSync, adaptiveSync, windowExtents, targetSurface);
        if (err != ErrorCode::NO_ERR) {
            return err;
        }

        return createFramebuffersInternal(windowExtents, targetSurface);
    }

    ErrorCode VKSwapChain::createSwapChainInternal(const bool vSync, const bool adaptiveSync, VkExtent2D windowExtents, VkSurfaceKHR targetSurface) {
        vkb::SwapchainBuilder swapchainBuilder{ _device.getDevice(), targetSurface };
        if (_swapChain.swapchain != VK_NULL_HANDLE) {
            swapchainBuilder.set_old_swapchain(_swapChain);
        }

        // adaptiveSync not supported yet
        const VkPresentModeKHR presentMode = vSync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

        auto vkbSwapchain = swapchainBuilder.use_default_format_selection()
                                            //use vsync present mode
                                            .set_desired_present_mode(presentMode)
                                            .set_desired_extent(windowExtents.width, windowExtents.height)
                                            .build();


        if (!vkbSwapchain) {
            Console::errorfn(Locale::Get(_ID("ERROR_VK_INIT")), vkbSwapchain.error().message().c_str());
            return ErrorCode::VK_OLD_HARDWARE;
        }

        destroy();
        
        _swapChain = vkbSwapchain.value();
        _swapchainImages = _swapChain.get_images().value();
        _swapchainImageViews = _swapChain.get_image_views().value();

        return ErrorCode::NO_ERR;
    }

    ErrorCode VKSwapChain::createFramebuffersInternal(VkExtent2D windowExtents, VkSurfaceKHR targetSurface) {
        const VkDevice device = _device.getVKDevice();

        if (_swapChain.image_format == VK_FORMAT_UNDEFINED) {
            return ErrorCode::VK_SURFACE_CREATE;
        }

        // the renderpass will use this color attachment.
        VkAttachmentDescription color_attachment = {};
        //the attachment will have the format needed by the swapchain
        color_attachment.format = _swapChain.image_format;
        //1 sample, we won't be doing MSAA
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // we Clear when this attachment is loaded
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // we keep the attachment stored when the renderpass ends
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        //we don't care about stencil
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        //we don't know or care about the starting layout of the attachment
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        //after the renderpass ends, the image has to be on a layout ready for display
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_ref = {};
        //attachment number will index into the pAttachments array in the parent renderpass itself
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        //we are going to create 1 subpass, which is the minimum you can do
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;

        VkRenderPassCreateInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

        //connect the color attachment to the info
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &color_attachment;
        //connect the subpass to the info
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;

        VK_CHECK(vkCreateRenderPass(device, &render_pass_info, nullptr, &_renderPass));

        //create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.pNext = nullptr;

        fb_info.renderPass = _renderPass;
        fb_info.attachmentCount = 1;
        fb_info.width = windowExtents.width;
        fb_info.height = windowExtents.height;
        fb_info.layers = 1;

        //grab how many images we have in the swapchain
        const size_t swapchain_imagecount = _swapchainImages.size();
        _framebuffers.resize(swapchain_imagecount);

        //create framebuffers for each of the swapchain image views
        for (size_t i = 0u; i < swapchain_imagecount; i++) {

            fb_info.pAttachments = &_swapchainImageViews[i];
            VK_CHECK(vkCreateFramebuffer(device, &fb_info, nullptr, &_framebuffers[i]));
        }

        _imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        _renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        _inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        _imagesInFlight.resize(swapchain_imagecount, VK_NULL_HANDLE);

        //create synchronization structures
        //for the semaphores we don't need any flags
        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        //we want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU command (for the first frame)
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (U8 i = 0u; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &_imageAvailableSemaphores[i]));
            VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &_renderFinishedSemaphores[i]));
            VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &_inFlightFences[i]));
        }

        return ErrorCode::NO_ERR;
    }

    VkResult VKSwapChain::beginFrame() {
        //wait until the GPU has finished rendering the last frame.
        VK_CHECK(vkWaitForFences(_device.getVKDevice(),
                                 1,
                                 &_inFlightFences[_currentFrameIdx],
                                 VK_TRUE,
                                 std::numeric_limits<uint64_t>::max()));

        //request image from the swapchain, one second timeout
        return vkAcquireNextImageKHR(_device.getVKDevice(),
                                     _swapChain.swapchain,
                                     std::numeric_limits<uint64_t>::max(),
                                     _imageAvailableSemaphores[_currentFrameIdx],
                                     nullptr,
                                     &_swapchainImageIndex);
    }

    VkResult VKSwapChain::endFrame(VkQueue queue, VkCommandBuffer& cmdBuffer) {
        //prepare the submission to the queue.
        //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
        //we will signal the _renderSemaphore, to signal that rendering has finished

        if (_imagesInFlight[_swapchainImageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(_device.getVKDevice(),
                            1,
                            &_imagesInFlight[_swapchainImageIndex],
                            VK_TRUE,
                            std::numeric_limits<U64>::max());
        }
        _imagesInFlight[_swapchainImageIndex] = _inFlightFences[_currentFrameIdx];

        VkSubmitInfo submit = {};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { _imageAvailableSemaphores[_currentFrameIdx] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = waitSemaphores;
        submit.pWaitDstStageMask = waitStages;

        VkSemaphore signalSemaphores[] = { _renderFinishedSemaphores[_currentFrameIdx] };
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = signalSemaphores;

        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmdBuffer;

        VK_CHECK(vkResetFences(_device.getVKDevice(), 1, &_inFlightFences[_currentFrameIdx]));
        //submit command buffer to the queue and execute it.
        // _renderFence will now block until the graphic commands finish execution
        VK_CHECK(vkQueueSubmit(queue, 1, &submit, _inFlightFences[_currentFrameIdx]));

        // this will put the image we just rendered into the visible window.
        // we want to wait on the _renderSemaphore for that,
        // as it's necessary that drawing commands have finished before the image is displayed to the user
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pSwapchains = &_swapChain.swapchain;
        presentInfo.swapchainCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pImageIndices = &_swapchainImageIndex;

        const VkResult result = vkQueuePresentKHR(queue, &presentInfo);

        _currentFrameIdx = (_currentFrameIdx + 1) % MAX_FRAMES_IN_FLIGHT;

        return result;
    }

    vkb::Swapchain VKSwapChain::getSwapChain() const noexcept {
        return _swapChain;
    }
    
    VkRenderPass VKSwapChain::getRenderPass() const noexcept {
        return _renderPass;
    }

    VkFramebuffer VKSwapChain::getCurrentFrameBuffer() const noexcept {
        return _framebuffers[_swapchainImageIndex];
    }
}; //namespace Divide
