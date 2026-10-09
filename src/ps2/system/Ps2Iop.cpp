#ifdef PS2_PLATFORM

#include "platform/Log.h"
#include "ps2/system/Ps2Iop.h"
#include "ps2/storage/Ps2Storage.h"
#include "platform/storage/PathUtils.h"

#include <cstdio>
#include <string>
#include <vector>

#include <delaythread.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <unistd.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>

extern "C"
{
int __iomanX_id = -1;
int __fileXio_id = -1;
}


namespace
{

bool fileXioServerAvailable()
{
    SifRpcClientData_t client = {};
    for (int waited = 0; waited < 1000; waited += 10)
    {
        if (sceSifBindRpc(&client, FILEXIO_IRX, 0) < 0)
            return false;
        if (client.server)
            return true;
        DelayThread(10000);
    }
    return false;
}

bool loadFileXioModule(int argc, char** argv)
{
    std::vector<std::string> paths;
    if (argc > 0 && argv && argv[0] && argv[0][0] != '\0')
    {
        const std::string executablePath = PlatformStorage::normalizeSlashes(argv[0]);
        const std::string launchDirectory = PlatformStorage::parent(executablePath);
        if (!launchDirectory.empty())
            paths.push_back(PlatformStorage::join(launchDirectory, "data/irx/fileXio.irx"));
    }

    char cwd[512] = {};
    if (::getcwd(cwd, sizeof(cwd)) && cwd[0] != '\0')
        paths.push_back(PlatformStorage::join(cwd, "data/irx/fileXio.irx"));
    paths.push_back("data/irx/fileXio.irx");

    for (const std::string& path : paths)
    {
        const int result = SifLoadModule(path.c_str(), 0, nullptr);
        MC_LOG_INFO("platform", "[PS2] load %s -> %d\n", path.c_str(), result);
        if (result >= 0)
            return fileXioServerAvailable();
    }
    MC_LOG_WARN("platform", "[PS2] could not load fileXio.irx from the install data/irx directory\n");
    return false;
}

const char* romModulePath(Ps2Iop::RomModule module)
{
    switch (module)
    {
        case Ps2Iop::RomModule::Sio2: return "rom0:SIO2MAN";
        case Ps2Iop::RomModule::Pad: return "rom0:PADMAN";
        case Ps2Iop::RomModule::MemoryCardManager: return "rom0:MCMAN";
        case Ps2Iop::RomModule::MemoryCardServer: return "rom0:MCSERV";
    }
    return nullptr;
}

int moduleIndex(Ps2Iop::RomModule module)
{
    return static_cast<int>(module);
}

} // namespace

namespace Ps2Iop
{

int loadModule(const char* path)
{
    const int result = SifLoadModule(path, 0, nullptr);
    MC_LOG_INFO("platform", "[PS2] load %s -> %d\n", path, result);
    return result;
}

int ensureRomModule(RomModule module)
{
    static bool loaded[4] = {};
    static int results[4] = {};

    const int index = moduleIndex(module);
    if (loaded[index])
        return results[index];

    const char* path = romModulePath(module);
    results[index] = path ? loadModule(path) : -1;
    loaded[index] = results[index] >= 0;
    return results[index];
}

void initFileServices(int argc, char** argv)
{
    static bool initialized = false;
    if (initialized)
        return;

    SifLoadFileInit();
    if (!fileXioServerAvailable())
        loadFileXioModule(argc, argv);

    if (!fileXioServerAvailable())
        MC_LOG_WARN("platform", "[PS2] FILEXIO RPC server unavailable; skipping fileXioInit\n");
    else
    {
        const int fileXioResult = fileXioInit();
        if (fileXioResult < 0)
            MC_LOG_WARN("platform", "[PS2] fileXioInit failed: %d; OPL BDM storage may be unavailable\n",
                        fileXioResult);
        else
            MC_LOG_INFO("platform", "[PS2] fileXio initialized for POSIX filesystem access\n");
    }

    Ps2Storage::setFileIoReady();
    ensureRomModule(RomModule::Sio2);
    initialized = true;
}

} // namespace Ps2Iop

#endif // PS2_PLATFORM
