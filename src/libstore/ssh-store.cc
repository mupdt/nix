#include "ssh-store-config.hh"
#include "store-api.hh"
#include "local-fs-store.hh"
#include "remote-store.hh"
#include "remote-fs-accessor.hh"
#include "archive.hh"
#include "worker-protocol.hh"
#include "pool.hh"
#include "ssh.hh"

namespace nix {

struct SSHStoreConfig : virtual RemoteStoreConfig, virtual CommonSSHStoreConfig
{
    using RemoteStoreConfig::RemoteStoreConfig;
    using CommonSSHStoreConfig::CommonSSHStoreConfig;

    const Setting<Path> remoteProgram{(StoreConfig*) this, "nix-daemon", "remote-program",
        "Path to the `nix-daemon` executable on the remote machine."};

    const std::string name() override { return "Experimental SSH Store"; }

    std::string doc() override
    {
        return
          #include "ssh-store.md"
          ;
    }
};

class SSHStore : public virtual SSHStoreConfig, public virtual RemoteStore
{
public:

    SSHStore(const std::string & scheme, const std::string & host, const Params & params)
        : StoreConfig(params)
        , RemoteStoreConfig(params)
        , CommonSSHStoreConfig(params)
        , SSHStoreConfig(params)
        , Store(params)
        , RemoteStore(params)
        , host(host)
        , master(
            host,
            sshKey,
            sshPublicHostKey,
            // Use SSH master only if using more than 1 connection.
            connections->capacity() > 1,
            compress)
    {
    }

    static std::set<std::string> uriSchemes() { return {"ssh-ng"}; }

    std::string getUri() override
    {
        return *uriSchemes().begin() + "://" + host;
    }

    // FIXME extend daemon protocol, move implementation to RemoteStore
    std::optional<std::string> getBuildLogExact(const StorePath & path) override
    { unsupported("getBuildLogExact"); }

protected:

    struct Connection : RemoteStore::Connection
    {
        std::unique_ptr<SSHMaster::Connection> sshConn;

        void closeWrite() override
        {
            sshConn->in.close();
        }
    };

    ref<RemoteStore::Connection> openConnection() override;

    std::string host;

    std::string extraRemoteProgramArgs;

    SSHMaster master;

    void setOptions(RemoteStore::Connection & conn) override
    {
        /* TODO Add a way to explicitly ask for some options to be
           forwarded. One option: A way to query the daemon for its
           settings, and then a series of params to SSHStore like
           forward-cores or forward-overridden-cores that only
           override the requested settings.
        */
    };
};

/**
 * The mounted ssh store assumes that filesystems on the remote host are shared
 * with the local host. This means that the remote nix store is available
 * locally and is therefore treated as a local filesystem store.
 */
class MountedSSHStore : public virtual SSHStore, public virtual LocalFSStore
{
public:

    MountedSSHStore(const std::string & scheme, const std::string & host, const Params & params)
        : StoreConfig(params)
        , RemoteStoreConfig(params)
        , CommonSSHStoreConfig(params)
        , SSHStoreConfig(params)
        , Store(params)
        , RemoteStore(params)
        , SSHStore(scheme, host, params)
        , LocalFSStoreConfig(params)
        , LocalFSStore(params)
    {
        extraRemoteProgramArgs = "--process-ops --allow-perm-roots";
    }

    static std::set<std::string> uriSchemes()
    {
        return {"mounted-ssh-ng"};
    }

    std::string getUri() override
    {
        return *uriSchemes().begin() + "://" + host;
    }

    void narFromPath(const StorePath & path, Sink & sink) override
    {
        return SSHStore::narFromPath(path, sink);
    }

    ref<FSAccessor> getFSAccessor() override
    {
        return SSHStore::getFSAccessor();
    }

    std::optional<std::string> getBuildLogExact(const StorePath & path) override
    {
        return SSHStore::getBuildLogExact(path);
    }

    Path addPermRoot(const StorePath & path, const Path & gcRoot) override
    {
        auto conn(getConnection());
        conn->to << wopAddPermRoot << printStorePath(path) << gcRoot;
        conn.processStderr();
        return readString(conn->from);
    }
};

ref<RemoteStore::Connection> SSHStore::openConnection()
{
    auto conn = make_ref<Connection>();
    conn->sshConn = master.startCommand(
        fmt("%s --stdio", remoteProgram)
        + (remoteStore.get() == "" ? "" : " --store " + shellEscape(remoteStore.get()))
        + (extraRemoteProgramArgs == "" ? "" : " " + extraRemoteProgramArgs));
    conn->to = FdSink(conn->sshConn->in.get());
    conn->from = FdSource(conn->sshConn->out.get());
    return conn;
}

static RegisterStoreImplementation<SSHStore, SSHStoreConfig> regSSHStore;
static RegisterStoreImplementation<MountedSSHStore, SSHStoreConfig> regMountedSSHStore;

}
