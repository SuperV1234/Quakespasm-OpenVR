#include "quakedef.h"
#include "vr.h"
#include "wpnoffset_menu.h"
#include <string>
#include <cassert>
#include <array>
#include <tuple>
#include "cmd.h"

static int wpnoff_cursor = 0;

extern void M_DrawSlider (int x, int y, float range);

static void WpnOffset_MenuPlaySound(const char *sound, float fvol)
{
	sfx_t *sfx = S_PrecacheSound( sound );

	if ( sfx ) {
		S_StartSound( cl.viewentity, 0, sfx, vec3_origin, fvol, 1 );
	}
}

static auto getCvars()
{
	return std::tie(
		vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON],		// OffsetX
		vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 1],	// OffsetY
		vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 2],	// OffsetZ
		vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 3],	// Scale
		vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 7],	// Roll
		vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 5],	// Pitch
		vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 6]		// Yaw
	);
}

static void WpnOffset_MenuPrintOptionValue(int cx, int cy, WpnOffsetMenuOpt option)
{
	char value_buffer[32] = {0};

	const auto printAsStr = [&](const auto& cvar)
	{
		snprintf(value_buffer, sizeof(value_buffer), "%.4f", cvar.value);
		M_Print(cx, cy, value_buffer);
	};

	auto [ox, oy, oz, sc, rr, rp, ry] = getCvars();

	switch (option)
	{
		case WpnOffsetMenuOpt::OffsetX:	printAsStr(ox); break;
		case WpnOffsetMenuOpt::OffsetY:	printAsStr(oy); break;
		case WpnOffsetMenuOpt::OffsetZ:	printAsStr(oz); break;
		case WpnOffsetMenuOpt::Scale:	printAsStr(sc); break;
		case WpnOffsetMenuOpt::Roll:	printAsStr(rr); break;
		case WpnOffsetMenuOpt::Pitch:	printAsStr(rp); break;
		case WpnOffsetMenuOpt::Yaw:		printAsStr(ry); break;
	}
}

static void WpnOffset_MenuKeyOption(int key, WpnOffsetMenuOpt option)
{
	const bool isLeft = (key == K_LEFTARROW);

	const auto adjustF = [&isLeft](const cvar_t& cvar, auto incr, auto min, auto max) {
		Cvar_SetValue(cvar.name, CLAMP(min, isLeft ? cvar.value - incr : cvar.value + incr, max));
	};

	auto [ox, oy, oz, sc, rr, rp, ry] = getCvars();

	constexpr float oInc = 0.1f;
	constexpr float oBound = 100.f;

	constexpr float rInc = 0.5f;
	constexpr float rBound = 90.f;

	switch (option)
	{
		case WpnOffsetMenuOpt::OffsetX:	adjustF(ox, oInc, -oBound, oBound); break;
		case WpnOffsetMenuOpt::OffsetY:	adjustF(oy, oInc, -oBound, oBound); break;
		case WpnOffsetMenuOpt::OffsetZ:	adjustF(oz, oInc, -oBound, oBound); break;
		case WpnOffsetMenuOpt::Scale:	adjustF(sc, 0.01f, 0.01f, 2.f); break;
		case WpnOffsetMenuOpt::Roll:	adjustF(rr, rInc, -rBound, rBound); break;
		case WpnOffsetMenuOpt::Pitch:	adjustF(rp, rInc, -rBound, rBound); break;
		case WpnOffsetMenuOpt::Yaw:		adjustF(ry, rInc, -rBound, rBound); break;
	}
}

void WpnOffset_MenuKey(int key)
{
	switch ( key ) {
		case K_ESCAPE:
			VID_SyncCvars(); // sync cvars before leaving menu. FIXME: there are other ways to leave menu
			S_LocalSound( "misc/menu1.wav" );
			M_Menu_Options_f();
			break;

		case K_UPARROW:
			S_LocalSound( "misc/menu1.wav" );
			wpnoff_cursor--;
			if ( wpnoff_cursor < 0 ) {
				wpnoff_cursor = (int)WpnOffsetMenuOpt::Max - 1;
			}
			break;

		case K_DOWNARROW:
			S_LocalSound( "misc/menu1.wav" );
			wpnoff_cursor++;
			if ( wpnoff_cursor >= (int)WpnOffsetMenuOpt::Max ) {
				wpnoff_cursor = 0;
			}
			break;

		case K_LEFTARROW:
			[[fallthrough]];
		case K_RIGHTARROW:
			S_LocalSound ("misc/menu3.wav");
			WpnOffset_MenuKeyOption( key, (WpnOffsetMenuOpt)wpnoff_cursor );
			break;

		case K_ENTER:
			m_entersound = true;
			WpnOffset_MenuKeyOption( key, (WpnOffsetMenuOpt)wpnoff_cursor );
			break;

		default: break;
	}
}

void WpnOffset_MenuDraw (void)
{
	int y = 4;

	// plaque
	M_DrawTransPic (16, y, Draw_CachePic("gfx/qplaque.lmp"));

	// customize header
	qpic_t* p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ( (320-p->width)/2, y, p);

	y += 28;

	// title
	const char* title = "Weapon Offsets";
	M_PrintWhite ((320-8*strlen(title))/2, y, title);

	y += 16;
	int idx = 0;

	static const auto adjustedLabels = [](auto... labels){
		return std::array{
			(std::string(24 - strlen(labels), ' ') + labels)...
		};
	}(
		"Offset X",
		"Offset Y",
		"Offset Z",
		"Scale",
		"Roll",
		"Pitch",
		"Yaw"
	);

	static_assert(adjustedLabels.size() == (int)WpnOffsetMenuOpt::Max);

	for (const std::string& label : adjustedLabels)
	{
		M_Print(16, y, label.data());
		WpnOffset_MenuPrintOptionValue(240, y, (WpnOffsetMenuOpt) idx);

		// draw the blinking cursor
		if (wpnoff_cursor == idx) {
			M_DrawCharacter(220, y, 12 + ((int)(realtime * 4) & 1));
		}

		++idx;
		y += 8;
	}
}

void WpnOffset_Menu_f (void)
{
	const char *sound = "items/r_item1.wav";

	IN_Deactivate(modestate == MS_WINDOWED);
	key_dest = key_menu;
	m_state = m_wpn_offset;
	m_entersound = true;

	WpnOffset_MenuPlaySound( sound, 0.5 );
}
