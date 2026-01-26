//
// Created by yoloyyh on 25-11-26.
//

#ifndef GO_SYMBOL_STRUCT_H
#define GO_SYMBOL_STRUCT_H

#include <go/version.h>
#include <go/endian.h>
#include <elf/reader.h>
#include <go/symbol/module_data.h>
namespace go::symbol {

    enum Kind {
        INVALID,
        BOOL,
        INT,
        INT8,
        INT16,
        INT32,
        INT64,
        UINT,
        UINT8,
        UINT16,
        UINT32,
        UINT64,
        UINTPTR,
        FLOAT32,
        FLOAT64,
        COMPLEX64,
        COMPLEX128,
        ARRAY,
        CHAN,
        FUNC,
        INTERFACE,
        MAP,
        POINTER,
        SLICE,
        STRING,
        STRUCT,
    };

    class Struct;
    class StructTable;
    class StructIterator;

    class Struct {
    public:
        Struct(const StructTable *table, elf::Reader* reader, uint64_t address, size_t ptrSize);

        [[nodiscard]]  uint64_t address() const;
        [[nodiscard]]  std::optional<std::string> name() const;
        [[nodiscard]]  std::optional<int> kind() const;
        [[nodiscard]]  size_t fieldCount() const;
        [[nodiscard]]  std::optional<std::pair<std::string, uint64_t>> field(size_t index) const;
        [[nodiscard]]  const StructTable *table() const;
        [[nodiscard]]  size_t ptrSize() const;

    private:
        const StructTable *mTable;
        elf::Reader* mReader;
        uint64_t mAddress;
        size_t mPtrSize;
        mutable std::optional<std::string> mNameCache;
        mutable std::optional<int> mKindCache;
        mutable std::optional<std::pair<uint64_t, size_t>> mFieldsCache;

    };

    class StructTable {
        friend class Struct;
        friend class StructIterator;

    public:
        StructTable(
                elf::Reader* reader,
                std::shared_ptr<elf::ISection> section,
                Version version,
                uint64_t types,
                uint64_t base,
                endian::Converter converter
        );

        StructTable(
                elf::Reader* reader,
                const std::byte* data,
                size_t count,
                Version version,
                uint64_t types,
                uint64_t base,
//                ModuleRange range,
                endian::Converter converter
        );

        size_t size() const;

        Struct operator[](size_t index) const;

        StructIterator begin() const;
        StructIterator end() const;
        endian::Converter converter() const { return mConverter; }
        uint64_t types() const { return mTypes; }

    private:
        elf::Reader* mReader{nullptr};
        std::shared_ptr<elf::ISection> mSection{nullptr};
        const std::byte* mData = nullptr;
        size_t mCount = 0;
        Version mVersion;
        uint64_t mTypes;
        uint64_t mBase;
        endian::Converter mConverter;
        size_t mPtrSize = 0;
    };

    class StructIterator {
    public:

        StructIterator(const StructTable *table, const std::byte *buffer);
        StructIterator(const StructTable *table, const std::byte *buffer, const std::byte* e);

        Struct operator*();
        StructIterator &operator++();

        bool operator==(const StructIterator &rhs);
        bool operator!=(const StructIterator &rhs);

        std::ptrdiff_t operator-(const StructIterator &rhs);

    private:
        const StructTable *mTable;
        const std::byte* mP;
        const std::byte* mEnd;
    };
}


#endif //GO_SYMBOL_STRUCT_H
