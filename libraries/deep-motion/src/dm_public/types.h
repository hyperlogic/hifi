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
    ARTICULATION_LINK_HANDLE
};

enum ResourceIdentifierTypes
{
    ANIM_RESOURCE
};
}

struct Vector3
{
    float GetAt(size_t index) const { return m_V[index]; }
    void SetAt(size_t index, float val) { m_V[index] = val; }
    float m_V[3];
};

struct Quaternion
{
    float GetAt(size_t index) const { return m_V[index]; }
    void SetAt(size_t index, float val) { m_V[index] = val; }
    float m_V[4];
};

struct Transform
{
    Vector3 m_Position{{0,0,0}};
    Quaternion m_Orientation{{0,0,0,1}};
    Vector3 m_Scale{{1,1,1}};
};

}