#include "LegacyControllerLayoutScreen.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "LegacyGuiButton.h"
#include "net/minecraft/src/FontRenderer.h"
#include "net/minecraft/src/GameSettings.h"
#include "net/minecraft/src/GuiButton.h"
#include "net/minecraft/src/KeyBinding.h"
#include "net/minecraft/src/Minecraft.h"
#include "net/minecraft/src/SoundManager.h"
#include "net/minecraft/src/UiStrings.h"
#include "LegacyMenuHints.h"
#include "pc/lwjgl/Keyboard.h"
#include "platform/Input.h"
#include "platform/PlatformConfig.h"

#if PLATFORM_PS2
#include "ps2/input/Ps2PadKeyCodes.h"
#include "ps2/input/Ps2PadState.h"
#endif

namespace
{
constexpr int_t BUTTON_ACTION_BASE = 7300;
constexpr int_t BUTTON_RESET = 7400;
constexpr int_t BUTTON_BACK = 7401;

bool pointInside(int_t x, int_t y, int_t left, int_t top, int_t width, int_t height)
{
    return x >= left && y >= top && x < left + width && y < top + height;
}

std::string capturePrompt()
{
#if PLATFORM_PS2
    return uiText("Press a button...");
#else
    return uiText("Press a key...");
#endif
}

bool bindableControllerKey(int_t key)
{
#if PLATFORM_PS2
    switch (key)
    {
    case PS2_KEY_CROSS:
    case PS2_KEY_CIRCLE:
    case PS2_KEY_TRIANGLE:
    case PS2_KEY_SQUARE:
    case PS2_KEY_L1:
    case PS2_KEY_R1:
    case PS2_KEY_L2:
    case PS2_KEY_R2:
    case PS2_KEY_L3:
    case PS2_KEY_R3:
    case PS2_KEY_DPAD_UP:
    case PS2_KEY_DPAD_DOWN:
    case PS2_KEY_DPAD_LEFT:
    case PS2_KEY_DPAD_RIGHT:
    case PS2_KEY_SELECT:
        return true;
    default:
        break;
    }
#endif
    return false;
}

void drawPanelTitle(FontRenderer *font, const std::string &text, int_t centerX, int_t y)
{
    if (font == nullptr)
        return;

    const int_t x = centerX - font->getStringWidth(text) / 2;
    font->drawString(text, x + 1, y + 1, 0xd0d0d0);
    font->drawString(text, x, y, 0x303030);
}
} // namespace

LegacyControllerLayoutScreen::LegacyControllerLayoutScreen(GuiScreen *parent, GameSettings *settings,
    LegacyOptionsBackgroundMode backgroundMode)
    : LegacyOptionsScreen(parent, settings, backgroundMode),
            resetButton(nullptr), backButton(nullptr), captureBindingIndex(-1)
{
}

void LegacyControllerLayoutScreen::initGui()
{
    clearControlList();
    actionSlots.clear();
    captureBindingIndex = -1;
    platformSetPadRebindExclusive(false);
    const int_t layoutRows = std::max<int_t>(1, std::min<int_t>(10,
        legacyOptionsMaxRows(width, height, LegacyOptionsLayoutPreset::Wide)));
    configureLegacyLayout(layoutRows, true, LegacyOptionsLayoutPreset::Wide);

    // Widen this screen for two readable columns while preserving the footer.
    const int_t originalPanelWidth = legacyLayout.panelWidth;
    const int_t widenedPanelWidth = std::min<int_t>(std::max<int_t>(1, width - 12), originalPanelWidth + 48);
    legacyLayout.panelWidth = std::max<int_t>(originalPanelWidth, widenedPanelWidth);
    legacyLayout.panelX = (width - legacyLayout.panelWidth) / 2;
    legacyLayout.contentX = legacyLayout.panelX + 8;
    legacyLayout.contentWidth = std::max<int_t>(1, legacyLayout.panelWidth - 16);

    const int_t panelTop = legacyLayout.panelY;
    const int_t panelBottom = panelTop + legacyLayout.panelHeight;
    const int_t contentX = legacyLayout.contentX;
    const int_t contentW = legacyLayout.contentWidth;

    const int_t columnGap = 8;
    const int_t buttonW = std::max<int_t>(48, (contentW - columnGap) / 2);
    const int_t buttonH = 10;
    const int_t rowGap = 1;
    const int_t leftX = contentX;
    const int_t rightX = contentX + contentW - buttonW;
    const int_t buttonTop = panelTop + 36;

    std::vector<int_t> bindings;
    auto appendBinding = [&](KeyBinding *binding)
    {
        if (binding == nullptr || settings == nullptr)
            return;
        for (int_t i = 0; i < static_cast<int_t>(settings->keyBindings.size()); ++i)
        {
            if (settings->keyBindings[i] == binding && std::find(bindings.begin(), bindings.end(), i) == bindings.end())
            {
                bindings.push_back(i);
                break;
            }
        }
    };

    // Movement stays on the analog stick; list only button-driven actions.
    appendBinding(settings->keyBindUseItem);
    appendBinding(settings->keyBindAttack);
    appendBinding(settings->keyBindJump);
    appendBinding(settings->keyBindInventory);
    if (settings->legacyCrafting)
        appendBinding(settings->keyBindCrafting);
    appendBinding(settings->keyBindDrop);
    appendBinding(settings->keyBindSneak);
    appendBinding(settings->keyBindSprint);
    appendBinding(settings->keyBindChat);
    appendBinding(settings->keyBindPlayerList);
    appendBinding(settings->keyBindDebug);
    appendBinding(settings->keyBindCycleItemLeft);
    appendBinding(settings->keyBindCycleItemRight);
    appendBinding(settings->keyBindTogglePerspective);

    const int_t leftCount = (static_cast<int_t>(bindings.size()) + 1) / 2;
    for (int_t i = 0; i < static_cast<int_t>(bindings.size()); ++i)
    {
        const bool leftSide = i < leftCount;
        const int_t row = leftSide ? i : i - leftCount;
        GuiButton *button = new LegacyGuiButton(BUTTON_ACTION_BASE + i,
            leftSide ? leftX : rightX,
            buttonTop + row * (buttonH + rowGap), buttonW, buttonH, "");
        controlList.push_back(button);

        ActionSlot slot;
        slot.bindingIndex = bindings[i];
        slot.button = button;
        actionSlots.push_back(slot);
    }

    const int_t footerY = panelBottom - legacyLayout.rowHeight - 4;
    const int_t gap = 2;
    const int_t halfWidth = (contentW - gap) / 2;
    resetButton = new LegacyGuiButton(BUTTON_RESET, contentX, footerY, halfWidth,
        legacyLayout.rowHeight, uiText("Reset"));
    backButton = new LegacyGuiButton(BUTTON_BACK, contentX + halfWidth + gap, footerY,
        contentW - halfWidth - gap, legacyLayout.rowHeight, uiText("Back"));
    controlList.push_back(resetButton);
    controlList.push_back(backButton);

    hoveredControlIndex = -1;
    selectedControlIndex = 0;
    refreshLabels();
    syncSelectedControl();
}

int_t LegacyControllerLayoutScreen::currentKeyCodeForSlot(const ActionSlot &slot) const
{
    if (settings == nullptr || slot.bindingIndex < 0 ||
        slot.bindingIndex >= static_cast<int_t>(settings->keyBindings.size()) ||
        settings->keyBindings[slot.bindingIndex] == nullptr)
        return 0;
    return settings->keyBindings[slot.bindingIndex]->keyCode;
}

int_t LegacyControllerLayoutScreen::visualPadKeyForSlot(const ActionSlot &slot) const
{
    const int_t keyCode = currentKeyCodeForSlot(slot);
#if PLATFORM_PS2
    if (keyCode >= PS2_KEY_CROSS && keyCode < PS2_KEY_SENTINEL_END)
        return keyCode;

    // Attack/use keep the original mouse-button defaults for gameplay, while
    // the PS2 input layer exposes them physically on R2/L2.
    if (settings != nullptr && slot.bindingIndex >= 0 &&
        slot.bindingIndex < static_cast<int_t>(settings->keyBindings.size()))
    {
        KeyBinding *binding = settings->keyBindings[slot.bindingIndex];
        if (binding == settings->keyBindAttack && keyCode == -100)
            return PS2_KEY_R2;
        if (binding == settings->keyBindUseItem && keyCode == -99)
            return PS2_KEY_L2;
    }
#endif
    return keyCode;
}

bool LegacyControllerLayoutScreen::actionHasConflict(int_t bindingIndex) const
{
    if (settings == nullptr || bindingIndex < 0 ||
        bindingIndex >= static_cast<int_t>(settings->keyBindings.size()) ||
        settings->keyBindings[bindingIndex] == nullptr)
        return false;

    const int_t keyCode = settings->keyBindings[bindingIndex]->keyCode;
    if (keyCode == 0)
        return false;

    for (int_t i = 0; i < static_cast<int_t>(settings->keyBindings.size()); ++i)
    {
        if (i != bindingIndex && settings->keyBindings[i] != nullptr && settings->keyBindings[i]->keyCode == keyCode)
            return true;
    }
    return false;
}

void LegacyControllerLayoutScreen::refreshLabels()
{
    for (const ActionSlot &slot : actionSlots)
    {
        if (slot.button == nullptr || settings == nullptr || slot.bindingIndex < 0 ||
            slot.bindingIndex >= static_cast<int_t>(settings->keyBindings.size()))
            continue;

        const int_t keyCode = visualPadKeyForSlot(slot);
        std::string buttonName = keyCode == 0
            ? uiText("Unbound")
            : settings->getKeyDisplayString(keyCode);
#if PLATFORM_PS2
        if (const char *padName = ps2PadKeyName(keyCode))
            buttonName = padName;
#endif
        std::string label = settings->getKeyBindingDescription(slot.bindingIndex) + ": " + buttonName;
        if (actionHasConflict(slot.bindingIndex))
            label = std::string("\xc2\xa7" "c") + label;
        if (fontRenderer != nullptr)
            label = fontRenderer->trimStringToWidth(label, slot.button->getButtonWidth() - 6);
        slot.button->displayString = label;
    }
}

int_t LegacyControllerLayoutScreen::selectionForButton(const GuiButton *button) const
{
    for (int_t i = 0; i < static_cast<int_t>(actionSlots.size()); ++i)
        if (actionSlots[i].button == button)
            return i;
    if (button == resetButton)
        return static_cast<int_t>(actionSlots.size());
    if (button == backButton)
        return static_cast<int_t>(actionSlots.size()) + 1;
    return -1;
}

GuiButton *LegacyControllerLayoutScreen::buttonForSelection(int_t index) const
{
    if (index >= 0 && index < static_cast<int_t>(actionSlots.size()))
        return actionSlots[index].button;
    if (index == static_cast<int_t>(actionSlots.size()))
        return resetButton;
    if (index == static_cast<int_t>(actionSlots.size()) + 1)
        return backButton;
    return nullptr;
}

bool LegacyControllerLayoutScreen::isSelectionAvailable(int_t index) const
{
    GuiButton *button = buttonForSelection(index);
    return button != nullptr && button->enabled && button->enabled2;
}

void LegacyControllerLayoutScreen::syncSelectedControl()
{
    for (GuiButton *button : controlList)
    {
        if (button != nullptr)
            button->setKeyboardSelected(false);
    }

    if (hoveredControlIndex >= 0)
    {
        selectedControlIndex = hoveredControlIndex;
        hoveredControlIndex = -1;
        syncSelectedControl();
        return;
    }

    if (!isSelectionAvailable(selectedControlIndex))
    {
        for (int_t i = 0; i < static_cast<int_t>(actionSlots.size()) + 2; ++i)
        {
            if (isSelectionAvailable(i))
            {
                selectedControlIndex = i;
                break;
            }
        }
    }

    GuiButton *button = buttonForSelection(selectedControlIndex);
    if (button != nullptr)
        button->setKeyboardSelected(true);
}

void LegacyControllerLayoutScreen::updatePointerHover(int_t mouseX, int_t mouseY)
{
    int_t hover = -1;
#if PLATFORM_PS2
    mouseX = mouseY = -10000;
#endif
    for (GuiButton *button : controlList)
    {
        if (button == nullptr || !button->enabled2)
            continue;
        if (pointInside(mouseX, mouseY, button->xPosition, button->yPosition,
            button->getButtonWidth(), button->getButtonHeight()))
        {
            hover = selectionForButton(button);
            break;
        }
    }

    if (hoveredControlIndex != hover)
    {
        hoveredControlIndex = hover;
        if (hoveredControlIndex >= 0)
            selectedControlIndex = hoveredControlIndex;
        syncSelectedControl();
    }
}

void LegacyControllerLayoutScreen::moveSelection(int_t dirX, int_t dirY)
{
    if (hoveredControlIndex >= 0)
    {
        selectedControlIndex = hoveredControlIndex;
        hoveredControlIndex = -1;
        syncSelectedControl();
    }

    if (!isSelectionAvailable(selectedControlIndex))
        syncSelectedControl();

    GuiButton *current = buttonForSelection(selectedControlIndex);
    if (current == nullptr)
        return;

    const int_t currentCx = current->xPosition + current->getButtonWidth() / 2;
    const int_t currentCy = current->yPosition + current->getButtonHeight() / 2;
    int_t bestIndex = -1;
    long bestScore = 0;

    for (int_t i = 0; i < static_cast<int_t>(actionSlots.size()) + 2; ++i)
    {
        if (i == selectedControlIndex || !isSelectionAvailable(i))
            continue;

        GuiButton *candidate = buttonForSelection(i);
        const int_t candidateCx = candidate->xPosition + candidate->getButtonWidth() / 2;
        const int_t candidateCy = candidate->yPosition + candidate->getButtonHeight() / 2;
        const int_t dx = candidateCx - currentCx;
        const int_t dy = candidateCy - currentCy;

        if (dirX < 0 && dx >= -4)
            continue;
        if (dirX > 0 && dx <= 4)
            continue;
        if (dirY < 0 && dy >= -4)
            continue;
        if (dirY > 0 && dy <= 4)
            continue;

        const long primary = dirX != 0 ? std::labs(dx) : std::labs(dy);
        const long secondary = dirX != 0 ? std::labs(dy) : std::labs(dx);
        const long score = primary * primary + secondary * secondary * 4L;
        if (bestIndex < 0 || score < bestScore)
        {
            bestIndex = i;
            bestScore = score;
        }
    }

    if (bestIndex >= 0)
    {
        selectedControlIndex = bestIndex;
        syncSelectedControl();
        if (mc != nullptr && mc->sndManager != nullptr)
            mc->sndManager->playSoundFX("random.focus", 1.0f, 1.0f);
    }
}

void LegacyControllerLayoutScreen::activateSelection()
{
    const int_t index = hoveredControlIndex >= 0 ? hoveredControlIndex : selectedControlIndex;
    GuiButton *button = buttonForSelection(index);
    if (button == nullptr || !button->enabled || !button->enabled2)
        return;

    if (mc != nullptr && mc->sndManager != nullptr)
        mc->sndManager->playSoundFX("random.action", 1.0f, 1.0f);
    actionPerformed(button);
}

void LegacyControllerLayoutScreen::beginCapture(int_t bindingIndex)
{
    if (settings == nullptr || bindingIndex < 0 ||
        bindingIndex >= static_cast<int_t>(settings->keyBindings.size()))
        return;

    captureBindingIndex = bindingIndex;
    platformSetPadRebindExclusive(true);
}

void LegacyControllerLayoutScreen::cancelCapture()
{
    if (captureBindingIndex < 0)
        return;
    captureBindingIndex = -1;
    platformSetPadRebindExclusive(false);
}

void LegacyControllerLayoutScreen::assignActionToCode(int_t keyCode, int_t bindingIndex)
{
    if (settings == nullptr || bindingIndex < 0 ||
        bindingIndex >= static_cast<int_t>(settings->keyBindings.size()))
        return;

    KeyBinding *binding = settings->keyBindings[bindingIndex];
    if (binding == nullptr)
        return;

    const int_t oldCode = binding->keyCode;
    if (oldCode == keyCode)
        return;

    int_t previousOwner = -1;
    for (int_t i = 0; i < static_cast<int_t>(settings->keyBindings.size()); ++i)
    {
        if (i != bindingIndex && settings->keyBindings[i] != nullptr &&
            settings->keyBindings[i]->keyCode == keyCode)
        {
            previousOwner = i;
            break;
        }
    }

    // Swap the two actions rather than leaving duplicate physical buttons.
    if (previousOwner >= 0)
        settings->setKeyBinding(previousOwner, oldCode);
    settings->setKeyBinding(bindingIndex, keyCode);
}

void LegacyControllerLayoutScreen::applyCapturedKey(int_t keyCode)
{
    if (captureBindingIndex < 0 || !bindableControllerKey(keyCode))
        return;

    assignActionToCode(keyCode, captureBindingIndex);
    captureBindingIndex = -1;
    platformSetPadRebindExclusive(false);
    refreshLabels();
}

void LegacyControllerLayoutScreen::resetDefaults()
{
    cancelCapture();
    if (settings == nullptr)
        return;
    settings->resetControlBindingsToDefaults();
    refreshLabels();
}

void LegacyControllerLayoutScreen::unbindSelectedAction()
{
    if (settings == nullptr || selectedControlIndex < 0 ||
        selectedControlIndex >= static_cast<int_t>(actionSlots.size()))
        return;

    settings->setKeyBinding(actionSlots[selectedControlIndex].bindingIndex, 0);
    refreshLabels();
}

void LegacyControllerLayoutScreen::actionPerformed(GuiButton *button)
{
    if (button == nullptr || !button->enabled)
        return;

    const int_t selection = selectionForButton(button);
    if (selection >= 0)
        selectedControlIndex = selection;

    if (button == resetButton)
    {
        resetDefaults();
        return;
    }
    if (button == backButton)
    {
        cancelCapture();
        returnToParent();
        return;
    }

    for (const ActionSlot &slot : actionSlots)
    {
        if (slot.button == button)
        {
            beginCapture(slot.bindingIndex);
            return;
        }
    }
}

void LegacyControllerLayoutScreen::keyTyped(char_t c, int_t key)
{
    (void)c;
    if (captureBindingIndex >= 0)
    {
        applyCapturedKey(key);
        return;
    }

    if (key == lwjgl::Keyboard::KEY_ESCAPE)
    {
        if (mc != nullptr && mc->sndManager != nullptr)
            mc->sndManager->playSoundFX("random.back", 1.0f, 1.0f);
        returnToParent();
        return;
    }
#if !PLATFORM_PS2 && !PLATFORM_WII
    if (key == lwjgl::Keyboard::KEY_UP)
    {
        moveSelection(0, -1);
        return;
    }
    if (key == lwjgl::Keyboard::KEY_DOWN)
    {
        moveSelection(0, 1);
        return;
    }
    if (key == lwjgl::Keyboard::KEY_LEFT)
    {
        moveSelection(-1, 0);
        return;
    }
    if (key == lwjgl::Keyboard::KEY_RIGHT)
    {
        moveSelection(1, 0);
        return;
    }
    if (key == lwjgl::Keyboard::KEY_RETURN || c == '\r')
    {
        activateSelection();
        return;
    }
#endif
}

void LegacyControllerLayoutScreen::mouseClicked(int_t x, int_t y, int_t button)
{
#if !PLATFORM_PS2 && !PLATFORM_WII
    if (captureBindingIndex >= 0)
    {
        applyCapturedKey(-100 + button);
        return;
    }
#else
    if (captureBindingIndex >= 0)
        return;
#endif
    GuiScreen::mouseClicked(x, y, button);
}

void LegacyControllerLayoutScreen::updateScreen()
{
    if (captureBindingIndex >= 0)
    {
        GuiScreen::updateScreen();
        syncSelectedControl();
#if PLATFORM_PS2
        // Start is deliberately not bindable on PS2, so it is a safe way to
        // leave capture without sacrificing Circle or any other remappable key.
        const Ps2PadSnapshot &pad = ps2PadGetSnapshot(platformMenuPad());
        if ((pad.pressed & PS2_PAD_TRIANGLE) != 0 && captureBindingIndex >= 0)
        {
            applyCapturedKey(PS2_KEY_TRIANGLE);
            ps2PadClearLatchedPressed(platformMenuPad());
        }
        if ((pad.pressed & PS2_PAD_START) != 0)
            cancelCapture();
#endif
        return;
    }

    GuiScreen::updateScreen();
    syncSelectedControl();

#if PLATFORM_PS2
    const Ps2PadSnapshot &padSnapshot = ps2PadGetSnapshot(platformMenuPad());
    if ((padSnapshot.pressed & PS2_PAD_TRIANGLE) != 0 && selectedControlIndex >= 0 &&
        selectedControlIndex < static_cast<int_t>(actionSlots.size()))
    {
        unbindSelectedAction();
    }
#endif

#if PLATFORM_PS2 || PLATFORM_WII
    if (platformTextInputExclusive())
        return;

    const PlatformTextInputSnapshot pad = platformTextInputSnapshot(platformMenuPad());
#if PLATFORM_PS2
    if ((pad.pressed & PLATFORM_TEXT_CLOSE) != 0)
    {
        if (mc != nullptr && mc->sndManager != nullptr)
            mc->sndManager->playSoundFX("random.back", 1.0f, 1.0f);
        returnToParent();
        return;
    }
    if ((pad.pressed & PLATFORM_TEXT_UP) != 0)
        moveSelection(0, -1);
    else if ((pad.pressed & PLATFORM_TEXT_DOWN) != 0)
        moveSelection(0, 1);
    if ((pad.pressed & PLATFORM_TEXT_LEFT) != 0)
        moveSelection(-1, 0);
    else if ((pad.pressed & PLATFORM_TEXT_RIGHT) != 0)
        moveSelection(1, 0);
    if ((pad.pressed & PLATFORM_TEXT_TYPE) != 0)
        activateSelection();
#endif
#endif
}

void LegacyControllerLayoutScreen::drawScreen(int_t mouseX, int_t mouseY, float_t partialTick)
{
    drawLegacyBackground(partialTick);
    drawPanelTitle(fontRenderer, uiText("Edit Layout"), width / 2, legacyLayout.panelY + 8);

    std::string subtitle = uiText("Triangle unbinds selected action.");
    if (captureBindingIndex >= 0 && settings != nullptr &&
        captureBindingIndex < static_cast<int_t>(settings->keyBindings.size()))
    {
        subtitle = uiText("Press a button for") + " " + settings->getKeyBindingDescription(captureBindingIndex);
    }
    drawCenteredString(fontRenderer, subtitle, width / 2, legacyLayout.panelY + 24, 0x404040);

    updatePointerHover(mouseX, mouseY);
    GuiScreen::drawScreen(mouseX, mouseY, partialTick);
}

void LegacyControllerLayoutScreen::drawFooterHints()
{
#if PLATFORM_PS2
    if (captureBindingIndex < 0 && selectedControlIndex >= 0 &&
        selectedControlIndex < static_cast<int_t>(actionSlots.size()))
    {
        drawLegacyMenuHints(mc, width, height, true, "Triangle", "Unbind");
        return;
    }
#endif
    drawLegacyMenuHints(mc, width, height, true);
}

void LegacyControllerLayoutScreen::onGuiClosed()
{
    platformSetPadRebindExclusive(false);
    captureBindingIndex = -1;
}
