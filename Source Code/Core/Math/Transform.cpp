#include "stdafx.h"

#include "Headers/Transform.h"

namespace Divide {

Transform::Transform(const Quaternion<F32>& orientation, const vec3<F32>& translation, const vec3<F32>& scale) noexcept
{
    _scale.set(scale);
    _translation.set(translation);
    _orientation.set(orientation);
}

void Transform::setTransforms(const mat4<F32>& transform) {
    vec3<F32> tempEuler = VECTOR3_ZERO;
    if (Util::decomposeMatrix(transform, _translation, _scale, tempEuler)) {
        _orientation.fromEuler(-Angle::to_DEGREES(tempEuler));
    }
}

void Transform::identity() noexcept {
    _scale.set(VECTOR3_UNIT);
    _translation.set(VECTOR3_ZERO);
    _orientation.identity();
}

}  // namespace Divide