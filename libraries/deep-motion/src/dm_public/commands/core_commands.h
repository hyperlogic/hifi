#pragma once
#include <cstdio>

namespace avatar
{

struct CoreCommands
{
    typedef void*(*AllocateFn)(size_t);
    AllocateFn m_AllocateFn;
    typedef void(*FreeFn)(void*);
    FreeFn m_FreeFn;
};

}
