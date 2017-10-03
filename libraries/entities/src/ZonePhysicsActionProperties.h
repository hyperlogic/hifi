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

struct ZonePhysicsActionProperties {
    enum Type { None, Spherical, Planetoid, Linear };

    struct SphericalGravityZone {
        float gforce;
    };

    struct PlanetoidGravityZone {
        float gforce;
        float radius;
    };

    struct LinearGravityZone {
        float gforce;
        float up[3];
    };

    union ZoneUnion {
        SphericalGravityZone spherical;
        PlanetoidGravityZone planetoid;
        LinearGravityZone linear;
    };

    Type type;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 dimensions;
    glm::vec3 registrationPoint;
    ZoneUnion d;
};

#endif // hifi_ZonePhysicsActionProperties_h
