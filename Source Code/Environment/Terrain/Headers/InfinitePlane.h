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
#ifndef _INFINITE_PLANE_H_
#define _INFINITE_PLANE_H_

#include "Graphs/Headers/SceneNode.h"

namespace Divide {

FWD_DECLARE_MANAGED_CLASS(Quad3D);
class InfinitePlane final : public SceneNode {
public:
    explicit InfinitePlane(GFXDevice& context, ResourceCache* parentCache, size_t descriptorHash, const Str256& name, vec2<U32> dimensions);

protected:
    void postLoad(SceneGraphNode* sgn) override;

    void buildDrawCommands(SceneGraphNode* sgn, vector_fast<GFX::DrawCommand>& cmdsOut) override;
    void sceneUpdate(U64 deltaTimeUS, SceneGraphNode* sgn, SceneState& sceneState) override;

protected:
    template <typename T>
    friend class ImplResourceLoader;

    bool load() override;

    [[nodiscard]] const char* getResourceTypeName() const noexcept override { return "InfinitePlane"; }

private:
    GFXDevice& _context;
    vec2<U32> _dimensions;
    Quad3D_ptr _plane = nullptr;
    size_t _planeRenderStateHash = 0u;
    size_t _planeRenderStateHashPrePass = 0u;
}; //InfinitePlane
} //namespace Divide
#endif
