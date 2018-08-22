#pragma once
#include <stdint.h>
#include <type_traits>

namespace avatar
{
    template<typename T, bool = std::is_copy_constructible<T>::value>
    class IArrayInterface
    {
    public:
        virtual size_t Size() const = 0;
        virtual void Reserve(size_t size) = 0;
        virtual void PushBack(T&& value) = 0;
        virtual const T& operator[](size_t index) const = 0;
        virtual T& operator[](size_t index) = 0;
        virtual void Resize(size_t newSize) = 0;
        virtual T& Grow() = 0;
    };

    template<typename T>
    class IArrayInterface<T, true> : public IArrayInterface<T, false>
    {
    public:
        virtual void PushBack(const T& value) = 0;
    };

}