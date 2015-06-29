#ifndef __STRUCTS_H__
#define __STRUCTS_H__


struct InitialLoginData
{
   int32 positionX; //0
   int32 positionY; //4
   uint32 mapId; //8
   uint16 serverId; //12
   uint16 unk; //14 high bits of serverid, seems to be 32bit, unused
   uint16 unk2; //16 seems to be initial login (show tutorial if 1)
   uint16 unk3; //18
   uint16 serverId2; //20 if this != previous id, do not login, don't know why this is here, maybe an old hack by that hollow guy
   uint16 unk4; //22 high bits of serverid, unused
   float time_current; //24 time as of login
   float time_roc; //28 rate of change (every 50 milliseconds passing, rate value changes)
   uint8 unk7; //32
};

struct UpdatePacketSelf
{
   uint16 positionX; //0
   uint16 positionY; //2
   uint16 spellflags; //4
   uint16 colorbits; //6
   uint16 numInRangeDynamicObjects; //8
   uint8 numInRangeUnits; //10
   uint8 unklol; //11
   uint8 flags; //12 if & 1, expect self say text at end, if & 2, spellflags contain cooldown data, expect no buffs or skills this update if & 4 you're in pvp mode
   uint8 rotation; //13
   uint8 animation; //14
   uint8 spellEffect; //15
   uint8 numInRangeSpellEffects; //16
   uint8 unklol2; //17
   uint8 unklol3; //18
   uint8 currentHPPct; //19
   uint8 currentMPPct; //20 for hp mp and exp, 0 = 0%, 255 = 100%
   uint8 currentExpPct; //21
};

struct UpdatePacketUnitMovement
{
   uint16 positionX;
   uint16 positionY;
   uint8 rotation;
};

struct UpdatePacketUnitAuras
{
   uint16 auraflags; //(&0x08 = invis)
   uint8 auraflagsextra; //calv head glow, only time seen in use
};

struct UpdatePacketUnitModels
{
   uint16 model;
   uint16 weapon;
   uint8 shield;
   uint8 helmet;
   uint16 colorbits;
};

struct UpdatePacketUnitAnim
{
   uint8 anim;
};

struct UpdatePacketUnitSpellEffect
{
   uint8 spelleff;
};

struct UpdatePacketUnitChat
{
   uint8 stringsize;
};

#endif