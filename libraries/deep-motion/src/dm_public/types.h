#pragma once
#include <stdint.h>
#include "opaque_handle.h"
#include "guid.h"

namespace avatar
{	
namespace Detail
{
enum HandleTypes : uint32_t
{
    MULTI_BODY_LINK_HANDLE
};

enum ResourceIdentifierTypes
{
    ANIM_RESOURCE
};
}

struct Vector3
{
    float m_V[3];
};

struct Quaternion
{
    float m_V[4];
};

struct Transform
{
    Vector3 m_Position;
    Quaternion m_Orientation;
};

}