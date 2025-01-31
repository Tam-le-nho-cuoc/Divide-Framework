#include "stdafx.h"

#include "Headers/Gizmo.h"
#include "Editor/Headers/Editor.h"
#include "Managers/Headers/SceneManager.h"
#include "ECS/Components/Headers/BoundsComponent.h"

#include <imgui_internal.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4505) //Unreferenced local function has been removed
#endif
#include <ImGuizmo/ImGuizmo.cpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif


namespace ImGuizmo {
    struct GizmoBounds
    {
        float  mRadiusSquareCenter = 0.0f;
        ImVec2 mScreenSquareCenter = { 0.0f, 0.0f };
        ImVec2 mScreenSquareMin = { 0.0f, 0.0f };
        ImVec2 mScreenSquareMax = { 0.0f, 0.0f };
    } gBounds = {};
    
    const GizmoBounds& GetBounds() noexcept {
        gBounds.mRadiusSquareCenter = gContext.mRadiusSquareCenter;
        gBounds.mScreenSquareCenter = gContext.mScreenSquareCenter;
        gBounds.mScreenSquareMin = gContext.mScreenSquareMin;
        gBounds.mScreenSquareMax = gContext.mScreenSquareMax;
        return gBounds;
    }
};

namespace Divide {
    namespace {
        constexpr U8 g_maxSelectedNodes = 12;
        using TransformCache = std::array<TransformValues, g_maxSelectedNodes + 1>;
        using NodeCache = std::array<SceneGraphNode*, g_maxSelectedNodes + 1>;

        TransformCache g_transformCache;
        NodeCache g_selectedNodesCache;
        UndoEntry<std::pair<TransformCache, NodeCache>> g_undoEntry;
    }

    Gizmo::Gizmo(Editor& parent, ImGuiContext* targetContext)
        : _parent(parent),
          _imguiContext(targetContext)
    {
        g_undoEntry._name = "Gizmo Manipulation";
        _selectedNodes.reserve(g_maxSelectedNodes);
    }

    Gizmo::~Gizmo()
    {
        _imguiContext= nullptr;
    }

    ImGuiContext& Gizmo::getContext() noexcept {
        assert(_imguiContext != nullptr);
        return *_imguiContext;
    }

    const ImGuiContext& Gizmo::getContext() const noexcept {
        assert(_imguiContext != nullptr);
        return *_imguiContext;
    }

    void Gizmo::enable(const bool state) noexcept {
        _enabled = state;
    }

    bool Gizmo::enabled() const noexcept {
        return _enabled;
    }

    bool Gizmo::active() const noexcept {
        return enabled() && !_selectedNodes.empty();
    }

    void Gizmo::update([[maybe_unused]] const U64 deltaTimeUS) {

        if (_shouldRegisterUndo) {
            _parent.registerUndoEntry(g_undoEntry);
            _shouldRegisterUndo = false;
        }
    }

    void Gizmo::render(const Camera* camera, const Rect<I32>& targetViewport, GFX::CommandBuffer& bufferInOut) {
        const auto GetSnapValues = [](const TransformSettings& settings) -> const F32* {
            if (settings.useSnap) {
                if (IsTranslationOperation(settings)) {
                    return &settings.snapTranslation[0];
                } else if (IsRotationOperation(settings)) {
                    return &settings.snapRotation[0];
                } else if (IsScaleOperation(settings)) {
                    return &settings.snapScale[0];
                }
            }

            return nullptr;
        };

        if (!active() || _selectedNodes.empty()) {
            return;
        }

        ImGui::SetCurrentContext(_imguiContext);
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
        ImGuizmo::SetGizmoSizeClipSpace(0.2f);

        const DisplayWindow* mainWindow = static_cast<DisplayWindow*>(ImGui::GetMainViewport()->PlatformHandle);
        const vec2<U16> size = mainWindow->getDrawableSize();
        ImGuizmo::SetRect(0.f, 0.f, to_F32(size.width), to_F32(size.height));

        const bool dirty = Manipulate(camera->viewMatrix(),
                                      camera->projectionMatrix(),
                                      _transformSettings.currentGizmoOperation,
                                      _transformSettings.currentGizmoMode,
                                      _workMatrix,
                                      _deltaMatrix,
                                      GetSnapValues(_transformSettings));

        if (dirty && ImGuizmo::IsUsing()) {
            if (_selectedNodes.size() == 1) {
                renderSingleSelection(camera);
            } else {
                renderMultipleSelections(camera);
            }
        }


        ImGui::Render();
        Attorney::EditorGizmo::renderDrawList(_parent, ImGui::GetDrawData(), targetViewport, -1, bufferInOut);
    }

    void Gizmo::applyTransforms(const SelectedNode& node, const vec3<F32>& position, const vec3<Angle::DEGREES<F32>>& euler, const vec3<F32>& scale) {
        switch (_transformSettings.currentGizmoOperation) {
            case ImGuizmo::TRANSLATE   : node.tComp->translate(position); break;
            case ImGuizmo::TRANSLATE_X : node.tComp->translateX(position.x); break;
            case ImGuizmo::TRANSLATE_Y : node.tComp->translateY(position.y); break;
            case ImGuizmo::TRANSLATE_Z : node.tComp->translateZ(position.z); break;
            case ImGuizmo::SCALE       : node.tComp->scale(Max(scale, vec3<F32>(EPSILON_F32))); break;
            case ImGuizmo::SCALE_X     : node.tComp->scaleX(std::max(scale.x, EPSILON_F32)); break;
            case ImGuizmo::SCALE_Y     : node.tComp->scaleY(std::max(scale.y, EPSILON_F32)); break;
            case ImGuizmo::SCALE_Z     : node.tComp->scaleZ(std::max(scale.z, EPSILON_F32)); break;
            case ImGuizmo::ROTATE      : node.tComp->rotate(-euler); break;
            case ImGuizmo::ROTATE_X    : node.tComp->rotateX(-euler.x); break;
            case ImGuizmo::ROTATE_Y    : node.tComp->rotateY(-euler.y); break;
            case ImGuizmo::ROTATE_Z    : node.tComp->rotateZ(-euler.z); break;
        }
    }

    void Gizmo::renderSingleSelection(const Camera* camera) {
        const SelectedNode& node = _selectedNodes.front();
        if (!_wasUsed) {
            g_transformCache.back() = node._initialValues;
            g_selectedNodesCache.back() = node.tComp->parentSGN();
            g_undoEntry._oldVal = { g_transformCache, g_selectedNodesCache };
        }

        static vec3<F32> position, euler, scale;
        ImGuizmo::DecomposeMatrixToComponents(_deltaMatrix, position._v, euler._v, scale._v);
        
        position = (_localToWorldMatrix * vec4<F32>(position, 1.f)).xyz;

        applyTransforms(node, position, euler, scale);

        g_transformCache.back() = node.tComp->getLocalValues();
        _wasUsed = true;

        g_undoEntry._newVal = { g_transformCache, g_selectedNodesCache };
        g_undoEntry._dataSetter = [](const std::pair<TransformCache, NodeCache>& data) {
            const SceneGraphNode* node = data.second.back();
            assert(node != nullptr);
            TransformComponent* tComp = node->get<TransformComponent>();
            tComp->setTransform(data.first.back());
        };
    }

    void Gizmo::renderMultipleSelections(const Camera* camera) {
        //ToDo: This is still very buggy! -Ionut

        assert(_selectedNodes.front().tComp != nullptr);

        if (!_wasUsed) {
            U32 selectionCounter = 0u;
            for (const SelectedNode& node : _selectedNodes) {
                g_transformCache[selectionCounter] = node._initialValues;
                g_selectedNodesCache[selectionCounter] = node.tComp->parentSGN();
                if (++selectionCounter == g_maxSelectedNodes) {
                    break;
                }
            }

            g_undoEntry._oldVal = { g_transformCache, g_selectedNodesCache };
        }

        static vec3<F32> position, euler, scale;
        ImGuizmo::DecomposeMatrixToComponents(_deltaMatrix, position._v, euler._v, scale._v);
        position = (_localToWorldMatrix * vec4<F32>(position, 1.f)).xyz;

        U32 selectionCounter = 0u;
        for (const SelectedNode& node : _selectedNodes) {
            applyTransforms(node, position, euler, scale);

            g_transformCache[selectionCounter] = node.tComp->getLocalValues();
            if (++selectionCounter == g_maxSelectedNodes) {
                break;
            }
        }
        _wasUsed = true;
        

        g_undoEntry._newVal = { g_transformCache, g_selectedNodesCache };
        g_undoEntry._dataSetter = [](const std::pair<TransformCache, NodeCache>& data) {
            for (U32 i = 0u; i < g_maxSelectedNodes; ++i) {
                const SceneGraphNode* node = data.second[i];
                if (node != nullptr) {
                    TransformComponent* tComp = node->get<TransformComponent>();
                    if (tComp != nullptr) {
                        tComp->setTransform(data.first[i]);
                    }
                }
            }
        };
    }

    void Gizmo::updateSelections(const vector<SceneGraphNode*>& nodes) {
         _selectedNodes.resize(0);
         _workMatrix.identity();
         _localToWorldMatrix.identity();
         _deltaMatrix.identity();

         U32 selectionCounter = 0u;
         for (const SceneGraphNode* node : nodes) {
            TransformComponent* tComp = node->get<TransformComponent>();
            if (tComp != nullptr) {
                _selectedNodes.push_back(SelectedNode{tComp, tComp->getLocalValues()});
                if (++selectionCounter == g_maxSelectedNodes) {
                    break;
                }
            }
         }

         if (selectionCounter == 1u) {
             const TransformComponent* tComp = _selectedNodes.front().tComp;
             _workMatrix = tComp->getWorldMatrix();
             SceneGraphNode* parent = tComp->parentSGN()->parent();
             if (parent != nullptr) {
                 parent->get<TransformComponent>()->getWorldMatrix().getInverse(_localToWorldMatrix);
             }
             const BoundsComponent* bComp = tComp->parentSGN()->get<BoundsComponent>();
             if (bComp != nullptr) {
                 _workMatrix.setTranslation(bComp->getBoundingSphere().getCenter());
             }
         } else {
             BoundingBox nodesBB = {};
             for (const SelectedNode& node : _selectedNodes) {
                 const TransformComponent* tComp = node.tComp;
                 const BoundsComponent* bComp = tComp->parentSGN()->get<BoundsComponent>();
                 if (bComp != nullptr) {
                     nodesBB.add(bComp->getBoundingSphere().getCenter());
                 } else {
                     nodesBB.add(tComp->getWorldPosition());
                 }
             }

             _workMatrix.setScale(VECTOR3_UNIT);
             _workMatrix.setTranslation(nodesBB.getCenter());
         }
    }

    void Gizmo::setTransformSettings(const TransformSettings& settings) noexcept {
        _transformSettings = settings;
    }

    const TransformSettings& Gizmo::getTransformSettings() const noexcept {
        return _transformSettings;
    }

    void Gizmo::onSceneFocus([[maybe_unused]] const bool state) noexcept {

        ImGuiIO & io = _imguiContext->IO;
        _wasUsed = false;
        io.KeyCtrl = io.KeyShift = io.KeyAlt = io.KeySuper = false;
    }

    bool Gizmo::onKey(const bool pressed, const Input::KeyEvent& key) {
        if (pressed) {
            _wasUsed = false;
        } else if (_wasUsed) {
            _shouldRegisterUndo = true;
            _wasUsed = false;
        }

        ImGuiIO & io = _imguiContext->IO;

        io.KeysDown[to_I32(key._key)] = pressed;

        if (key._key == Input::KeyCode::KC_LCONTROL || key._key == Input::KeyCode::KC_RCONTROL) {
            io.KeyCtrl = pressed;
        }

        if (key._key == Input::KeyCode::KC_LSHIFT || key._key == Input::KeyCode::KC_RSHIFT) {
            io.KeyShift = pressed;
        }

        if (key._key == Input::KeyCode::KC_LMENU || key._key == Input::KeyCode::KC_RMENU) {
            io.KeyAlt = pressed;
        }

        if (key._key == Input::KeyCode::KC_LWIN || key._key == Input::KeyCode::KC_RWIN) {
            io.KeySuper = pressed;
        }

        bool ret = false;
        if (active() && io.KeyCtrl) {
            TransformSettings settings = _parent.getTransformSettings();
            if (key._key == Input::KeyCode::KC_T) {
                settings.currentGizmoOperation = ImGuizmo::TRANSLATE;
                ret = true;
            } else if (key._key == Input::KeyCode::KC_R) {
                settings.currentGizmoOperation = ImGuizmo::ROTATE;
                ret = true;
            } else if (key._key == Input::KeyCode::KC_S) {
                settings.currentGizmoOperation = ImGuizmo::SCALE;
                ret = true;
            }
            if (ret && !pressed) {
                _parent.setTransformSettings(settings);
            }
        }

        return ret;
    }

    void Gizmo::onMouseButton(const bool pressed) noexcept {
        if (pressed) {
            _wasUsed = false;
        } else if (_wasUsed) {
            _shouldRegisterUndo = true;
            _wasUsed = false;
        }
    }

    bool Gizmo::needsMouse() const {
        if (active()) {
            ImGuiContext* crtContext = ImGui::GetCurrentContext();
            ImGui::SetCurrentContext(_imguiContext);
            const bool imguizmoState = ImGuizmo::IsOver();
            ImGui::SetCurrentContext(crtContext);
            return imguizmoState;
        }

        return false;
    }

    bool Gizmo::hovered() const noexcept {
        if (active()) {
            const ImGuizmo::GizmoBounds& bounds = ImGuizmo::GetBounds();
            const ImGuiIO& io = _imguiContext->IO;
            const vec2<F32> deltaScreen = {
                io.MousePos.x - bounds.mScreenSquareCenter.x,
                io.MousePos.y - bounds.mScreenSquareCenter.y
            };
            const F32 dist = deltaScreen.length();
            return dist < bounds.mRadiusSquareCenter + 5.0f;
        }

        return false;
    }
} //namespace Divide