
#ifndef GO_SYMBOL_PC_HEADER_H
#define GO_SYMBOL_PC_HEADER_H

#include <go/version.h>
#include <go/endian.h>
#include <elf/reader.h>
#include <optional>

namespace go::symbol {
    class PcHeader {
    public:
        PcHeader(elf::Reader reader, uint64_t address, size_t ptrSize);

        std::optional<go::Version> version();
        uint64_t size() const;

    private:
        elf::Reader mReader;
        uint64_t mAddress;
        size_t mPtrSize;

        uint32_t mMagic;
        std::optional<go::Version> mVersion;
        uint64_t mSize = 0;

        void parse();
    };
}
#endif //GO_SYMBOL_PC_HEADER_H
