#ifdef PS2_PLATFORM
#include "ps2/boot/SavesPromptPS2.h"
#include "ps2/storage/save/Ps2SaveSetup.h"
#include "ps2/storage/save/Ps2MemoryCard.h"
#include "ps2/storage/save/Ps2SaveStorage.h"
#include "platform/Storage.h"
#include "platform/Log.h"
#include <vector>

namespace Ps2SaveSetup
{
void selectStorage()
{
    const bool ready = Ps2MemoryCard::initialize();
    const SaveLocation selected = ps2_show_saves_prompt();
    Ps2SaveStorage::Target target = Ps2SaveStorage::Target::Disabled;
    if (selected == SAVE_LOC_MC && ready)
        target = Ps2SaveStorage::Target::MemoryCard;
    else if (selected == SAVE_LOC_MASS && Ps2SaveStorage::available(Ps2SaveStorage::Target::MassStorage))
        target = Ps2SaveStorage::Target::MassStorage;

    Ps2SaveStorage::setTarget(target);
    Ps2SaveStorage::reportConfigurationSave(ready);
    if (!ready)
    {
        MC_LOG_WARN("save", "[PS2] Memory Card unavailable; configuration remains on mc0.\n");
        return;
    }
    const std::string config = Ps2SaveStorage::configRoot();
    PlatformStorage::mkdirs(config);
    // Non-destructive compatibility import from the old Memory Card root.
    // Existing destination files always win; USB files and world folders are untouched.
    for (const char *name : {"options.txt", "servers.dat"})
    {
        const std::string destination = PlatformStorage::join(config, name);
        std::vector<unsigned char> bytes;
        if (!PlatformStorage::exists(destination) && !PlatformStorage::exists(destination + ".pending") &&
            PlatformStorage::readFile(PlatformStorage::join("mc0:", name), bytes))
        {
            if (!Ps2SaveStorage::writeConfiguration(destination, bytes.data(), bytes.size()))
                Ps2SaveStorage::reportConfigurationSave(false);
        }
    }
}
}
#endif
