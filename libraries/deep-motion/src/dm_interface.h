#pragma once
#include "dm_public/interfaces/i_engine_interface.h"

#ifdef _WIN32
#   ifdef DM_INTERFACE_DLL
#       define AVATAR_DLL __declspec(dllexport)
#   else
#       define AVATAR_DLL __declspec(dllimport)
#   endif
#else
#   define AVATAR_DLL
#endif

namespace avatar
{
    AVATAR_DLL IEngineInterface& GetEngineInterface();
}
