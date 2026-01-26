#ifndef GO_SYMBOL_READER_H
#define GO_SYMBOL_READER_H


#include <go/symbol/symbol.h>
#include <go/symbol/interface.h>
#include <go/symbol/build_info.h>
#include <go/symbol/struct.h>
#include <go/symbol/module_data.h>
#include <go/symbol/pc_header.h>
#include <elf/symbol.h>



namespace go::symbol {
    enum AccessMethod {
        FileMapping,
        AnonymousMemory,
        Attached
    };

    class Reader {
    public:
        Reader(elf::Reader reader, std::filesystem::path path);

    private:
        size_t ptrSize();

        elf::endian::Type endian();

    public:
        std::optional<Version> version();

    public:
        std::optional<BuildInfo> buildInfo();
        std::optional<seek::SymbolTable> symbols(uint64_t base = 0);
        std::optional<SymbolTable> symbols(AccessMethod method, uint64_t base = 0);
        std::optional<InterfaceTable> interfaces(uint64_t base = 0);
        std::optional<StructTable> typeLinks(uint64_t base = 0);
        std::optional<std::string> findSymtabByKey(const std::string &key);
        std::optional<uint64_t> findSymbolAddress(const std::string &key);

    private:
        void ensureVersion();
        void ensureModuleData();
        void ensureRuntimeTypesAddress();
        void ensureModuleDataObject();
        std::optional<uint64_t> findModuleData();
        bool validateModuleData(uint64_t address, uint64_t pclntab_address);
        bool findSymtabSymbol();
        std::optional<std::pair<std::shared_ptr<elf::ISection>, uint64_t>> findSectionAndBase(const std::string& sectionName, uint64_t base);



    private:
        elf::Reader mReader;
        std::filesystem::path mPath;
        std::optional<go::Version> mVersion;
        std::optional<uint64_t> mModuleDataAddress;
        bool mModuleDataSearched{false};
        std::optional<ModuleData> mModuleData;
        bool mRuntimeTypesAddressSearched{false};
        std::optional<uint64_t> mRuntimeTypesAddress;
        std::optional<elf::SymbolTable> mSymbolTable;
    };

    std::optional<Reader> openFile(const std::filesystem::path &path);
}

#endif //GO_SYMBOL_READER_H
