#include "stdafx.h"

#include "Headers/RenderQueue.h"

#include "Core/Headers/Kernel.h"
#include "Core/Headers/PlatformContext.h"
#include "ECS/Components/Headers/RenderingComponent.h"
#include "Geometry/Material/Headers/Material.h"
#include "Geometry/Shapes/Headers/Object3D.h"
#include "Graphs/Headers/SceneGraphNode.h"
#include "Managers/Headers/RenderPassManager.h"
#include "Utility/Headers/Localization.h"

namespace Divide {

RenderQueue::RenderQueue(Kernel& parent, const RenderStage stage)
    : KernelComponent(parent),
      _stage(stage),
      _renderBins{nullptr}
{
    for (U8 i = 0u; i < to_U8(RenderBinType::COUNT); ++i) {
        const RenderBinType rbType = static_cast<RenderBinType>(i);
        if (rbType == RenderBinType::COUNT) {
            continue;
        }

        _renderBins[i] = eastl::make_unique<RenderBin>(rbType, stage);
    }
}

U16 RenderQueue::getRenderQueueStackSize() const noexcept {
    U16 temp = 0u;
    for (const auto& bin : _renderBins) {
        if (bin != nullptr) {
            temp += bin->getBinSize();
        }
    }
    return temp;
}

RenderingOrder RenderQueue::getSortOrder(const RenderStagePass stagePass, const RenderBinType rbType) const {
    RenderingOrder sortOrder = RenderingOrder::COUNT;
    switch (rbType) {
        case RenderBinType::OPAQUE: {
            // Opaque items should be rendered front to back in depth passes for early-Z reasons
            sortOrder = IsDepthPass(stagePass) ? RenderingOrder::FRONT_TO_BACK
                                               : RenderingOrder::BY_STATE;
        } break;
        case RenderBinType::SKY: {
            sortOrder = RenderingOrder::NONE;
        } break;
        case RenderBinType::IMPOSTOR:
        case RenderBinType::WATER:
        case RenderBinType::TERRAIN: 
        case RenderBinType::TERRAIN_AUX: {
            sortOrder = RenderingOrder::FRONT_TO_BACK;
        } break;
        case RenderBinType::TRANSLUCENT: {
            // We are using weighted blended OIT. State is fine (and faster)
            // Use an override one level up from this if we need a regular forward-style pass
            sortOrder = RenderingOrder::BY_STATE;
        } break;
        default:
        case RenderBinType::COUNT: {
            Console::errorfn(Locale::Get(_ID("ERROR_INVALID_RENDER_BIN_CREATION")));
        } break;
    };
    
    return sortOrder;
}

RenderBin* RenderQueue::getBinForNode(const SceneGraphNode* node, const Material_ptr& matInstance) {
    switch (node->getNode().type()) {
        case SceneNodeType::TYPE_TRANSFORM:
        {
            constexpr U32 compareMask = to_U32(ComponentType::SPOT_LIGHT) |
                                        to_U32(ComponentType::POINT_LIGHT) |
                                        to_U32(ComponentType::DIRECTIONAL_LIGHT) |
                                        to_U32(ComponentType::ENVIRONMENT_PROBE);
            if (AnyCompare(node->componentMask(), compareMask)) {
                return getBin(RenderBinType::IMPOSTOR);
            }
            return nullptr;
        }

        case SceneNodeType::TYPE_VEGETATION:
        case SceneNodeType::TYPE_PARTICLE_EMITTER:
            return getBin(RenderBinType::TRANSLUCENT);

        case SceneNodeType::TYPE_SKY:
            return getBin(RenderBinType::SKY);

        case SceneNodeType::TYPE_WATER:
            return getBin(RenderBinType::WATER);

        case SceneNodeType::TYPE_INFINITEPLANE:
            return getBin(RenderBinType::TERRAIN_AUX);

        // Water is also opaque as refraction and reflection are separate textures
        // We may want to break this stuff up into mesh rendering components and not care about specifics anymore (i.e. just material checks)
        //case SceneNodeType::TYPE_WATER:
        case SceneNodeType::TYPE_OBJECT3D: {
            if (node->getNode().type() == SceneNodeType::TYPE_OBJECT3D) {
                switch (node->getNode<Object3D>().geometryType()) {
                    case ObjectType::TERRAIN:
                        return getBin(RenderBinType::TERRAIN);

                    case ObjectType::DECAL:
                        return getBin(RenderBinType::TRANSLUCENT);
                    default: break;
                }
            }
            // Check if the object has a material with transparency/translucency
            if (matInstance != nullptr && matInstance->hasTransparency()) {
                // Add it to the appropriate bin if so ...
                return getBin(RenderBinType::TRANSLUCENT);
            }

            //... else add it to the general geometry bin
            return getBin(RenderBinType::OPAQUE);
        }
        default:
        case SceneNodeType::COUNT:
        case SceneNodeType::TYPE_TRIGGER: break;
    }
    return nullptr;
}

void RenderQueue::addNodeToQueue(const SceneGraphNode* sgn,
                                 const RenderStagePass stagePass,
                                 const F32 minDistToCameraSq,
                                 const RenderBinType targetBinType)
{
    const RenderingComponent* const renderingCmp = sgn->get<RenderingComponent>();
    // We need a rendering component to render the node
    assert(renderingCmp != nullptr);
    RenderBin* rb = getBinForNode(sgn, renderingCmp->getMaterialInstance());
    assert(rb != nullptr);

    if (targetBinType == RenderBinType::COUNT || rb->getType() == targetBinType) {
        rb->addNodeToBin(sgn, stagePass, minDistToCameraSq);
    }
}

void RenderQueue::populateRenderQueues(const PopulateQueueParams& params, RenderQueuePackages& queueInOut) {
    OPTICK_EVENT();

    if (params._binType == RenderBinType::COUNT) {
        if (!params._filterByBinType) {
            for (const auto& renderBin : _renderBins) {
                renderBin->populateRenderQueue(params._stagePass, queueInOut);
            }
        } else {
            // Why are we allowed to exclude everything? idk.
            NOP();
        }
    } else {
        if (!params._filterByBinType) {
            for (const auto& renderBin : _renderBins) {
                if (renderBin->getType() == params._binType) {
                    renderBin->populateRenderQueue(params._stagePass, queueInOut);
                }
            }
        } else {
            for (const auto& renderBin : _renderBins) {
                if (renderBin->getType() != params._binType) {
                    renderBin->populateRenderQueue(params._stagePass, queueInOut);
                }
            }
        }
    }
}

void RenderQueue::postRender(const SceneRenderState& renderState, const RenderStagePass stagePass, GFX::CommandBuffer& bufferInOut) {
    for (const auto& renderBin : _renderBins) {
        renderBin->postRender(renderState, stagePass, bufferInOut);
    }
}

void RenderQueue::sort(const RenderStagePass stagePass, const RenderBinType targetBinType, const RenderingOrder renderOrder) {
    OPTICK_EVENT();

    // How many elements should a render bin contain before we decide that sorting should happen on a separate thread
    constexpr U16 k_threadBias = 64u;
    
    if (targetBinType != RenderBinType::COUNT)
    {
        const RenderingOrder sortOrder = renderOrder == RenderingOrder::COUNT ? getSortOrder(stagePass, targetBinType) : renderOrder;
        _renderBins[to_base(targetBinType)]->sort(sortOrder);
    }
    else
    {
        TaskPool& pool = parent().platformContext().taskPool(TaskPoolType::HIGH_PRIORITY);
        Task* sortTask = CreateTask(TASK_NOP);
        for (const auto& renderBin : _renderBins) {
            if (renderBin->getBinSize() > k_threadBias) {
                const RenderingOrder sortOrder = renderOrder == RenderingOrder::COUNT ? getSortOrder(stagePass, renderBin->getType()) : renderOrder;
                Start(*CreateTask(sortTask,
                                    [&renderBin, sortOrder](const Task&) {
                                        renderBin->sort(sortOrder);
                                    }),
                      pool);
            }
        }

        Start(*sortTask, pool);

        for (const auto& renderBin : _renderBins) {
            if (renderBin->getBinSize() <= k_threadBias) {
                const RenderingOrder sortOrder = renderOrder == RenderingOrder::COUNT ? getSortOrder(stagePass, renderBin->getType()) : renderOrder;
                renderBin->sort(sortOrder);
            }
        }

        Wait(*sortTask, pool);
    }
}

void RenderQueue::refresh(const RenderBinType targetBinType) noexcept {
    if (targetBinType == RenderBinType::COUNT) {
        for (const auto& renderBin : _renderBins) {
            renderBin->refresh();
        }
    } else {
        for (const auto& renderBin : _renderBins) {
            if (renderBin->getType() == targetBinType) {
                renderBin->refresh();
            }
        }
    }
}

U16 RenderQueue::getSortedQueues(const vector<RenderBinType>& binTypes, RenderBin::SortedQueues& queuesOut) const {
    OPTICK_EVENT();

    U16 countOut = 0u;

    if (binTypes.empty()) {
        for (const auto& renderBin : _renderBins) {
            countOut += renderBin->getSortedNodes(queuesOut[to_base(renderBin->getType())]);
        }
    } else {
        for (const RenderBinType type : binTypes) {
            countOut += getBin(type)->getSortedNodes(queuesOut[to_base(type)]);
        }
    }
    return countOut;
}

};