#pragma once
#include <array>
#include <stdint.h>

namespace avatar
{

class Guid
{
  public:
    static Guid CreateNewGuid();
    Guid();
    bool isEmpty() const;
    std::array<uint8_t, 16> m_Data;
};
}
