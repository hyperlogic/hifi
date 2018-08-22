#pragma once
#include "dm_public/i_array_interface.h"

namespace avatar
{
    template<typename T, bool = std::is_copy_constructible<T>::value>
    class STDVectorArrayInterface : public IArrayInterface<T>
    {
    public:
        explicit STDVectorArrayInterface(std::vector<T>& vec)
            : m_Vec(vec)
        {
        }

        virtual size_t Size() const override { return m_Vec.size(); }
        virtual void Reserve(size_t size) override { m_Vec.reserve(size); }
        virtual void PushBack(T&& value) override { m_Vec.push_back(std::move(value)); }
        virtual const T& operator[](size_t index) const override { return m_Vec[index]; }
        virtual T& operator[](size_t index) override { return m_Vec[index]; }
        virtual void Resize(size_t newSize) override { m_Vec.resize(newSize); }
        virtual T& Grow() override { m_Vec.push_back(T{}); return m_Vec.back(); }
    protected:
        std::vector<T>& m_Vec;
    };

    template<typename T>
    class STDVectorArrayInterface<T, true> : public STDVectorArrayInterface<T, false>
    {
    public:
        explicit STDVectorArrayInterface(std::vector<T>& vec) : STDVectorArrayInterface<T, false>(vec) {}
        virtual void PushBack(const T& value) override { this->m_Vec.push_back(value); }
    };
}
