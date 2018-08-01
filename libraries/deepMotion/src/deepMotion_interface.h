#pragma once

#ifdef WIN32
#ifdef COMPILING_DLL
#define AVATAR_DLL __declspec(dllexport)
#else
#define AVATAR_DLL __declspec(dllimport)
#endif // COMPILING_DLL
#elif defined UNIX
#define AVATAR_DLL __declspec(dllexport)
#else
#define AVATAR_DLL __attribute__ ((visibility ("default")))
#endif // WIN32 OR UNIX

extern "C" AVATAR_DLL void InitializeIntegration();
extern "C" AVATAR_DLL void FinalizeIntegration();
extern "C" AVATAR_DLL void TickIntegration(float deltaTime);
