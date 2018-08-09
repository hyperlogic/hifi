#pragma once

#include "dm_public/types.h"

#ifdef __cplusplus
    extern "C" {
#endif

#ifdef _WIN32
#   ifdef AVATAR_API_EXPORT
#       define AVATAR_DLL __declspec(dllexport)
#   else
#       define AVATAR_DLL __declspec(dllimport)
#   endif
#else
#   define AVATAR_DLL
#endif

AVATAR_DLL void InitializeIntegration();
AVATAR_DLL avatar::SimpleGenericHandle LoadCharacterOnScene(const uint8_t* rawData);
AVATAR_DLL void FinalizeIntegration();
AVATAR_DLL void TickIntegration(float deltaTime);

#ifdef __cplusplus
    }
#endif