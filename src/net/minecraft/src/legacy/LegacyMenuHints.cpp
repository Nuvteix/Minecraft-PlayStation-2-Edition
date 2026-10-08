#include "LegacyMenuHints.h"
#include "net/minecraft/src/ControlIcon.h"
#include "net/minecraft/src/UiStrings.h"
#include "platform/PlatformConfig.h"

void drawLegacyMenuHints(Minecraft *mc, int_t screenWidth, int_t screenHeight, bool showBack,
    const char *extraButton, const char *extraAction)
{
#if PLATFORM_PS2
    const std::string defaults[] = {"D-Pad", "Cross", "Circle"};
#elif PLATFORM_WII
    const std::string defaults[] = {"D-Pad", "A", "B"};
#else
    const std::string defaults[] = {"Up/Down", "Enter", "Esc"};
#endif
    std::string buttons[4] = {defaults[0], defaults[1], defaults[2], ""};
    std::string actions[4] = {uiText("Navigate"), uiText("Select"), uiText("Back"), ""};
    int_t count = showBack ? 3 : 2;
    if (extraButton != nullptr && extraAction != nullptr && count < 4)
    {
        buttons[count] = extraButton;
        actions[count] = uiText(extraAction);
        ++count;
    }
    drawControlHintRow(mc, screenWidth, legacyHintRowY(screenHeight), buttons, actions, count);
}
