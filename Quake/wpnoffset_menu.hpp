#pragma once

enum class WpnOffsetMenuOpt
{
    OffHand,

    OffsetX,
    OffsetY,
    OffsetZ,
    Scale,
    Roll,
    Pitch,
    Yaw,

    Max
};

void WpnOffset_Menu_f(void);
void WpnOffset_MenuDraw(void);
void WpnOffset_MenuKey(int key);
