//
// Created by ByteDance on 25-11-26.
//

#include <go/symbol/struct_field.h>
#include <go/symbol/struct.h>
#include <go/binary.h>

namespace go::symbol {
    StructField::StructField(
            const Struct *parent,
            elf::Reader *reader,
            Version version,
            uint64_t name,
            uint64_t typeAddress,
            uintptr_t offset
    ) : mParent(parent), mReader(reader), mVersion(version), mName(name), mTypeAddress(typeAddress), mOffset(offset) {

    }

    std::optional<std::string> StructField::name() const {
        if (mName == 0) {
            return std::nullopt;
        }

        uint64_t name_addr = mName;

        if (mVersion < Version{1, 17}) {
            auto name_len_buf_opt = mReader->readVirtualMemory(name_addr + 1, 2);
            if(!name_len_buf_opt) {
                return std::nullopt;
            }
            auto& name_len_buf = *name_len_buf_opt;
            size_t len = (size_t)(std::to_integer<uint8_t>(name_len_buf[0]) << 8 | std::to_integer<uint8_t>(name_len_buf[1]));
            if (len > 4096) return std::nullopt;

            auto name_buf_opt = mReader->readVirtualMemory(name_addr + 3, len);
            if(!name_buf_opt) {
                return std::nullopt;
            }
            return std::string{(const char*)name_buf_opt->data(), len};
        }

        auto varint_buf_opt = mReader->readVirtualMemory(name_addr + 1, 10);
        if (!varint_buf_opt) {
            return std::nullopt;
        }

        std::optional<std::pair<int64_t, int>> result = go::binary::uVarInt(varint_buf_opt->data());
        if (!result) {
            return std::nullopt;
        }

        auto name_buf_opt = mReader->readVirtualMemory(name_addr + 1 + result->second, result->first);
        if(!name_buf_opt) {
            return std::nullopt;
        }

        return std::string{(const char*)name_buf_opt->data(), (size_t)result->first};
    }

    uintptr_t StructField::offset() const {
        return mOffset;
    }

    uint64_t StructField::address() const {
        return mOffset + mTypeAddress;
    }
}
