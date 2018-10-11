#pragma once
#include <memory>
#include <vector>
#include <utility>

#include "dm_public/types.h"

namespace avatar
{
    enum class ColliderType : uint8_t
    {
        BOX,
        CAPSULE,
        SPHERE,
        CYLINDER,
        COMPOUND
    };

    struct ColliderDefinition
    {
        inline virtual ~ColliderDefinition() = 0;
        virtual ColliderType GetColliderType() const = 0;
    };

    struct BoxColliderDefinition : ColliderDefinition
    {
        virtual ColliderType GetColliderType() const override { return ColliderType::BOX; }
        Vector3 m_HalfSize = {{ 1.0f, 1.0f, 1.0f }};
    };

    struct CapsuleColliderDefinition : ColliderDefinition
    {
        virtual ColliderType GetColliderType() const override { return ColliderType::CAPSULE; }
        float m_Radius = 1.0f;
        float m_HalfHeight = 1.0f;
    };

    struct SphereColliderDefinition : ColliderDefinition
    {
        virtual ColliderType GetColliderType() const override { return ColliderType::SPHERE; }
        float m_Radius = 1.0f;
    };

    struct CylinderColliderDefinition : ColliderDefinition
    {
        virtual ColliderType GetColliderType() const override { return ColliderType::CYLINDER; }
        float m_Radius = 1.0f;
        float m_HalfHeight = 1.0f;
    };

    struct CompoundColliderDefinition : ColliderDefinition
    {
        struct ChildCollider
        {
            Transform m_Transform;
            std::shared_ptr<ColliderDefinition> m_Collider;
        };
        virtual ColliderType GetColliderType() const override { return ColliderType::COMPOUND; }
        std::vector<ChildCollider> m_ChildColliders;
    };

    inline ColliderDefinition::~ColliderDefinition() {}
}
