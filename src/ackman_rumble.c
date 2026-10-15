#include "ackman_rumble.h"
#include <stdio.h>
#include <stdatomic.h>
/* Armed and pending share one atomic word: a late DSP callback cannot
 * resurrect a note after pause/load clears the gate. */
static atomic_uint climax_state; /* bit 0: armed, bit 1: note pending */
static atomic_uint ignition_state; /* bit 0: scene armed, bit 2: SFX playing */
static bool ignition_playing;
static atomic_uint boss_state; /* bit 0: armed, bit 2: SFX $1D executing */
static bool boss_session,boss_destroying,boss_playing,boss_fatal_pending;
static bool enabled=true,suspended,recording,charging,charge_changed;
static SDL_JoystickID preferred,driven;
static SDL_JoystickID connected[32];
static AckmanRumbleScene base_scene;
static unsigned pending,host_frame;
static uint16_t before_script,before_position,script,position;
static Uint16 pulse_low,pulse_high;
static Uint64 pulse_until,last_refresh;
static Uint64 activation_until;
static FILE *trace;
static bool trace_checked;
enum { HIT=1,BOMB=2,BLAST=4,FALL=8,ENEMY=16,INTERLUDE=32,CHARGE=64,AIR=256,CLIMAX=512,MINE=1024 };
static SDL_Joystick *controller(void) {
    SDL_Joystick *joy=preferred?SDL_GetJoystickFromID(preferred):NULL;
    if(joy)return joy;
    for(unsigned i=0;i<sizeof(connected)/sizeof(connected[0]) && !joy;i++) {
        SDL_Joystick *candidate=connected[i]?SDL_GetJoystickFromID(connected[i]):NULL;
        if(candidate && SDL_GetBooleanProperty(SDL_GetJoystickProperties(candidate),SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN,false))joy=candidate;
    }
    return joy;
}
static void send(Uint16 low,Uint16 high,Uint32 duration,const char *reason) {
    SDL_Joystick *joy=controller();
    SDL_JoystickID id=joy?SDL_GetJoystickID(joy):0;
    if(driven && driven!=id) {
        SDL_Joystick *old=SDL_GetJoystickFromID(driven);if(old)SDL_RumbleJoystick(old,0,0,0);
    }
    bool ok=joy && SDL_RumbleJoystick(joy,low,high,duration);
    driven=(low||high)?id:0;
    if(!trace_checked) {
        const char *path=SDL_getenv("ACKMAN_RUMBLE_TRACE");trace_checked=true;
        if(path && *path) {trace=fopen(path,"w");if(trace)fputs("frame,script,position,event,low,high,duration_ms,device,accepted\n",trace);}
    }
    if(trace) {fprintf(trace,"%u,%04X,%04X,%s,%u,%u,%u,%u,%d\n",host_frame,script,position,reason,low,high,duration,id,ok);fflush(trace);}
    last_refresh=SDL_GetTicks();
}
static void stop(void) {
    activation_until=0;
    atomic_store(&climax_state,0);
    atomic_store(&ignition_state,0);
    atomic_store(&boss_state,0);
    if(driven || pulse_until || base_scene!=ACK_RUMBLE_NONE || ignition_playing || boss_playing)send(0,0,0,"stop");
    boss_session=boss_destroying=boss_playing=boss_fatal_pending=false;
    ignition_playing=false;
    recording=false;pending=0;pulse_until=0;base_scene=ACK_RUMBLE_NONE;charging=charge_changed=false;
}
void AckmanRumbleSetEnabled(bool value) {enabled=value;if(!enabled)stop();}
void AckmanRumbleActivationFeedback(void) {
    if(enabled) {activation_until=SDL_GetTicks()+300;send(0x8000,0x8000,300,"activation-feedback");}
}
void AckmanRumbleSuspend(bool paused) {suspended=paused;if(paused && SDL_GetTicks()>=activation_until)stop();}
void AckmanRumbleEvent(const SDL_Event *e) {
    if(e->type==SDL_EVENT_GAMEPAD_ADDED || e->type==SDL_EVENT_JOYSTICK_ADDED) {
        SDL_JoystickID id=e->gdevice.which;
        for(unsigned i=0;i<sizeof(connected)/sizeof(connected[0]);i++)if(connected[i]==id)return;
        for(unsigned i=0;i<sizeof(connected)/sizeof(connected[0]);i++)if(!connected[i]){connected[i]=id;break;}
    }
    if(e->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN)preferred=e->gbutton.which;
    else if(e->type==SDL_EVENT_GAMEPAD_AXIS_MOTION && SDL_abs(e->gaxis.value)>16000)preferred=e->gaxis.which;
    else if(e->type==SDL_EVENT_JOYSTICK_BUTTON_DOWN)preferred=e->jbutton.which;
    else if(e->type==SDL_EVENT_JOYSTICK_AXIS_MOTION && SDL_abs(e->jaxis.value)>16000)preferred=e->jaxis.which;
    else if(e->type==SDL_EVENT_GAMEPAD_REMOVED || e->type==SDL_EVENT_JOYSTICK_REMOVED) {
        for(unsigned i=0;i<sizeof(connected)/sizeof(connected[0]);i++)if(connected[i]==e->jdevice.which)connected[i]=0;
        if(e->jdevice.which==driven || e->jdevice.which==preferred){stop();preferred=0;}
    }
}
void AckmanRumbleBegin(bool real,uint16_t value,uint16_t pos) {
    recording=real && enabled && !suspended;
    bool white=recording && value==0x9be4 && pos>=0x1f;
    if(white)atomic_fetch_or(&climax_state,1);
    else if(real)atomic_store(&climax_state,0);
    else atomic_fetch_and(&climax_state,~1u);
    if(recording && value==0xa22e && pos<0x5c)atomic_fetch_or(&ignition_state,1);
    else if(real)atomic_store(&ignition_state,0);
    else atomic_fetch_and(&ignition_state,~1u);
    if(recording && boss_session)atomic_fetch_or(&boss_state,1);
    else if(real)atomic_store(&boss_state,0);
    else atomic_fetch_and(&boss_state,~1u);
    if(!real)return; /* Run-ahead must not stop or duplicate the real effect. */
    host_frame++;pending=0;charge_changed=false;before_script=value;before_position=pos;
}
bool AckmanRumbleFrameActive(void) {return recording;}
void AckmanRumbleEnemyHit(void) {if(recording)pending|=ENEMY;}
void AckmanRumbleAirHit(void) {if(recording)pending|=AIR;}
void AckmanRumbleInterludeHit(void) {if(recording)pending|=INTERLUDE;}
void AckmanRumbleMineExplosion(void) {if(recording)pending|=MINE;}
void AckmanRumbleBossContext(bool destroying) {
    if(!recording)return;
    boss_destroying=destroying;
    if(destroying){boss_session=true;atomic_fetch_or(&boss_state,1);}
}
void AckmanRumbleBossHit(void) {
    if(!recording)return;
    boss_session=boss_fatal_pending=true;atomic_fetch_or(&boss_state,1);
}
void AckmanRumbleBossSpc(const uint8_t commands[8]) {
    bool active=false;
    for(unsigned ch=0;ch<8;ch++)active|=commands[ch]==0x9d;
    unsigned state=atomic_load(&boss_state);
    while((state&1) && !atomic_compare_exchange_weak(&boss_state,&state,active?state|4:state&~4u)) {}
}
void AckmanRumbleDspKeyOn(uint8_t voices,const uint8_t commands[8]) {
    for(unsigned ch=0;ch<8;ch++)if((voices&(1u<<ch)) && commands[ch]==0xb0) {
        unsigned state=atomic_load(&climax_state);
        while((state&1) && !atomic_compare_exchange_weak(&climax_state,&state,state|2)) {}
        break;
    }
    for(unsigned ch=0;ch<8;ch++)if((voices&(1u<<ch)) && commands[ch]==0xb8) {
        unsigned state=atomic_load(&ignition_state);
        while((state&1) && !atomic_compare_exchange_weak(&ignition_state,&state,state|4)) {}
        break;
    }
}
void AckmanRumbleSpcCommands(const uint8_t commands[8]) {
    for(unsigned ch=0;ch<8;ch++)if(commands[ch]==0xb8)return;
    unsigned state=atomic_load(&ignition_state);
    while((state&1) && !atomic_compare_exchange_weak(&ignition_state,&state,state&~4u)) {}
}
void AckmanRumbleCharge(bool value,bool released) {
    if(!recording)return;
    charge_changed=charging!=value;charging=value;
    if(released)pending|=CHARGE;
}
void AckmanRumbleSound(uint8_t command,uint16_t selector,uint16_t scene_script) {
    if(!recording)return;
    unsigned effect=command&0x7f; /* Bit 7 toggles the SPC acknowledgment. */
    if(effect==0x38 && selector==0x2800 && scene_script==0xa22e)atomic_fetch_or(&ignition_state,1);
    if(effect==0x0b)pending|=HIT; /* Player hurt; charge/swing/launch are excluded. */
    if(effect==4)pending|=(selector==0x2d00 && (scene_script==0x9b2f || scene_script==0x9bdc || scene_script==0x9be4))?BLAST:BOMB;
    if(effect==0x45 && selector==0x2d00 && scene_script==0x9f08)pending|=FALL;
}
void AckmanRumbleEnd(AckmanRumbleScene scene,uint16_t value,uint16_t pos) {
    if(!recording)return;
    recording=false;script=value;position=pos;
    if(SDL_GetTicks()<activation_until){pending=0;return;}
    if((atomic_fetch_and(&climax_state,~2u)&2) && scene==ACK_RUMBLE_END_CLIMAX)pending|=CLIMAX;
    // $81:9B2F spawns explosion actors (animation $0118) one per
    // command at +$27/$3D/$53/$69/$7F/$95. The final spawn ends at +$AF.
    // Its SFX follows the group; pulse on the visual spawns as well.
    if(script==0x9b2f && before_script==script && before_position!=position &&
       ((position>=0x27 && position<=0x95 && (position-0x27)%0x16==0) || position==0xaf))pending|=BLAST;
    Uint64 now=SDL_GetTicks();
    bool changed=scene!=base_scene;
    bool ignition_on=(atomic_load(&ignition_state)&4)!=0;
    bool ignition_changed=ignition_on!=ignition_playing;
    bool ignition_ended=ignition_playing && !ignition_on;
    ignition_playing=ignition_on;
    if(ignition_changed){pulse_until=0;changed=true;}
    if(charge_changed && !charging)pulse_until=0;
    if(changed){pulse_until=0;base_scene=scene;}
    // The native chain owns the motors from the fatal hit, including gaps
    // between individual bursts. After actor removal, drain the SPC's last
    // $1D execution instead of cutting the tail or inventing a host timeout.
    bool boss_on=boss_session && (boss_fatal_pending || boss_destroying || (atomic_load(&boss_state)&4));
    boss_fatal_pending=false;
    if(boss_on) {
        pending=0;pulse_until=0;
        if(!boss_playing || now-last_refresh>=60)
            send(0x8000,0x8000,150,boss_playing?"boss-defeat-hold":"boss-defeat-start");
        boss_playing=true;return;
    }
    if(boss_session){boss_session=false;atomic_store(&boss_state,0);}
    if(boss_playing){boss_playing=false;pending=0;pulse_until=0;send(0,0,0,"boss-defeat-end");return;}
    if(pending) {
        const char *reason=pending&INTERLUDE?"interlude-impact":pending&AIR?"air-impact":pending&ENEMY?"enemy-impact":"player-impact";Uint32 duration=110;pulse_low=0x9000;pulse_high=0xa000;
        if(pending&CHARGE){reason="charge-release";duration=150;pulse_low=0xb000;pulse_high=0xc000;}
        if(pending&BOMB){reason="bomb-explosion";duration=240;pulse_low=0xe000;pulse_high=0xf000;}
        if(pending&BLAST){reason="castle-explosion";duration=150;pulse_low=pulse_high=0xffff;}
        if(pending&FALL){reason="tenshi-impact";duration=700;pulse_low=0xffff;pulse_high=0xe000;}
        if(pending&MINE){reason="car-mine-explosion";duration=110;pulse_low=0x9000;pulse_high=0xa000;}
        if(pending&CLIMAX){reason="climax-sfx";duration=150;pulse_low=pulse_high=0xffff;}
        pulse_until=now+duration;send(pulse_low,pulse_high,duration,reason);pending=0;return;
    }
    Uint16 low=0,high=0;const char *reason="silence";
    if(scene==ACK_RUMBLE_ROAD){low=0x2500;reason="car-engine";}
    if(scene==ACK_RUMBLE_CASTLE || scene==ACK_RUMBLE_END_FLIGHT){low=0x6000;high=0x2800;reason="castle-flight";}
    if(charging){low=0x6000;high=0x7000;reason="charge-hold";}
    if(ignition_on){low=0x6000;high=0x2800;reason=ignition_changed?"castle-ignition-start":"castle-ignition-hold";}
    else if(ignition_ended)reason="castle-ignition-end";
    changed|=charge_changed;
    if(pulse_until && now<pulse_until && !charge_changed)return;
    if(pulse_until){pulse_until=0;changed=true;}
    if(changed || ((low||high) && now-last_refresh>=60))send(low,high,(low||high)?150:0,reason);
}
void AckmanRumbleShutdown(void) {stop();if(trace)fclose(trace);trace=NULL;trace_checked=false;preferred=0;SDL_zero(connected);}
