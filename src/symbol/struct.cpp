//
// Created by ByteDance on 25-11-26.
//

#include <go/symbol/struct.h>
#include <go/binary.h>
#include <go/endian.h>
#include <go/symbol/struct_field.h>

go::symbol::StructTable::StructTable(
        elf::Reader* reader,
        std::shared_ptr<elf::ISection> section,
        Version version,
        uint64_t types,
        uint64_t base,
        endian::Converter converter ) : mReader(reader), mSection(std::move(section)), mVersion(version), mTypes(types), mBase(base), mConverter(converter)  {

    mCount = mSection->size() / 4;
}

go::symbol::StructTable::StructTable(
        elf::Reader* reader,
        const std::byte* data,
        size_t count,
        Version version,
        uint64_t types,
        uint64_t base,
        endian::Converter converter
) : mReader(reader), mData(data), mCount(count), mVersion(version), mTypes(types), mBase(base), mConverter(converter) {
}

size_t go::symbol::StructTable::size() const {
    return mCount;
}

go::symbol::Struct go::symbol::StructTable::operator[](size_t index) const {
    auto it = begin();
    for (size_t i = 0; i < index; ++i) {
        ++it;
    }
    return *it;
}

go::symbol::StructIterator go::symbol::StructTable::begin() const {
    if (mSection) {
        const std::byte* p = mSection->data();
        return {this, p};
    }
    const std::byte* p =  mData;
    const std::byte* e = p ? p + (mCount * 4) : p;
    return {this, p, e};

}

go::symbol::StructIterator go::symbol::StructTable::end() const {
    if (mSection) {
        const std::byte* p = mSection->data();
        const std::byte* e = p + mSection->size();
        return {this, e};
    }
    const std::byte* p = mData;
    const std::byte* e = p ? p + (size() * 4) : p;
    return {this, e, e};
}

go::symbol::StructIterator::StructIterator(const go::symbol::StructTable *table, const std::byte *buffer)
        : mTable(table), mP(buffer), mEnd(nullptr) {
}

go::symbol::StructIterator::StructIterator(const go::symbol::StructTable *table, const std::byte *buffer, const std::byte* e)
        : mTable(table), mP(buffer), mEnd(e) {
}

go::symbol::Struct go::symbol::StructIterator::operator*() {

    uint32_t type_offset = mTable->converter()(mP, 4);

    uint64_t type_address = mTable->mTypes + type_offset;
    size_t ptrSize = mTable->mReader->header()->ident()[EI_CLASS] == ELFCLASS64 ? 8 : 4;
    return {mTable, mTable->mReader, type_address, ptrSize};
}

go::symbol::StructIterator &go::symbol::StructIterator::operator++() {
    mP += 4;
    return *this;
}

bool go::symbol::StructIterator::operator==(const go::symbol::StructIterator &rhs) {
    return mP == rhs.mP;
}

bool go::symbol::StructIterator::operator!=(const go::symbol::StructIterator &rhs) {
    return !operator==(rhs);
}

std::ptrdiff_t go::symbol::StructIterator::operator-(const go::symbol::StructIterator &rhs) {
    return (mP - rhs.mP) / 4;
}

go::symbol::Struct::Struct(const StructTable *table, elf::Reader* reader, uint64_t address, size_t ptrSize)
        : mTable(table), mReader(reader), mAddress(address), mPtrSize(ptrSize) {
}

uint64_t go::symbol::Struct::address() const {
    return mAddress + mTable->mBase;
}

std::optional<std::string> go::symbol::Struct::name() const {
    auto rbuf = mReader->readVirtualMemory(mAddress, mPtrSize * 8);
    if (!rbuf) return std::nullopt;
    auto rbuf_ptr = rbuf->data();
    uint64_t nameOff = mTable->converter()(rbuf_ptr + (mPtrSize == 8 ? 40 : 24), 4);
    if (!nameOff) return std::nullopt;
    const std::byte* nbuf = mReader->virtualMemory(mTable->mTypes + nameOff);
    if (!nbuf) return std::nullopt;
    if (mTable->mVersion <= Version{1,16}) {
        size_t len = (std::to_integer<size_t>(nbuf[1]) << 8) | std::to_integer<size_t>(nbuf[2]);
        return std::string((const char*)nbuf + 3, len);
    }
    auto u = go::binary::uVarInt(nbuf + 1);
    if (!u) return std::nullopt;
    return std::string((const char*)nbuf + 1 + u->second, (size_t)u->first);
}

const go::symbol::StructTable *go::symbol::Struct::table() const {
    return mTable;
}

size_t go::symbol::Struct::ptrSize() const {
    return mPtrSize;

}

size_t go::symbol::Struct::fieldCount() const {
    auto kind_val = this->kind();
    if (!kind_val) {
        return 0;
    }

    size_t elemOffset = 0;
    size_t typeSize = (mPtrSize == 8) ? 48 : 32;
    auto cur_kind = *kind_val;
    bool need_tran = false;

    // Based on reflect.Kind
    switch (cur_kind) {
        case 17: // Array
        case 18: // Chan
        case 22: // Ptr
        case 23: // Slice
            need_tran = true;
            elemOffset = typeSize; // elem is right after the _type header
            break;
        case 21: // Map
            need_tran = true;
            elemOffset = typeSize + mPtrSize; // elem is after _type and key pointer
            break;
        case 25: // Struct
            need_tran = false;
            break;
        default:
            return 0;
    }

    if (need_tran) {
        auto buf = mReader->readVirtualMemory(mAddress, elemOffset + mPtrSize);
        if (!buf) {
            return 0;
        }
        uint64_t elem_addr = mTable->converter()(buf->data() + elemOffset, mPtrSize);
        if (!elem_addr) {
            return 0;
        }
        Struct elem_struct(mTable, mReader, elem_addr, mPtrSize);
        return elem_struct.fieldCount();
    }

    size_t fields_slice_offset = (mPtrSize == 8) ? 56 : 36; // offsetof(structType, fields)

    auto type_buffer_opt = mReader->readVirtualMemory(mAddress, fields_slice_offset + 2 * mPtrSize);
    if (!type_buffer_opt) {
        return 0;
    }

    return mTable->converter()(type_buffer_opt->data() + fields_slice_offset + mPtrSize, mPtrSize);
}

std::optional<std::pair<std::string, uint64_t>> go::symbol::Struct::field(size_t index) const {
    auto kind_val = this->kind();
    if (!kind_val) {
        return std::nullopt;
    }
    
    uint8_t type_kind = *kind_val;
    size_t rtype_size = (mPtrSize == 8) ? 48 : 32;
    size_t elem_ptr_offset = 0;
    bool recurse = false;

    // Based on reflect.Kind
    switch (type_kind) {
        case 17: // Array
        case 18: // Chan
        case 22: // Ptr
        case 23: // Slice
            recurse = true;
            elem_ptr_offset = rtype_size;
            break;
        case 21: // Map
            recurse = true;
            elem_ptr_offset = rtype_size + mPtrSize;
            break;
        case 25: // Struct
            break; // Fall through to struct handling logic
        default:
            return std::nullopt;
    }

    if (recurse) {
        auto buf = mReader->readVirtualMemory(mAddress, elem_ptr_offset + mPtrSize);
        if (!buf) {
            return std::nullopt;
        }
        uint64_t elem_addr = mTable->converter()(buf->data() + elem_ptr_offset, mPtrSize);
        if (!elem_addr) {
            return std::nullopt;
        }
        Struct elem_struct(mTable, mReader, elem_addr, mPtrSize);
        return elem_struct.field(index);
    }

    // This is a struct type.
    // type structType struct {
    //     rtype
    //     pkgPath name
    //     fields  []structField
    // }
    // 'fields' slice is after rtype and pkgPath.
    size_t fields_slice_offset = (mPtrSize == 8) ? 56 : 36; // offsetof(structType, fields)

    // Read memory for the fields slice header (ptr + len)
    auto type_buffer_opt = mReader->readVirtualMemory(mAddress, fields_slice_offset + 2 * mPtrSize);
    if (!type_buffer_opt) {
        return std::nullopt;
    }
    auto& type_buffer = *type_buffer_opt;

    // Get fields array pointer and count
    uint64_t fields_ptr = mTable->converter()(type_buffer.data() + fields_slice_offset, mPtrSize);
    size_t fields_count = mTable->converter()(type_buffer.data() + fields_slice_offset + mPtrSize, mPtrSize);
    
    if (index >= fields_count || fields_ptr == 0) {
        return std::nullopt;
    }
    
    // Size of structField may vary in different Go versions, but it's typically 3 pointers.
    size_t struct_field_size = mPtrSize * 3; // name, typ, offset
    
    uint64_t field_addr = fields_ptr + index * struct_field_size;
    
    // Read field info
    auto field_buffer_opt = mReader->readVirtualMemory(field_addr, struct_field_size);
    if (!field_buffer_opt) {
        return std::nullopt;
    }

    auto& field_buffer = *field_buffer_opt;
    
    // Parse field info
    uint64_t name_ptr = mTable->converter()(field_buffer.data(), mPtrSize);
    uint64_t type_addr = mTable->converter()(field_buffer.data() + mPtrSize, mPtrSize);
    uintptr_t offset = mTable->converter()(field_buffer.data() + mPtrSize * 2, mPtrSize);

    if (mTable->mVersion <= go::Version{1,18}) {
        offset = offset >> 1;
    }
    if (name_ptr == 0) {
        return std::nullopt;
    }
    
    // Create field object
    StructField field{this, mReader, mTable->mVersion, name_ptr, type_addr, offset};
    
    // Get field name
    auto name_opt = field.name();
    if (!name_opt) {
        return std::nullopt;
    }

    auto address = field.offset();
    // Return field name and offset
    return std::make_pair(*name_opt, address);
}

std::optional<int> go::symbol::Struct::kind() const {
    auto type_buffer_opt = mReader->readVirtualMemory(mAddress, 2 * mPtrSize + 8);
    if (!type_buffer_opt) {
        return std::nullopt;
    }
    
    auto& type_buffer = *type_buffer_opt;

    size_t kind_offset = (mPtrSize == 8) ? 23 : 15;
    

    if (type_buffer.size() <= kind_offset) {
        return std::nullopt;
    }
    
    uint8_t kind_val = mTable->converter()(type_buffer.data() + kind_offset, 1);
    return kind_val & 0x1F;
}
