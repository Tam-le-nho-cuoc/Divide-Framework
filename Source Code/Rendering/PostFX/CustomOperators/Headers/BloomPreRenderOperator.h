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
#ifndef _BLOOM_PRE_RENDER_OPERATOR_H_
#define _BLOOM_PRE_RENDER_OPERATOR_H_

#include "Rendering/PostFX/Headers/PreRenderOperator.h"

namespace Divide {

class RenderTarget;
class ShaderProgram;

class BloomPreRenderOperator final : public PreRenderOperator {
   public:
    BloomPreRenderOperator(GFXDevice& context, PreRenderBatch& parent, ResourceCache* cache);
    ~BloomPreRenderOperator();

    [[nodiscard]] bool execute(PlayerIndex idx, const CameraSnapshot& cameraSnapshot, const RenderTargetHandle& input, const RenderTargetHandle& output, GFX::CommandBuffer& bufferInOut) override;
    void reshape(U16 width, U16 height) override;

    [[nodiscard]] F32 luminanceThreshold() const noexcept { return _bloomThreshold; }
    void luminanceThreshold(F32 val);

    [[nodiscard]] bool ready() const noexcept override;

   private:
    RenderTargetHandle _bloomOutput;
    RenderTargetHandle _bloomBlurBuffer[2];

    ShaderProgram_ptr _bloomCalc = nullptr;
    ShaderProgram_ptr _bloomApply = nullptr;

    Pipeline* _bloomCalcPipeline = nullptr;
    Pipeline* _bloomApplyPipeline = nullptr;
    PushConstants _bloomApplyConstants;
    PushConstants _bloomCalcConstants;

    F32 _bloomThreshold = 0.99f;
};

}  // namespace Divide

#endif