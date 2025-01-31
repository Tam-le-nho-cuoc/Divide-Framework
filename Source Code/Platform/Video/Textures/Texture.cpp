#include "stdafx.h"

#include "Headers/Texture.h"

#include "Core/Headers/ByteBuffer.h"
#include "Core/Headers/Kernel.h"
#include "Core/Headers/EngineTaskPool.h"
#include "Core/Headers/PlatformContext.h"
#include "Core/Headers/StringHelper.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Platform/File/Headers/FileManagement.h"
#include "Platform/Video/Headers/GFXDevice.h"
#include "Utility/Headers/Localization.h"

namespace Divide {

constexpr U16 BYTE_BUFFER_VERSION = 1u;

ResourcePath Texture::s_missingTextureFileName("missing_texture.jpg");

Texture_ptr Texture::s_defaulTexture = nullptr;
bool Texture::s_useDDSCache = true;

void Texture::OnStartup(GFXDevice& gfx) {
    ImageTools::OnStartup(gfx.renderAPI() != RenderAPI::OpenGL);

    TextureDescriptor textureDescriptor(TextureType::TEXTURE_2D_ARRAY);
    textureDescriptor.srgb(false);
    textureDescriptor.baseFormat(GFXImageFormat::RGBA);

    ResourceDescriptor textureResourceDescriptor("defaultEmptyTexture");
    textureResourceDescriptor.propertyDescriptor(textureDescriptor);
    textureResourceDescriptor.waitForReady(true);
    s_defaulTexture = CreateResource<Texture>(gfx.parent().resourceCache(), textureResourceDescriptor);

    Byte* defaultTexData = MemoryManager_NEW Byte[1u * 1u * 4];
    defaultTexData[0] = defaultTexData[1] = defaultTexData[2] = to_byte(0u); //RGB: black
    defaultTexData[3] = to_byte(1u); //Alpha: 1

    ImageTools::ImageData imgDataDefault = {};
    if (!imgDataDefault.loadFromMemory(defaultTexData, 4, 1u, 1u, 1u, 4)) {
        DIVIDE_UNEXPECTED_CALL();
    }
    s_defaulTexture->loadData(imgDataDefault);
    MemoryManager::DELETE_ARRAY(defaultTexData);
}

void Texture::OnShutdown() noexcept {
    s_defaulTexture.reset();
    ImageTools::OnShutdown();
}

bool Texture::UseTextureDDSCache() noexcept {
    return s_useDDSCache;
}

const Texture_ptr& Texture::DefaultTexture() noexcept {
    return s_defaulTexture;
}

ResourcePath Texture::GetCachePath(ResourcePath originalPath) noexcept {
    constexpr std::array<std::string_view, 2> searchPattern = {
        "//", "\\"
    };

    Util::ReplaceStringInPlace(originalPath, searchPattern, "/");
    Util::ReplaceStringInPlace(originalPath, "/", "_");
    if (originalPath.str().back() == '_') {
        originalPath.pop_back();
    }
    const ResourcePath cachePath = Paths::g_cacheLocation + Paths::Textures::g_metadataLocation + originalPath + "/";

    return cachePath;
}

Texture::Texture(GFXDevice& context,
                 const size_t descriptorHash,
                 const Str256& name,
                 const ResourcePath& assetNames,
                 const ResourcePath& assetLocations,
                 const TextureDescriptor& texDescriptor,
                 ResourceCache& parentCache)
    : CachedResource(ResourceType::GPU_OBJECT, descriptorHash, name, assetNames, assetLocations),
      GraphicsResource(context, Type::TEXTURE, getGUID(), _ID(name.c_str())),
      _descriptor(texDescriptor),
      _data{0u, TextureType::COUNT},
      _numLayers(texDescriptor.layerCount()),
      _parentCache(parentCache)
{
}

Texture::~Texture()
{
    _parentCache.remove(this);
}

bool Texture::load() {
    Start(*CreateTask([this]([[maybe_unused]] const Task & parent) { threadedLoad(); }),
            _context.context().taskPool(TaskPoolType::HIGH_PRIORITY));

    return true;
}

/// Load texture data using the specified file name
void Texture::threadedLoad() {
    OPTICK_EVENT();

    if (!assetLocation().empty()) {

        const GFXDataFormat requestedFormat = _descriptor.dataType();
        assert(requestedFormat == GFXDataFormat::UNSIGNED_BYTE ||  // Regular image format
               requestedFormat == GFXDataFormat::UNSIGNED_SHORT || // 16Bit
               requestedFormat == GFXDataFormat::FLOAT_32 ||       // HDR
               requestedFormat == GFXDataFormat::COUNT);           // Auto

        constexpr std::array<std::string_view, 2> searchPattern = {
         "//", "\\"
        };


        // Each texture face/layer must be in a comma separated list
        stringstream textureLocationList(assetLocation().str());
        stringstream textureFileList(assetName().c_str());

        ImageTools::ImageData dataStorage = {};
        dataStorage.requestedFormat(requestedFormat);

        bool loadedFromFile = false;
        // We loop over every texture in the above list and store it in this temporary string
        string currentTextureFile;
        string currentTextureLocation;
        ResourcePath currentTextureFullPath;
        while (std::getline(textureLocationList, currentTextureLocation, ',') &&
               std::getline(textureFileList, currentTextureFile, ','))
        {
            Util::Trim(currentTextureFile);

            // Skip invalid entries
            if (!currentTextureFile.empty()) {
                Util::ReplaceStringInPlace(currentTextureFile, searchPattern, "/");
                currentTextureFullPath = currentTextureLocation.empty() ? Paths::g_texturesLocation : ResourcePath{ currentTextureLocation };
                auto[file, path] = splitPathToNameAndLocation(currentTextureFile.c_str());
                const ResourcePath fileName = file;
                if (!path.empty()) {
                    currentTextureFullPath += path;
                }
                
                Util::ReplaceStringInPlace(currentTextureFullPath, searchPattern, "/");
                
                // Attempt to load the current entry
                if (!loadFile(currentTextureFullPath, file, dataStorage)) {
                    // Invalid texture files are not handled yet, so stop loading
                    continue;
                }

                loadedFromFile = true;
            }
        }

        if (loadedFromFile) {
            // Create a new Rendering API-dependent texture object
            _descriptor.baseFormat(dataStorage.format());
            _descriptor.dataType(dataStorage.dataType());
            // Uploading to the GPU dependents on the rendering API
            loadData(dataStorage);

            if (_descriptor.texType() == TextureType::TEXTURE_CUBE_MAP ||
                _descriptor.texType() == TextureType::TEXTURE_CUBE_ARRAY) {
                if (dataStorage.layerCount() % 6 != 0) {
                    Console::errorfn(
                        Locale::Get(_ID("ERROR_TEXTURE_LOADER_CUBMAP_INIT_COUNT")),
                        resourceName().c_str());
                    return;
                }
            }

            if (_descriptor.texType() == TextureType::TEXTURE_2D_ARRAY ||
                _descriptor.texType() == TextureType::TEXTURE_2D_ARRAY_MS) {
                if (dataStorage.layerCount() != _numLayers) {
                    Console::errorfn(
                        Locale::Get(_ID("ERROR_TEXTURE_LOADER_ARRAY_INIT_COUNT")),
                        resourceName().c_str());
                    return;
                }
            }

            /*if (_descriptor.texType() == TextureType::TEXTURE_CUBE_ARRAY) {
                if (dataStorage.layerCount() / 6 != _numLayers) {
                    Console::errorfn(
                        Locale::Get(_ID("ERROR_TEXTURE_LOADER_ARRAY_INIT_COUNT")),
                        resourceName().c_str());
                }
            }*/
        }
    }

    CachedResource::load();
}

U8 Texture::numChannels() const noexcept {
    switch(descriptor().baseFormat()) {
        case GFXImageFormat::RED:  return 1u;
        case GFXImageFormat::RG:   return 2u;
        case GFXImageFormat::RGB:  return 3u;
        case GFXImageFormat::RGBA: return 4u;
    }

    return 0u;
}

bool Texture::loadFile(const ResourcePath& path, const ResourcePath& name, ImageTools::ImageData& fileData) {

    if (!fileExists(path + name) || 
        !fileData.loadFromFile(_descriptor.srgb(),
                               _width,
                               _height,
                               path,
                               name,
                               _descriptor.textureOptions())) 
    {
        if (fileData.layerCount() > 0) {
            Console::errorfn(Locale::Get(_ID("ERROR_TEXTURE_LAYER_LOAD")), name.c_str());
            return false;
        }
        Console::errorfn(Locale::Get(_ID("ERROR_TEXTURE_LOAD")), name.c_str());
        // missing_texture.jpg must be something that really stands out
        if (!fileData.loadFromFile(_descriptor.srgb(), _width, _height, Paths::g_assetsLocation + Paths::g_texturesLocation, s_missingTextureFileName)) {
            DIVIDE_UNEXPECTED_CALL();
        }
    } else {
        return checkTransparency(path, name, fileData);
    }

    return true;
}

bool Texture::checkTransparency(const ResourcePath& path, const ResourcePath& name, ImageTools::ImageData& fileData) {

    if (fileData.ignoreAlphaChannelTransparency() || fileData.hasDummyAlphaChannel()) {
        _hasTransparency = false;
        _hasTranslucency = false;
        return true;
    }

    const U32 layer = to_U32(fileData.layerCount() - 1);

    // Extract width, height and bit depth
    const U16 width = fileData.dimensions(layer, 0u).width;
    const U16 height = fileData.dimensions(layer, 0u).height;
    // If we have an alpha channel, we must check for translucency/transparency

    const ResourcePath cachePath = GetCachePath(path);
    const ResourcePath cacheName = name + ".cache";

    ByteBuffer metadataCache;
    bool skip = false;
    if (metadataCache.loadFromFile(cachePath.c_str(), cacheName.c_str())) {
        auto tempVer = decltype(BYTE_BUFFER_VERSION){0};
        metadataCache >> tempVer;
        if (tempVer == BYTE_BUFFER_VERSION) {
            metadataCache >> _hasTransparency;
            metadataCache >> _hasTranslucency;
            skip = true;
        } else {
            metadataCache.clear();
        }
    }

    if (!skip) {
        if (HasAlphaChannel(fileData.format())) {
            bool hasTransulenctOrOpaquePixels = false;
            // Allo about 4 pixels per partition to be ignored
            constexpr U32 transparentPixelsSkipCount = 4u;

            std::atomic_uint transparentPixelCount = 0u;

            ParallelForDescriptor descriptor = {};
            descriptor._iterCount = width;
            descriptor._partitionSize = std::max(16u, to_U32(width / 10));
            descriptor._useCurrentThread = true;
            descriptor._cbk =  [this, &fileData, &hasTransulenctOrOpaquePixels, height, layer, &transparentPixelCount, transparentPixelsSkipCount](const Task* /*parent*/, const U32 start, const U32 end) {
                U8 tempA = 0u;
                for (U32 i = start; i < end; ++i) {
                    for (I32 j = 0; j < height; ++j) {
                        if (_hasTransparency && (_hasTranslucency || hasTransulenctOrOpaquePixels)) {
                            return;
                        }
                        fileData.getAlpha(i, j, tempA, layer);
                        if (IS_IN_RANGE_INCLUSIVE(tempA, 0, 250)) {
                            if (transparentPixelCount.fetch_add(1u) >= transparentPixelsSkipCount) {
                                _hasTransparency = true;
                                _hasTranslucency = tempA > 1;
                                if (_hasTranslucency) {
                                    hasTransulenctOrOpaquePixels = true;
                                    return;
                                }
                            }
                        } else if (tempA > 250) {
                            hasTransulenctOrOpaquePixels = true;
                        }
                    }
                }
            };
            if (_hasTransparency && !_hasTranslucency && !hasTransulenctOrOpaquePixels) {
                // All the alpha values are 0, so this channel is useless.
                _hasTransparency = _hasTranslucency = false;
            }
            parallel_for(_context.context(), descriptor);
            metadataCache << BYTE_BUFFER_VERSION;
            metadataCache << _hasTransparency;
            metadataCache << _hasTranslucency;
            if (!metadataCache.dumpToFile(cachePath.c_str(), cacheName.c_str())) {
                DIVIDE_UNEXPECTED_CALL();
            }
        }
    }

    Console::printfn(Locale::Get(_ID("TEXTURE_HAS_TRANSPARENCY_TRANSLUCENCY")),
                                    name.c_str(),
                                    _hasTransparency ? "yes" : "no",
                                    _hasTranslucency ? "yes" : "no");

    return true;
}

void Texture::setSampleCount(U8 newSampleCount) { 
    CLAMP(newSampleCount, to_U8(0u), _context.gpuState().maxMSAASampleCount());
    if (_descriptor.msaaSamples() != newSampleCount) {
        _descriptor.msaaSamples(newSampleCount);
        loadData(nullptr, 0u, { width(), height() });
    }
}

};
