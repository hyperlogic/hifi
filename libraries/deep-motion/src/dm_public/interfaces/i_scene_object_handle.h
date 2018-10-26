#pragma once
#include "dm_public/non_copyable.h"

namespace avatar
{
    class ISceneObjectHandle : public NonCopyable
    {
    public:
        virtual ~ISceneObjectHandle() {}
        enum class ObjectType
        {
            RigidBody,
            Articulation,
            Controller
        };
        virtual const char* GetName() const = 0;
        virtual ObjectType GetType() const = 0;
    };
}