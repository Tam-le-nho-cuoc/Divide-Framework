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
#ifndef _SCENE_VIEW_WINDOW_H_
#define _SCENE_VIEW_WINDOW_H_

#include "Editor/Widgets/Headers/DockedWindow.h"

struct ImGuiWindow;

namespace Divide {

    class SceneViewWindow final : public DockedWindow {
    public:
        SceneViewWindow(Editor& parent, const Descriptor& descriptor);

        void drawInternal() override;

        [[nodiscard]] const Rect<I32>& sceneRect() const noexcept;
        [[nodiscard]] const vec2<I32>& getWindowOffset() const noexcept;

    protected:
        Rect<I32> _sceneRect;
        vec2<I32> _windowOffset;
        string    _originalName;
    };
} //namespace Divide

#endif //_SCENE_VIEW_WINDOW_H_