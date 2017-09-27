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
#include "ObjectMotionState.h"

class GravityZoneMotionState : public ObjectMotionState {
};

GravityZoneAction::GravityZoneAction() {
    _ghost.setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
}

GravityZoneAction::~GravityZoneAction() {
    removeFromWorld();
    _ghost.setCollisionShape(nullptr);
    _box.reset(nullptr);
}

void GravityZoneAction::setCollisionWorld(btCollisionWorld* world) {
    if (world != _world) {
        removeFromWorld();
        _world = world;
        addToWorld();
    }
}

void GravityZoneAction::setShapeFromZoneEntity(const glm::vec3& position, const glm::quat& rotation,
                                               const glm::vec3& dimensions, const glm::vec3& registrationPoint) {

    btVector3 btPosition = glmToBullet(position);
    btQuaternion btRotation = glmToBullet(rotation);
    btVector3 btDimensions = glmToBullet(dimensions);
    btVector3 btRegistrationPoint = glmToBullet(registrationPoint);

    _box.reset(new btBoxShape(btDimensions * 0.5f));
    const btVector3 ONE_HALF(0.5f, 0.5f, 0.5f);
    btVector3 registrationOffset = rotateVector(btRotation, (ONE_HALF - btRegistrationPoint) * btDimensions);
    _ghost.setWorldTransform(btTransform(btRotation, btPosition + registrationOffset));
    _ghost.setCollisionShape(_box.get());
}

void GravityZoneAction::updateAction(btCollisionWorld* collisionWorld, btScalar deltaTimeStep) {
    qDebug() << "AJT: GravityZoneAction::updateAction(), " << _ghost.getNumOverlappingObjects() << " objects";

    btTransform invGhostTransform = _ghost.getWorldTransform().inverse();

    btVector3 Y_AXIS(0.0f, 1.0f, 0.0f);
    for (int i = 0; i < _ghost.getNumOverlappingObjects(); i++) {
        btCollisionObject* obj = _ghost.getOverlappingObject(i);
        ObjectMotionState* motionState = static_cast<ObjectMotionState*>(obj->getUserPointer());
        if (motionState && motionState->getType() == MOTIONSTATE_TYPE_ENTITY) {
            btVector3 entityPosition = glmToBullet(motionState->getObjectPosition());
            btVector3 localEntityPosition = invGhostTransform(entityPosition);
            if (_box->isInside(localEntityPosition, 0.0001f)) {
                btRigidBody* body = motionState->getRigidBody();
                if (body) {
                    btVector3 entityGravity = glmToBullet(motionState->getObjectGravity());
                    float signedMagnitude = entityGravity.dot(Y_AXIS);
                    btVector3 newGravity = entityPosition.normalize();
                    newGravity *= btVector3(signedMagnitude, signedMagnitude, signedMagnitude);
                    body->setGravity(newGravity);
                }
            }
        }
    }
}

void GravityZoneAction::debugDraw(btIDebugDraw* debugDrawer) {
}

void GravityZoneAction::addToWorld() {
    if (_world && !_inWorld) {
        const int16_t GRAVITY_ZONE_GHOST_GROUP = -1;
        const int16_t GRAVITY_ZONE_GHOST_MASK = -1;
        _world->addCollisionObject(&_ghost, GRAVITY_ZONE_GHOST_GROUP, GRAVITY_ZONE_GHOST_MASK);
        _inWorld = true;
    }
}

void GravityZoneAction::removeFromWorld() {
    if (_world && _inWorld) {
        _world->removeCollisionObject(&_ghost);
        _inWorld = false;
    }
}


