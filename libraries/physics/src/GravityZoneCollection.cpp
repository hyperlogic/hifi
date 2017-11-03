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

#include "GravityZoneCollection.h"
#include "GravityZoneAction.h"
#include "GLMHelpers.h"

GravityZoneCollection::GravityZoneCollection() {
}

GravityZoneCollection::~GravityZoneCollection() {
}

void GravityZoneCollection::updateZoneAction(const GravityZoneAction* action) {
    void* key = (void*)action;
    auto zoneInfoIter = std::find_if(_zoneInfos.begin(), _zoneInfos.end(), [key](const ZoneInfo& zoneInfo) {
        return zoneInfo.key == key;
    });

    if (zoneInfoIter != _zoneInfos.end()) {
        // doing iterator arithmetic here to find matching iterator in the _zoneInfos vector.
        zoneInfoIter->key = key;
        zoneInfoIter->properties = action->getZonePhysicsActionProperties();
    } else {
        _zoneInfos.emplace_back(key, action->getZonePhysicsActionProperties());
    }
}

void GravityZoneCollection::removeZoneAction(const GravityZoneAction* action) {
    void* key = (void*)action;
    auto zoneInfoIter = std::find_if(_zoneInfos.begin(), _zoneInfos.end(), [key](const ZoneInfo& zoneInfo) {
        return zoneInfo.key == key;
    });

    if (zoneInfoIter != _zoneInfos.end()) {
        // erase from _zoneInfo vector moving last value over.
        // this is O(1) rathern then O(n) like std::vector::erase()
        *zoneInfoIter = std::move(_zoneInfos.back());
        _zoneInfos.pop_back();
    }
}

glm::vec3 GravityZoneCollection::getUpDirectionAtPosition(const glm::vec3& position) const {
    const ZoneInfo* zoneInfo = findZoneInfo(position);
    if (zoneInfo) {
        return zoneInfo->properties.getUpDirectionAtPosition(position);
    } else {
        return GRAVITY_ZONE_DEFAULT_UP;
    }
}

glm::vec3 GravityZoneCollection::getGravityAtPosition(const glm::vec3& position) const {
    const ZoneInfo* zoneInfo = findZoneInfo(position);
    if (zoneInfo) {
        return zoneInfo->properties.getGravityAtPosition(position);
    } else {
        return GRAVITY_ZONE_DEFAULT_UP * GRAVITY_ZONE_DEFAULT_MAGNITUDE;
    }
}

const GravityZoneCollection::ZoneInfo* GravityZoneCollection::findZoneInfo(const glm::vec3& position) const {
    float smallestVolume = FLT_MAX;
    auto bestIter = _zoneInfos.end();
    for (auto iter = _zoneInfos.begin(); iter != _zoneInfos.end(); ++iter) {
        if (iter->properties.volume < smallestVolume && iter->properties.contains(position)) {
            smallestVolume = iter->properties.volume;
            bestIter = iter;
        }
    }
    if (bestIter != _zoneInfos.end()) {
        return &*bestIter;
    } else {
        return nullptr;
    }
}
