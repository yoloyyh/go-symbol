#ifndef GO_SYMBOL_OFFSET_MAP_H
#define GO_SYMBOL_OFFSET_MAP_H


#include <optional>
#include <go/version.h>

namespace go {
    namespace symbol {

        struct Offsets {
            uint64_t types;
            uint64_t etypes;
            uint64_t typelinks_ptr;
            uint64_t typelinks_len;
            uint64_t itablinks_ptr;
            uint64_t itablinks_len;
        };

        inline std::optional<Offsets> getOffsets(const go::Version& version, size_t ptrSize) {
            if (version >= go::Version{1, 20}) {
                return Offsets{
                        .types = 39 * ptrSize,
                        .etypes = 40 * ptrSize,
                        .typelinks_ptr = 44 * ptrSize,
                        .typelinks_len = 45 * ptrSize,
                        .itablinks_ptr = 47 * ptrSize,
                        .itablinks_len = 48 * ptrSize
                };
            } else if (version >= go::Version{1, 18}) {
                return Offsets{
                        .types = 35 * ptrSize,
                        .etypes = 36 * ptrSize,
                        .typelinks_ptr = 42 * ptrSize,
                        .typelinks_len = 43 * ptrSize,
                        .itablinks_ptr = 45 * ptrSize,
                        .itablinks_len = 46 * ptrSize
                };
            } else if (version >= go::Version{1, 16}) {
                return Offsets{
                        .types = 35 * ptrSize,
                        .etypes = 36 * ptrSize,
                        .typelinks_ptr = 40 * ptrSize,
                        .typelinks_len = 41 * ptrSize,
                        .itablinks_ptr = 43 * ptrSize,
                        .itablinks_len = 44 * ptrSize
                };
            } else if (version >= go::Version{1, 10}) {
                return Offsets{
                        .types = 25 * ptrSize,
                        .etypes = 26 * ptrSize,
                        .typelinks_ptr = 30 * ptrSize,
                        .typelinks_len = 31 * ptrSize,
                        .itablinks_ptr = 33 * ptrSize,
                        .itablinks_len = 34 * ptrSize
                };
            }

            return std::nullopt; // Unsupported version
        }

    } // namespace symbol
} // namespace go

#endif // GO_SYMBOL_OFFSET_MAP_H