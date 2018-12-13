#pragma once

// Macro to call derived class member function from base class.
#define STATIC_DISPATCH(T, FUNC, ...) (static_cast<T *>(this)->T::FUNC(__VA_ARGS__))

