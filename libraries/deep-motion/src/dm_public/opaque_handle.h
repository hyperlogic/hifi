#pragma once

namespace avatar
{

template<typename UnderlyingType, uint32_t UNIQUE_ID>
class OpaqueHandle
{
public:
    explicit OpaqueHandle(UnderlyingType val) : m_Val(val) {}
    explicit operator UnderlyingType() const { return m_Val; }
    
    bool operator==(const OpaqueHandle& other) const { return m_Val == other.m_Val; }
    bool operator!=(const OpaqueHandle& other) const { return m_Val != other.m_Val; }
private:
    UnderlyingType m_Val;
};

}
