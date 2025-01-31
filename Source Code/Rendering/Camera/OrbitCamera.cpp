#include "stdafx.h"

#include "Headers/OrbitCamera.h"

#include "Graphs/Headers/SceneGraphNode.h"
#include "ECS/Components//Headers/TransformComponent.h"

namespace Divide {

OrbitCamera::OrbitCamera(const Str256& name, const CameraType& type, const vec3<F32>& eye)
    : FreeFlyCamera(name, type, eye)
{
}

void OrbitCamera::fromCamera(const Camera& camera) {
    if (camera.type() == Type()) {
        const OrbitCamera& orbitCam = static_cast<const OrbitCamera&>(camera);
        _maxRadius = orbitCam._maxRadius;
        _minRadius = orbitCam._minRadius;
        _curRadius = orbitCam._curRadius;
        _currentRotationX = orbitCam._currentRotationX;
        _currentRotationY = orbitCam._currentRotationY;
        _rotationDirty = true;
        _offsetDir.set(orbitCam._offsetDir);
        _cameraRotation.set(orbitCam._cameraRotation);
        _targetTransform = orbitCam._targetTransform;
    }

    FreeFlyCamera::fromCamera(camera);
}

void OrbitCamera::setTarget(TransformComponent* tComp) noexcept {
    _targetTransform = tComp;
}

void OrbitCamera::setTarget(TransformComponent* tComp, const vec3<F32>& offsetDirection) noexcept {
    setTarget(tComp);
    _offsetDir = Normalized(offsetDirection);
}

void OrbitCamera::update() noexcept {
    FreeFlyCamera::update();

    if (!_targetTransform) {
        return;
    }

    vec3<F32> newTargetOrientation;
    if (/*trans->changedLastFrame() || */ _rotationDirty || true) {
        newTargetOrientation = _targetTransform->getWorldOrientation().getEuler();
        newTargetOrientation.yaw = M_PI_f - newTargetOrientation.yaw;
        newTargetOrientation += _cameraRotation;
        Util::Normalize(newTargetOrientation, false);
        _rotationDirty = false;
    }

    _data._orientation.fromEuler(Angle::to_DEGREES(newTargetOrientation));
    setEye(_targetTransform->getWorldPosition() + _data._orientation * (_offsetDir * _curRadius));
    _viewMatrixDirty = true;
}

bool OrbitCamera::zoom(const F32 zoomFactor) noexcept {
    if (!IS_ZERO(zoomFactor)) {
        curRadius(_curRadius += zoomFactor * getZoomSpeedFactor() * s_lastFrameTimeSec * -0.01f);
    }

    return FreeFlyCamera::zoom(zoomFactor);
}

void OrbitCamera::saveToXML(boost::property_tree::ptree& pt, const string prefix) const {
    FreeFlyCamera::saveToXML(pt, prefix);

    const string savePath = xmlSavePath(prefix);

    pt.put(savePath + ".maxRadius", maxRadius());
    pt.put(savePath + ".minRadius", minRadius());
    pt.put(savePath + ".curRadius", curRadius());
    pt.put(savePath + ".cameraRotation.<xmlattr>.x", _cameraRotation.x);
    pt.put(savePath + ".cameraRotation.<xmlattr>.y", _cameraRotation.y);
    pt.put(savePath + ".cameraRotation.<xmlattr>.z", _cameraRotation.z); 
    pt.put(savePath + ".offsetDir.<xmlattr>.x", _offsetDir.x);
    pt.put(savePath + ".offsetDir.<xmlattr>.y", _offsetDir.y);
    pt.put(savePath + ".offsetDir.<xmlattr>.z", _offsetDir.z);
}

void OrbitCamera::loadFromXML(const boost::property_tree::ptree& pt, const string prefix) {
    FreeFlyCamera::loadFromXML(pt, prefix);

    const string savePath = xmlSavePath(prefix);

    maxRadius(pt.get(savePath + ".maxRadius", maxRadius()));
    minRadius(pt.get(savePath + ".minRadius", minRadius()));
    curRadius(pt.get(savePath + ".curRadius", curRadius()));
    _cameraRotation.set(
        pt.get(savePath + ".cameraRotation.<xmlattr>.x", _cameraRotation.x),
        pt.get(savePath + ".cameraRotation.<xmlattr>.y", _cameraRotation.y),
        pt.get(savePath + ".cameraRotation.<xmlattr>.z", _cameraRotation.z)
    );
    _offsetDir.set(
        pt.get(savePath + ".offsetDir.<xmlattr>.x", _offsetDir.x),
        pt.get(savePath + ".offsetDir.<xmlattr>.y", _offsetDir.y),
        pt.get(savePath + ".offsetDir.<xmlattr>.z", _offsetDir.z)
    );
}

}  // namespace Divide