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
#ifndef _POINT_LIGHT_SYSTEM_H_
#define _POINT_LIGHT_SYSTEM_H_

#include "ECSSystem.h"
#include "Core/Headers/PlatformContextComponent.h"
#include "ECS/Components/Headers/PointLightComponent.h"

namespace Divide {
    class PointLightSystem final : public PlatformContextComponent,
        public ECSSystem<PointLightSystem, PointLightComponent>
    {
        using Parent = ECSSystem<PointLightSystem, PointLightComponent>;
    public:
        PointLightSystem(ECS::ECSEngine& parentEngine, PlatformContext& context);
        virtual ~PointLightSystem();

        void PreUpdate(F32 dt) override;
        void Update(F32 dt) override;
        void PostUpdate(F32 dt) override;
        void OnFrameStart() override;
        void OnFrameEnd() override;

        bool saveCache(const SceneGraphNode* sgn, ByteBuffer& outputBuffer) override;
        bool loadCache(SceneGraphNode* sgn, ByteBuffer& inputBuffer) override;
    };
}

#endif //_POINT_LIGHT_SYSTEM_H_
