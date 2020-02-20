#include "quakedef.h"
#include "vr.h"

#ifndef __R_VR_MENU_H
#define __R_VR_MENU_H

enum class VRMenuOpt
{
	VR_ENABLED,

	VR_AIMMODE,
	VR_DEADZONE,
	VR_CROSSHAIR,
	VR_CROSSHAIR_DEPTH,
	VR_CROSSHAIR_SIZE,
	VR_CROSSHAIR_ALPHA,
	VR_WORLD_SCALE,
	VR_MOVEMENT_MODE,
	VR_SNAP_TURN,
	VR_TURN_SPEED,
	VR_MSAA,
	VR_GUNANGLE,
	VR_FLOOR_OFFSET,
	VR_GUNMODELPITCH,
	VR_GUNMODELSCALE,
	VR_GUNMODELY,
	VR_CROSSHAIRY,
	VR_PROJECTILESPAWN_Z_OFFSET,
	VR_HUD_SCALE,
	VR_MENU_SCALE,
	VR_MELEE_THRESHOLD,
	VR_GUNYAW,
	VR_GUN_Z_OFFSET,
	VR_SBAR_MODE,
	VR_IMPULSE9,

	VR_MAX
};

void VR_Menu_Init ();
void VR_Menu_f (void);
void VR_MenuDraw (void);
void VR_MenuKey (int key);

extern void (*vr_menucmdfn)(void);
extern void (*vr_menudrawfn)(void);
extern void (*vr_menukeyfn)(int key);

#endif
