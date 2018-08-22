#pragma once
#include "dm_public/interfaces/i_engine_interface.h"

namespace avatar
{
    class EngineInterfaceStub : public IEngineInterface
    {
    public:
        static EngineInterfaceStub& getInstance() {
            static EngineInterfaceStub _instance;
            return _instance;
        }

        virtual void InitializeRuntime() override {};
        virtual void FinalizeRuntime() override {};
        virtual void RegisterCoreCommands(CoreCommands&) override {};
        virtual void RegisterAssetCommands(AssetCommands&) override {};
        virtual void TickGeneralPurposeRuntime(float) override {};
        virtual std::shared_ptr<ISceneHandle> CreateNewScene() override { return nullptr; };
        virtual std::shared_ptr<ISceneHandle> CreateNewSceneFromJSON(const uint8_t*, uint32_t) override { return nullptr; };

        virtual ~EngineInterfaceStub() {}
    private:
        

        EngineInterfaceStub() = default;
        EngineInterfaceStub(const EngineInterfaceStub&) = delete;
        EngineInterfaceStub& operator=(const EngineInterfaceStub&) = delete;
    };

    IEngineInterface& GetEngineInterface() { return EngineInterfaceStub::getInstance(); }
}
