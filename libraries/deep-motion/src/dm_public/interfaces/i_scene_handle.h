#pragma once
#include "dm_public/non_copyable.h"
#include "dm_public/i_array_interface.h"
#include "dm_public/object_definitions/rigid_body_definition.h"
#include <memory>

namespace avatar
{
    class ISceneObjectHandle;
    class IRigidBodyHandle;

    class ISceneHandle : public NonCopyable
    {
    public:
        virtual ~ISceneHandle() {}

        virtual float GetSceneTime() const = 0;
        virtual void GetSceneObjects(IArrayInterface<ISceneObjectHandle*>& sceneObjectsOut) = 0;
        virtual bool DeleteSceneObject(ISceneObjectHandle*) = 0;
        virtual IRigidBodyHandle* AddNewRigidBody(const char* name, const RigidBodyDefinition& objectDefinition) = 0;
    };
}