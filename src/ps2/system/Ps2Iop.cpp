#ifdef PS2_PLATFORM

#include "platform/Log.h"
#include "ps2/system/Ps2Iop.h"
#include "ps2/storage/Ps2Storage.h"
#include "ps2/storage/assets/Ps2AssetLocator.h"

#include <cstdio>

#include <delaythread.h>
#include <iopcontrol.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <sifrpc.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>

extern "C"
{
extern unsigned char iomanx_irx[];
extern unsigned int size_iomanx_irx;
extern unsigned char filexio_irx[];
extern unsigned int size_filexio_irx;
#if defined(PS2_ENABLE_MX4SIO)
extern unsigned char usbd_irx[];
extern unsigned int size_usbd_irx;
extern unsigned char usbmass_bd_irx[];
extern unsigned int size_usbmass_bd_irx;
extern unsigned char sio2man_irx[];
extern unsigned int size_sio2man_irx;
extern unsigned char bdm_irx[];
extern unsigned int size_bdm_irx;
extern unsigned char bdmfs_fatfs_irx[];
extern unsigned int size_bdmfs_fatfs_irx;
extern unsigned char mx4sio_bd_irx[];
extern unsigned int size_mx4sio_bd_irx;
#endif
}

extern "C"
{
int __iomanX_id = -1;
int __fileXio_id = -1;
}


namespace
{

void resetIopForElfHandoff()
{
    MC_LOG_INFO("platform", "[PS2] resetting IOP to clear launcher driver state\n");
    while (!SifIopReset(nullptr, 0))
    {
        MC_LOG_WARN("platform", "[PS2] IOP reset request failed; retrying\n");
        DelayThread(100000);
    }

    while (!SifIopSync())
        DelayThread(1000);

    SifInitRpc(0);
    MC_LOG_INFO("platform", "[PS2] IOP reset complete; RPC restarted\n");
}

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

int loadEmbeddedIrx(const char* name, unsigned char* data, unsigned int size)
{
    int moduleResult = 1;
    const int result = SifExecModuleBuffer(data, static_cast<int>(size), 0, nullptr, &moduleResult);
    const bool loaded = result >= 0 && moduleResult >= 0 && moduleResult != 1;
    MC_LOG_INFO("platform", "[PS2] embedded %s -> load=%d start=%d\n",
                name, result, moduleResult);
    return loaded ? result : -1;
}

bool loadFileIoModules()
{
    if (fileXioServerAvailable())
    {
        Ps2AssetLocator::addDiagnostic("FILEXIO service: already available");
        return true;
    }

    loadEmbeddedIrx("iomanX", iomanx_irx, size_iomanx_irx);
    loadEmbeddedIrx("fileXio", filexio_irx, size_filexio_irx);
    const bool available = fileXioServerAvailable();
    Ps2AssetLocator::addDiagnostic(available
        ? "FILEXIO service: ready"
        : "FILEXIO service: unavailable");
    return available;
}

#if defined(PS2_ENABLE_MX4SIO)
bool loadMx4sioModules()
{
    const Ps2Storage::MassStorageProbe mounted = Ps2Storage::probeMassStorage(true);
    if (!mounted.root.empty())
    {
        MC_LOG_INFO("platform", "[PS2] using existing BDM storage at %s\n",
                    mounted.root.c_str());
        Ps2AssetLocator::addDiagnostic("Existing BDM mount: " + mounted.root);
        return true;
    }

    const int sio2 = loadEmbeddedIrx("sio2man", sio2man_irx, size_sio2man_irx);
    const int bdm = loadEmbeddedIrx("bdm", bdm_irx, size_bdm_irx);
    const int filesystem = loadEmbeddedIrx(
        "bdmfs_fatfs (FAT/exFAT)", bdmfs_fatfs_irx, size_bdmfs_fatfs_irx);
    const int usbd = loadEmbeddedIrx("usbd", usbd_irx, size_usbd_irx);
    const int usbMass = loadEmbeddedIrx("usbmass_bd", usbmass_bd_irx, size_usbmass_bd_irx);
    const int mx4sio = loadEmbeddedIrx("mx4sio_bd", mx4sio_bd_irx, size_mx4sio_bd_irx);
    const bool stackLoaded = sio2 >= 0 && bdm >= 0 && filesystem >= 0 && usbd >= 0 &&
                             usbMass >= 0 && mx4sio >= 0;

    for (int waited = 0; waited < 3000; waited += 100)
    {
        const Ps2Storage::MassStorageProbe probe = Ps2Storage::probeMassStorage(true);
        if (!probe.root.empty())
        {
            MC_LOG_INFO("platform", "[PS2] MX4SIO mounted at %s\n", probe.root.c_str());
            Ps2AssetLocator::addDiagnostic("BDM mounted: " + probe.root);
            return true;
        }
        DelayThread(100000);
    }

    MC_LOG_WARN("platform", "[PS2] BDM/MX4SIO driver stack %s; no massN: volume appeared\n",
                stackLoaded ? "loaded" : "had module errors");
    Ps2AssetLocator::addDiagnostic(stackLoaded
        ? "BDM FAT/exFAT + USB/MX4SIO IRXs loaded; no massN: device"
        : "BDM FAT/exFAT + USB/MX4SIO IRX load error");
    return false;
}
#endif

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

    // Launchers can leave IOP filesystem and mass-storage modules in a state
    // that accepts path probes but stalls on real file reads. The ELF is
    // already loaded into EE memory, so reset the IOP and load our own drivers
    // from the embedded IRXs instead of inheriting the launcher's stack.
    resetIopForElfHandoff();

    SifLoadFileInit();
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();

    if (!loadFileIoModules())
        MC_LOG_WARN("platform", "[PS2] FILEXIO RPC server unavailable\n");
    else
    {
        const int fileXioResult = fileXioInit();
        if (fileXioResult < 0)
            MC_LOG_WARN("platform", "[PS2] fileXioInit failed: %d; OPL BDM storage may be unavailable\n",
                        fileXioResult);
        else
            MC_LOG_INFO("platform", "[PS2] fileXio initialized for POSIX filesystem access\n");
    }

#if defined(PS2_ENABLE_MX4SIO)
    // Bring up BDM only after the display is live, and before PADMAN claims SIO2.
    loadMx4sioModules();
#endif

    Ps2Storage::setFileIoReady();
    initialized = true;
}

} // namespace Ps2Iop

#endif // PS2_PLATFORM
