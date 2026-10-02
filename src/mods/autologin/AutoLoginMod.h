#pragma once
#include "mods/IMod.h"
#include <string>

class Minecraft;

class AutoLoginMod : public IMod
{
public:
    AutoLoginMod();
    std::string getId() const override { return "autologin"; }
    std::string getName() const override { return "Auto-Login"; }
    std::string getVersion() const override { return "1.0"; }
    std::string getDescription() const override { return "Automatically logs in to servers"; }
    std::string getAuthor() const override { return "You"; }
    
    bool isEnabled() const override { return enabled; }
    void setEnabled(bool enabledValue) override { enabled = enabledValue; }
    
    void onInit(Minecraft *mc) override;
    void onChatMessageReceived(const std::string &message) override;
    
    bool hasSettings() const override { return true; }
    void openSettings(Minecraft *mc) override;
    
    std::string getPasswordForGui() const;
    void setPasswordFromGui(const std::string &newPass);
    
private:
    bool enabled = true;
    Minecraft *mcInstance = nullptr;
    std::string password;
    
    void loadConfig();
    void saveConfig();
    std::string toLower(const std::string &str) const;
};