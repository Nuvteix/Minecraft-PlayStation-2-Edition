#ifdef PS2_PLATFORM

#include "platform/Log.h"
#include "ps2/system/Ps2Iop.h"
#include "ps2/storage/Ps2Storage.h"

#include <cstdio>

#include <loadfile.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>

extern "C"
{
int __iomanX_id = -1;
int __fileXio_id = -1;
}


namespace
{

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

void initFileServices()
{
    static bool initialized = false;
    if (initialized)
        return;

    SifLoadFileInit();
    const int fileXioResult = fileXioInit();
    if (fileXioResult < 0)
        MC_LOG_WARN("platform", "[PS2] fileXioInit failed: %d; OPL BDM storage may be unavailable\n",
                    fileXioResult);
    else
        MC_LOG_INFO("platform", "[PS2] fileXio initialized for POSIX filesystem access\n");

    Ps2Storage::setFileIoReady();
    ensureRomModule(RomModule::Sio2);
    initialized = true;
}

} // namespace Ps2Iop

#endif // PS2_PLATFORM
