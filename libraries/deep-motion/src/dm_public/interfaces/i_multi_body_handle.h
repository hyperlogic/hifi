#pragma once
#include "dm_public/interfaces/i_scene_object_handle.h"

#include <memory>
#include <vector>
#include <string>

#include "dm_public/i_array_interface.h"
#include "dm_public/opaque_handle.h"
#include "dm_public/types.h"

namespace avatar
{
    class IColliderHandle;

    struct MultiBodyJointInfo
    {
        std::string m_Name;
        Transform m_Transform;
    };
    struct MultiBodyTransformInfo
    {
        Transform m_Transform;
        std::vector<MultiBodyJointInfo> m_JointInfos;
    };

    class IMultiBodyHandle : public ISceneObjectHandle
    {
    public:
        typedef OpaqueHandle<const void*, Detail::HandleTypes::MULTI_BODY_LINK_HANDLE> LinkHandle;

        virtual Transform GetTransform() const = 0;
        virtual bool SetTransform(const Transform&) = 0;
        virtual void GetLinkHandles(IArrayInterface<LinkHandle>& handlesOut) = 0;
        virtual void GetLinkHierarchy(IArrayInterface<uint32_t>& parentIndicesOut) const = 0;
        virtual const char* GetLinkName(LinkHandle) const = 0;
        virtual const char* GetTargetBoneName(LinkHandle) const = 0;
        virtual Transform GetLinkTransform(LinkHandle) const = 0;
        virtual IColliderHandle* GetLinkCollider(LinkHandle) = 0;

        virtual MultiBodyTransformInfo GetTransformSnapShot() const = 0;

        virtual bool GetIsKinematic() const = 0;
        virtual void SetIsKinematic(bool flag) = 0;
    };
}