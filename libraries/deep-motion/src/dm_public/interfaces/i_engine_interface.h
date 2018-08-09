#pragma once

#include "dm_public/types.h"

namespace avatar
{
struct CoreCommands;
struct AssetCommands;

class IEngineInterface
{
public:
    IEngineInterface() {}
    virtual void InitializeRuntime() = 0;
    virtual void FinalizeRuntime() = 0;
    virtual void RegisterCoreCommands(CoreCommands& commandStruct) = 0;
    virtual void RegisterAssetCommands(AssetCommands& assetCommands) = 0;
    virtual void TickGeneralPurposeRuntime(float detlaTime) = 0;

    virtual SceneHandle DeserializeAndLoadScene(const uint8_t* rawBuffer) = 0;
    virtual RigidBodyHandle GetRigidBodyByName(const char* name) = 0;
    virtual bool GetRigidBodyTransform(RigidBodyHandle rbHandle, Float3* pos, Float4* ori) = 0;

    virtual ~IEngineInterface() {}
private:
    IEngineInterface(const IEngineInterface&) = delete;
    IEngineInterface& operator=(const IEngineInterface&) = delete;
};

extern IEngineInterface& GetAvatarEngineInterfaceController();

}
