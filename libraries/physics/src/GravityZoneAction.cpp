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
    btVector3 btPosition = glmToBullet(zoneActionProperties.position);
    btQuaternion btRotation = glmToBullet(zoneActionProperties.rotation);
    btVector3 btDimensions = glmToBullet(zoneActionProperties.dimensions);
    btVector3 btRegistrationPoint = glmToBullet(zoneActionProperties.registrationPoint);

    _box.reset(new btBoxShape(btDimensions * 0.5f));
    _volume = btDimensions.getX() * btDimensions.getY() * btDimensions.getZ();
    const btVector3 ONE_HALF(0.5f, 0.5f, 0.5f);
    btVector3 registrationOffset = rotateVector(btRotation, (ONE_HALF - btRegistrationPoint) * btDimensions);
    btTransform ghostTransform(btRotation, btPosition + registrationOffset);
    _ghost.setWorldTransform(ghostTransform);
    _invGhostTransform = ghostTransform.inverse();
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
            btVector3 entityPosition = glmToBullet(motionState->getObjectPosition());
            if (_volume < entityMotionState->getGravityZoneVolume() && contains(entityPosition)) {
                btRigidBody* body = motionState->getRigidBody();
                btVector3 tempVec3;
                if (body) {
                    btVector3 entityGravity = glmToBullet(motionState->getObjectGravity());
                    float signedMagnitude = entityGravity.dot(Y_AXIS);
                    switch (_zoneActionProperties.type) {
                    case ZonePhysicsActionProperties::Linear:
                        entityMotionState->setGravityZoneGravityAndVolume(btVector3(
                            signedMagnitude * _zoneActionProperties.d.linear.gforce * _zoneActionProperties.d.linear.up[0],
                            signedMagnitude * _zoneActionProperties.d.linear.gforce * _zoneActionProperties.d.linear.up[1],
                            signedMagnitude * _zoneActionProperties.d.linear.gforce * _zoneActionProperties.d.linear.up[2]), _volume);
                        break;
                    case ZonePhysicsActionProperties::Spherical:
                        tempVec3 = (entityPosition - glmToBullet(_zoneActionProperties.position)).normalize();
                        tempVec3 *= signedMagnitude * _zoneActionProperties.d.spherical.gforce;
                        entityMotionState->setGravityZoneGravityAndVolume(tempVec3, _volume);
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

bool GravityZoneAction::contains(const btVector3& point) const {
    btVector3 localEntityPosition = _invGhostTransform(point);
    return _box->isInside(localEntityPosition, 0.0001f);
}

void GravityZoneAction::computeAABB(glm::vec3& aabbMinOut, glm::vec3& aabbMaxOut) const {
    aabbMinOut.x = FLT_MAX;
    aabbMinOut.y = FLT_MAX;
    aabbMinOut.z = FLT_MAX;
    aabbMaxOut.x = -FLT_MAX;
    aabbMaxOut.y = -FLT_MAX;
    aabbMaxOut.z = -FLT_MAX;

    btVector3 vertex;
    for (int i = 0; i < _box->getNumVertices(); ++i) {
        _box->getVertex(i, vertex);
        if (vertex.getX() < aabbMinOut.x) {
            aabbMinOut.x = vertex.getX();
        }
        if (vertex.getX() > aabbMaxOut.x) {
            aabbMaxOut.x = vertex.getX();
        }
        if (vertex.getY() < aabbMinOut.y) {
            aabbMinOut.y = vertex.getY();
        }
        if (vertex.getY() > aabbMaxOut.y) {
            aabbMaxOut.y = vertex.getY();
        }
        if (vertex.getZ() < aabbMinOut.z) {
            aabbMinOut.z = vertex.getZ();
        }
        if (vertex.getZ() > aabbMaxOut.z) {
            aabbMaxOut.z = vertex.getZ();
        }
    }
}

float GravityZoneAction::getVolume() const {
    return _volume;
}

void GravityZoneAction::debugDraw(btIDebugDraw* debugDrawer) {
}


