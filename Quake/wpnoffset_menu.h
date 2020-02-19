#include "quakedef.h"
#include "vr.h"

#ifndef __R_WPNOFFSET_MENU_H
#define __R_WPNOFFSET_MENU_H

enum class WpnOffsetMenuOpt
{
	OffsetX,
    OffsetY,
    OffsetZ,
    Scale,
    Roll,
    Pitch,
    Yaw,

	Max
};

void WpnOffset_Menu_Init ();
void WpnOffset_Menu_f (void);
void WpnOffset_MenuDraw (void);
void WpnOffset_MenuKey (int key);

extern void (*wpnoffset_menucmdfn)(void);
extern void (*wpnoffset_menudrawfn)(void);
extern void (*wpnoffset_menukeyfn)(int key);

#endif
