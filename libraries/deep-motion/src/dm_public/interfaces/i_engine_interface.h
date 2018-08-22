#pragma once
#include <memory>

namespace avatar
{
struct CoreCommands;
struct AssetCommands;
class ISceneHandle;

class IEngineInterface
{
public:
    IEngineInterface() = default;
    virtual void InitializeRuntime() = 0;
    virtual void FinalizeRuntime() = 0;
    virtual void RegisterCoreCommands(CoreCommands& commandStruct) = 0;
    virtual void RegisterAssetCommands(AssetCommands& assetCommands) = 0;
    virtual void TickGeneralPurposeRuntime(float detlaTime) = 0;
    virtual std::shared_ptr<ISceneHandle> CreateNewScene() = 0;
    virtual std::shared_ptr<ISceneHandle> CreateNewSceneFromJSON(const uint8_t* jsonBuffer, uint32_t size) = 0;

    virtual ~IEngineInterface() {}
private:
    IEngineInterface(const IEngineInterface&) = delete;
    IEngineInterface& operator=(const IEngineInterface&) = delete;
};

extern IEngineInterface& GetAvatarEngineInterfaceController();

}
