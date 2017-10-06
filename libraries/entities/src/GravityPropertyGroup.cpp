//
//  GravityPropertyGroup.cpp
//  libraries/entities/src
//
//  Created by Anthony Thibault on October 5 2017.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <OctreePacketData.h>

#include "GravityPropertyGroup.h"
#include "EntityItemProperties.h"

const uint8_t GravityPropertyGroup::DEFAULT_GRAVITY_G_TYPE = 0;
const float GravityPropertyGroup::DEFAULT_GRAVITY_G_FORCE = 1.0f;
const glm::vec3 GravityPropertyGroup::DEFAULT_GRAVITY_UP = glm::vec3(0.0f, 1.0f, 0.0f);

void GravityPropertyGroup::copyToScriptValue(const EntityPropertyFlags& desiredProperties, QScriptValue& properties, QScriptEngine* engine, bool skipDefaults, EntityItemProperties& defaultEntityProperties) const {
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_GRAVITY_G_TYPE, GravityPropertyGroup, gravityPropertyGroup, GType, gType);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_GRAVITY_G_FORCE, GravityPropertyGroup, gravityPropertyGroup, GForce, gForce);
    COPY_GROUP_PROPERTY_TO_QSCRIPTVALUE(PROP_GRAVITY_UP, GravityPropertyGroup, gravityPropertyGroup, Up, up);
}

void GravityPropertyGroup::copyFromScriptValue(const QScriptValue& object, bool& _defaultSettings) {

    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(gravityPropertyGroup, gType, uint8_t, setGType);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(gravityPropertyGroup, gForce, float, setGForce);
    COPY_GROUP_PROPERTY_FROM_QSCRIPTVALUE(gravityPropertyGroup, up, glmVec3, setUp);
}

void GravityPropertyGroup::merge(const GravityPropertyGroup& other) {
    COPY_PROPERTY_IF_CHANGED(gType);
    COPY_PROPERTY_IF_CHANGED(gForce);
    COPY_PROPERTY_IF_CHANGED(up);
}


void GravityPropertyGroup::debugDump() const {
    qCDebug(entities) << "   GravityPropertyGroup: ---------------------------------------------";
    qCDebug(entities) << "     _type:" << (int)_gType;
    qCDebug(entities) << "   _gForce:" << _gForce;
    qCDebug(entities) << "       _up:" << _up;
}

void GravityPropertyGroup::listChangedProperties(QList<QString>& out) {
    if (gTypeChanged()) {
        out << "gravity-gType";
    }
    if (gForceChanged()) {
        out << "gravity-gForce";
    }
    if (upChanged()) {
        out << "gravity-up";
    }
}

bool GravityPropertyGroup::appendToEditPacket(OctreePacketData* packetData,
                                              EntityPropertyFlags& requestedProperties,
                                              EntityPropertyFlags& propertyFlags,
                                              EntityPropertyFlags& propertiesDidntFit,
                                              int& propertyCount,
                                              OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_GRAVITY_G_TYPE, getGType());
    APPEND_ENTITY_PROPERTY(PROP_GRAVITY_G_FORCE, getGForce());
    APPEND_ENTITY_PROPERTY(PROP_GRAVITY_UP, getUp());

    return true;
}


bool GravityPropertyGroup::decodeFromEditPacket(EntityPropertyFlags& propertyFlags, const unsigned char*& dataAt , int& processedBytes) {

    int bytesRead = 0;
    bool overwriteLocalData = true;
    bool somethingChanged = false;

    READ_ENTITY_PROPERTY(PROP_GRAVITY_G_TYPE, uint8_t, setGType);
    READ_ENTITY_PROPERTY(PROP_GRAVITY_G_FORCE, float, setGForce);
    READ_ENTITY_PROPERTY(PROP_GRAVITY_UP, glm::vec3, setUp);

    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_GRAVITY_G_TYPE, GType);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_GRAVITY_G_FORCE, GForce);
    DECODE_GROUP_PROPERTY_HAS_CHANGED(PROP_GRAVITY_UP, Up);

    processedBytes += bytesRead;

    Q_UNUSED(somethingChanged);

    return true;
}

void GravityPropertyGroup::markAllChanged() {
    _gTypeChanged = true;
    _gForceChanged = true;
    _upChanged = true;
}

EntityPropertyFlags GravityPropertyGroup::getChangedProperties() const {
    EntityPropertyFlags changedProperties;

    CHECK_PROPERTY_CHANGE(PROP_GRAVITY_G_TYPE, gType);
    CHECK_PROPERTY_CHANGE(PROP_GRAVITY_G_FORCE, gForce);
    CHECK_PROPERTY_CHANGE(PROP_GRAVITY_UP, up);

    return changedProperties;
}

void GravityPropertyGroup::getProperties(EntityItemProperties& properties) const {
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(GravityPropertyGroup, GType, getGType);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(GravityPropertyGroup, GForce, getGForce);
    COPY_ENTITY_GROUP_PROPERTY_TO_PROPERTIES(GravityPropertyGroup, Up, getUp);
}

bool GravityPropertyGroup::setProperties(const EntityItemProperties& properties) {
    bool somethingChanged = false;

    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(GravityPropertyGroup, GType, gType, setGType);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(GravityPropertyGroup, GForce, gForce, setGForce);
    SET_ENTITY_GROUP_PROPERTY_FROM_PROPERTIES(GravityPropertyGroup, Up, up, setUp);

    return somethingChanged;
}

EntityPropertyFlags GravityPropertyGroup::getEntityProperties(EncodeBitstreamParams& params) const {
    EntityPropertyFlags requestedProperties;

    requestedProperties += PROP_GRAVITY_G_TYPE;
    requestedProperties += PROP_GRAVITY_G_FORCE;
    requestedProperties += PROP_GRAVITY_UP;

    return requestedProperties;
}

void GravityPropertyGroup::appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                              EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                              EntityPropertyFlags& requestedProperties,
                                              EntityPropertyFlags& propertyFlags,
                                              EntityPropertyFlags& propertiesDidntFit,
                                              int& propertyCount,
                                              OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;

    APPEND_ENTITY_PROPERTY(PROP_GRAVITY_G_TYPE, getGType());
    APPEND_ENTITY_PROPERTY(PROP_GRAVITY_G_FORCE, getGForce());
    APPEND_ENTITY_PROPERTY(PROP_GRAVITY_UP, getUp());
}

int GravityPropertyGroup::readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead,
                                                           ReadBitstreamToTreeParams& args,
                                                           EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                                           bool& somethingChanged) {

    int bytesRead = 0;
    const unsigned char* dataAt = data;

    READ_ENTITY_PROPERTY(PROP_GRAVITY_G_TYPE, uint8_t, setGType);
    READ_ENTITY_PROPERTY(PROP_GRAVITY_G_FORCE, float, setGForce);
    READ_ENTITY_PROPERTY(PROP_GRAVITY_UP, glm::vec3, setUp);

    return bytesRead;
}
