#ifdef PS2_PLATFORM

#include "ps2/input/Ps2UsbKeyboard.h"

#include "lwjgl/Keyboard.h"
#include "platform/Log.h"
#include "ps2/system/Ps2Iop.h"
#include "ps2/system/Ps2IrxLoader.h"

#include <libkbd.h>

namespace
{
bool s_initialized = false;
bool s_ready = false;

void pushCharacter(unsigned char character)
{
    if (character == '\r' || character == '\n')
    {
        lwjgl::Keyboard::detail::pushKey(lwjgl::Keyboard::KEY_RETURN, true);
        lwjgl::Keyboard::detail::pushKey(lwjgl::Keyboard::KEY_RETURN, false);
    }
    else if (character == 8 || character == 127)
    {
        lwjgl::Keyboard::detail::pushKey(lwjgl::Keyboard::KEY_BACK, true);
        lwjgl::Keyboard::detail::pushKey(lwjgl::Keyboard::KEY_BACK, false);
    }
    else if (character >= 0x20 && character <= 0x7e)
    {
        lwjgl::Keyboard::detail::pushChar(character);
    }
}
}

namespace Ps2UsbKeyboard
{

bool initialize()
{
    if (s_initialized)
        return s_ready;
    s_initialized = true;

    const int usbHost = Ps2Iop::loadModule("rom0:USBD");
    if (usbHost < 0)
    {
        const int fallback = Ps2IrxLoader::load("irx/usbd.irx");
        MC_LOG_INFO("input", "[PS2] USB host module: rom=%d packaged=%d\n", usbHost, fallback);
    }

    const int keyboardDriver = Ps2IrxLoader::load("irx/ps2kbd.irx");
    const int initResult = PS2KbdInit();
    s_ready = initResult > 0;
    if (s_ready)
    {
        PS2KbdSetBlockingMode(PS2KBD_NONBLOCKING);
        MC_LOG_INFO("input", "[PS2] USB keyboard ready (driver=%d init=%d)\n",
                    keyboardDriver, initResult);
    }
    else
    {
        MC_LOG_INFO("input", "[PS2] USB keyboard unavailable (driver=%d init=%d)\n",
                    keyboardDriver, initResult);
    }
    return s_ready;
}

void poll()
{
    if (!s_ready)
        return;

    for (int count = 0; count < 32; ++count)
    {
        char character = 0;
        if (PS2KbdRead(&character) <= 0)
            break;
        pushCharacter(static_cast<unsigned char>(character));
    }
}

}

#endif // PS2_PLATFORM