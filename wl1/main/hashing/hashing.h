#ifndef __HASHING_H__
#define __HASHING_H__

#include <cstdint>
#include <cstddef>

namespace hsh {
    void hash_bytes(const uint8_t* data, size_t len, uint8_t* result);
}

#endif