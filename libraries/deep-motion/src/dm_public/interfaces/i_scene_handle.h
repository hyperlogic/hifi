#pragma once
#include "dm_public/non_copyable.h"
#include "dm_public/i_array_interface.h"
#include <memory>

namespace avatar
{
    class ISceneObjectHandle;

    class ISceneHandle : public NonCopyable
    {
    public:
        virtual ~ISceneHandle() {}

        virtual float GetSceneTime() const = 0;
        virtual void GetSceneObjects(IArrayInterface<ISceneObjectHandle*>& sceneObjectsOut) = 0;
    };
}