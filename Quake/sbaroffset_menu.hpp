#include "quakedef.hpp"
#include "vr.hpp"

#ifndef __R_SBAROFFSET_MENU_H
#define __R_SBAROFFSET_MENU_H

enum class SbarOffsetMenuOpt
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

void SbarOffset_Menu_f(void);
void SbarOffset_MenuDraw(void);
void SbarOffset_MenuKey(int key);

#endif
