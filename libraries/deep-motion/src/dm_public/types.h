#pragma once
#include <stdint.h>
#include "opaque_handle.h"
#include "guid.h"

namespace avatar
{	
namespace Detail
{
enum HandleTypes
{
    INVALID_HANDLE,
	ANIMATION_HANDLE,
	DISPLACEMENT_HANDLE,
    SCENE_HANDLE,
    RIGIDBODY_HANDLE
};

enum ResourceIdentifierTypes
{
	ANIM_RESOURCE
};
}

using AnimHandle = OpaqueHandle<void*, Detail::ANIMATION_HANDLE>;
using AnimResId = OpaqueHandle<Guid, Detail::ANIM_RESOURCE>;
using DisplacementHandle = OpaqueHandle<void*, Detail::DISPLACEMENT_HANDLE>;
using SceneHandle = OpaqueHandle<void*, Detail::SCENE_HANDLE>;
using RigidBodyHandle = OpaqueHandle<void*, Detail::RIGIDBODY_HANDLE>;

struct SimpleGenericHandle
{
    void* m_rawHandle = nullptr;
    Detail::HandleTypes m_type = Detail::INVALID_HANDLE;
};

struct Float3
{
    float m[3];
};

struct Float4
{
    float m[4];
};

}