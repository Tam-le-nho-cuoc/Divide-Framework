#include "stdafx.h"

#include "Headers/ScenePool.h"

#include "Managers/Headers/SceneManager.h"
#include "Scenes/DefaultScene/Headers/DefaultScene.h"

#include "DefaultScene/Headers/DefaultScene.h"
#include "MainScene/Headers/MainScene.h"
#include "PingPongScene/Headers/PingPongScene.h"
#include "WarScene/Headers/WarScene.h"

namespace Divide {

namespace SceneList {
    [[nodiscard]] SceneFactoryMap& sceneFactoryMap() {
        static SceneFactoryMap sceneFactory{};
        return sceneFactory;
    }

    [[nodiscard]] SceneNameMap& sceneNameMap() {
        static SceneNameMap sceneNameMap{};
        return sceneNameMap;
    }

    void registerSceneFactory(const char* name, const ScenePtrFactory& factoryFunc) {
        sceneNameMap()[_ID(name)] = name;
        sceneFactoryMap()[_ID(name)] = factoryFunc;
    }
}

ScenePool::ScenePool(SceneManager& parentMgr)
  : _parentMgr(parentMgr)
{
    assert(!SceneList::sceneFactoryMap().empty());
}

ScenePool::~ScenePool()
{
    vector<std::shared_ptr<Scene>> tempScenes;
    {   
        SharedLock<SharedMutex> r_lock(_sceneLock);
        tempScenes.insert(eastl::cend(tempScenes),
                          eastl::cbegin(_createdScenes),
                          eastl::cend(_createdScenes));
    }

    for (const std::shared_ptr<Scene>& scene : tempScenes) {
        Attorney::SceneManagerScenePool::unloadScene(_parentMgr, scene.get());
        deleteScene(scene->getGUID());
    }

    {
        ScopedLock<SharedMutex> w_lock(_sceneLock);
        _createdScenes.clear();
    }
}

bool ScenePool::defaultSceneActive() const noexcept {
    return !_defaultScene || !_activeScene ||
            _activeScene->getGUID() == _defaultScene->getGUID();
}

Scene& ScenePool::activeScene() noexcept {
    return *_activeScene;
}

const Scene& ScenePool::activeScene() const noexcept {
    return *_activeScene;
}

void ScenePool::activeScene(Scene& scene) noexcept {
    _activeScene = &scene;
}

Scene& ScenePool::defaultScene() noexcept {
    return *_defaultScene;
}

const Scene& ScenePool::defaultScene() const noexcept {
    return *_defaultScene;
}

Scene* ScenePool::getOrCreateScene(PlatformContext& context, ResourceCache* cache, SceneManager& parent, const Str256& name, bool& foundInCache) {
    assert(!name.empty());

    foundInCache = false;
    std::shared_ptr<Scene> ret = nullptr;

    ScopedLock<SharedMutex> lock(_sceneLock);
    for (const std::shared_ptr<Scene>& scene : _createdScenes) {
        if (scene->resourceName().compare(name) == 0) {
            ret = scene;
            foundInCache = true;
            break;
        }
    }

    if (ret == nullptr) {
        const auto creationFunc = SceneList::sceneFactoryMap()[_ID(name.c_str())];
        if (creationFunc) {
            ret = creationFunc(context, cache, parent, name);
        } else {
            ret = std::make_shared<Scene>(context, cache, parent, name);
        }

        // Default scene is the first scene we load
        if (!_defaultScene) {
            _defaultScene = ret.get();
        }

        if (ret != nullptr) {
            _createdScenes.push_back(ret);
        }
    }
    
    return ret.get();
}

bool ScenePool::deleteScene(const I64 targetGUID) {
    if (targetGUID != -1) {
        const I64 defaultGUID = _defaultScene ? _defaultScene->getGUID() : 0;
        const I64 activeGUID = _activeScene ? _activeScene->getGUID() : 0;

        if (targetGUID != defaultGUID) {
            if (targetGUID == activeGUID && defaultGUID != 0) {
                _parentMgr.setActiveScene(_defaultScene);
            }
        } else {
            _defaultScene = nullptr;
        }

        {
            ScopedLock<SharedMutex> w_lock(_sceneLock);
            erase_if(_createdScenes, [&targetGUID](const auto& s) noexcept { return s->getGUID() == targetGUID; });
        }
        return true;
    }

    return false;
}

vector<Str256> ScenePool::sceneNameList(const bool sorted) const {
    vector<Str256> scenes;
    for (const SceneList::SceneNameMap::value_type& it : SceneList::sceneNameMap()) {
        scenes.push_back(it.second);
    }

    if (sorted) {
        eastl::sort(begin(scenes),
                    end(scenes),
                    [](const Str256& a, const Str256& b)-> bool {
                        return a < b;
                    });
    }

    return scenes;
}

} //namespace Divide