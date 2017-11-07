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
    btVector3 btPosition = glmToBullet(zoneActionProperties.localToWorldTranslation);
    btQuaternion btRotation = glmToBullet(zoneActionProperties.localToWorldRotation);
    btTransform ghostTransform(btRotation, btPosition);

    glm::vec3 dims = zoneActionProperties.oobbMax - zoneActionProperties.oobbMin;
    btVector3 btHalfExtents = glmToBullet(dims);
    _box.reset(new btBoxShape(btHalfExtents));
    _ghost.setWorldTransform(ghostTransform);
    _ghost.setCollisionShape(_box.get());
    _zoneActionProperties = zoneActionProperties;
}

void GravityZoneAction::updateAction(btCollisionWorld* collisionWorld, btScalar deltaTimeStep) {
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


