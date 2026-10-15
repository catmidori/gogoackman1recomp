#pragma once
#include "recomp_menu_hotkeys.h"
#include "ackman_conflict.h"
#define ACK_HOTKEY_COUNT RECOMP_HOTKEY_COUNT
#define AckmanConflict RecompConflict
#define AckmanHotkeysActive RecompHotkeysActive
#define AckmanHotkeysBegin RecompHotkeysBegin
#define AckmanHotkeysCancel RecompHotkeysCancel
#define AckmanHotkeysCaptureEvent RecompHotkeysCaptureEvent
#define AckmanHotkeysCapturing RecompHotkeysCapturing
#define AckmanHotkeysClear RecompHotkeysClear
#define AckmanHotkeysConflict RecompHotkeysConflict
#define AckmanHotkeysConflicts RecompHotkeysConflicts
#define AckmanHotkeysFixedEvent RecompHotkeysFixedEvent
#define AckmanHotkeysGameplayConflict RecompHotkeysGameplayConflict
#define AckmanHotkeysGameplayConflicts RecompHotkeysGameplayConflicts
#define AckmanHotkeysHostEvent RecompHotkeysHostEvent
#define AckmanHotkeysInit RecompHotkeysInit
#define AckmanHotkeysInputBlocked RecompHotkeysInputBlocked
#define AckmanHotkeysKey RecompHotkeysKey
#define AckmanHotkeysLabel RecompHotkeysLabel
#define AckmanHotkeysName RecompHotkeysName
#define AckmanHotkeysRaw RecompHotkeysRaw
#define AckmanHotkeysReleaseRewindEvent RecompHotkeysReleaseRewindEvent
#ifdef __cplusplus
extern "C" {
#endif
/* Local runner dispatch injected through RecompMenuHost.HostHotkey. */
void AckmanHostHotkey(int command,bool pressed);
#ifdef __cplusplus
}
#endif
