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

void AddRemovePairGhostObject::addOverlappingObjectInternal(btBroadphaseProxy* otherProxy, btBroadphaseProxy* thisProxy) {
    btGhostObject::addOverlappingObjectInternal(otherProxy, thisProxy);

    if (otherProxy) {
        btCollisionObject* obj = static_cast<btCollisionObject*>(otherProxy->m_clientObject);
        if (obj && obj->getInternalType() == btCollisionObject::CO_RIGID_BODY) {
            btRigidBody* body = static_cast<btRigidBody*>(obj);
            ObjectMotionState* motionState = static_cast<ObjectMotionState*>(obj->getUserPointer());
            if (motionState && motionState->getType() == MOTIONSTATE_TYPE_ENTITY) {
                EntityMotionState* entityMotionState = static_cast<EntityMotionState*>(motionState);
                entityMotionState->incrementGravityZoneOverlapCount();
                entityMotionState->resetGravityZoneAccumulators();
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
                    entityMotionState->resetGravityZoneAccumulators();
                }
            }
        }
    }
}

GravityZoneAction::GravityZoneAction(const ZonePhysicsActionProperties& zoneActionProperties, btDynamicsWorld* world) {
    _ghost.setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
    updateProperties(zoneActionProperties);
    _world = world;
    const int16_t GRAVITY_ZONE_GHOST_GROUP = -1;
    const int16_t GRAVITY_ZONE_GHOST_MASK = -1;
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

    // TODO: detect "hard" and easy changes
    btVector3 btPosition = glmToBullet(zoneActionProperties.position);
    btQuaternion btRotation = glmToBullet(zoneActionProperties.rotation);
    btVector3 btDimensions = glmToBullet(zoneActionProperties.dimensions);
    btVector3 btRegistrationPoint = glmToBullet(zoneActionProperties.registrationPoint);

    _box.reset(new btBoxShape(btDimensions * 0.5f));
    const btVector3 ONE_HALF(0.5f, 0.5f, 0.5f);
    btVector3 registrationOffset = rotateVector(btRotation, (ONE_HALF - btRegistrationPoint) * btDimensions);
    _ghost.setWorldTransform(btTransform(btRotation, btPosition + registrationOffset));
    _ghost.setCollisionShape(_box.get());
    _zoneActionProperties = zoneActionProperties;
}

void GravityZoneAction::updateAction(btCollisionWorld* collisionWorld, btScalar deltaTimeStep) {
    btTransform invGhostTransform = _ghost.getWorldTransform().inverse();

    btVector3 Y_AXIS(0.0f, 1.0f, 0.0f);
    for (int i = 0; i < _ghost.getNumOverlappingObjects(); i++) {
        btCollisionObject* obj = _ghost.getOverlappingObject(i);
        ObjectMotionState* motionState = static_cast<ObjectMotionState*>(obj->getUserPointer());
        if (motionState && motionState->getType() == MOTIONSTATE_TYPE_ENTITY) {
            EntityMotionState* entityMotionState = static_cast<EntityMotionState*>(motionState);
            btVector3 entityPosition = glmToBullet(motionState->getObjectPosition());
            btVector3 localEntityPosition = invGhostTransform(entityPosition);
            if (_box->isInside(localEntityPosition, 0.0001f)) {
                btRigidBody* body = motionState->getRigidBody();
                btVector3 tempVec3;
                if (body) {
                    btVector3 entityGravity = glmToBullet(motionState->getObjectGravity());
                    float signedMagnitude = entityGravity.dot(Y_AXIS);
                    switch (_zoneActionProperties.type) {
                    case ZonePhysicsActionProperties::Linear:
                        entityMotionState->gravityZoneAccumulate(btVector3(
                            signedMagnitude * _zoneActionProperties.d.linear.gforce * _zoneActionProperties.d.linear.up[0],
                            signedMagnitude * _zoneActionProperties.d.linear.gforce * _zoneActionProperties.d.linear.up[1],
                            signedMagnitude * _zoneActionProperties.d.linear.gforce * _zoneActionProperties.d.linear.up[2]));
                        break;
                    case ZonePhysicsActionProperties::Spherical:
                        tempVec3 = entityPosition.normalize();
                        tempVec3 *= signedMagnitude * _zoneActionProperties.d.spherical.gforce;
                        entityMotionState->gravityZoneAccumulate(tempVec3);
                        break;
                    default:
                        break;
                    }
                }
            }
            if (entityMotionState->incrementGravityZoneUpdateCount()) {
                // we are the last overlapping zone.
                btRigidBody* body = entityMotionState->getRigidBody();
                if (body) {
                    if (entityMotionState->hasGravityZoneAccumulated()) {
                        body->setGravity(entityMotionState->getGravityZoneAccumulator());
                    } else {
                        body->setGravity(glmToBullet(entityMotionState->getObjectGravity()));
                    }
                }
                entityMotionState->resetGravityZoneAccumulators();
            }
        }
    }
}

void GravityZoneAction::debugDraw(btIDebugDraw* debugDrawer) {
}


