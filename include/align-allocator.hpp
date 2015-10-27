#ifndef ALIGN_ALLOCATOR_HPP
#define ALIGN_ALLOCATOR_HPP

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>


template <typename T>
class AlignAllocator {
    private:
        void operator=(const AlignAllocator &);


    public:
        typedef T value_type;
        typedef T *pointer;
        typedef const T *const_pointer;
        typedef T &reference;
        typedef const T &const_reference;
        typedef size_t size_type;
        typedef ptrdiff_t difference_type;

        template <typename U>
        struct rebind {
            typedef AlignAllocator<U> other;
        };

        AlignAllocator(void) {}
        AlignAllocator(const AlignAllocator &) {}

        template <typename U>
        AlignAllocator(const AlignAllocator<U> &) {}

        ~AlignAllocator(void) {}

        T *address(T &ref) const { return &ref; }
        const T *address(const T &ref) const { return &ref; }

        T *allocate(size_t n, const T * = nullptr)
        {
            void *ptr = aligned_alloc(alignof(T), n * sizeof(T));
            if (!ptr) {
                throw std::bad_alloc();
            }
            return static_cast<T *>(ptr);
        }
        void deallocate(T *ptr, size_t) { free(ptr); }

        size_t max_size(void) const { return SIZE_MAX / sizeof(T); }

        void construct(T *ptr, const T &ref) { new (ptr) T(ref); }
        void destroy(T *ptr) { ptr->~T(); }

        bool operator==(const AlignAllocator<T> &) { return true; }
        bool operator!=(const AlignAllocator<T> &) { return false; }
};


template<> class AlignAllocator<void> {
    public:
        typedef void value_type;
        typedef void *pointer;
        typedef const void *const_pointer;

        template <class U>
        struct rebind {
            typedef AlignAllocator<U> other;
        };

        bool operator==(const AlignAllocator<void> &) { return true; }
        bool operator!=(const AlignAllocator<void> &) { return false; }
};


template <typename T> using AlignedVector = std::vector<T, AlignAllocator<T>>;

#endif
