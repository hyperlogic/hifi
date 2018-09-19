#pragma once
#include <memory>

#include "dm_public/interfaces/i_scene_object_handle.h"
#include "dm_public/types.h"

namespace avatar
{
    class IColliderHandle;

    class IRigidBodyHandle : public ISceneObjectHandle
    {
    public:
        virtual Transform GetTransform() const = 0;
        virtual IColliderHandle* GetCollider() = 0;
        virtual bool GetIsKinematic() const = 0;
        virtual void SetIsKinematic(bool) = 0;
    };
}
