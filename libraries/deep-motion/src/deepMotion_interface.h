#pragma once

namespace avatar
{
    class IEngineInterface;
} // avatar

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

AVATAR_DLL avatar::IEngineInterface& GetEngineInterface();

#ifdef __cplusplus
    }
#endif
