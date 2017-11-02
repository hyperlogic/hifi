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

#include "GravityZoneManager.h"
#include "GravityZoneAction.h"
#include "BulletUtil.h"

GravityZoneManager::ZoneInfo::ZoneInfo(const glm::vec3& aabbMinIn, const glm::vec3& aabbMaxIn, float volumeIn) :
     aabbMin(aabbMinIn), aabbMax(aabbMaxIn), volume(volumeIn) {
}

bool GravityZoneManager::ZoneInfo::contains(const glm::vec3& point) const {
    if (point.x < aabbMin.x || point.x > aabbMax.x) return false;
    if (point.y < aabbMin.y || point.y > aabbMax.y) return false;
    if (point.z < aabbMin.z || point.z > aabbMax.z) return false;
    return true;
}

GravityZoneManager::GravityZoneManager() {
}

GravityZoneManager::~GravityZoneManager() {
}

const GravityZoneAction* GravityZoneManager::findAction(const glm::vec3& position) const {
    float volume = FLT_MAX;
    const GravityZoneAction* action;
    btVector3 point = glmToBullet(position);
    for (size_t i = 0; i < _zoneInfos.size(); ++i) {
        if (_zoneInfos[i].volume < volume && _zoneInfos[i].contains(position) && _actions[i]->contains(point)) {
            volume = _zoneInfos[i].volume;
            action = _actions[i];
        }
    }
    return action;
}

void GravityZoneManager::updateAction(const GravityZoneAction* action) {
    auto actionIter = std::find(_actions.begin(), _actions.end(), action);

    glm::vec3 aabbMin, aabbMax;
    action->computeAABB(aabbMin, aabbMax);

    if (actionIter != _actions.end()) {
        // doing iterator arithmetic here to find matching iterator in the _zoneInfos vector.
        auto zoneInfoIter = _zoneInfos.begin() + (actionIter - _actions.begin());
        *zoneInfoIter = ZoneInfo(aabbMin, aabbMax, action->getVolume());
    } else {
        _zoneInfos.emplace_back(aabbMin, aabbMax, action->getVolume());
        _actions.push_back(action);
    }
}

void GravityZoneManager::removeAction(const GravityZoneAction* action) {
    auto actionIter = std::find(_actions.begin(), _actions.end(), action);
    if (actionIter != _actions.end()) {
        // erase from _actions vector moving last value over.
        // this is O(1) rathern then O(n) when using std::vector::erase()
        *actionIter = std::move(_actions.back());
        _actions.pop_back();

        // doing iterator arithmetic here to find matching iterator in the _zoneInfos vector.
        auto zoneInfoIter = _zoneInfos.begin() + (actionIter - _actions.begin());
        // O(1) erase
        *zoneInfoIter = std::move(_zoneInfos.back());
        _zoneInfos.pop_back();
    }
}
