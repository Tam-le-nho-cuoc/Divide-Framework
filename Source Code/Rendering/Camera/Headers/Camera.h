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
#ifndef _CAMERA_H
#define _CAMERA_H

#include "CameraSnapshot.h"
#include "Frustum.h"

#include "Core/Resources/Headers/Resource.h"

namespace Divide {

enum class FStops : U8
{
    F_1_4,
    F_1_8,
    F_2_0,
    F_2_8,
    F_3_5,
    F_4_0,
    F_5_6,
    F_8_0,
    F_11_0,
    F_16_0,
    F_22_0,
    F_32_0,
    COUNT
};

namespace Names {
    static const char* fStops[] = {
        "f/1.4", "f/1.8", "f/2", "f/2.8", "f/3.5", "f/4", "f/5.6", "f/8", "f/11", "f/16", "f/22", "f/32", "NONE"
    };
}

static constexpr std::array<F32, to_base(FStops::COUNT)> g_FStopValues = {
    1.4f, 1.8f, 2.0f, 2.8f, 3.5f, 4.0f, 5.6f, 8.0f, 11.0f, 16.0f, 22.0f, 32.0f
};

namespace TypeUtil {
    const char* FStopsToString(FStops stop) noexcept;
    FStops StringToFStops(const string& name);
};

class GFXDevice;
class Camera : public Resource {
   public:
    using CameraListener = DELEGATE<void, const Camera&>;

    static constexpr F32 s_minNearZ = 0.1f;

    enum class CameraType : U8 {
        FREE_FLY = 0,
        STATIC,
        FIRST_PERSON,
        THIRD_PERSON,
        ORBIT,
        SCRIPTED,
        COUNT
    };

    enum class UtilityCamera : U8 {
        _2D = 0,
        _2D_FLIP_Y,
        DEFAULT,
        CUBE,
        DUAL_PARABOLOID,
        COUNT
    };

   public:
     virtual ~Camera() = default;

    virtual void fromCamera(const Camera& camera);
    virtual void fromSnapshot(const CameraSnapshot& snapshot);
    [[nodiscard]] const CameraSnapshot& snapshot() const noexcept;

    // Return true if the cached camera state wasn't up-to-date
    bool updateLookAt();
    void setReflection(const Plane<F32>& reflectionPlane) noexcept;
    void clearReflection() noexcept;

    /// Global rotations are applied relative to the world axis, not the camera's
    virtual void setGlobalRotation(F32 yaw, F32 pitch, F32 roll = 0.0f) noexcept;
    void setGlobalRotation(const vec3<Angle::DEGREES<F32>>& euler) noexcept { setGlobalRotation(euler.yaw, euler.pitch, euler.roll); }

    const mat4<F32>& lookAt(const mat4<F32>& viewMatrix);
    /// Sets the camera's position, target and up directions
    const mat4<F32>& lookAt(const vec3<F32>& eye, const vec3<F32>& target, const vec3<F32>& up);
    /// Sets the camera to point at the specified target point
    const mat4<F32>& lookAt(const vec3<F32>& target) { return lookAt(_data._eye, target); }
    const mat4<F32>& lookAt(const vec3<F32>& eye, const vec3<F32>& target) { return lookAt(eye, target, getUpDir()); }

    /// Sets the camera's Yaw angle.
    /// This creates a new orientation quaternion for the camera and extracts the Euler angles
    void setYaw(const Angle::DEGREES<F32> angle) noexcept { setRotation(angle, _euler.pitch, _euler.roll); }
    /// Sets the camera's Pitch angle. Yaw and Roll are previous extracted values
    void setPitch(const Angle::DEGREES<F32> angle) noexcept { setRotation(_euler.yaw, angle, _euler.roll); }
    /// Sets the camera's Roll angle. Yaw and Pitch are previous extracted values
    void setRoll(const Angle::DEGREES<F32> angle) noexcept { setRotation(_euler.yaw, _euler.pitch, angle); }
    /// Sets the camera's Yaw angle.
    /// This creates a new orientation quaternion for the camera and extracts the Euler angles
    void setGlobalYaw(const Angle::DEGREES<F32> angle) noexcept { setGlobalRotation(angle, _euler.pitch, _euler.roll); }
    /// Sets the camera's Pitch angle. Yaw and Roll are previous extracted values
    void setGlobalPitch(const Angle::DEGREES<F32> angle) noexcept { setGlobalRotation(_euler.yaw, angle, _euler.roll); }
    /// Sets the camera's Roll angle. Yaw and Pitch are previous extracted values
    void setGlobalRoll(const Angle::DEGREES<F32> angle) noexcept { setGlobalRotation(_euler.yaw, _euler.pitch, angle); }

    void setEye(const F32 x, const F32 y, const F32 z) noexcept { _data._eye.set(x, y, z); _viewMatrixDirty = true; }
    void setEye(const vec3<F32>& position) noexcept { setEye(position.x, position.y, position.z); }

    void setRotation(const Quaternion<F32>& q) noexcept { _data._orientation = q; _viewMatrixDirty = true; }
    void setRotation(const Angle::DEGREES<F32> yaw, const Angle::DEGREES<F32> pitch, const Angle::DEGREES<F32> roll = 0.0f) noexcept;

    void setEuler(const vec3<Angle::DEGREES<F32>>& euler) noexcept { setRotation(euler.yaw, euler.pitch, euler.roll); }
    void setEuler(const Angle::DEGREES<F32>& pitch, const Angle::DEGREES<F32>& yaw, const Angle::DEGREES<F32>& roll) noexcept { setRotation(yaw, pitch, roll); }

    void setAspectRatio(F32 ratio) noexcept;
    [[nodiscard]] F32 getAspectRatio() const noexcept { return _data._aspectRatio; }

    void setVerticalFoV(Angle::DEGREES<F32> verticalFoV) noexcept;
    [[nodiscard]] Angle::DEGREES<F32> getVerticalFoV() const noexcept { return _data._FoV; }

    void setHorizontalFoV(Angle::DEGREES<F32> horizontalFoV) noexcept;
    [[nodiscard]] Angle::DEGREES<F32> getHorizontalFoV() const noexcept;

    [[nodiscard]] const CameraType& type()                    const noexcept { return _type; }
    [[nodiscard]] const vec3<F32>& getEye()                   const noexcept { return _data._eye; }
    [[nodiscard]] const vec3<Angle::DEGREES<F32>>& getEuler() const noexcept { return _euler; }
    [[nodiscard]] const Quaternion<F32>& getOrientation()     const noexcept { return _data._orientation; }
    [[nodiscard]] const vec2<F32>& getZPlanes()               const noexcept { return _data._zPlanes; }
    [[nodiscard]] const vec4<F32>& orthoRect()                const noexcept { return _orthoRect; }
    [[nodiscard]] bool isOrthoProjected()                     const noexcept { return _data._isOrthoCamera; }

    [[nodiscard]] FORCE_INLINE vec3<F32> getUpDir()      const noexcept { return viewMatrix().getUpVec(); }
    [[nodiscard]] FORCE_INLINE vec3<F32> getRightDir()   const noexcept { return viewMatrix().getRightVec(); }
    [[nodiscard]] FORCE_INLINE vec3<F32> getForwardDir() const noexcept { return viewMatrix().getForwardVec(); }

    [[nodiscard]] const mat4<F32>& viewMatrix() const noexcept { return _data._viewMatrix; }
    [[nodiscard]] const mat4<F32>& viewMatrix()       noexcept { updateViewMatrix(); return _data._viewMatrix; }

    [[nodiscard]] const mat4<F32>& projectionMatrix() const noexcept { return _data._projectionMatrix; }
    [[nodiscard]] const mat4<F32>& projectionMatrix()       noexcept { updateProjection(); return _data._projectionMatrix; }

    [[nodiscard]] const mat4<F32>& worldMatrix()            noexcept { return _data._invViewMatrix; }
    [[nodiscard]] const mat4<F32>& worldMatrix()      const noexcept { return _data._invViewMatrix; }

    /// Nothing really to unload
    virtual bool unload() noexcept { return true; }

    const mat4<F32>& setProjection(const vec2<F32>& zPlanes);
    const mat4<F32>& setProjection(F32 verticalFoV, const vec2<F32>& zPlanes);
    const mat4<F32>& setProjection(F32 aspectRatio, F32 verticalFoV, const vec2<F32>& zPlanes);
    const mat4<F32>& setProjection(const vec4<F32>& rect, const vec2<F32>& zPlanes);
    const mat4<F32>& setProjection(const mat4<F32>& projection, const vec2<F32>& zPlanes, bool isOrtho) noexcept;

    /// Extract the frustum associated with our current PoV
    virtual bool updateFrustum();
    /// Get the camera's current frustum
    [[nodiscard]] const Frustum& getFrustum() const noexcept { assert(!_frustumDirty); return _frustum; }
    [[nodiscard]] Frustum& getFrustum() noexcept { assert(!_frustumDirty); return _frustum; }
    void lockFrustum(const bool state) noexcept { _frustumLocked = state; }

    /// Returns the world space direction for the specified winCoords for this camera
    /// Use getEye() + unProject(...) * distance for a world-space position
    [[nodiscard]] vec3<F32> unProject(F32 winCoordsX, F32 winCoordsY, const Rect<I32>& viewport) const noexcept;
    [[nodiscard]] vec3<F32> unProject(const vec3<F32>& winCoords, const Rect<I32>& viewport) const noexcept { return unProject(winCoords.x, winCoords.y, viewport); }
    [[nodiscard]] vec2<F32> project(const vec3<F32>& worldCoords, const Rect<I32>& viewport) const noexcept;

    [[nodiscard]] bool removeUpdateListener(U32 id);
    [[nodiscard]] U32 addUpdateListener(const CameraListener& f);

    virtual void saveToXML(boost::property_tree::ptree& pt, string prefix = "") const;
    virtual void loadFromXML(const boost::property_tree::ptree& pt, string prefix = "");

    PROPERTY_R_IW(mat4<F32>, viewProjectionMatrix);

   protected:
    virtual bool updateViewMatrix() noexcept;
    virtual bool updateProjection() noexcept;
    virtual void update() noexcept;

    [[nodiscard]] const char* getResourceTypeName() const noexcept override { return "Camera"; }

    [[nodiscard]] bool dirty() const noexcept { return _projectionDirty || _viewMatrixDirty || _frustumDirty; }

    [[nodiscard]] string xmlSavePath(const string& prefix) const;

   protected:
    SET_DELETE_FRIEND
    SET_DELETE_HASHMAP_FRIEND
    explicit Camera(const Str256& name, const CameraType& type, const vec3<F32>& eye = VECTOR3_ZERO);

   protected:
    using ListenerMap = hashMap<U32, CameraListener>;
    ListenerMap _updateCameraListeners;

    CameraSnapshot _data;
    Frustum _frustum;
    Plane<F32> _reflectionPlane;
    vec4<F32> _orthoRect = VECTOR4_UNIT;
    vec3<Angle::DEGREES<F32>> _euler = VECTOR3_ZERO;
    Angle::DEGREES<F32> _accumPitchDegrees = 0.0f;

    U32 _updateCameraId = 0u;
    CameraType _type = CameraType::COUNT;
    // Since quaternion reflection is complicated and not really needed now, handle reflections a-la Ogre -Ionut
    bool _reflectionActive = false;
    bool _projectionDirty = true;
    bool _viewMatrixDirty = false;
    bool _frustumLocked = false;
    bool _frustumDirty = true;

    static F32 s_lastFrameTimeSec;

   // Camera pool
   public:
    static void Update(U64 deltaTimeUS);
    static void InitPool();
    static void DestroyPool();

    template <typename T>
    typename std::enable_if<std::is_base_of<Camera, T>::value, bool>::type
    static DestroyCamera(T*& camera) {
        if (DestroyCameraInternal(camera)) {
            camera = nullptr;
            return true;
        }

        return false;
    }

    template <typename T = Camera>
    typename std::enable_if<std::is_base_of<Camera, T>::value, T*>::type
    static GetUtilityCamera(const UtilityCamera type) { return static_cast<T*>(GetUtilityCameraInternal(type)); }

    template <typename T>
    static T* CreateCamera(const Str256& cameraName) { return static_cast<T*>(CreateCameraInternal(cameraName, T::Type())); }

    template <typename T = Camera>
    typename std::enable_if<std::is_base_of<Camera, T>::value, T*>::type
    static FindCamera(const U64 nameHash) { return static_cast<T*>(FindCameraInternal(nameHash)); }

    static bool RemoveChangeListener(U32 id);
    static U32  AddChangeListener(const CameraListener& f);

   private:
     static bool    DestroyCameraInternal(Camera* camera);
     static Camera* GetUtilityCameraInternal(UtilityCamera type);
     static Camera* CreateCameraInternal(const Str256& cameraName, CameraType type);
     static Camera* FindCameraInternal(U64 nameHash);

   private:
    using CameraPool = vector<Camera*>;

    static std::array<Camera*, to_base(UtilityCamera::COUNT)> _utilityCameras;

    static U32 s_changeCameraId;
    static ListenerMap s_changeCameraListeners;
    static CameraPool s_cameraPool;
    static SharedMutex s_cameraPoolLock;
};

TYPEDEF_SMART_POINTERS_FOR_TYPE(Camera);

// Just a simple alias for a regular camera
class StaticCamera final : public Camera {
protected:
    friend class Camera;
    explicit StaticCamera(const Str256& name, const vec3<F32>& eye = VECTOR3_ZERO)
        : Camera(name, Type(), eye)
    {
    }

public:
    static constexpr CameraType Type() noexcept { return CameraType::STATIC; }
};

};  // namespace Divide
#endif
