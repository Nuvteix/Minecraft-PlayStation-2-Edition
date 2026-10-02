#pragma once
#include "net/minecraft/src/GuiScreen.h"
#include "net/minecraft/src/GuiButton.h"
#include "net/minecraft/src/GuiTextField.h"
#include "mods/IMod.h"
#include <string>
#include <vector>
#include <memory>

class GuiModSettings : public GuiScreen
{
public:
    GuiModSettings(GuiScreen *parent, IMod *mod);
    ~GuiModSettings();
    
    void initGui() override;
    void drawScreen(int_t mouseX, int_t mouseY, float_t partialTick) override;
    void keyTyped(char_t c, int_t key) override;
    void mouseClicked(int_t x, int_t y, int_t button) override;
    void updateScreen() override;
    void onGuiClosed() override;
    void actionPerformed(GuiButton *button) override;

protected:
    GuiScreen *parentScreen;
    IMod *mod;
    std::vector<std::pair<std::string, std::string>> settings; // key-value pairs
    std::vector<GuiTextField*> textFields;
    std::vector<std::string> labels;
    int_t selectedField = -1;
    
    void loadSettings();
    void saveSettings();
    
public:
    // Helper methods for mods to register settings
    void addTextField(const std::string &key, const std::string &label, const std::string &defaultValue);
    std::string getFieldValue(const std::string &key) const;
};