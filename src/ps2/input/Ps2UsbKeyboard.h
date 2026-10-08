#pragma once

#ifdef PS2_PLATFORM

namespace Ps2UsbKeyboard
{
    bool initialize();
    void poll();
}

#endif // PS2_PLATFORM