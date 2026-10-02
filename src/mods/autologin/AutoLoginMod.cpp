#include "AutoLoginMod.h"
#include "mods/ModManager.h"
#include "client/Minecraft.h"
#include "net/minecraft/src/EntityPlayerSP.h"
#include "net/minecraft/src/EntityClientPlayerMP.h"
#include "platform/PlatformCompat.h"
#include "net/minecraft/src/GuiScreen.h"
#include "net/minecraft/src/GuiButton.h"
#include "net/minecraft/src/GuiTextField.h"
#include "net/minecraft/src/FontRenderer.h"
#include "pc/lwjgl/Keyboard.h"
#include "platform/Storage.h"
#include "platform/Log.h"

class GuiAutoLoginSettings : public GuiScreen
{
public:
    GuiAutoLoginSettings(GuiScreen *parent, AutoLoginMod *mod)
        : parentScreen(parent), mod(mod), passwordField(nullptr), closing(false) {}

    ~GuiAutoLoginSettings() override
    {
        delete passwordField;
        passwordField = nullptr;
    }

    void initGui() override
    {
        lwjgl::Keyboard::enableRepeatEvents(true);
        controlList.push_back(new GuiButton(0, width / 2 - 100, height / 4 + 96, 200, 20, "Done"));

        passwordField = new GuiTextField(this, fontRenderer, width / 2 - 100, height / 4 + 40, 200, 20, mod->getPasswordForGui());
        passwordField->setMaxStringLength(64);
        passwordField->setFocused(true);
    }

    void drawScreen(int_t mouseX, int_t mouseY, float_t partialTick) override
    {
        drawDefaultBackground();
        drawCenteredString(fontRenderer, "Auto-Login Password", width / 2, height / 4 - 20, 0xFFFFFF);
        drawString(fontRenderer, "Enter your server password:", width / 2 - 100, height / 4 + 28, 0xA0A0A0);
        if (passwordField != nullptr) passwordField->drawTextBox();
        GuiScreen::drawScreen(mouseX, mouseY, partialTick);
    }

    void keyTyped(char_t c, int_t key) override
    {
        if (closing) return;
        if (key == lwjgl::Keyboard::KEY_RETURN || key == lwjgl::Keyboard::KEY_ESCAPE)
        {
            saveAndClose();
            return;
        }
        if (passwordField != nullptr && passwordField->getFocused())
        {
            passwordField->textboxKeyTyped(c, key);
        }
    }

    void mouseClicked(int_t x, int_t y, int_t button) override
    {
        GuiScreen::mouseClicked(x, y, button);
        if (passwordField != nullptr) passwordField->mouseClicked(x, y, button);
    }

    void updateScreen() override
    {
        GuiScreen::updateScreen();
        if (passwordField != nullptr) passwordField->updateCursorCounter();
    }

    void onGuiClosed() override
    {
        lwjgl::Keyboard::enableRepeatEvents(false);
        if (!closing)
        {
            closing = true;
            if (passwordField != nullptr)
                mod->setPasswordFromGui(passwordField->getText());
        }
    }

    void actionPerformed(GuiButton *button) override
    {
        if (button->id == 0) saveAndClose();
    }

private:
    void saveAndClose()
    {
        if (closing) return;
        closing = true;
        if (passwordField != nullptr)
            mod->setPasswordFromGui(passwordField->getText());
        mc->displayGuiScreen(parentScreen);
    }

    GuiScreen *parentScreen;
    AutoLoginMod *mod;
    GuiTextField *passwordField;
    bool closing;
};

AutoLoginMod::AutoLoginMod() {}

void AutoLoginMod::onInit(Minecraft *mc)
{
    mcInstance = mc;
    loadConfig();
}

void AutoLoginMod::openSettings(Minecraft *mc)
{
    if (mc != nullptr)
    {
        mc->displayGuiScreen(new GuiAutoLoginSettings(mc->currentScreen, this));
    }
}

std::string AutoLoginMod::getPasswordForGui() const { return password; }

void AutoLoginMod::setPasswordFromGui(const std::string &newPass)
{
    password = newPass;
    saveConfig();
}

void AutoLoginMod::onChatMessageReceived(const std::string &message)
{
    if (!enabled || password.empty() || mcInstance == nullptr || mcInstance->thePlayer == nullptr)
        return;

    EntityClientPlayerMP *mp = dynamic_cast<EntityClientPlayerMP *>(mcInstance->thePlayer);
    if (mp == nullptr || mp->sendQueue == nullptr)
        return;

    static long long lastSendUs = 0;
    const long long nowUs = (long long)PlatformCompat::getMonotonicMicros();
    if (nowUs - lastSendUs < 5000000LL)
        return;

    std::string lowerMsg = toLower(message);

    if (lowerMsg.find("register") != std::string::npos)
    {
        lastSendUs = nowUs;
        MC_LOG_INFO("autologin", "Register prompt detected.");
        mp->sendChatMessage("/register " + password + " " + password);
    }
    else if (lowerMsg.find("login") != std::string::npos)
    {
        lastSendUs = nowUs;
        MC_LOG_INFO("autologin", "Login prompt detected.");
        mp->sendChatMessage("/login " + password);
    }
}

std::string AutoLoginMod::toLower(const std::string &str) const
{
    std::string lowerStr = str;
    for (char &c : lowerStr) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return lowerStr;
}

static std::string autoLoginConfigPath()
{
    return PlatformStorage::join(ModManager::getInstance().getGameModsDir(), "autologin/config.txt");
}

static std::string autoLoginConfigDir()
{
    return PlatformStorage::join(ModManager::getInstance().getGameModsDir(), "autologin");
}

void AutoLoginMod::loadConfig()
{
    std::string configPath = autoLoginConfigPath();
    std::vector<unsigned char> bytes;
    if (PlatformStorage::readFile(configPath, bytes) && !bytes.empty())
    {
        password = std::string(bytes.begin(), bytes.end());
        while (!password.empty() && (password.back() == '\r' || password.back() == '\n' || password.back() == ' '))
            password.pop_back();
        while (!password.empty() && password.front() == ' ')
            password.erase(password.begin());
    }
}

void AutoLoginMod::saveConfig()
{
    PlatformStorage::mkdirs(autoLoginConfigDir());   // <-- ITT VOLT A HIBA: most már a teljes útvonalat használja
    PlatformStorage::writeFile(autoLoginConfigPath(), password.data(), password.size());
}