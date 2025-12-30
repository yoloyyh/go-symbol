
#ifndef GO_SYMBOL_MODULEDATA_H
#define GO_SYMBOL_MODULEDATA_H

#include <go/version.h>
#include <go/endian.h>
#include <elf/reader.h>

namespace go::symbol {
    struct ModuleRange {
        uint64_t types{0};
        uint64_t etypes{0};
    };
    class ModuleData {
    public:

        ModuleData(const elf::Reader reader, uint64_t address, go::Version version, endian::Converter endian, size_t ptrSize);

        [[nodiscard]] std::optional<uint64_t> pcHeader() const;
        [[nodiscard]] std::optional<uint64_t> types() const;
        [[nodiscard]] std::optional<uint64_t> etypes() const;
        [[nodiscard]] std::optional<std::pair<const std::byte*, uint64_t>> typeLinks() const;
        [[nodiscard]] std::optional<std::pair<const std::byte*, uint64_t>> itabLinks() const;
        // 返回 {types, etypes}
        [[nodiscard]] std::optional<ModuleRange> ranges() const;

    private:
        elf::Reader mReader;
        uint64_t mAddress;
        go::Version mVersion;
        endian::Converter mConverter;
        size_t mPtrSize;
    };
}
#endif