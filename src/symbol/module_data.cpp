#include <go/symbol/module_data.h>
#include <go/symbol/offset_map.h>
#include <zero/log.h>
namespace go::symbol {
        namespace {
            std::optional<uint64_t> readUint(const elf::Reader reader, uint64_t address, endian::Converter converter, size_t ptrSize) {
                auto buffer = reader.readVirtualMemory(address, ptrSize);
                if (!buffer) {
                    return std::nullopt;
                }
                return converter(buffer->data(), ptrSize);
            }
        }

        ModuleData::ModuleData(const elf::Reader reader, uint64_t address, go::Version version, endian::Converter endian, size_t ptrSize)
                : mReader(reader), mAddress(address), mVersion(version), mConverter(endian), mPtrSize(ptrSize) {}

        std::optional<uint64_t> ModuleData::pcHeader() const {
            if (mVersion < go::Version{1, 16}) {
                return std::nullopt; // Not present in older versions
            }
            return readUint(mReader, mAddress, mConverter, mPtrSize);
        }

        std::optional<uint64_t> ModuleData::types() const {
            auto offsets = getOffsets(mVersion, mPtrSize);
            if (!offsets) return std::nullopt;
            return readUint(mReader, mAddress + offsets->types, mConverter, mPtrSize);
        }

        std::optional<uint64_t> ModuleData::etypes() const {
            auto offsets = getOffsets(mVersion, mPtrSize);
            if (!offsets) return std::nullopt;
            return readUint(mReader, mAddress + offsets->etypes, mConverter, mPtrSize);

        }

        std::optional<std::pair<const std::byte*, size_t>> ModuleData::typeLinks() const {
            auto offsets = getOffsets(mVersion, mPtrSize);
            if (!offsets) return std::nullopt;

            auto arrayAddr = readUint(mReader, mAddress + offsets->typelinks_ptr, mConverter, mPtrSize);
            size_t lenAddress;
            if (mVersion < go::Version{1, 16}) {
                lenAddress = mAddress + offsets->typelinks_len;
            } else {
                lenAddress = mAddress + offsets->typelinks_ptr + mPtrSize;
            }

            auto lenOpt = readUint(mReader, lenAddress, mConverter, mPtrSize);
            if (!lenOpt) {
                return std::nullopt;
            }
            size_t count = static_cast<size_t>(*lenOpt);
            if (count == 0) {
                return std::make_pair(nullptr, static_cast<size_t>(0));
            }
            const std::byte* data = mReader.virtualMemory(*arrayAddr);
            if (!data) {
                return std::nullopt;
            }
            LOG_INFO("typelinks base via virtualMemory: %p (addr=0x%lx, count=%zu)",
                     (const void*)data, *arrayAddr, count);
            return std::make_pair(data, count);
        }

        std::optional<std::pair<const std::byte*, uint64_t>> ModuleData::itabLinks() const {
            auto offsets = getOffsets(mVersion, mPtrSize);
            if (!offsets) return std::nullopt;

            auto arrayAddr = readUint(mReader, mAddress + offsets->itablinks_ptr, mConverter, mPtrSize);
            uint64_t lenAddress;
            if (mVersion < go::Version{1, 16}) {
                lenAddress = mAddress + offsets->itablinks_len;
            } else {
                lenAddress = mAddress + offsets->itablinks_ptr + mPtrSize;
            }

            auto lenOpt = readUint(mReader, lenAddress, mConverter, mPtrSize);
            if (!lenOpt) {
                return std::nullopt;
            }

            uint64_t count = *lenOpt;

            if (count == 0) {
                return std::make_pair(nullptr, static_cast<uint64_t>(0));
            }

            const std::byte* data = mReader.virtualMemory(*arrayAddr);
            if (!data) {
                return std::nullopt;
            }

            LOG_INFO("itablinks base via virtualMemory: %p (addr=0x%lx, count=%lu)",
                     (const void*)data, *arrayAddr, (unsigned long)count);

            return std::make_pair(data, count);
        }
        std::optional<ModuleRange> ModuleData::ranges() const {
            auto types_addr = types();
            auto etypes_addr = etypes();
            if (!types_addr ||!etypes_addr) return std::nullopt;
            return ModuleRange{
                    .types = *types_addr,
                    .etypes = *etypes_addr
            };
        }

}
