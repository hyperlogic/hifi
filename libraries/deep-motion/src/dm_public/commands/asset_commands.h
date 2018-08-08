#pragma once
#include "dm_public/assets.h"
#include "dm_public/guid.h"
#include "dm_public/i_array_interface.h"

namespace avatar
{    
    struct AssetCommands
    {
        typedef bool(*AssetQueryAvailableFn)();
        AssetQueryAvailableFn m_AssetQueryAvailableFn;
        typedef bool(*SaveAssetFn)(const AssetDescriptor& assetDescriptor, const uint8_t* data, uint32_t numBytes);
        SaveAssetFn m_SaveAssetFn;
        typedef bool(*LoadAssetFn)(const Guid& assetID, uint8_t* const& dataOut, uint32_t& numBytesOut);
        LoadAssetFn m_LoadAssetFn;
        typedef bool(*QueryAssetFn)(AssetType, IArrayInterface<AssetDescriptor>& assetsOut);
        QueryAssetFn m_QueryAssetFn;
    };
}