#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "desktop/sdl_compat.h"
typedef enum { ACK_RUMBLE_NONE,ACK_RUMBLE_ROAD,ACK_RUMBLE_CASTLE,
    ACK_RUMBLE_END_FLIGHT,ACK_RUMBLE_END_BLASTS,ACK_RUMBLE_FALL,ACK_RUMBLE_END_CLIMAX } AckmanRumbleScene;
#ifdef __cplusplus
extern "C" {
#endif
void AckmanRumbleSetEnabled(bool enabled);
void AckmanRumbleActivationFeedback(void);
void AckmanRumbleSuspend(bool paused);
void AckmanRumbleEvent(const SDL_Event *event);
bool AckmanRumbleFrameActive(void);
void AckmanRumbleBegin(bool real,uint16_t script,uint16_t position);
void AckmanRumbleSound(uint8_t command,uint16_t selector,uint16_t script);
void AckmanRumbleEnemyHit(void);
void AckmanRumbleAirHit(void);
void AckmanRumbleInterludeHit(void);
/* Called by the SPC/DSP thread; only queues a verified SFX note onset. */
void AckmanRumbleDspKeyOn(uint8_t voices,const uint8_t commands[8]);
void AckmanRumbleSpcCommands(const uint8_t commands[8]);
void AckmanRumbleBossContext(bool destroying);
void AckmanRumbleBossHit(void);
void AckmanRumbleBossSpc(const uint8_t commands[8]);
void AckmanRumbleMineExplosion(void);
void AckmanRumbleCharge(bool charging,bool released);
void AckmanRumbleEnd(AckmanRumbleScene scene,uint16_t script,uint16_t position);
void AckmanRumbleShutdown(void);

#ifdef __cplusplus
}
#endif

