#include <go/symbol/pc_header.h>
#include <zero/log.h>

// Magic numbers for different Go versions
constexpr uint32_t MAGIC_112 = 0xFFFFFFFB;
constexpr uint32_t MAGIC_116 = 0xFFFFFFFA;
constexpr uint32_t MAGIC_118 = 0xFFFFFFF0;
constexpr uint32_t MAGIC_120 = 0xFFFFFFF1;

namespace go::symbol {

    PcHeader::PcHeader(elf::Reader reader, uint64_t address, size_t ptrSize)
            : mReader(reader), mAddress(address), mPtrSize(ptrSize) {
        parse();
    }

    void PcHeader::parse() {
        auto magic_buf = mReader.readVirtualMemory(mAddress, sizeof(uint32_t));
        if (!magic_buf) {
            LOG_ERROR("Failed to read pcHeader magic");
            return;
        }

        mMagic = *reinterpret_cast<const uint32_t*>(magic_buf->data());

        switch (mMagic) {
            case MAGIC_120: // Go 1.20+
                mVersion = {1, 20};
                mSize = 8 + 7 * mPtrSize;
                break;
            case MAGIC_118: // Go 1.18 - 1.19
                mVersion = {1, 18};
                mSize = 8 + 7 * mPtrSize;
                break;
            case MAGIC_116: // Go 1.16 - 1.17
                mVersion = {1, 16};
                mSize = 8 + 6 * mPtrSize;
                break;
            case MAGIC_112: // Go 1.12 - 1.15
                mVersion = {1, 12};
                mSize = 8; // Only magic, minLC, ptrSize, nfunc
                break;
            default: {
                // Fallback for older or unknown versions, try to infer from binary name if possible
                // For now, we assume it's an old version if magic doesn't match.
                // Go 1.10 and 1.11 have different pclntab structure entirely.
                // Let's assume 1.10 for now if we can't determine otherwise.
                mVersion = {1, 10};
                mSize = 8;
                break;
            }
        }
    }

    std::optional<go::Version> PcHeader::version() {
        return mVersion;
    }

    uint64_t PcHeader::size() const {
        return mSize;
    }

}
