#pragma once
#include <memory>

#include "dm_public/object_definitions/collider_definitions.h"

namespace avatar
{
    struct RigidBodyDefinition
    {
        enum class CombineMode : uint8_t
        {
            Average,
            Multiply,
            Minimum,
            Maximum
        };
        std::shared_ptr<ColliderDefinition> m_Collider;
        float m_Mass = 0;
        Vector3 m_Moi = {{ 0,0,0 }};
        Transform m_Transform;
        float m_Friction = 1.0f;
        CombineMode m_FrictionCombineMode = CombineMode::Multiply;
        float m_RollingFriction = 1.0f;
        float m_Restitution = 0.0f;
        CombineMode m_RestitutionCombineMode = CombineMode::Multiply;
        int32_t m_CollisionLayerID = 0;
        bool m_Collidable = true;
    };
}
