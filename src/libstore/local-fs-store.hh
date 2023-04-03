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

    /**
     * Creates symlink from the `gcRoot` to the `storePath` and
     * registers the `gcRoot` as a permanent GC root. The `gcRoot`
     * symlink lives outside the store and is created and owned by the
     * user.
     *
     * @param gcRoot The location of the symlink.
     *
     * @param storePath The store object being rooted. The symlink will
     * point to `toRealPath(store.printStorePath(storePath))`.
     *
     * How the permanent GC root corresponding to this symlink is
     * managed is implementation-specific.
     */
    virtual Path addPermRoot(const StorePath & storePath, const Path & gcRoot) = 0;

    virtual Path getRealStoreDir() { return realStoreDir; }

    Path toRealPath(const Path & storePath) override
    {
        assert(isInStore(storePath));
        return getRealStoreDir() + "/" + std::string(storePath, storeDir.size() + 1);
    }

    std::optional<std::string> getBuildLogExact(const StorePath & path) override;

};

/**
 * Mix-in class for implementing permenent roots as a pair of a direct
 * (strong) reference and indirect weak reference to the first
 * reference.
 *
 * See methods for details.
 */
struct IndirectRootStore : public virtual LocalFSStore
{
    inline static std::string operationName = "Indirect GC roots registration";

    /**
     * Implementation of LocalFSStore::addPermRoot which the permanent
     * root is a pair of
     *
     * - The user-facing symlink which all implementations must create
     *
     * - An additional weak reference known as the "indirect root" that
     *   points to that symlink.
     *
     * The garbage collector will automatically remove the indirect root
     * when it finds that the symlink has disappeared.
     *
     * The implementation of this method is concrete, but it delegates
     * to addIndirectRoot() which is abstract.
     */
    Path addPermRoot(const StorePath & storePath, const Path & gcRoot) override final;

    /**
     * Add an indirect root, which is a weak reference to the
     * user-facing symlink created by addPermRoot().
     *
     * @param path user-facing and user-controlled symlink to a store
     * path.
     *
     * The form this weak-reference takes is implementation-specific.
     */
    virtual void addIndirectRoot(const Path & path) = 0;
};

}
