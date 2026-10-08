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
bool s_loggedFirstCharacter = false;
bool s_loggedReadError = false;

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

    int initResult = PS2KbdInit();
    int usbHost = -1;
    int keyboardDriver = -1;
    if (initResult <= 0)
    {
        usbHost = Ps2Iop::loadModule("rom0:USBD");
        keyboardDriver = Ps2IrxLoader::load("irx/ps2kbd.irx");
        initResult = PS2KbdInit();
    }

    if (initResult <= 0)
    {
        const int fallbackHost = Ps2IrxLoader::load("irx/usbd.irx");
        if (fallbackHost >= 0)
        {
            keyboardDriver = Ps2IrxLoader::load("irx/ps2kbd.irx");
            initResult = PS2KbdInit();
        }
        MC_LOG_ERROR("input", "[PS2] USB keyboard retry: rom-usbd=%d fallback-usbd=%d driver=%d init=%d\n",
                     usbHost, fallbackHost, keyboardDriver, initResult);
    }

    s_ready = initResult > 0;
    if (s_ready)
    {
        PS2KbdSetBlockingMode(PS2KBD_NONBLOCKING);
        MC_LOG_INFO("input", "[PS2] USB keyboard ready (driver=%d init=%d)\n",
                    keyboardDriver, initResult);
    }
    else
    {
        MC_LOG_ERROR("input", "[PS2] USB keyboard unavailable (driver=%d init=%d)\n",
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
        const int result = PS2KbdRead(&character);
        if (result < 0)
        {
            if (!s_loggedReadError)
            {
                MC_LOG_WARN("input", "[PS2] USB keyboard read failed: %d\n", result);
                s_loggedReadError = true;
            }
            break;
        }
        if (result == 0)
            break;
        if (!s_loggedFirstCharacter)
        {
            MC_LOG_INFO("input", "[PS2] USB keyboard received byte 0x%02X\n",
                        static_cast<unsigned char>(character));
            s_loggedFirstCharacter = true;
        }
        pushCharacter(static_cast<unsigned char>(character));
    }
}

}

#endif // PS2_PLATFORM