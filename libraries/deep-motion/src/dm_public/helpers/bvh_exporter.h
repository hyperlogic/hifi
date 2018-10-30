#pragma once
#include <memory>

namespace avatar
{
    class IArticulation;

    class BVHExporter final
    {
    public:
        BVHExporter();
        ~BVHExporter();
        bool Initialize(IArticulation& articulationHandle, const char* filePath);
        void UpdateWithNewFrame(float deltaTime);
        void Finalize();
    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}