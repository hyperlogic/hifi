//
//  ZonePhysicsActionProperties.h
//  libraries/physics/src
//
//  Created by Anthony Thibault 2017-09-27
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ZonePhysicsActionProperties_h
#define hifi_ZonePhysicsActionProperties_h

#include "GLMHelpers.h"

const glm::vec3 GRAVITY_ZONE_DEFAULT_UP = IDENTITY_UP;
const float GRAVITY_ZONE_DEFAULT_MAGNITUDE = -9.80665f; // m/s^2

struct ZonePhysicsActionProperties {
    enum Type { None = 0, Spherical, Linear };

    struct SphericalGravityZone {
        float gforce;
    };

    struct LinearGravityZone {
        float gforce;
        float up[3];
    };

    union ZoneUnion {
        SphericalGravityZone spherical;
        LinearGravityZone linear;
    };

    glm::vec3 getGravityAtPosition(const glm::vec3& position) const {
        const float EPSILON = 0.0001f;
        switch (type) {
        case ZonePhysicsActionProperties::None:
        default:
            return GRAVITY_ZONE_DEFAULT_UP * GRAVITY_ZONE_DEFAULT_MAGNITUDE;
        case ZonePhysicsActionProperties::Spherical: {
            glm::vec3 delta = position - localToWorldTranslation;
            float len = glm::length(delta);
            if (len > EPSILON) {
                return delta * (d.spherical.gforce / len);
            } else {
                return GRAVITY_ZONE_DEFAULT_UP * d.spherical.gforce;
            }
        }
        case ZonePhysicsActionProperties::Linear:
            return glm::vec3(d.linear.up[0],
                             d.linear.up[1],
                             d.linear.up[2]) * d.linear.gforce;
        }
    }

    glm::vec3 getUpDirectionAtPosition(const glm::vec3& position) const {
        const float EPSILON = 0.0001f;
        switch (type) {
        case ZonePhysicsActionProperties::None:
        default:
            return GRAVITY_ZONE_DEFAULT_UP;
        case ZonePhysicsActionProperties::Spherical: {
            glm::vec3 delta = position - localToWorldTranslation;
            float len = glm::length(delta);
            if (len > EPSILON) {
                return delta / len;
            } else {
                return GRAVITY_ZONE_DEFAULT_UP;
            }
        }
        case ZonePhysicsActionProperties::Linear:
            return glm::vec3(d.linear.up[0], d.linear.up[1], d.linear.up[2]);
        }
    }

    bool contains(const glm::vec3& point) const {
        // reject point if it is outside of aabb.
        if (point.x < aabbMin.x || point.x > aabbMax.x) {
            return false;
        }
        if (point.y < aabbMin.y || point.y > aabbMax.y) {
            return false;
        }
        if (point.z < aabbMin.z || point.z > aabbMax.z) {
            return false;
        }

        return containsNoAABBCheck(point);
    }

    bool containsNoAABBCheck(const glm::vec3& point) const {
        // transform point local coordinate space of oobb
        glm::vec3 localPoint = worldToLocalRotation * point + worldToLocalTranslation;

        // reject localPoint if it is outside of oobb.
        if (localPoint.x < oobbMin.x || localPoint.x > oobbMax.x) {
            return false;
        }
        if (localPoint.y < oobbMin.y || localPoint.y > oobbMax.y) {
            return false;
        }
        if (localPoint.z < oobbMin.z || localPoint.z > oobbMax.z) {
            return false;
        }

        return true;
    }

    ZoneUnion d;
    glm::quat localToWorldRotation;
    glm::quat worldToLocalRotation;
    glm::vec3 localToWorldTranslation;
    glm::vec3 worldToLocalTranslation;
    glm::vec3 aabbMin;  // world space bb
    glm::vec3 aabbMax;
    glm::vec3 oobbMin;  // local space bb
    glm::vec3 oobbMax;
    float volume;
    Type type;
};

#endif // hifi_ZonePhysicsActionProperties_h
