#include "stdafx.h"

#include "Headers/MeshImporter.h"
#include "Headers/DVDConverter.h"

#include "Core/Headers/ByteBuffer.h"
#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Time/Headers/ProfileTimer.h"
#include "Core/Resources/Headers/ResourceCache.h"

#include "Geometry/Shapes/Headers/Mesh.h"
#include "Geometry/Shapes/Headers/SubMesh.h"
#include "Utility/Headers/Localization.h"

#include "Platform/Video/Headers/GFXDevice.h"
#include "Platform/File/Headers/FileManagement.h"
#include "Platform/Video/Buffers/VertexBuffer/Headers/VertexBuffer.h"

namespace Divide {

namespace {
    constexpr U16 BYTE_BUFFER_VERSION = 1u;
    const char* g_parsedAssetGeometryExt = "DVDGeom";
    const char* g_parsedAssetAnimationExt = "DVDAnim";
};

GeometryFormat GetGeometryFormatForExtension(const char* extension) noexcept {
    if (Util::CompareIgnoreCase(extension, ".3ds")) {
        return GeometryFormat::_3DS;
    }
    if (Util::CompareIgnoreCase(extension, ".ase")) {
        return GeometryFormat::ASE;
    }
    if (Util::CompareIgnoreCase(extension, ".fbx")) {
        return GeometryFormat::FBX;
    }
    if (Util::CompareIgnoreCase(extension, ".md2")) {
        return GeometryFormat::MD2;
    }
    if (Util::CompareIgnoreCase(extension, ".md5mesh")) {
        return GeometryFormat::MD5;
    }
    if (Util::CompareIgnoreCase(extension, ".obj")) {
        return GeometryFormat::OBJ;
    }
    if (Util::CompareIgnoreCase(extension, ".x")) {
        return GeometryFormat::X;
    }
    if (Util::CompareIgnoreCase(extension, ".dae")) {
        return GeometryFormat::DAE;
    }
    if (Util::CompareIgnoreCase(extension, ".gltf") ||
        Util::CompareIgnoreCase(extension, ".glb")) {
        return GeometryFormat::GLTF;
    }
    if (Util::CompareIgnoreCase(extension, (string(".") + g_parsedAssetAnimationExt).c_str())) {
        return GeometryFormat::DVD_ANIM;
    }
    if (Util::CompareIgnoreCase(extension, (string(".") + g_parsedAssetGeometryExt).c_str())) {
        return GeometryFormat::DVD_GEOM;
    }
    return GeometryFormat::COUNT;
}

namespace Import {
    bool ImportData::saveToFile([[maybe_unused]] PlatformContext& context, const ResourcePath& path, const ResourcePath& fileName) {

        ByteBuffer tempBuffer;
        assert(_vertexBuffer != nullptr);
        tempBuffer << BYTE_BUFFER_VERSION;
        tempBuffer << _ID("BufferEntryPoint");
        tempBuffer << _modelName;
        tempBuffer << _modelPath;
        if (_vertexBuffer->serialize(tempBuffer)) {
            tempBuffer << to_U32(_subMeshData.size());
            for (const SubMeshData& subMesh : _subMeshData) {
                if (!subMesh.serialize(tempBuffer)) {
                    //handle error
                }
            }
            if (!_nodeData.serialize(tempBuffer)) {
                //handle error
            }

            tempBuffer << _hasAnimations;
            // Animations are handled by the SceneAnimator I/O
            return tempBuffer.dumpToFile(path.c_str(), (fileName.str() + "." + g_parsedAssetGeometryExt).c_str());
        }

        return false;
    }

    bool ImportData::loadFromFile(PlatformContext& context, const ResourcePath& path, const ResourcePath& fileName) {
        ByteBuffer tempBuffer;
        if (tempBuffer.loadFromFile(path.c_str(), (fileName.str() + "." + g_parsedAssetGeometryExt).c_str())) {

            auto tempVer = decltype(BYTE_BUFFER_VERSION){0};
            tempBuffer >> tempVer;
            if (tempVer == BYTE_BUFFER_VERSION) {
                U64 signature;
                tempBuffer >> signature;
                if (signature != _ID("BufferEntryPoint")) {
                    return false;
                }
                tempBuffer >> _modelName;
                tempBuffer >> _modelPath;
                _vertexBuffer = context.gfx().newVB();
                if (_vertexBuffer->deserialize(tempBuffer)) {
                    U32 subMeshCount = 0;
                    tempBuffer >> subMeshCount;
                    _subMeshData.resize(subMeshCount);
                    for (SubMeshData& subMesh : _subMeshData) {
                        if (!subMesh.deserialize(tempBuffer)) {
                            //handle error
                            DIVIDE_UNEXPECTED_CALL();
                        }
                    }
                    if (!_nodeData.deserialize(tempBuffer)) {
                        //handle error
                        DIVIDE_UNEXPECTED_CALL();
                    }

                    tempBuffer >> _hasAnimations;
                    _loadedFromFile = true;
                    return true;
                }
            }
        }
        return false;
    }

    bool SubMeshData::serialize(ByteBuffer& dataOut) const {
        dataOut << _name;
        dataOut << _index;
        dataOut << _boneCount;
        dataOut << _partitionIDs;
        dataOut << _minPos;
        dataOut << _maxPos;
        dataOut << _worldOffset;
        for (const auto& triangle : _triangles) {
            dataOut << triangle;
        }
        return _material.serialize(dataOut);
    }

    bool SubMeshData::deserialize(ByteBuffer& dataIn) {
        dataIn >> _name;
        dataIn >> _index;
        dataIn >> _boneCount;
        dataIn >> _partitionIDs;
        dataIn >> _minPos;
        dataIn >> _maxPos;
        dataIn >> _worldOffset;
        for (auto& triangle : _triangles) {
            dataIn >> triangle;
        }
        return _material.deserialize(dataIn);
    }

    bool MaterialData::serialize(ByteBuffer& dataOut) const {
        dataOut << _ignoreTexDiffuseAlpha;
        dataOut << _doubleSided;
        dataOut << string(_name.c_str());
        dataOut << to_U32(_shadingMode);
        dataOut << to_U32(_bumpMethod);
        dataOut << baseColour();
        dataOut << emissive();
        dataOut << ambient();
        dataOut << specular();
        dataOut << specGloss();
        dataOut << CLAMPED_01(metallic());
        dataOut << CLAMPED_01(roughness());
        dataOut << CLAMPED_01(parallaxFactor());
        for (const TextureEntry& texture : _textures) {
            if (!texture.serialize(dataOut)) {
                //handle error
                DIVIDE_UNEXPECTED_CALL();
            }
        }

        return true;
    }

    bool MaterialData::deserialize(ByteBuffer& dataIn) {
        FColour3 tempColourRGB = {};
        FColour4 tempColourRGBA = {};
        SpecularGlossiness tempSG = {};
        string tempStr = "";
        U32 temp = {};
        F32 temp2 = {};

        dataIn >> _ignoreTexDiffuseAlpha;
        dataIn >> _doubleSided;
        dataIn >> tempStr; _name = tempStr.c_str();
        dataIn >> temp; _shadingMode = static_cast<ShadingMode>(temp);
        dataIn >> temp; _bumpMethod = static_cast<BumpMethod>(temp);
        dataIn >> tempColourRGBA; baseColour(tempColourRGBA);
        dataIn >> tempColourRGB;  emissive(tempColourRGB);
        dataIn >> tempColourRGB;  ambient(tempColourRGB);
        dataIn >> tempColourRGBA; specular(tempColourRGBA);
        dataIn >> tempSG; specGloss(tempSG);
        dataIn >> temp2; metallic(temp2);
        dataIn >> temp2; roughness(temp2);
        dataIn >> temp2; parallaxFactor(temp2);
        for (TextureEntry& texture : _textures) {
            if (!texture.deserialize(dataIn)) {
                //handle error
                DIVIDE_UNEXPECTED_CALL();
            }
        }

        return true;
    }

    bool TextureEntry::serialize(ByteBuffer& dataOut) const {
        dataOut << _textureName;
        dataOut << _texturePath;
        dataOut << _srgb;
        dataOut << _useDDSCache;
        dataOut << _isNormalMap;
        dataOut << _alphaForTransparency;
        dataOut << to_U32(_wrapU);
        dataOut << to_U32(_wrapV);
        dataOut << to_U32(_wrapW);
        dataOut << to_U32(_operation);
        return true;
    }

    bool TextureEntry::deserialize(ByteBuffer& dataIn) {
        U32 data = 0u;
        dataIn >> _textureName;
        dataIn >> _texturePath;
        dataIn >> _srgb;
        dataIn >> _useDDSCache;
        dataIn >> _isNormalMap;
        dataIn >> _alphaForTransparency;
        dataIn >> data; _wrapU = static_cast<TextureWrap>(data);
        dataIn >> data; _wrapV = static_cast<TextureWrap>(data);
        dataIn >> data; _wrapW = static_cast<TextureWrap>(data);
        dataIn >> data; _operation = static_cast<TextureOperation>(data);
        return true;
    }
};
    bool MeshImporter::loadMeshDataFromFile(PlatformContext& context, Import::ImportData& dataOut) {
        Time::ProfileTimer importTimer = {};
        importTimer.start();

        bool success = false;
        if (!context.config().debug.useGeometryCache || !dataOut.loadFromFile(context, Paths::g_cacheLocation + Paths::g_geometryCacheLocation, dataOut.modelName())) {
            Console::printfn(Locale::Get(_ID("MESH_NOT_LOADED_FROM_FILE")), dataOut.modelName().c_str());

            if (DVDConverter::Load(context, dataOut)) {
                if (dataOut.saveToFile(context, Paths::g_cacheLocation + Paths::g_geometryCacheLocation, dataOut.modelName())) {
                    Console::printfn(Locale::Get(_ID("MESH_SAVED_TO_FILE")), dataOut.modelName().c_str());
                } else {
                    Console::printfn(Locale::Get(_ID("MESH_NOT_SAVED_TO_FILE")), dataOut.modelName().c_str());
                }
                success = true;
            }
        } else {
            Console::printfn(Locale::Get(_ID("MESH_LOADED_FROM_FILE")), dataOut.modelName().c_str());
            dataOut.fromFile(true);
            success = true;
        }

        importTimer.stop();
        Console::d_printfn(Locale::Get(_ID("LOAD_MESH_TIME")),
                           dataOut.modelName().c_str(),
                           Time::MicrosecondsToMilliseconds<F32>(importTimer.get()));

        return success;
    }

    bool MeshImporter::loadMesh(const bool loadedFromCache, const Mesh_ptr& mesh, PlatformContext& context, ResourceCache* cache, const Import::ImportData& dataIn) {
        Time::ProfileTimer importTimer;
        importTimer.start();

        mesh->setObjectFlag(dataIn.hasAnimations() ? Object3D::ObjectFlag::OBJECT_FLAG_SKINNED : Object3D::ObjectFlag::OBJECT_FLAG_NONE);
        mesh->renderState().drawState(true);
        mesh->geometryBuffer()->fromBuffer(*dataIn._vertexBuffer);
        mesh->geometryDirty(true);
        mesh->geometryBuffer()->create(true, true);

        std::atomic_uint taskCounter;
        std::atomic_init(&taskCounter, 0u);

        for (const Import::SubMeshData& subMeshData : dataIn._subMeshData) {
            // Submesh is created as a resource when added to the scenegraph
            SubMesh_ptr tempSubMesh = CreateResource<SubMesh>(cache, ResourceDescriptor(subMeshData.name()));
            tempSubMesh->id(subMeshData.index());
            tempSubMesh->setObjectFlag(subMeshData.boneCount() > 0u ? Object3D::ObjectFlag::OBJECT_FLAG_SKINNED : Object3D::ObjectFlag::OBJECT_FLAG_NONE);

            // it may already be loaded
            if (!tempSubMesh->parentMesh()) {
                Attorney::MeshImporter::addSubMesh(*mesh, tempSubMesh);

                for (U8 i = 0u, j = 0u; i < Import::MAX_LOD_LEVELS; ++i) {
                    if (!subMeshData._triangles[i].empty()) {
                        tempSubMesh->setGeometryPartitionID(j, subMeshData._partitionIDs[j]);
                        tempSubMesh->addTriangles(subMeshData._partitionIDs[j], subMeshData._triangles[j]);
                        ++j;
                    }
                }
                Attorney::SubMeshMeshImporter::setBoundingBox(*tempSubMesh, subMeshData.minPos(), subMeshData.maxPos(), subMeshData.worldOffset());

                if (!tempSubMesh->getMaterialTpl()) {
                    tempSubMesh->setMaterialTpl(loadSubMeshMaterial(cache, subMeshData._material, loadedFromCache, subMeshData.boneCount() > 0, taskCounter));
                }
            }
        }

        Attorney::MeshImporter::setNodeData(*mesh, dataIn._nodeData);

        WAIT_FOR_CONDITION(taskCounter.load() == 0)

        if (dataIn.hasAnimations()) {
            std::shared_ptr<SceneAnimator> animator;
            // Animation versioning is handled internally.
            ByteBuffer tempBuffer;
            animator.reset(new SceneAnimator());
            if (tempBuffer.loadFromFile((Paths::g_cacheLocation + Paths::g_geometryCacheLocation).c_str(),
                (dataIn.modelName() + "." + g_parsedAssetAnimationExt).c_str()))
            {
                animator->load(context, tempBuffer);
            } else {
                if (!dataIn.loadedFromFile()) {
                    // We lose ownership of animations here ...
                    Attorney::SceneAnimatorMeshImporter::registerAnimations(*animator, dataIn._animations);

                    animator->init(context, dataIn._skeleton, dataIn._bones);
                    animator->save(context, tempBuffer);
                    if (!tempBuffer.dumpToFile((Paths::g_cacheLocation + Paths::g_geometryCacheLocation).c_str(),
                        (dataIn.modelName() + "." + g_parsedAssetAnimationExt).c_str()))
                    {
                        //handle error
                        DIVIDE_UNEXPECTED_CALL();
                    }
                }
                else {
                    //handle error. No ASSIMP animation data available
                    DIVIDE_UNEXPECTED_CALL();
                }
            }

            mesh->setAnimator(animator);
        }

        importTimer.stop();
        Console::d_printfn(Locale::Get(_ID("PARSE_MESH_TIME")),
                           dataIn.modelName().c_str(),
                           Time::MicrosecondsToMilliseconds(importTimer.get()));

        return true;
    }

    /// Load the material for the current SubMesh
    Material_ptr MeshImporter::loadSubMeshMaterial(ResourceCache* cache, const Import::MaterialData& importData, const bool loadedFromCache, bool skinned, std::atomic_uint& taskCounter) {
        bool wasInCache = false;
        Material_ptr tempMaterial = CreateResource<Material>(cache, ResourceDescriptor(importData.name()), wasInCache);
        if (wasInCache) {
            return tempMaterial;
        }
        if (!loadedFromCache) {
            tempMaterial->ignoreXMLData(true);
        }

        tempMaterial->properties().hardwareSkinning(skinned);
        tempMaterial->properties().emissive(importData.emissive());
        tempMaterial->properties().ambient(importData.ambient());
        tempMaterial->properties().specular(importData.specular().rgb);
        tempMaterial->properties().shininess(importData.specular().a);
        tempMaterial->properties().specGloss(importData.specGloss());
        tempMaterial->properties().metallic(importData.metallic());
        tempMaterial->properties().roughness(importData.roughness());
        tempMaterial->properties().parallaxFactor(importData.parallaxFactor());

        tempMaterial->properties().baseColour(importData.baseColour());
        tempMaterial->properties().ignoreTexDiffuseAlpha(importData.ignoreTexDiffuseAlpha());
        tempMaterial->properties().shadingMode(importData.shadingMode());
        tempMaterial->properties().bumpMethod(importData.bumpMethod());
        tempMaterial->properties().doubleSided(importData.doubleSided());

        SamplerDescriptor textureSampler = {};

        TextureDescriptor textureDescriptor(TextureType::TEXTURE_2D_ARRAY);

        for (U32 i = 0; i < to_base(TextureUsage::COUNT); ++i) {
            const Import::TextureEntry& tex = importData._textures[i];
            if (!tex.textureName().empty()) {
                textureSampler.wrapU(tex.wrapU());
                textureSampler.wrapV(tex.wrapV());
                textureSampler.wrapW(tex.wrapW());

                ImageTools::ImportOptions importOptions{};
                importOptions._useDDSCache = tex.useDDSCache();
                importOptions._isNormalMap = tex.isNormalMap();
                importOptions._alphaChannelTransparency = tex.alphaForTransparency();

                textureDescriptor.srgb(tex.srgb());
                textureDescriptor.textureOptions(importOptions);
                ResourceDescriptor texture(tex.textureName().str());
                texture.assetName(tex.textureName());
                texture.assetLocation(tex.texturePath());
                texture.propertyDescriptor(textureDescriptor);
                // No need to fire off additional threads just to wait on the result immediately after
                texture.waitForReady(true);
                Texture_ptr texPtr = CreateResource<Texture>(cache, texture, taskCounter);
                texPtr->addStateCallback(ResourceState::RES_LOADED, [tempMaterial, i, texPtr, tex, textureSampler](CachedResource*) {
                    tempMaterial->setTexture(static_cast<TextureUsage>(i), texPtr, textureSampler.getHash(),tex.operation());
                });
            }
        }

        return tempMaterial;
    }
}; //namespace Divide