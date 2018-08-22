#pragma once
#include "dm_public/interfaces/i_scene_object_handle.h"

#include <memory>

#include "dm_public/i_array_interface.h"
#include "dm_public/opaque_handle.h"
#include "dm_public/types.h"

namespace avatar
{
    class IColliderHandle;

    class IMultiBodyHandle : public ISceneObjectHandle
    {
    public:
        typedef OpaqueHandle<const void*, Detail::HandleTypes::MULTI_BODY_LINK_HANDLE> LinkHandle;

        virtual Transform GetTransform() const = 0;
        virtual void GetLinkHandles(IArrayInterface<LinkHandle>& handlesOut) = 0;
        virtual const char* GetLinkName(LinkHandle) const = 0;
        virtual const char* GetTargetBoneName(LinkHandle) const = 0;
        virtual Transform GetLinkTransform(LinkHandle) const = 0;
        virtual std::unique_ptr<IColliderHandle> GetLinkCollider(LinkHandle) = 0;
    };
}