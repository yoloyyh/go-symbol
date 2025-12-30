//
// Created by yoloyyh on 25-11-26.
//

#ifndef GO_SYMBOL_STRUCT_FIELD_H
#define GO_SYMBOL_STRUCT_FIELD_H

#include <go/version.h>
#include <elf/reader.h>

namespace go::symbol {
    class Struct;

    class StructField {
    public:
        StructField(
                const Struct *parent,
                elf::Reader *reader,
                Version version,
                uint64_t name,
                uint64_t typeAddress,
                uintptr_t offset
        );

        uint64_t address() const;
        std::optional<std::string> name() const;
        std::optional<std::string> typeName() const;
        uintptr_t offset() const;

    private:
        const Struct *mParent;
        elf::Reader *mReader;
        Version mVersion;
        uint64_t mName;
        uint64_t mTypeAddress;
        uintptr_t mOffset;
    };
}

#endif //GO_SYMBOL_STRUCT_FIELD_H
