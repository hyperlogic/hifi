#pragma once
#include <string>
#include "dm_public/guid.h"

namespace avatar
{
    enum class AssetType : uint8_t
    {
        GENERIC_ASSET,
        CHARACTER_ASSET,
        ANIMATION_ASSET,
        MAP_ASSET,
        CONTROL_PARAMS,
		FRAGMENT_HOST
    };

    struct AssetDescriptor
    {
        AssetType m_AssetType;
        Guid m_AssetID;
        std::string m_SuggestedFileName;
        std::string m_SuggestedFolder;
    };
}
