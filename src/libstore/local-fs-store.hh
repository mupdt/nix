#pragma once
///@file

#include "store-api.hh"
#include "gc-store.hh"
#include "log-store.hh"

namespace nix {

struct LocalFSStoreConfig : virtual StoreConfig
{
    using StoreConfig::StoreConfig;

    // FIXME: the (StoreConfig*) cast works around a bug in gcc that causes
    // it to omit the call to the Setting constructor. Clang works fine
    // either way.

    const PathSetting rootDir{(StoreConfig*) this, true, "",
        "root",
        "Directory prefixed to all other paths."};

    const PathSetting stateDir{(StoreConfig*) this, false,
        rootDir != "" ? rootDir + "/nix/var/nix" : settings.nixStateDir,
        "state",
        "Directory where Nix will store state."};

    const PathSetting logDir{(StoreConfig*) this, false,
        rootDir != "" ? rootDir + "/nix/var/log/nix" : settings.nixLogDir,
        "log",
        "directory where Nix will store log files."};

    const PathSetting realStoreDir{(StoreConfig*) this, false,
        rootDir != "" ? rootDir + "/nix/store" : storeDir, "real",
        "Physical path of the Nix store."};
};

class LocalFSStore : public virtual LocalFSStoreConfig,
    public virtual Store,
    public virtual GcStore,
    public virtual LogStore
{
public:
    inline static std::string operationName = "Local Filesystem Store";

    const static std::string drvsLogDir;

    LocalFSStore(const Params & params);

    void narFromPath(const StorePath & path, Sink & sink) override;
    ref<FSAccessor> getFSAccessor() override;

    /* Creates the `gcRoot` symlink to the `storePath` and registers
       the `gcRoot` as a permanent GC root. The `gcRoot` symlink lives
       outside the store and is created and owned by the user. */
    virtual Path addPermRoot(const StorePath & storePath, const Path & gcRoot) = 0;

    virtual Path getRealStoreDir() { return realStoreDir; }

    Path toRealPath(const Path & storePath) override
    {
        assert(isInStore(storePath));
        return getRealStoreDir() + "/" + std::string(storePath, storeDir.size() + 1);
    }

    std::optional<std::string> getBuildLogExact(const StorePath & path) override;

};

struct IndirectRootStore : public virtual LocalFSStore
{
    inline static std::string operationName = "Indirect GC roots registration";

    /* Add an indirect root, which is merely a symlink to `path' from
       /nix/var/nix/gcroots/auto/<hash of `path'>.  `path' is supposed
       to be a symlink to a store path.  The garbage collector will
       automatically remove the indirect root when it finds that
       `path' has disappeared. */
    virtual void addIndirectRoot(const Path & path) = 0;
};


struct LocalPermRootStore : public virtual IndirectRootStore
{
    inline static std::string operationName = "Local permanent GC roots registration";

    Path addPermRoot(const StorePath & storePath, const Path & gcRoot) override;
};

}
