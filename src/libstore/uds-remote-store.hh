#pragma once
///@file

#include "remote-store.hh"
#include "local-fs-store.hh"

namespace nix {

struct UDSRemoteStoreConfig : virtual LocalFSStoreConfig, virtual RemoteStoreConfig
{
    UDSRemoteStoreConfig(const Store::Params & params)
        : StoreConfig(params)
        , LocalFSStoreConfig(params)
        , RemoteStoreConfig(params)
    {
    }

    const std::string name() override { return "Local Daemon Store"; }

    std::string doc() override
    {
        return
          #include "uds-remote-store.md"
          ;
    }
};

class UDSRemoteStore : public virtual UDSRemoteStoreConfig
    , public virtual IndirectRootStore
    , public virtual RemoteStore
{
public:

    UDSRemoteStore(const Params & params);
    UDSRemoteStore(const std::string scheme, std::string path, const Params & params);

    std::string getUri() override;

    static std::set<std::string> uriSchemes()
    { return {"unix"}; }

    ref<FSAccessor> getFSAccessor() override
    { return LocalFSStore::getFSAccessor(); }

    void narFromPath(const StorePath & path, Sink & sink) override
    { LocalFSStore::narFromPath(path, sink); }

    /**
     * Implementation of IndirectRootStore::addIndirectRoot() which
     * delegates to the remote store.
     *
     * The idea is that the client makes the direct symlink, so it is
     * owned managed by the client's user account, and the server makes
     * the indirect symlink.
     */
    void addIndirectRoot(const Path & path) override;

private:

    struct Connection : RemoteStore::Connection
    {
        AutoCloseFD fd;
        void closeWrite() override;
    };

    ref<RemoteStore::Connection> openConnection() override;
    std::optional<std::string> path;
};

}
