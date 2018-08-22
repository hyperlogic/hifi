#pragma once
#include "dm_public/non_copyable.h"

namespace avatar
{
    class ISceneObjectHandle : public NonCopyable
    {
    public:
        virtual const char* GetName() const = 0;
    };
}