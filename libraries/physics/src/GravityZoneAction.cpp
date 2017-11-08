//
//  GravityZoneAction.h
//  libraries/physics/src
//
//  Created by Anthony Thibault 2017-09-27
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "GravityZoneAction.h"

#include <QDebug>
#include "BulletUtil.h"
#include "EntityMotionState.h"
#include "GLMHelpers.h"

#ifdef WANT_DEBUG_DRAW
#include <DebugDraw.h>
#endif

void AddRemovePairGhostObject::addOverlappingObjectInternal(btBroadphaseProxy* otherProxy, btBroadphaseProxy* thisProxy) {
    btGhostObject::addOverlappingObjectInternal(otherProxy, thisProxy);

    if (otherProxy) {
        btCollisionObject* obj = static_cast<btCollisionObject*>(otherProxy->m_clientObject);
        if (obj && obj->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
            ObjectMotionState* motionState = static_cast<ObjectMotionState*>(obj->getUserPointer());
            if (motionState && motionState->getType() == MOTIONSTATE_TYPE_ENTITY) {
                EntityMotionState* entityMotionState = static_cast<EntityMotionState*>(motionState);
                entityMotionState->incrementGravityZoneOverlapCount();
                entityMotionState->resetGravityZoneProperties();
            }
        }
    }
}

void AddRemovePairGhostObject::removeOverlappingObjectInternal(btBroadphaseProxy* otherProxy, btDispatcher* dispatcher, btBroadphaseProxy* thisProxy) {
    btGhostObject::removeOverlappingObjectInternal(otherProxy, dispatcher, thisProxy);

    if (otherProxy) {
        btCollisionObject* obj = static_cast<btCollisionObject*>(otherProxy->m_clientObject);
        if (obj && obj->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
            btRigidBody* body = static_cast<btRigidBody*>(obj);
            ObjectMotionState* motionState = static_cast<ObjectMotionState*>(obj->getUserPointer());
            if (motionState && motionState->getType() == MOTIONSTATE_TYPE_ENTITY) {
                EntityMotionState* entityMotionState = static_cast<EntityMotionState*>(motionState);
                if (entityMotionState->decrementGravityZoneOverlapCount()) {
                    // if we are leaving the last zone, set gravity back to it's original entity value.
                    btVector3 entityGravity = glmToBullet(motionState->getObjectGravity());
                    body->setGravity(entityGravity);
                    entityMotionState->resetGravityZoneProperties();
                }
            }
        }
    }
}

GravityZoneAction::GravityZoneAction(const ZonePhysicsActionProperties& zoneActionProperties, btDynamicsWorld* world) {
    _ghost.setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
    updateProperties(zoneActionProperties);
    _world = world;
    const int16_t GRAVITY_ZONE_GHOST_GROUP = BULLET_COLLISION_GROUP_DYNAMIC;
    const int16_t GRAVITY_ZONE_GHOST_MASK = BULLET_COLLISION_GROUP_DYNAMIC;
    _world->addCollisionObject(&_ghost, GRAVITY_ZONE_GHOST_GROUP, GRAVITY_ZONE_GHOST_MASK);
    _world->addAction(this);
}

GravityZoneAction::~GravityZoneAction() {
    _world->removeAction(this);
    _world->removeCollisionObject(&_ghost);
    _ghost.setCollisionShape(nullptr);
    _box.reset(nullptr);
}

void GravityZoneAction::updateProperties(const ZonePhysicsActionProperties& zoneActionProperties) {
    assert(zoneActionProperties.type != ZonePhysicsActionProperties::None);

    // AJT: TODO: detect "hard" and easy changes
    glm::vec3 localZoneCenter = (zoneActionProperties.oobbMax + zoneActionProperties.oobbMin) * 0.5f;
    glm::vec3 worldZoneCenter = zoneActionProperties.localToWorldRotation * localZoneCenter + zoneActionProperties.localToWorldTranslation;
    btVector3 btPosition = glmToBullet(worldZoneCenter);
    btQuaternion btRotation = glmToBullet(zoneActionProperties.localToWorldRotation);
    btTransform ghostTransform(btRotation, btPosition);

    btVector3 halfExtents = glmToBullet((zoneActionProperties.oobbMax - zoneActionProperties.oobbMin) * 0.5f);
    _box.reset(new btBoxShape(halfExtents));
    _ghost.setWorldTransform(ghostTransform);
    _ghost.setCollisionShape(_box.get());
    _zoneActionProperties = zoneActionProperties;
}

void GravityZoneAction::updateAction(btCollisionWorld* collisionWorld, btScalar deltaTimeStep) {

#ifdef WANT_DEBUG_DRAW
    const glm::vec4 WHITE(1.0f);
    const glm::vec4 GREEN(0.0f, 1.0f, 0.0f, 1.0f);

    glm::quat rot = _zoneActionProperties.localToWorldRotation;
    glm::vec3 pos = _zoneActionProperties.localToWorldTranslation;
    DebugDraw::getInstance().addMarker(QString("zone%1").arg((uint64_t)this), rot, pos, WHITE);
    glm::vec3 oobbMin = _zoneActionProperties.oobbMin;
    glm::vec3 oobbMax = _zoneActionProperties.oobbMax;

    // transform corners of oobb into world space
    const int NUM_BOX_CORNERS = 8;
    glm::vec3 corners[NUM_BOX_CORNERS] = {
        rot * glm::vec3(oobbMin.x, oobbMin.y, oobbMin.z) + pos,
        rot * glm::vec3(oobbMin.x, oobbMin.y, oobbMax.z) + pos,
        rot * glm::vec3(oobbMin.x, oobbMax.y, oobbMin.z) + pos,
        rot * glm::vec3(oobbMin.x, oobbMax.y, oobbMax.z) + pos,
        rot * glm::vec3(oobbMax.x, oobbMin.y, oobbMin.z) + pos,
        rot * glm::vec3(oobbMax.x, oobbMin.y, oobbMax.z) + pos,
        rot * glm::vec3(oobbMax.x, oobbMax.y, oobbMin.z) + pos,
        rot * glm::vec3(oobbMax.x, oobbMax.y, oobbMax.z) + pos
    };
    const int NUM_INDICES = 24;
    const int indices[NUM_INDICES] = {0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7, 7, 6, 6, 4, 0, 4, 1, 5, 2, 6, 3, 7};

    for (int i = 0; i < NUM_INDICES; i += 2) {
        DebugDraw::getInstance().drawRay(corners[indices[i]], corners[indices[i+1]], GREEN);
    }
#endif

    btVector3 Y_AXIS(0.0f, 1.0f, 0.0f);
    for (int i = 0; i < _ghost.getNumOverlappingObjects(); i++) {
        btCollisionObject* obj = _ghost.getOverlappingObject(i);
        ObjectMotionState* motionState = static_cast<ObjectMotionState*>(obj->getUserPointer());
        if (motionState && motionState->getType() == MOTIONSTATE_TYPE_ENTITY) {
            EntityMotionState* entityMotionState = static_cast<EntityMotionState*>(motionState);
            glm::vec3 entityPosition = motionState->getObjectPosition();
            if (_zoneActionProperties.volume < entityMotionState->getGravityZoneVolume() && _zoneActionProperties.containsNoAABBCheck(entityPosition)) {
                btRigidBody* body = motionState->getRigidBody();
                if (body) {
                    btVector3 entityGravity = glmToBullet(motionState->getObjectGravity());
                    float signedMagnitude = entityGravity.dot(Y_AXIS);
                    btVector3 zoneGravity = glmToBullet(_zoneActionProperties.getGravityAtPosition(entityPosition) * signedMagnitude);
                    entityMotionState->setGravityZoneGravityAndVolume(zoneGravity, _zoneActionProperties.volume);
                }
            }
            if (entityMotionState->incrementGravityZoneUpdateCount()) {
                // we are the last overlapping zone.
                btRigidBody* body = entityMotionState->getRigidBody();
                if (body) {
                    if (entityMotionState->isGravityZoneSet()) {
                        body->setGravity(entityMotionState->getGravityZoneGravity());
                    } else {
                        body->setGravity(glmToBullet(entityMotionState->getObjectGravity()));
                    }
                }
                entityMotionState->resetGravityZoneProperties();
            }
        }
    }
}

void GravityZoneAction::debugDraw(btIDebugDraw* debugDrawer) {
}


