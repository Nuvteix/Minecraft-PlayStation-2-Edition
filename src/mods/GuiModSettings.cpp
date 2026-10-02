#include "GuiModSettings.h"
#include "net/minecraft/src/Minecraft.h"
#include "net/minecraft/src/FontRenderer.h"
#include "pc/lwjgl/Keyboard.h"
#include "platform/Storage.h"
#include "platform/Log.h"
#include <algorithm>
#include <sstream>

GuiModSettings::GuiModSettings(GuiScreen *parent, IMod *modValue)
    : parentScreen(parent), mod(modValue)
{
}

GuiModSettings::~GuiModSettings()
{
    for (auto *field : textFields)
        delete field;
    textFields.clear();
}

void GuiModSettings::addTextField(const std::string &key, const std::string &label, const std::string &defaultValue)
{
    settings.push_back({key, defaultValue});
    labels.push_back(label);
}

std::string GuiModSettings::getFieldValue(const std::string &key) const
{
    for (const auto &setting : settings)
    {
        if (setting.first == key)
            return setting.second;
    }
    return "";
}

void GuiModSettings::loadSettings()
{
    if (mod == nullptr)
        return;
    
    std::string configPath = "mods/" + mod->getId() + "/config.txt";
    std::vector<unsigned char> bytes;
    
    if (!PlatformStorage::readFile(configPath, bytes) || bytes.empty())
        return;
    
    std::string content(bytes.begin(), bytes.end());
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line))
    {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        
        if (line.empty() || line[0] == '#')
            continue;
        
        size_t sep = line.find('=');
        if (sep != std::string::npos)
        {
            std::string key = line.substr(0, sep);
            std::string value = line.substr(sep + 1);
            
            for (auto &setting : settings)
            {
                if (setting.first == key)
                {
                    setting.second = value;
                    break;
                }
            }
        }
    }
}

void GuiModSettings::saveSettings()
{
    if (mod == nullptr)
        return;
    
    std::string configPath = "mods/" + mod->getId() + "/config.txt";
    std::string content = "# " + mod->getName() + " Configuration\n";
    
    for (size_t i = 0; i < settings.size(); ++i)
    {
        std::string value = settings[i].second;
        if (i < textFields.size() && textFields[i] != nullptr)
            value = textFields[i]->getText();
        
        content += settings[i].first + "=" + value + "\n";
    }
    
    PlatformStorage::mkdirs("mods/" + mod->getId());
    PlatformStorage::writeFile(configPath, content.data(), content.size());
    MC_LOG_INFO("mods", "Saved config for mod: %s", mod->getId().c_str());
}

void GuiModSettings::initGui()
{
    lwjgl::Keyboard::enableRepeatEvents(true);
    
    // Load saved settings
    loadSettings();
    
    // Create text fields for each setting
    int_t y = height / 4 + 24;
    for (size_t i = 0; i < settings.size(); ++i)
    {
        GuiTextField *field = new GuiTextField(this, fontRenderer, width / 2 - 100, y, 200, 20, settings[i].second);
        field->setMaxStringLength(64);
        textFields.push_back(field);
        y += 32;
    }
    
    // Done button
    controlList.push_back(new GuiButton(0, width / 2 - 100, height / 4 + 120, 200, 20, "Done"));
}

void GuiModSettings::drawScreen(int_t mouseX, int_t mouseY, float_t partialTick)
{
    drawDefaultBackground();
    
    // Draw title
    if (mod != nullptr)
    {
        std::string title = mod->getName() + " Settings";
        drawCenteredString(fontRenderer, title, width / 2, height / 4 - 40, 0xFFFFFF);
    }
    
    // Draw labels
    int_t y = height / 4 + 24;
    for (size_t i = 0; i < labels.size(); ++i)
    {
        drawString(fontRenderer, labels[i], width / 2 - 100, y - 12, 0xA0A0A0);
        y += 32;
    }
    
    // Draw text fields
    for (auto *field : textFields)
    {
        if (field != nullptr)
            field->drawTextBox();
    }
    
    GuiScreen::drawScreen(mouseX, mouseY, partialTick);
}

void GuiModSettings::keyTyped(char_t c, int_t key)
{
    for (auto *field : textFields)
    {
        if (field != nullptr && field->getFocused())
        {
            field->textboxKeyTyped(c, key);
            return;
        }
    }
    
    if (key == lwjgl::Keyboard::KEY_ESCAPE)
    {
        saveSettings();
        mc->displayGuiScreen(parentScreen);
        return;
    }
    
    if (key == lwjgl::Keyboard::KEY_TAB)
    {
        // Cycle through text fields
        for (size_t i = 0; i < textFields.size(); ++i)
        {
            if (textFields[i] != nullptr && textFields[i]->getFocused())
            {
                textFields[i]->setFocused(false);
                size_t next = (i + 1) % textFields.size();
                textFields[next]->setFocused(true);
                return;
            }
        }
        if (!textFields.empty() && textFields[0] != nullptr)
            textFields[0]->setFocused(true);
    }
}

void GuiModSettings::mouseClicked(int_t x, int_t y, int_t button)
{
    GuiScreen::mouseClicked(x, y, button);
    
    for (auto *field : textFields)
    {
        if (field != nullptr)
            field->mouseClicked(x, y, button);
    }
}

void GuiModSettings::updateScreen()
{
    GuiScreen::updateScreen();
    for (auto *field : textFields)
    {
        if (field != nullptr)
            field->updateCursorCounter();
    }
}

void GuiModSettings::onGuiClosed()
{
    lwjgl::Keyboard::enableRepeatEvents(false);
    saveSettings();
}

void GuiModSettings::actionPerformed(GuiButton *button)
{
    if (button->id == 0) // Done
    {
        saveSettings();
        mc->displayGuiScreen(parentScreen);
    }
}