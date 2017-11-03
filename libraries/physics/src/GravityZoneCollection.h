//
//  GravityZoneCollection.h
//  libraries/physics/src
//
//  Created by Anthony Thibault 2017-11-1
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_GravityZoneCollection_h
#define hifi_GravityZoneCollection_h

#include <vector>
#include <glm/glm.hpp>
#include "ZonePhysicsActionProperties.h"

class GravityZoneAction;

class GravityZoneCollection {
public:
    GravityZoneCollection();
    virtual ~GravityZoneCollection();

    void updateZoneAction(const GravityZoneAction* action);
    void removeZoneAction(const GravityZoneAction* action);

    glm::vec3 getUpDirectionAtPosition(const glm::vec3& position) const;
    glm::vec3 getGravityAtPosition(const glm::vec3& position) const;

protected:
    struct ZoneInfo {
        ZoneInfo();
        ZoneInfo(void* keyIn, const ZonePhysicsActionProperties& propertiesIn) :
            key(keyIn), properties(propertiesIn) {}
        void* key;
        ZonePhysicsActionProperties properties;
    };

    const ZoneInfo* findZoneInfo(const glm::vec3& position) const;

    std::vector<ZoneInfo> _zoneInfos;
};

#endif // hifi_GravityZoneCollection_h
