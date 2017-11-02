//
//  GravityZoneManager.h
//  libraries/physics/src
//
//  Created by Anthony Thibault 2017-11-1
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_GravityZoneManager_h
#define hifi_GravityZoneManager_h

#include <vector>
#include <glm/glm.hpp>

class GravityZoneAction;

class GravityZoneManager {
public:
    GravityZoneManager();
    virtual ~GravityZoneManager();

    const GravityZoneAction* findAction(const glm::vec3& position) const;
    void updateAction(const GravityZoneAction* action);
    void removeAction(const GravityZoneAction* action);

    struct ZoneInfo {
        ZoneInfo() {}
        ZoneInfo(const glm::vec3& aabbMinIn, const glm::vec3& aabbMaxIn, float volumeIn);
        bool contains(const glm::vec3& point) const;
        glm::vec3 aabbMin;
        glm::vec3 aabbMax;
        float volume;
    };

protected:
    std::vector<ZoneInfo> _zoneInfos;
    std::vector<const GravityZoneAction*> _actions;
};

#endif // hifi_GravityZoneManager_h
