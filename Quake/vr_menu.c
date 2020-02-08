#include "quakedef.h"
#include "vr.h"
#include "vr_menu.h"
#include <string>
#include <array>

extern cvar_t vr_enabled;
extern cvar_t vr_crosshair;
extern cvar_t vr_crosshair_depth;
extern cvar_t vr_crosshair_size;
extern cvar_t vr_crosshair_alpha;
extern cvar_t vr_aimmode;
extern cvar_t vr_deadzone;
extern cvar_t vr_world_scale;
extern cvar_t vr_snap_turn;
extern cvar_t vr_turn_speed;

static int	vr_options_cursor = 0;

#define VR_MAX_TURN_SPEED 10.0f
#define VR_MAX_FLOOR_OFFSET 200.0f
#define VR_MAX_GUNANGLE 180.0f

extern void M_DrawSlider (int x, int y, float range);

void VR_Menu_Init()
{
    // VR menu function pointers
    vr_menucmdfn = VR_Menu_f;
    vr_menudrawfn = VR_MenuDraw;
    vr_menukeyfn = VR_MenuKey;
}

static void VR_MenuPlaySound(const char *sound, float fvol)
{
    sfx_t *sfx = S_PrecacheSound( sound );

    if ( sfx ) {
        S_StartSound( cl.viewentity, 0, sfx, vec3_origin, fvol, 1 );
    }
}

static void VR_MenuPrintOptionValue(int cx, int cy, int option)
{
    char value_buffer[16] = {0};
    const char *value_string = NULL;

#ifdef _MSC_VER
#define snprintf sprintf_s
#endif
    switch ( option ) {
        default: break;

        case VR_OPTION_ENABLED:
            M_DrawCheckbox( cx, cy, (int)vr_enabled.value );
            break;
        /*case VR_OPTION_PERFHUD:
            if (vr_perfhud.value == 1) value_string = "Latency Timing";
            else if (vr_perfhud.value == 2) value_string = "Render Timing";
            else if (vr_perfhud.value == 3) value_string = "Perf Headroom";
            else if (vr_perfhud.value == 4) value_string = "Version Info";
            else value_string = "off";
            break;*/
        case VR_OPTION_AIMMODE:
            switch ( (int)vr_aimmode.value ) {
                case VR_AIMMODE_HEAD_MYAW:
                    value_string = "HEAD_MYAW";
                    break;
                case VR_AIMMODE_HEAD_MYAW_MPITCH:
                    value_string = "HEAD_MYAW_MPITCH";
                    break;
                case VR_AIMMODE_MOUSE_MYAW:
                    value_string = "MOUSE_MYAW";
                    break;
                case VR_AIMMODE_MOUSE_MYAW_MPITCH:
                    value_string = "MOUSE_MYAW_MPITCH";
                    break;
                default:
                case VR_AIMMODE_BLENDED:
                    value_string = "BLENDED";
                    break;
                case VR_AIMMODE_BLENDED_NOPITCH:
                    value_string = "BLENDED_NOPITCH";
                    break;
                case VR_AIMMODE_CONTROLLER:
                    value_string = "CONTROLLER";
                    break;
            }
            break;
        case VR_OPTION_DEADZONE:
            if ( vr_deadzone.value > 0 ) {
                snprintf( value_buffer, sizeof(value_buffer), "%.0f degrees", vr_deadzone.value );
                value_string = value_buffer;
            } else {
                value_string = "off";
            }
            break;
        case VR_OPTION_CROSSHAIR:
            if ( (int)vr_crosshair.value == 2 ) {
                value_string = "line";
            } else if ( (int)vr_crosshair.value == 1 ) {
                value_string = "point";
            } else {
                value_string = "off";
            }
            break;
        case VR_OPTION_CROSSHAIR_DEPTH:
            if ( vr_crosshair_depth.value > 0 ) {
                snprintf( value_buffer, sizeof(value_buffer), "%.0f units", vr_crosshair_depth.value );
                value_string = value_buffer;
            } else {
                value_string = "off";
            }
            break;
        case VR_OPTION_CROSSHAIR_SIZE:
            if ( vr_crosshair_size.value > 0 ) {
                snprintf( value_buffer, sizeof(value_buffer), "%.0f pixels", vr_crosshair_size.value );
                value_string = value_buffer;
            } else {
                value_string = "off";
            }
            break;
        case VR_OPTION_CROSSHAIR_ALPHA:
            M_DrawSlider( cx, cy, vr_crosshair_alpha.value );
            break;
        case VR_OPTION_WORLD_SCALE:
            M_DrawSlider(cx, cy, vr_world_scale.value / 2.0f);
            break;
        case VR_OPTION_MOVEMENT_MODE:
            switch ((int)vr_movement_mode.value)
            {
            case VR_MOVEMENT_MODE_FOLLOW_HAND: value_string = "Follow hand"; break;
            case VR_MOVEMENT_MODE_RAW_INPUT: value_string = "Raw input"; break;
            }
            break;
        case VR_OPTION_SNAP_TURN:
            if (vr_snap_turn.value == 0)
            {
                value_string = "Smooth";
            }
            else
            {
                snprintf(value_buffer, sizeof(value_buffer), "%d Degrees", (int)vr_snap_turn.value);
                value_string = value_buffer;
            }
            break;
        case VR_OPTION_TURN_SPEED:
            M_DrawSlider(cx, cy, vr_turn_speed.value / VR_MAX_TURN_SPEED);
            break;
        case VR_OPTION_MSAA:
            if (vr_msaa.value == 0)
            {
                value_string = "Off";
            }
            else
            {
                snprintf(value_buffer, sizeof(value_buffer), "%d Samples", (int)vr_msaa.value);
                value_string = value_buffer;
            }
            break;
        case VR_OPTION_GUNANGLE:
            M_DrawSlider(cx, cy, vr_gunangle.value);
            break;
        case VR_OPTION_FLOOR_OFFSET:
            M_DrawSlider(cx, cy, vr_floor_offset.value);
            break;
        case VR_OPTION_GUNMODELPITCH:
            M_DrawSlider(cx, cy, vr_gunmodelpitch.value);
            break;
        case VR_OPTION_GUNMODELSCALE:
            M_DrawSlider(cx, cy, vr_gunmodelscale.value);
            break;
        case VR_OPTION_GUNMODELY:
            M_DrawSlider(cx, cy, vr_gunmodely.value);
            break;
        case VR_OPTION_CROSSHAIRY:
            M_DrawSlider(cx, cy, vr_crosshairy.value);
            break;
    }
#ifdef _MSC_VER
#undef snprintf
#endif
    if ( value_string ) {
        M_Print( cx, cy, value_string );
    }
}

static void VR_MenuKeyOption(int key, int option)
{
#define _sizeofarray(x) ( ( sizeof(x) / sizeof(x[0]) ) )
#define _maxarray(x) ( _sizeofarray(x) - 1 )

    qboolean isLeft = (key == K_LEFTARROW);
    int intValue = 0;
    float floatValue = 0.0f;
    int i = 0;

    int debug[] = { 0, 1, 2, 3, 4 };
    float ipdDiff = 0.2f;
    int position[] = { 0, 1, 2 };
    float multisample[] = { 1.0f, 1.25f, 1.50f, 1.75f, 2.0f };
    int aimmode[] = { 1, 2, 3, 4, 5, 6, 7 };
    int deadzoneDiff = 5;
    int crosshair[] = { 0, 1, 2 };
    int crosshairDepthDiff = 32;
    int crosshairSizeDiff = 1;
    float crosshairAlphaDiff = 0.05f;

    const auto adjustF = [&isLeft](const cvar_t& cvar, auto incr, auto min, auto max) {
        Cvar_SetValue(cvar.name, CLAMP(min, isLeft ? cvar.value - incr : cvar.value + incr, max));
    };

    const auto adjustI = [&isLeft](const cvar_t& cvar, auto incr, auto min, auto max) {
        Cvar_SetValue(cvar.name, (int) CLAMP(min, isLeft ? cvar.value - incr : cvar.value + incr, max));
    };

    switch ( option ) {
        case VR_OPTION_ENABLED:
            //Cvar_SetValue( "vr_enabled", ! (int)vr_enabled.value );
            if ( (int)vr_enabled.value ) {
                VR_MenuPlaySound( "items/r_item2.wav", 0.5 );
            }
            break;
        /*case VR_OPTION_PERFHUD:
            intValue = (int)vr_perfhud.value;
            intValue = CLAMP( debug[0], isLeft ? intValue - 1 : intValue + 1, debug[_maxarray( debug )] );
            Cvar_SetValue( "vr_perfhud", intValue );
            break;*/
        case VR_OPTION_AIMMODE:
            intValue = (int)vr_aimmode.value;
            intValue = CLAMP( aimmode[0], isLeft ? intValue - 1 : intValue + 1, _sizeofarray( aimmode ) );
            intValue -= 1;
            Cvar_SetValue( "vr_aimmode", aimmode[intValue] );
            break;
        case VR_OPTION_DEADZONE:
            adjustF(vr_deadzone, deadzoneDiff, 0.f, 180.f);
            break;
        case VR_OPTION_CROSSHAIR:
            intValue = (int)vr_crosshair.value;
            intValue = CLAMP( crosshair[0], isLeft ? intValue - 1 : intValue + 1, crosshair[_maxarray( crosshair) ] );
            Cvar_SetValue( "vr_crosshair", intValue );
            break;
        case VR_OPTION_CROSSHAIR_DEPTH:
            adjustF(vr_crosshair_depth, crosshairDepthDiff, 0.f, 4096.f);
            break;
        case VR_OPTION_CROSSHAIR_SIZE:
            Cvar_SetValue( "vr_crosshair_size", intValue );
            adjustF(vr_crosshair_size, crosshairSizeDiff, 0.f, 32.f);
            break;
        case VR_OPTION_CROSSHAIR_ALPHA:
            adjustF(vr_crosshair_alpha, crosshairAlphaDiff, 0.f, 1.f);
            break;
        case VR_OPTION_WORLD_SCALE:
            adjustF(vr_world_scale, crosshairAlphaDiff, 0.f, 2.f);
            break; 
        case VR_OPTION_MOVEMENT_MODE:
            adjustI(vr_movement_mode, 1, 0.f, VR_MAX_MOVEMENT_MODE);
            break;
        case VR_OPTION_SNAP_TURN:
            adjustI(vr_snap_turn, 45, 0.f, 90.f);
            break;
        case VR_OPTION_TURN_SPEED:
            adjustF(vr_turn_speed, 0.25f, 0.f, VR_MAX_TURN_SPEED);
            break;
        case VR_OPTION_MSAA:
            intValue = (int)vr_msaa.value;
            int max;
            glGetIntegerv(GL_MAX_SAMPLES, &max);
            intValue = CLAMP(0, isLeft ? intValue - 1 : intValue + 1, max - 1);
            Cvar_SetValue("vr_msaa", intValue);
            break;
        case VR_OPTION_GUNANGLE:
            adjustF(vr_gunangle, 2.5f, -VR_MAX_GUNANGLE, VR_MAX_GUNANGLE);
            break;
        case VR_OPTION_FLOOR_OFFSET:
            adjustF(vr_floor_offset, 2.5f, -VR_MAX_FLOOR_OFFSET, VR_MAX_FLOOR_OFFSET);
            break;
        case VR_OPTION_GUNMODELPITCH:
            adjustF(vr_gunmodelpitch, 0.5f, -90.f, 90.f);
            break;
        case VR_OPTION_GUNMODELSCALE:
            adjustF(vr_gunmodelscale, 0.05f, 0.1f, 2.f);
            break;
        case VR_OPTION_GUNMODELY:
            adjustF(vr_gunmodely, 0.1f, -5.0f, 5.f);
            break;
        case VR_OPTION_CROSSHAIRY:
            adjustF(vr_crosshairy, 0.05f, -10.0f, 10.f);
            break;
    }

#undef _maxarray
#undef _sizeofarray
}

static void VR_MenuKey(int key)
{
    switch ( key ) {
        case K_ESCAPE:
            VID_SyncCvars(); // sync cvars before leaving menu. FIXME: there are other ways to leave menu
            S_LocalSound( "misc/menu1.wav" );
            M_Menu_Options_f();
            break;

        case K_UPARROW:
            S_LocalSound( "misc/menu1.wav" );
            vr_options_cursor--;
            if ( vr_options_cursor < 0 ) {
                vr_options_cursor = VR_OPTION_MAX - 1;
            }
            break;

        case K_DOWNARROW:
            S_LocalSound( "misc/menu1.wav" );
            vr_options_cursor++;
            if ( vr_options_cursor >= VR_OPTION_MAX ) {
                vr_options_cursor = 0;
            }
            break;

        case K_LEFTARROW:
            S_LocalSound ("misc/menu3.wav");
            VR_MenuKeyOption( key, vr_options_cursor );
            break;

        case K_RIGHTARROW:
            S_LocalSound ("misc/menu3.wav");
            VR_MenuKeyOption( key, vr_options_cursor );
            break;

        case K_ENTER:
            m_entersound = true;
            VR_MenuKeyOption( key, vr_options_cursor );
            break;

        default: break;
    }
}

static void VR_MenuDraw (void)
{
    int i, y;
    qpic_t *p;
    const char *title;

    y = 4;

    // plaque
    p = Draw_CachePic ("gfx/qplaque.lmp");
    M_DrawTransPic (16, y, p);


    // customize header
    p = Draw_CachePic ("gfx/ttl_cstm.lmp");
    M_DrawPic ( (320-p->width)/2, y, p);

    y += 28;

    // title
    title = "VR/HMD OPTIONS";
    M_PrintWhite ((320-8*strlen(title))/2, y, title);

    y += 16;
    int idx = 0;

    static const auto adjustedLabels = [](auto... labels){
        return std::array{
            (std::string(24 - strlen(labels), ' ') + labels)...
        };
    }(
        "VR Enabled",
        "Aim Mode",
        "Deadzone",
        "Crosshair",
        "Crosshair Depth",
        "Crosshair Size",
        "Crosshair Alpha",
        "World Scale",
        "Movement mode",
        "Turn",
        "Turn Speed",
        "MSAA",
        "Gun Angle",
        "Floor Offset",
        "Gun Model Pitch",
        "Gun Model Scale",
        "Gun Model Y",
        "Crosshair Y"
    );

    for (const std::string& label : adjustedLabels)
    {
        M_Print(16, y, label.data());
        VR_MenuPrintOptionValue(240, y, idx);

        // draw the blinking cursor
        if (vr_options_cursor == idx) {
            M_DrawCharacter(220, y, 12 + ((int)(realtime * 4) & 1));
        }

        ++idx;
        y += 8;
    }
}

void VR_Menu_f (void)
{
    const char *sound = "items/r_item1.wav";

    IN_Deactivate(modestate == MS_WINDOWED);
    key_dest = key_menu;
    m_state = m_vr;
    m_entersound = true;

    VR_MenuPlaySound( sound, 0.5 );
}
