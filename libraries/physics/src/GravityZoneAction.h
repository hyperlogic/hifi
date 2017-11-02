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
//  http://bulletphysics.org/Bullet/BulletFull/classbtActionInterface.html

#ifndef hifi_GravityZoneAction_h
#define hifi_GravityZoneAction_h

#include <memory>
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <ZonePhysicsActionProperties.h>

class AddRemovePairGhostObject : public btGhostObject {
    virtual void addOverlappingObjectInternal(btBroadphaseProxy* otherProxy, btBroadphaseProxy* thisProxy=0) override;
    virtual void removeOverlappingObjectInternal(btBroadphaseProxy* otherProxy, btDispatcher* dispatcher, btBroadphaseProxy* thisProxy=0) override;
};

class GravityZoneAction : public btActionInterface {
public:
    GravityZoneAction(const ZonePhysicsActionProperties& zoneActionProperties, btDynamicsWorld* world);

    virtual ~GravityZoneAction();

    void updateProperties(const ZonePhysicsActionProperties& zoneActionProperties);
    bool contains(const btVector3& point) const;

    void computeAABB(glm::vec3& aabbMinOut, glm::vec3& aabbMaxOut) const;
    float getVolume() const;

    // these are from btActionInterface
    virtual void updateAction(btCollisionWorld* collisionWorld, btScalar deltaTimeStep) override;
    virtual void debugDraw(btIDebugDraw* debugDrawer) override;
protected:

    std::unique_ptr<btBoxShape> _box;
    float _volume { FLT_MAX };
    AddRemovePairGhostObject _ghost;
    btDynamicsWorld* _world { nullptr };
    ZonePhysicsActionProperties _zoneActionProperties;
    btTransform _invGhostTransform;
};

#endif // hifi_GravityZoneAction_h
