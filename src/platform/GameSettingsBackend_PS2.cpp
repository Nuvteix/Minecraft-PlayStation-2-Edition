#include "platform/GameSettingsBackend.h"

#include <algorithm>
#include <ostream>
#include "lwjgl/Keyboard.h"
#include "net/minecraft/src/GameSettings.h"
#include "net/minecraft/src/KeyBinding.h"
#include "platform/PlatformTuning.h"
#include "ps2/input/Ps2PadKeyCodes.h"

namespace
{
void migrateKey(KeyBinding* binding, int_t fallback)
{
	// 0 is unbound. Mouse buttons are negative. Neither is a leftover PC key.
	// Treating 0 as "needs a pad fallback" was binding WASD onto the D-pad on
	// first launch, before options.txt existed to restore the empty slots.
	if (binding->keyCode <= 0 || binding->keyCode >= lwjgl::Keyboard::KEY_MAX)
		return;
	binding->keyCode = fallback;
}
}

void platformGameSettingsApplyLegacyCrafting(GameSettings& settings)
{
	if (settings.legacyCrafting)
	{
		settings.keyBindInventory->keyCode = PS2_KEY_TRIANGLE;
		if (settings.keyBindCrafting != nullptr)
			settings.keyBindCrafting->keyCode = PS2_KEY_SQUARE;
		settings.keyBindDrop->keyCode = PS2_KEY_CIRCLE;
	}
	else
	{
		settings.keyBindInventory->keyCode = PS2_KEY_SQUARE;
		if (settings.keyBindCrafting != nullptr)
			settings.keyBindCrafting->keyCode = 0;
		settings.keyBindDrop->keyCode = PS2_KEY_CIRCLE;
	}
}

void platformGameSettingsInitialize(GameSettings& settings)
{
	settings.keyBindForward->keyCode = 0;
	settings.keyBindLeft->keyCode = 0;
	settings.keyBindBack->keyCode = 0;
	settings.keyBindRight->keyCode = 0;
	settings.keyBindJump->keyCode = PS2_KEY_CROSS;
	settings.keyBindSneak->keyCode = PS2_KEY_L3;
	settings.keyBindSprint->keyCode = 0;
	settings.keyBindChat->keyCode = PS2_KEY_SELECT;
	settings.keyBindPlayerList->keyCode = 0;
	settings.keyBindDebug->keyCode = 0;
	settings.keyBindCycleItemLeft->keyCode = PS2_KEY_L1;
	settings.keyBindCycleItemRight->keyCode = PS2_KEY_R1;
	settings.keyBindTogglePerspective->keyCode = PS2_KEY_R3;
	platformGameSettingsApplyLegacyCrafting(settings);
}

void platformGameSettingsResetControlBindings(GameSettings& settings)
{
	settings.keyBindForward->keyCode = 0;
	settings.keyBindLeft->keyCode = 0;
	settings.keyBindBack->keyCode = 0;
	settings.keyBindRight->keyCode = 0;
	settings.keyBindJump->keyCode = PS2_KEY_CROSS;
	settings.keyBindSneak->keyCode = PS2_KEY_L3;
	settings.keyBindSprint->keyCode = 0;
	settings.keyBindChat->keyCode = PS2_KEY_SELECT;
	settings.keyBindPlayerList->keyCode = 0;
	settings.keyBindDebug->keyCode = 0;
	settings.keyBindCycleItemLeft->keyCode = PS2_KEY_L1;
	settings.keyBindCycleItemRight->keyCode = PS2_KEY_R1;
	settings.keyBindTogglePerspective->keyCode = PS2_KEY_R3;
	platformGameSettingsApplyLegacyCrafting(settings);
}

int_t platformGameSettingsDefaultChunkUpdates() { return (int_t)PLATFORM_MAX_RENDERER_UPDATES_PER_FRAME; }
int_t platformGameSettingsDefaultConnectedTextures() { return 0; }
int_t platformGameSettingsCycleRenderDistance(int_t, int_t) { return PLATFORM_DEFAULT_RENDER_DISTANCE; }
int_t platformGameSettingsClampRenderDistance(int_t) { return PLATFORM_DEFAULT_RENDER_DISTANCE; }
int_t platformGameSettingsClampFineRenderDistance(int_t value)
{
	return value < 32 ? 32 : (value > PLATFORM_VISIBLE_CHUNK_RADIUS * 16 ? PLATFORM_VISIBLE_CHUNK_RADIUS * 16 : value);
}
void platformGameSettingsUpdateRenderDistanceFromFine(int_t, int_t&) {}
bool platformGameSettingsAnaglyphValue(bool, bool requested) { return requested; }
bool platformGameSettingsLoadOption(GameSettings&, const std::string& key, const std::string&)
{
	return key == "worldStorage";
}

void platformGameSettingsFinalizeLoad(GameSettings& settings)
{
	settings.ofChunkUpdates = std::max(settings.ofChunkUpdates, (int_t)PLATFORM_MAX_RENDERER_UPDATES_PER_FRAME);
	settings.limitFramerate = 30;
	migrateKey(settings.keyBindForward, 0);
	migrateKey(settings.keyBindLeft, 0);
	migrateKey(settings.keyBindBack, 0);
	migrateKey(settings.keyBindRight, 0);
	migrateKey(settings.keyBindJump, PS2_KEY_CROSS);
	migrateKey(settings.keyBindSneak, PS2_KEY_L3);
	if (settings.keyBindPlayerList->keyCode == lwjgl::Keyboard::KEY_TAB)
		settings.keyBindPlayerList->keyCode = 0;
	if (settings.keyBindDebug->keyCode == lwjgl::Keyboard::KEY_F3)
		settings.keyBindDebug->keyCode = 0;
	if (settings.keyBindChat->keyCode == lwjgl::Keyboard::KEY_T)
		settings.keyBindChat->keyCode = PS2_KEY_SELECT;
	if (settings.keyBindCycleItemLeft->keyCode != 0)
		migrateKey(settings.keyBindCycleItemLeft, PS2_KEY_L1);
	if (settings.keyBindCycleItemRight->keyCode != 0)
		migrateKey(settings.keyBindCycleItemRight, PS2_KEY_R1);
	if (settings.keyBindTogglePerspective->keyCode != 0)
		migrateKey(settings.keyBindTogglePerspective, PS2_KEY_R3);
	platformGameSettingsApplyLegacyCrafting(settings);
}

void platformGameSettingsSanitizeLoadedBindings(GameSettings& settings)
{
	// An older first-boot path copied WASD onto the D-pad. Movement is analog,
	// and the layout screen does not expose those four slots, so this quartet
	// cannot be a user choice. Clear it so D-pad stays free for Chat/Debug/etc.
	if (settings.keyBindForward->keyCode == PS2_KEY_DPAD_UP &&
		settings.keyBindLeft->keyCode == PS2_KEY_DPAD_LEFT &&
		settings.keyBindBack->keyCode == PS2_KEY_DPAD_DOWN &&
		settings.keyBindRight->keyCode == PS2_KEY_DPAD_RIGHT)
	{
		settings.keyBindForward->keyCode = 0;
		settings.keyBindLeft->keyCode = 0;
		settings.keyBindBack->keyCode = 0;
		settings.keyBindRight->keyCode = 0;
	}
}

void platformGameSettingsSyncControllerBindings(const GameSettings&) {}
void platformGameSettingsAddKnownKeys(std::unordered_set<std::string>& keys) { keys.insert("worldStorage"); }
void platformGameSettingsWriteOptions(const GameSettings&, std::ostream&) {}
