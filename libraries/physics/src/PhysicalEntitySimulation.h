//
//  PhysicalEntitySimulation.h
//  libraries/physics/src
//
//  Created by Andrew Meadows 2015.04.27
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PhysicalEntitySimulation_h
#define hifi_PhysicalEntitySimulation_h

#include <stdint.h>

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include <EntityItem.h>
#include <EntitySimulation.h>

#include "PhysicsEngine.h"
#include "EntityMotionState.h"
#include "GravityZoneCollection.h"

class PhysicalEntitySimulation;
using PhysicalEntitySimulationPointer = std::shared_ptr<PhysicalEntitySimulation>;
using SetOfEntityMotionStates = QSet<EntityMotionState*>;
class GravityZoneAction;

class PhysicalEntitySimulation : public EntitySimulation {
public:
    PhysicalEntitySimulation();
    ~PhysicalEntitySimulation();

    void init(EntityTreePointer tree, PhysicsEnginePointer engine, EntityEditPacketSender* packetSender);

    virtual void addDynamic(EntityDynamicPointer dynamic) override;
    virtual void applyDynamicChanges() override;
    void applyZoneChanges(btDynamicsWorld* world);

    virtual void takeEntitiesToDelete(VectorOfEntities& entitiesToDelete) override;

protected: // only called by EntitySimulation
    // overrides for EntitySimulation
    virtual void updateEntitiesInternal(const quint64& now) override;
    virtual void addEntityInternal(EntityItemPointer entity) override;
    virtual void removeEntityInternal(EntityItemPointer entity) override;
    virtual void changeEntityInternal(EntityItemPointer entity) override;
    virtual void clearEntitiesInternal() override;

public:
    virtual void prepareEntityForDelete(EntityItemPointer entity) override;

    void getObjectsToRemoveFromPhysics(VectorOfMotionStates& result);
    void deleteObjectsRemovedFromPhysics();
    void getObjectsToAddToPhysics(VectorOfMotionStates& result);
    void setObjectsToChange(const VectorOfMotionStates& objectsToChange);
    void getObjectsToChange(VectorOfMotionStates& result);

    void handleDeactivatedMotionStates(const VectorOfMotionStates& motionStates);
    void handleChangedMotionStates(const VectorOfMotionStates& motionStates);
    void handleCollisionEvents(const CollisionEvents& collisionEvents);

    EntityEditPacketSender* getPacketSender() { return _entityPacketSender; }

    // thread-safe
    glm::vec3 getUpDirectionAtPosition(const glm::vec3& position) const;

    // thread-safe
    glm::vec3 getGravityAtPosition(const glm::vec3& position) const;

protected:
    void queueZoneUpdateTransaction(const EntityItemID& entityItemID, const ZonePhysicsActionProperties zoneActionProperties);
    void queueZoneRemoveTransaction(const EntityItemID& entityItemID);

private:
    SetOfEntities _entitiesToRemoveFromPhysics;
    SetOfEntities _entitiesToRelease;
    SetOfEntities _entitiesToAddToPhysics;

    // used to queue up change transactions to the zoneActionMap.
    struct ZoneUpdateTransaction {
        enum CommandType { Update, Remove };
        ZoneUpdateTransaction(CommandType commandTypeIn, const EntityItemID& entityItemIDIn) : commandType(commandTypeIn), entityItemID(entityItemIDIn) {
            zoneActionProperties.type = ZonePhysicsActionProperties::None;
        }
        ZoneUpdateTransaction(CommandType commandTypeIn, const EntityItemID& entityItemIDIn,
                              const ZonePhysicsActionProperties& zoneActionPropertiesIn) :
            commandType(commandTypeIn), entityItemID(entityItemIDIn), zoneActionProperties(zoneActionPropertiesIn) {}
        CommandType commandType;
        EntityItemID entityItemID;
        ZonePhysicsActionProperties zoneActionProperties;
    };
    std::mutex _zoneUpdateMutex;  // guards the zoneUpdateTransactions queue from multithreaded access
    std::vector<ZoneUpdateTransaction> _zoneUpdateTransactions;  // queue of changes
    std::map<EntityItemID, std::unique_ptr<GravityZoneAction>> _zoneActionMap;

    SetOfEntityMotionStates _pendingChanges; // EntityMotionStates already in PhysicsEngine that need their physics changed
    SetOfEntityMotionStates _outgoingChanges; // EntityMotionStates for which we may need to send updates to entity-server

    SetOfMotionStates _physicalObjects; // MotionStates of entities in PhysicsEngine

    PhysicsEnginePointer _physicsEngine = nullptr;
    EntityEditPacketSender* _entityPacketSender = nullptr;
    mutable std::mutex _gravityZoneCollectionMutex;  // guards the gravityZoneCollection from multithreaded access
    GravityZoneCollection _gravityZoneCollection;

    uint32_t _lastStepSendPackets { 0 };
};


typedef std::shared_ptr<PhysicalEntitySimulation> PhysicalEntitySimulationPointer;

#endif // hifi_PhysicalEntitySimulation_h
