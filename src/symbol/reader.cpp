#include <go/symbol/reader.h>
#include <elf/symbol.h>
#include <zero/log.h>
#include <algorithm>

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

constexpr auto SYMBOL_SECTION = ".gopclntab";
constexpr auto BUILD_INFO_SECTION = "buildinfo";
constexpr auto INTERFACE_SECTION = ".itablink";
constexpr auto TYPELINK_SECTION = ".typelink";
constexpr auto SYMBOL_RODATA_SECTION = ".rodata";
constexpr auto SYMBOL_NOPTRDATA_SECTION = ".noptrdata";
constexpr auto SYMBOL_DATA_SECTION = ".data";

constexpr auto BUILD_INFO_MAGIC = "\xff Go buildinf:";
constexpr auto BUILD_INFO_MAGIC_SIZE = 14;

constexpr auto TYPES_SYMBOL = "runtime.types";
constexpr auto VERSION_SYMBOL = "runtime.buildVersion";
constexpr auto MODULE_DATA_SYMBOL = "runtime.firstmoduledata";

constexpr auto SYMBOL_MAGIC_12 = 0xfffffffb;
constexpr auto SYMBOL_MAGIC_116 = 0xfffffffa;
constexpr auto SYMBOL_MAGIC_118 = 0xfffffff0;
constexpr auto SYMBOL_MAGIC_120 = 0xfffffff1;

go::symbol::Reader::Reader(elf::Reader reader, std::filesystem::path path)
        : mReader(std::move(reader)), mPath(std::move(path)) {

}

void go::symbol::Reader::ensureVersion() {
    if (mVersion) {
        return;
    }

    mVersion = version();

    if (!mVersion) {
        LOG_ERROR("Failed to determine Go version.");
        return;
    }
}

void go::symbol::Reader::ensureModuleData() {
    if (mModuleDataAddress) {
        return;
    }

    ensureVersion();
    if (!mVersion) {
        return;
    }

    mModuleDataAddress = findModuleData();
}

size_t go::symbol::Reader::ptrSize() {
    return mReader.header()->ident()[EI_CLASS] == ELFCLASS64 ? 8 : 4;
}

elf::endian::Type go::symbol::Reader::endian() {
    return mReader.header()->ident()[EI_DATA] == ELFDATA2MSB ? elf::endian::Big : elf::endian::Little;
}

bool go::symbol::Reader::findSymtabSymbol() {
    std::vector<std::shared_ptr<elf::ISection>> sections = mReader.sections();

    auto it = std::find_if(sections.begin(), sections.end(), [](const auto &section) {
        return section->type() == SHT_SYMTAB && section->name() == ".symtab";
    });
    if (it != sections.end()) {
        mSymbolTable = elf::SymbolTable(mReader, *it);
        return true;
    }

    LOG_ERROR("Failed to find or create symbol table");
    return false;
}

std::optional<go::Version> go::symbol::Reader::version() {
    LOG_INFO("start to get version");
    if (mVersion) {
        return mVersion;
    }
    std::optional<go::symbol::BuildInfo> buildInfo = this->buildInfo();

    if (buildInfo) {
        mVersion = buildInfo->version();
        return mVersion;
    }
    auto findGoVersion = findSymtabByKey(VERSION_SYMBOL);
    if (findGoVersion) {
        return parseVersion(*findGoVersion);
    }

    return std::nullopt;
}

std::optional<go::symbol::BuildInfo> go::symbol::Reader::buildInfo() {
    std::vector<std::shared_ptr<elf::ISection>> sections = mReader.sections();

    auto it = std::find_if(
            sections.begin(),
            sections.end(),
            [](const auto &section) {
                return section->name().find(BUILD_INFO_SECTION) != std::string::npos;
            }
    );

    if (it == sections.end()) {
        LOG_ERROR("build info section not found");
        return std::nullopt;
    }

    if (memcmp((*it)->data(), BUILD_INFO_MAGIC, BUILD_INFO_MAGIC_SIZE) != 0) {
        LOG_ERROR("invalid build info magic");
        return std::nullopt;
    }

    return BuildInfo(mReader, *it);
}

std::optional<go::symbol::seek::SymbolTable> go::symbol::Reader::symbols(uint64_t base) {
    std::vector<std::shared_ptr<elf::ISection>> sections = mReader.sections();

    auto it = std::find_if(
            sections.begin(),
            sections.end(),
            [](const auto &section) {
                return section->name().find(SYMBOL_SECTION) != std::string::npos;
            }
    );

    if (it == sections.end()) {
        LOG_ERROR("symbol section not found");
        return std::nullopt;
    }

    const std::byte *data = (*it)->data();

    elf::endian::Type endian = this->endian();
    endian::Converter converter(endian);

    uint32_t magic = converter(*(uint32_t *) data);

    SymbolVersion version;

    switch (magic) {
        case SYMBOL_MAGIC_12:
            version = VERSION12;
            break;

        case SYMBOL_MAGIC_116:
            version = VERSION116;
            break;

        case SYMBOL_MAGIC_118:
            version = VERSION118;
            break;

        case SYMBOL_MAGIC_120:
            version = VERSION120;
            break;

        default:
            return std::nullopt;
    }

    bool dynamic = mReader.header()->type() == ET_DYN;

    std::vector<std::shared_ptr<elf::ISegment>> loads;
    std::vector<std::shared_ptr<elf::ISegment>> segments = mReader.segments();

    std::copy_if(
            segments.begin(),
            segments.end(),
            std::back_inserter(loads),
            [](const auto &segment) {
                return segment->type() == PT_LOAD;
            }
    );

    Elf64_Addr minVA = std::min_element(
            loads.begin(),
            loads.end(),
            [](const auto &i, const auto &j) {
                return i->virtualAddress() < j->virtualAddress();
            }
    )->operator*().virtualAddress() & ~(PAGE_SIZE - 1);

    std::ifstream stream(mPath);

    if (!stream.is_open()) {
        LOG_ERROR("open %s failed: %s", mPath.string().c_str(), strerror(errno));
        return std::nullopt;
    }

    return seek::SymbolTable(
            version,
            converter,
            std::move(stream),
            (std::streamoff) (*it)->offset(),
            (*it)->address(),
            dynamic ? base - minVA : 0
    );
}

std::optional<go::symbol::SymbolTable> go::symbol::Reader::symbols(AccessMethod method, uint64_t base) {
    std::vector<std::shared_ptr<elf::ISection>> sections = mReader.sections();

    auto it = std::find_if(
            sections.begin(),
            sections.end(),
            [](const auto &section) {
                return section->name().find(SYMBOL_SECTION) != std::string::npos;
            }
    );

    if (it == sections.end()) {
        LOG_ERROR("symbol section not found");
        return std::nullopt;
    }

    const std::byte *data = (*it)->data();

    elf::endian::Type endian = this->endian();
    endian::Converter converter(endian);

    uint32_t magic = converter(*(uint32_t *) data);

    SymbolVersion version;

    switch (magic) {
        case SYMBOL_MAGIC_12:
            version = VERSION12;
            break;

        case SYMBOL_MAGIC_116:
            version = VERSION116;
            break;

        case SYMBOL_MAGIC_118:
            version = VERSION118;
            break;

        case SYMBOL_MAGIC_120:
            version = VERSION120;
            break;

        default:
            return std::nullopt;
    }

    bool dynamic = mReader.header()->type() == ET_DYN;

    std::vector<std::shared_ptr<elf::ISegment>> loads;
    std::vector<std::shared_ptr<elf::ISegment>> segments = mReader.segments();

    std::copy_if(
            segments.begin(),
            segments.end(),
            std::back_inserter(loads),
            [](const auto &segment) {
                return segment->type() == PT_LOAD;
            }
    );

    Elf64_Addr minVA = std::min_element(
            loads.begin(),
            loads.end(),
            [](const auto &i, const auto &j) {
                return i->virtualAddress() < j->virtualAddress();
            }
    )->operator*().virtualAddress() & ~(PAGE_SIZE - 1);

    if (method == FileMapping) {
        return SymbolTable(version, converter, *it,  0);
    } else if (method == AnonymousMemory) {
        std::unique_ptr<std::byte[]> buffer = std::make_unique<std::byte[]>((*it)->size());
        memcpy(buffer.get(), (*it)->data(), (*it)->size());
        return SymbolTable(version, converter, std::move(buffer), 0);
    }

    if (!dynamic)
        return SymbolTable(version, converter, (const std::byte *) (*it)->address(), 0);

    return SymbolTable(version, converter, (const std::byte *) base + (*it)->address() - minVA, 0);
}

std::optional<std::pair<std::shared_ptr<elf::ISection>, uint64_t>> go::symbol::Reader::findSectionAndBase(const std::string& sectionName, uint64_t base) {
    const auto& sections = mReader.sections();
    auto section_it = std::find_if(sections.begin(), sections.end(), [&](const auto &section) {
        return section->name() == sectionName;
    });

    if (section_it == sections.end()) {
        return std::nullopt;
    }

    bool dynamic = mReader.header()->type() == ET_DYN;
    uint64_t base_addr = 0;
    if (dynamic) {
        std::vector<std::shared_ptr<elf::ISegment>> loads;
        std::vector<std::shared_ptr<elf::ISegment>> segments = mReader.segments();

        std::copy_if(
                segments.begin(),
                segments.end(),
                std::back_inserter(loads),
                [](const auto &segment) {
                    return segment->type() == PT_LOAD;
                }
        );

        if (!loads.empty()) {
             Elf64_Addr minVA = std::min_element(
                    loads.begin(),
                    loads.end(),
                    [](const auto &i, const auto &j) {
                        return i->virtualAddress() < j->virtualAddress();
                    }
            )->operator*().virtualAddress() & ~(PAGE_SIZE - 1);

            base_addr = base - minVA;
        }
    }

    return std::make_pair(*section_it, base_addr);
}

std::optional<go::symbol::InterfaceTable> go::symbol::Reader::interfaces(uint64_t base) {
    LOG_INFO("start to get interfaces");
    ensureVersion();
    if (!mVersion) {
        LOG_ERROR("Initialization failed: no version");
        return std::nullopt;
    }

    auto typesAddr = findSymbolAddress(TYPES_SYMBOL);
    if (typesAddr) {
        LOG_INFO("get runtime.types: %p", typesAddr);
        auto result = findSectionAndBase(INTERFACE_SECTION, base);
        if (result) {
            return InterfaceTable(mReader,
                                  result->first->data(),
                                  result->first->size() / ptrSize(),
                                  *mVersion,
                                  *typesAddr,
                                  result->second,
                                  ptrSize(),
                                  endian::Converter(endian()));
        }
    }

    ensureModuleData();
    if (!mModuleDataAddress) {
        LOG_ERROR("Initialization failed or moduledata not found");
        return std::nullopt;
    }
    LOG_INFO("start to get moduladata");

    ModuleData module_data(mReader, *mModuleDataAddress, *mVersion, endian::Converter(endian()), ptrSize());
    auto types_base_opt = module_data.types();
    auto itablinks_opt = module_data.itabLinks();

    if (!types_base_opt || !itablinks_opt) {
        LOG_ERROR("Failed to get types or itablinks from moduledata");
        return std::nullopt;
    }

    LOG_INFO("start to get interface table");
    return InterfaceTable(mReader,
                          itablinks_opt->first,
                          itablinks_opt->second,
                          *mVersion,
                          *types_base_opt,
                          0,
                          ptrSize(),
                          endian::Converter(endian()));

}

std::optional<uint64_t> go::symbol::Reader::findSymbolAddress(const std::string &key) {
    if (!mSymbolTable) {
        findSymtabSymbol();
    }
    if (mSymbolTable) {
        auto symbolIterator = std::find_if(mSymbolTable->begin(), mSymbolTable->end(), [&key](const auto &symbol) {
            return symbol->name() == key;
        });
        if (symbolIterator != mSymbolTable->end()) {
            return (*symbolIterator)->value();
        }
    }

    return std::nullopt;
}

std::optional<std::string> go::symbol::Reader::findSymtabByKey(const std::string &key) {
    if (!mSymbolTable) {
        findSymtabSymbol();
    }
    if (mSymbolTable) {
        auto symbolIterator = std::find_if(mSymbolTable->begin(), mSymbolTable->end(), [&key](const auto &symbol) {
            return symbol->name() == key;
        });
        if (symbolIterator != mSymbolTable->end()) {
            size_t ptrSize = this->ptrSize();
            endian::Converter converter(endian());
            std::optional<std::vector<std::byte>> buffer = mReader.readVirtualMemory(
                    (*symbolIterator)->value(),
                    ptrSize * 2
            );
            if (!buffer)
                return std::nullopt;
            buffer = mReader.readVirtualMemory(
                    converter(buffer->data(), ptrSize),
                    converter(buffer->data() + ptrSize, ptrSize)
            );
            if (!buffer)
                return std::nullopt;
            return std::string{(const char *) buffer->data(), buffer->size()};
        }
    }

    return std::nullopt;
}

std::optional<go::symbol::StructTable> go::symbol::Reader::typeLinks(uint64_t base) {
    LOG_INFO("start to get typeLinks");
    ensureVersion();
    if (!mVersion) {
        LOG_ERROR("Initialization failed: no version");
        return std::nullopt;
    }

    auto typesAddr = findSymbolAddress(TYPES_SYMBOL);
    if (typesAddr) {
        LOG_INFO("get runtime.types addr: %p", typesAddr);
        auto result = findSectionAndBase(TYPELINK_SECTION, base);
        if (result) {
            LOG_INFO("start to parse struct");
            return StructTable(
                    &mReader,
                    result->first->data(),
                    result->first->size() / 4,
                    *mVersion,
                    *typesAddr,
                    result->second,
                    endian::Converter(endian()));
        }
    }

    ensureModuleData();
    if (!mModuleDataAddress) {
        LOG_ERROR("Initialization failed or moduledata not found");
        return std::nullopt;
    }

    ModuleData module_data(mReader, *mModuleDataAddress, *mVersion, endian::Converter(endian()), ptrSize());
    auto types_base_opt = module_data.types();
    auto typelinks_opt = module_data.typeLinks();

    if (!types_base_opt || !typelinks_opt) {
        LOG_ERROR("Failed to get types or typelinks from moduledata");
        return std::nullopt;
    }

    return StructTable(
            &mReader,
            typelinks_opt->first,
            typelinks_opt->second,
            *mVersion,
            *types_base_opt,
            0,
            endian::Converter(endian()));
}

bool go::symbol::Reader::validateModuleData(uint64_t address, uint64_t pclntab_address) {
    if (!mVersion) return false;

    if (*mVersion >= go::Version{1, 16}) {
        ModuleData candidate(mReader, address, *mVersion, endian::Converter(endian()), ptrSize());
        auto pcheader_ptr_opt = candidate.pcHeader();
        if (!pcheader_ptr_opt) {
            LOG_ERROR("Failed to get pcheader from moduledata");
            return false;
        }
        if (*pcheader_ptr_opt == pclntab_address) {
            return true;
        }
        return false;
    } else {
        auto pclntab_ptr_from_candidate_buf = mReader.readVirtualMemory(address, ptrSize());
        if (!pclntab_ptr_from_candidate_buf) {
            return false;
        }
        uint64_t pclntab_ptr_from_candidate = endian::Converter(endian())(pclntab_ptr_from_candidate_buf->data(), ptrSize());

        if (pclntab_ptr_from_candidate != pclntab_address) {
            return false;
        }

        auto candidate_text_addr_buf = mReader.readVirtualMemory(address + 12 * ptrSize(), ptrSize());
        if (!candidate_text_addr_buf) {
            return false;
        }
        uint64_t candidate_text_addr = endian::Converter(endian())(candidate_text_addr_buf->data(), ptrSize());

        auto pclntab_text_addr_buf = mReader.readVirtualMemory(pclntab_address + 8 + ptrSize(), ptrSize());
        if (!pclntab_text_addr_buf) {
            return false;
        }
        uint64_t pclntab_text_addr = endian::Converter(endian())(pclntab_text_addr_buf->data(), ptrSize());

        if (candidate_text_addr != pclntab_text_addr) {
            LOG_WARNING("candidate text addr: %lx, pclntab text addr: %lx cannot match", candidate_text_addr, pclntab_text_addr);
            return false;
        }

        std::vector<std::shared_ptr<elf::ISection>> sections = mReader.sections();

        auto text_section_it = std::find_if(sections.begin(), sections.end(), [](const auto &section) {
            return section->name() == ".text";
        });
        if (text_section_it == mReader.sections().end()) {
            return false;
        }
        auto text_section = *text_section_it;

        auto min_pc_buf = mReader.readVirtualMemory(address + 10 * ptrSize(), ptrSize());
        if (!min_pc_buf) return false;
        uint64_t min_pc = endian::Converter(endian())(min_pc_buf->data(), ptrSize());

        auto max_pc_buf = mReader.readVirtualMemory(address + 11 * ptrSize(), ptrSize());
        if (!max_pc_buf) return false;
        uint64_t max_pc = endian::Converter(endian())(max_pc_buf->data(), ptrSize());

        return min_pc == text_section->address() && max_pc <= (text_section->address() + text_section->size());
    }
}


std::optional<uint64_t> go::symbol::Reader::findModuleData() {
    if (!mVersion) {
        LOG_ERROR("Cannot find moduledata without Go version");
        return std::nullopt;
    }

    if (!mSymbolTable) {
       findSymtabSymbol();
    }

    if (mSymbolTable && mSymbolTable->size() > 0) {
        auto symbolIterator = std::find_if(mSymbolTable->begin(), mSymbolTable->end(), [](const auto &symbol) {
            return symbol->name() == MODULE_DATA_SYMBOL;
        });
        if (symbolIterator != mSymbolTable->end()) {
            LOG_INFO("Found moduledata symbol: %s", symbolIterator.operator*()->name().c_str());
            return symbolIterator.operator*()->value();
        }
    }


    const auto& sections = mReader.sections();
    auto pclntab_it = std::find_if(sections.begin(), sections.end(), [](const auto &section) {
        return section->name() == SYMBOL_SECTION;
    });

    if (pclntab_it == sections.end()) {
        LOG_ERROR(".gopclntab section not found");
        return std::nullopt;
    }

    uint64_t pclntab_addr = (*pclntab_it)->address();
    PcHeader header(mReader, pclntab_addr, ptrSize());

    const char* section_names[] = {SYMBOL_RODATA_SECTION, SYMBOL_NOPTRDATA_SECTION, SYMBOL_DATA_SECTION};
    for (const char* section_name : section_names) {
        auto data_section_it = std::find_if(sections.begin(), sections.end(),
                                            [&](const auto& s){ return s->name() == section_name; });

        if (data_section_it != sections.end()) {
            auto data_section = *data_section_it;
            uint64_t search_start = data_section->address();
            uint64_t search_end = search_start + data_section->size();

            for (uint64_t current_addr = search_start; current_addr < search_end; current_addr += ptrSize()) {
                auto buffer_opt = mReader.readVirtualMemory(current_addr, ptrSize());
                if (!buffer_opt) continue;

                uint64_t value = endian::Converter(endian())(buffer_opt->data(), ptrSize());

                if (value == pclntab_addr) {
                    if (validateModuleData(current_addr, pclntab_addr)) {
                        return current_addr;
                    } else {
                        LOG_WARNING("Failed to validate moduledata at address: %lx, pclntab addr: %lx", current_addr, pclntab_addr);
                    }
                }
            }
        } else {
            LOG_WARNING("Data section not found: %s", section_name);
        }

    }

    LOG_ERROR("Failed to find valid moduledata");
    return std::nullopt;
}

std::optional<go::symbol::Reader> go::symbol::openFile(const std::filesystem::path &path) {
    std::optional<elf::Reader> reader = elf::openFile(path);

    if (!reader) {
        LOG_ERROR("open elf file failed");
        return std::nullopt;
    }

    return Reader(*reader, path);
}