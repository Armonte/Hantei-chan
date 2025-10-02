#ifndef STDALLOCATOR_HPP_GUARD
#define STDALLOCATOR_HPP_GUARD

#include "tinyalloc.h"
#include <stdlib.h> //size_t, malloc, free
#include <new>

template <class T>
struct LinearAllocator
{
	typedef T value_type;
	LinearAllocator() noexcept {} //default ctor not required by C++ Standard Library

	// A converting copy constructor:
	template<class U> LinearAllocator(const LinearAllocator<U>&) noexcept {}
	template<class U> bool operator==(const LinearAllocator<U>&) const noexcept
	{
		return true;
	}
	template<class U> bool operator!=(const LinearAllocator<U>&) const noexcept
	{
		return false;
	}
	T* allocate(const size_t n) const;
	void deallocate(T* const p, size_t) const noexcept;
};

template <class T>
T* LinearAllocator<T>::allocate(const size_t n) const
{
	if (n == 0)
	{
		return nullptr;
	}
	if (n > static_cast<size_t>(-1) / sizeof(T))
	{
		throw std::bad_array_new_length();
	}
	void* const pv = ta_alloc(n * sizeof(T));
	if (!pv) { throw std::bad_alloc(); }
	return static_cast<T*>(pv);
}

template<class T>
void LinearAllocator<T>::deallocate(T * const p, size_t) const noexcept
{
	ta_free(p);
}

#endif /* STDALLOCATOR_HPP_GUARD */
