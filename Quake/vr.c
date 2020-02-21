#include "vr.h"
#include "quakedef.h"
#include "vr_menu.h"
#include <cassert>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <gtx/euler_angles.hpp>
#include <gtx/quaternion.hpp>
#include <vector>

[[nodiscard]] constexpr glm::vec3 toVec3(vec3_t v) noexcept
{
    return {v[0], v[1], v[2]};
}

static double lerp(double a, double b, double f)
{
    return (a * (1.0 - f)) + (b * f);
}

static void vec3lerp(vec3_t out, vec3_t start, vec3_t end, double f)
{
    out[0] = lerp(start[0], end[0], f);
    out[1] = lerp(start[1], end[1], f);
    out[2] = lerp(start[2], end[2], f);
}

// TODO VR:
template <typename _Fp>
constexpr _Fp __lerp(_Fp __a, _Fp __b, _Fp __t) noexcept
{
    if((__a <= 0 && __b >= 0) || (__a >= 0 && __b <= 0))
        return __t * __b + (1 - __t) * __a;

    if(__t == 1) return __b;
    const _Fp __x = __a + __t * (__b - __a);
    if((__t > 1) == (__b > __a))
        return __b < __x ? __x : __b;
    else
        return __x < __b ? __x : __b;
}

#ifdef WIN32
#define UNICODE 1
#include <mmsystem.h>
#undef UNICODE
#endif

#include "openvr.h"

#if SDL_MAJOR_VERSION < 2
FILE* __iob_func()
{
    FILE result[3] = {*stdin, *stdout, *stderr};
    return result;
}
#endif

struct fbo_t
{
    GLuint framebuffer, depth_texture, texture;
    GLuint msaa_framebuffer, msaa_texture, msaa_depth_texture;
    int msaa;
    struct
    {
        float width, height;
    } size;
};

struct vr_eye_t
{
    int index;
    fbo_t fbo;
    vr::EVREye eye;
    vr::HmdVector3_t position;
    vr::HmdQuaternion_t orientation;
    float fov_x, fov_y;
};

struct vr_controller
{
    vr::VRControllerState_t state;
    vr::VRControllerState_t lastState;
    vec3_t position;
    vec3_t orientation;
    vec3_t velocity;
    vr::HmdVector3_t rawvector;
    vr::HmdQuaternion_t raworientation;
};

// OpenGL Extensions
#define GL_READ_FRAMEBUFFER_EXT 0x8CA8
#define GL_DRAW_FRAMEBUFFER_EXT 0x8CA9
#define GL_FRAMEBUFFER_SRGB_EXT 0x8DB9

typedef void(APIENTRYP PFNGLBLITFRAMEBUFFEREXTPROC)(
    GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
typedef bool(APIENTRYP PFNWGLSWAPINTERVALEXTPROC)(int);

static PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT;
static PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glCheckFramebufferStatusEXT;
static PFNGLBLITFRAMEBUFFEREXTPROC glBlitFramebufferEXT;
static PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT;
static PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT;
static PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT;
static PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT;
static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
static PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisampleEXT;

struct
{
    void* func;
    const char* name;
} gl_extensions[] = {
    {&glBindFramebufferEXT, "glBindFramebufferEXT"},
    {&glBlitFramebufferEXT, "glBlitFramebufferEXT"},
    {&glDeleteFramebuffersEXT, "glDeleteFramebuffersEXT"},
    {&glGenFramebuffersEXT, "glGenFramebuffersEXT"},
    {&glTexImage2DMultisampleEXT, "glTexImage2DMultisample"},
    {&glFramebufferTexture2DEXT, "glFramebufferTexture2DEXT"},
    {&glFramebufferRenderbufferEXT, "glFramebufferRenderbufferEXT"},
    {&glCheckFramebufferStatusEXT, "glCheckFramebufferStatusEXT"},
    {&wglSwapIntervalEXT, "wglSwapIntervalEXT"},
    {nullptr, nullptr},
};

// main screen & 2D drawing
extern void SCR_SetUpToDrawConsole(void);
extern void SCR_UpdateScreenContent();
extern qboolean scr_drawdialog;
extern void SCR_DrawNotifyString(void);
extern qboolean scr_drawloading;
extern void SCR_DrawLoading(void);
extern void SCR_CheckDrawCenterString(void);
extern void SCR_DrawRam(void);
extern void SCR_DrawNet(void);
extern void SCR_DrawTurtle(void);
extern void SCR_DrawPause(void);
extern void SCR_DrawDevStats(void);
extern void SCR_DrawFPS(void);
extern void SCR_DrawClock(void);
extern void SCR_DrawConsole(void);

// rendering
extern void R_SetupView(void);
extern void R_RenderScene(void);
extern int glx, gly, glwidth, glheight;
extern refdef_t r_refdef;
extern vec3_t vright;

static float vrYaw;
static bool readbackYaw;

vec3_t vr_viewOffset;
glm::vec3 lastHudPosition{};
glm::vec3 lastMenuPosition{};

vr::IVRSystem* ovrHMD;
vr::TrackedDevicePose_t ovr_DevicePose[vr::k_unMaxTrackedDeviceCount];

static vr_eye_t eyes[2];
static vr_eye_t* current_eye = nullptr;
static vr_controller controllers[2];
static vec3_t lastOrientation = {0, 0, 0};
static vec3_t lastAim = {0, 0, 0};

static bool vr_initialized = false;

static vec3_t headOrigin;
static vec3_t lastHeadOrigin;

vec3_t vr_room_scale_move;

// Wolfenstein 3D, DOOM and QUAKE use the same coordinate/unit system:
// 8 foot (96 inch) height wall == 64 units, 1.5 inches per pixel unit
// 1.0 pixel unit / 1.5 inch == 0.666666 pixel units per inch
#define meters_to_units (vr_world_scale.value / (1.5f * 0.0254f))

extern cvar_t gl_farclip;
extern int glwidth, glheight;
extern void SCR_UpdateScreenContent();
extern refdef_t r_refdef;

static std::vector<cvar_t*> cvarsToRegister;

#define DEFINE_CVAR(name, defaultValue, type)         \
    cvar_t name = {#name, #defaultValue, type};       \
    static struct _cvar_registrar##name##__LINE__##_t \
    {                                                 \
        _cvar_registrar##name##__LINE__##_t()         \
        {                                             \
            cvarsToRegister.emplace_back(&name);      \
        }                                             \
    } _cvar_registrar##name##__LINE__

DEFINE_CVAR(vr_enabled, 0, CVAR_NONE);
DEFINE_CVAR(vr_viewkick, 0, CVAR_NONE);
DEFINE_CVAR(vr_lefthanded, 0, CVAR_NONE);
DEFINE_CVAR(vr_crosshair, 1, CVAR_ARCHIVE);
DEFINE_CVAR(vr_crosshair_depth, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_crosshair_size, 3.0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_crosshair_alpha, 0.25, CVAR_ARCHIVE);
DEFINE_CVAR(vr_aimmode, 7, CVAR_ARCHIVE);
DEFINE_CVAR(vr_deadzone, 30, CVAR_ARCHIVE);
DEFINE_CVAR(vr_gunangle, 32, CVAR_ARCHIVE);
DEFINE_CVAR(vr_gunmodelpitch, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_gunmodelscale, 1.0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_gunmodely, 0, CVAR_ARCHIVE);
// TODO VR:
// DEFINE_CVAR(vr_projectilespawn_z_offset, 24, CVAR_ARCHIVE);
DEFINE_CVAR(vr_crosshairy, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_world_scale, 1.0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_floor_offset, -16, CVAR_ARCHIVE);
DEFINE_CVAR(vr_snap_turn, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_turn_speed, 1, CVAR_ARCHIVE);
DEFINE_CVAR(vr_msaa, 4, CVAR_ARCHIVE);
DEFINE_CVAR(vr_movement_mode, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_joystick_yaw_multi, 1.0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_joystick_axis_deadzone, 0.25, CVAR_ARCHIVE);
DEFINE_CVAR(vr_joystick_axis_menu_deadzone_extra, 0.25, CVAR_ARCHIVE);
DEFINE_CVAR(vr_joystick_axis_exponent, 1.0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_joystick_deadzone_trunc, 1, CVAR_ARCHIVE);
DEFINE_CVAR(vr_hud_scale, 0.025, CVAR_ARCHIVE);
DEFINE_CVAR(vr_menu_scale, 0.13, CVAR_ARCHIVE);
DEFINE_CVAR(vr_melee_threshold, 7, CVAR_ARCHIVE);
DEFINE_CVAR(vr_gunyaw, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_gun_z_offset, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_sbar_mode, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_sbar_offset_x, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_sbar_offset_y, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_sbar_offset_z, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_sbar_offset_pitch, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_sbar_offset_yaw, 0, CVAR_ARCHIVE);
DEFINE_CVAR(vr_sbar_offset_roll, 0, CVAR_ARCHIVE);

static qboolean InitOpenGLExtensions()
{
    int i;
    static qboolean extensions_initialized;

    if(extensions_initialized) return true;

    for(i = 0; gl_extensions[i].func; i++)
    {
        void* func = SDL_GL_GetProcAddress(gl_extensions[i].name);
        if(!func) return false;

        *((void**)gl_extensions[i].func) = func;
    }

    extensions_initialized = true;
    return extensions_initialized;
}

void RecreateTextures(fbo_t* fbo, int width, int height)
{
    GLuint oldDepth = fbo->depth_texture;
    GLuint oldTexture = fbo->texture;

    glGenTextures(1, &fbo->depth_texture);
    glGenTextures(1, &fbo->texture);

    if(oldDepth)
    {
        glDeleteTextures(1, &oldDepth);
        glDeleteTextures(1, &oldTexture);
    }

    glBindTexture(GL_TEXTURE_2D, fbo->depth_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
        GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

    glBindTexture(GL_TEXTURE_2D, fbo->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
        GL_UNSIGNED_BYTE, nullptr);

    fbo->size.width = width;
    fbo->size.height = height;

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo->framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
        GL_TEXTURE_2D, fbo->texture, 0);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
        GL_TEXTURE_2D, fbo->depth_texture, 0);
}


fbo_t CreateFBO(int width, int height)
{
    fbo_t fbo;

    glGenFramebuffersEXT(1, &fbo.framebuffer);

    fbo.depth_texture = 0;

    RecreateTextures(&fbo, width, height);

    fbo.msaa = 0;
    fbo.msaa_framebuffer = 0;
    fbo.msaa_texture = 0;

    return fbo;
}

void CreateMSAA(fbo_t* fbo, int width, int height, int msaa)
{
    fbo->msaa = msaa;

    if(fbo->msaa_framebuffer)
    {
        glDeleteFramebuffersEXT(1, &fbo->msaa_framebuffer);
        glDeleteTextures(1, &fbo->msaa_texture);
        glDeleteTextures(1, &fbo->msaa_depth_texture);
    }

    glGenFramebuffersEXT(1, &fbo->msaa_framebuffer);
    glGenTextures(1, &fbo->msaa_texture);
    glGenTextures(1, &fbo->msaa_depth_texture);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, fbo->msaa_texture);
    glTexImage2DMultisampleEXT(
        GL_TEXTURE_2D_MULTISAMPLE, msaa, GL_RGBA8, width, height, false);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, fbo->msaa_depth_texture);
    glTexImage2DMultisampleEXT(GL_TEXTURE_2D_MULTISAMPLE, msaa,
        GL_DEPTH_COMPONENT24, width, height, false);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo->msaa_framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
        GL_TEXTURE_2D_MULTISAMPLE, fbo->msaa_texture, 0);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
        GL_TEXTURE_2D_MULTISAMPLE, fbo->msaa_depth_texture, 0);

    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if(status != GL_FRAMEBUFFER_COMPLETE)
    {
        Con_Printf("Framebuffer incomplete %x", status);
    }
}

void DeleteFBO(fbo_t fbo)
{
    glDeleteFramebuffersEXT(1, &fbo.framebuffer);
    glDeleteTextures(1, &fbo.depth_texture);
    glDeleteTextures(1, &fbo.texture);
}

void QuatToYawPitchRoll(vr::HmdQuaternion_t q, vec3_t out)
{
    auto sqw = q.w * q.w;
    auto sqx = q.x * q.x;
    auto sqy = q.y * q.y;
    auto sqz = q.z * q.z;

    out[ROLL] = -atan2(2 * (q.x * q.y + q.w * q.z), sqw - sqx + sqy - sqz) /
                M_PI_DIV_180;
    out[PITCH] = -asin(-2 * (q.y * q.z - q.w * q.x)) / M_PI_DIV_180;
    out[YAW] = atan2(2 * (q.x * q.z + q.w * q.y), sqw - sqx - sqy + sqz) /
                   M_PI_DIV_180 +
               vrYaw;
}

void Vec3RotateZ(vec3_t in, float angle, vec3_t out)
{
    out[0] = in[0] * cos(angle) - in[1] * sin(angle);
    out[1] = in[0] * sin(angle) + in[1] * cos(angle);
    out[2] = in[2];
}

vr::HmdMatrix44_t TransposeMatrix(vr::HmdMatrix44_t in)
{
    vr::HmdMatrix44_t out;
    int y, x;
    for(y = 0; y < 4; y++)
        for(x = 0; x < 4; x++) out.m[x][y] = in.m[y][x];

    return out;
}

vr::HmdVector3_t AddVectors(vr::HmdVector3_t a, vr::HmdVector3_t b)
{
    vr::HmdVector3_t out;

    out.v[0] = a.v[0] + b.v[0];
    out.v[1] = a.v[1] + b.v[1];
    out.v[2] = a.v[2] + b.v[2];

    return out;
}

// Rotates a vector by a quaternion and returns the results
// Based on math from
// https://gamedev.stackexchange.com/questions/28395/rotating-vector3-by-a-quaternion
vr::HmdVector3_t RotateVectorByQuaternion(
    vr::HmdVector3_t v, vr::HmdQuaternion_t q)
{
    vr::HmdVector3_t u, result;
    u.v[0] = q.x;
    u.v[1] = q.y;
    u.v[2] = q.z;
    float s = q.w;

    // Dot products of u,v and u,u
    float uvDot = (u.v[0] * v.v[0] + u.v[1] * v.v[1] + u.v[2] * v.v[2]);
    float uuDot = (u.v[0] * u.v[0] + u.v[1] * u.v[1] + u.v[2] * u.v[2]);

    // Calculate cross product of u, v
    vr::HmdVector3_t uvCross;
    uvCross.v[0] = u.v[1] * v.v[2] - u.v[2] * v.v[1];
    uvCross.v[1] = u.v[2] * v.v[0] - u.v[0] * v.v[2];
    uvCross.v[2] = u.v[0] * v.v[1] - u.v[1] * v.v[0];

    // Calculate each vectors' result individually because there aren't
    // arthimetic functions for HmdVector3_t dsahfkldhsaklfhklsadh
    result.v[0] = u.v[0] * 2.0f * uvDot + (s * s - uuDot) * v.v[0] +
                  2.0f * s * uvCross.v[0];
    result.v[1] = u.v[1] * 2.0f * uvDot + (s * s - uuDot) * v.v[1] +
                  2.0f * s * uvCross.v[1];
    result.v[2] = u.v[2] * 2.0f * uvDot + (s * s - uuDot) * v.v[2] +
                  2.0f * s * uvCross.v[2];

    return result;
}

// Transforms a HMD Matrix34 to a Vector3
// Math borrowed from https://github.com/Omnifinity/OpenVR-Tracking-Example
vr::HmdVector3_t Matrix34ToVector(vr::HmdMatrix34_t in)
{
    vr::HmdVector3_t vector;

    vector.v[0] = in.m[0][3];
    vector.v[1] = in.m[1][3];
    vector.v[2] = in.m[2][3];

    return vector;
}

// Transforms a HMD Matrix34 to a Quaternion
// Function logic nicked from
// https://github.com/Omnifinity/OpenVR-Tracking-Example
vr::HmdQuaternion_t Matrix34ToQuaternion(vr::HmdMatrix34_t in)
{
    vr::HmdQuaternion_t q;

    q.w = sqrt(fmax(0, 1.0 + in.m[0][0] + in.m[1][1] + in.m[2][2])) / 2.0;
    q.x = sqrt(fmax(0, 1.0 + in.m[0][0] - in.m[1][1] - in.m[2][2])) / 2.0;
    q.y = sqrt(fmax(0, 1.0 - in.m[0][0] + in.m[1][1] - in.m[2][2])) / 2.0;
    q.z = sqrt(fmax(0, 1.0 - in.m[0][0] - in.m[1][1] + in.m[2][2])) / 2.0;
    q.x = copysign(
        q.x, static_cast<double>(in.m[2][1]) - static_cast<double>(in.m[1][2]));
    q.y = copysign(
        q.y, static_cast<double>(in.m[0][2]) - static_cast<double>(in.m[2][0]));
    q.z = copysign(
        q.z, static_cast<double>(in.m[1][0]) - static_cast<double>(in.m[0][1]));
    return q;
}

void HmdVec3RotateY(vr::HmdVector3_t* pos, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    float x = c * pos->v[0] - s * pos->v[2];
    float y = s * pos->v[0] + c * pos->v[2];

    pos->v[0] = x;
    pos->v[2] = y;
}

// ----------------------------------------------------------------------------
// Callbacks for cvars

static void VR_Enabled_f(cvar_t* var)
{
    (void)var;

    VID_VR_Disable();

    if(!vr_enabled.value) return;

    if(!VR_Enable()) Cvar_SetValueQuick(&vr_enabled, 0);
}



static void VR_Deadzone_f(cvar_t* var)
{
    (void)var;

    // clamp the mouse to a max of 0 - 70 degrees
    const auto deadzone = CLAMP(0.0f, vr_deadzone.value, 70.0f);
    if(deadzone != vr_deadzone.value)
    {
        Cvar_SetValueQuick(&vr_deadzone, deadzone);
    }
}

// Weapon scale/position stuff
cvar_t vr_weapon_offset[MAX_WEAPONS * VARS_PER_WEAPON];

aliashdr_t* lastWeaponHeader;
int weaponCVarEntry;

void Mod_Weapon(const char* name, aliashdr_t* hdr)
{
    if(lastWeaponHeader != hdr)
    {
        lastWeaponHeader = hdr;
        weaponCVarEntry = -1;
        for(int i = 0; i < MAX_WEAPONS; i++)
        {
            if(!strcmp(vr_weapon_offset[i * VARS_PER_WEAPON + 4].string, name))
            {
                weaponCVarEntry = i;
                break;
            }
        }
        if(weaponCVarEntry == -1)
        {
            Con_Printf("No VR offset for weapon: %s\n", name);
        }
    }

    if(weaponCVarEntry != -1)
    {
        float scaleCorrect =
            (vr_world_scale.value / 0.75f) *
            vr_gunmodelscale.value; // initial version had 0.75 default world
                                    // scale, so weapons reflect that
        VectorScale(hdr->original_scale,
            vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 3].value *
                scaleCorrect,
            hdr->scale);

        // TODO VR:
        vec3_t ofs = {vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON].value,
            vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 1].value,
            vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 2].value +
                vr_gunmodely.value};

        VectorAdd(hdr->original_scale_origin, ofs, hdr->scale_origin);
        VectorScale(hdr->scale_origin, scaleCorrect, hdr->scale_origin);
    }
}

char* CopyWithNumeral(const char* str, int i)
{
    auto len = strlen(str);
    char* ret = (char*)malloc(len + 1);
    assert(ret != 0);
    strcpy(ret, str);
    ret[len - 1] = '0' + (i % 10);
    ret[len - 2] = '0' + (i / 10);
    return ret;
}

void InitWeaponCVar(cvar_t* cvar, const char* name, int i, const char* value)
{
    const char* cvarname = CopyWithNumeral(name, i + 1);
    if(!Cvar_FindVar(cvarname))
    {
        cvar->name = cvarname;
        cvar->string = value;
        cvar->flags = CVAR_ARCHIVE;
        Cvar_RegisterVariable(cvar);
    }
    else
    {
        Cvar_SetQuick(cvar, value);
    }
}

void InitWeaponCVars(int i, const char* id, const char* offsetX,
    const char* offsetY, const char* offsetZ, const char* scale,
    const char* roll = "0.0", const char* pitch = "0.0",
    const char* yaw = "0.0")
{
    constexpr const char* nameOffsetX = "vr_wofs_x_nn";
    constexpr const char* nameOffsetY = "vr_wofs_y_nn";
    constexpr const char* nameOffsetZ = "vr_wofs_z_nn";
    constexpr const char* nameScale = "vr_wofs_scale_nn";
    constexpr const char* nameID = "vr_wofs_id_nn";
    constexpr const char* nameRoll = "vr_wofs_roll_nn";
    constexpr const char* namePitch = "vr_wofs_pitch_nn";
    constexpr const char* nameYaw = "vr_wofs_yaw_nn";

    InitWeaponCVar(
        &vr_weapon_offset[i * VARS_PER_WEAPON], nameOffsetX, i, offsetX);
    InitWeaponCVar(
        &vr_weapon_offset[i * VARS_PER_WEAPON + 1], nameOffsetY, i, offsetY);
    InitWeaponCVar(
        &vr_weapon_offset[i * VARS_PER_WEAPON + 2], nameOffsetZ, i, offsetZ);
    InitWeaponCVar(
        &vr_weapon_offset[i * VARS_PER_WEAPON + 3], nameScale, i, scale);
    InitWeaponCVar(&vr_weapon_offset[i * VARS_PER_WEAPON + 4], nameID, i, id);
    InitWeaponCVar(
        &vr_weapon_offset[i * VARS_PER_WEAPON + 5], nameRoll, i, roll);
    InitWeaponCVar(
        &vr_weapon_offset[i * VARS_PER_WEAPON + 6], namePitch, i, pitch);
    InitWeaponCVar(&vr_weapon_offset[i * VARS_PER_WEAPON + 7], nameYaw, i, yaw);
}

void InitAllWeaponCVars()
{
    int i = 0;
    if(!strcmp(COM_SkipPath(com_gamedir), "ad"))
    { // weapons for Arcane Dimensions mod; initially made for v1.70 + patch1
        InitWeaponCVars(i++, "progs/v_shadaxe0.mdl", "-1.5", "43.1", "41",
            "0.25"); // shadow axe
        InitWeaponCVars(i++, "progs/v_shadaxe3.mdl", "-1.5", "43.1", "41",
            "0.25"); // shadow axe upgrade, same numbers
        InitWeaponCVars(
            i++, "progs/v_shot.mdl", "1.5", "1.7", "17.5", "0.33"); // shotgun
        InitWeaponCVars(i++, "progs/v_shot2.mdl", "-3.5", "0.4", "8.5",
            "0.8"); // double barrel shotgun
        InitWeaponCVars(i++, "progs/v_shot3.mdl", "-3.5", "0.4", "8.5",
            "0.8"); // triple barrel shotgun ("Widowmaker")
        InitWeaponCVars(
            i++, "progs/v_nail.mdl", "-9.5", "3", "17", "0.5"); // nailgun
        InitWeaponCVars(
            i++, "progs/v_nail2.mdl", "-6", "3.5", "20", "0.4"); // supernailgun
        InitWeaponCVars(
            i++, "progs/v_rock.mdl", "-3", "1.25", "17", "0.5"); // grenade
        InitWeaponCVars(
            i++, "progs/v_rock2.mdl", "0", "5.55", "22.5", "0.45"); // rocket
        InitWeaponCVars(
            i++, "progs/v_light.mdl", "-4", "3.1", "13", "0.5"); // lightning
        InitWeaponCVars(
            i++, "progs/v_plasma.mdl", "2.8", "1.8", "22.5", "0.5"); // plasma
    }
    else
    { // weapons for vanilla Quake, Scourge of Armagon, Dissolution of Eternity
        // vanilla quake weapons
        InitWeaponCVars(i++, "progs/v_axe.mdl", "-4", "24", "37", "0.33");
        InitWeaponCVars(i++, "progs/v_shot.mdl", "1.5", "1", "10", "0.5"); // gun
        InitWeaponCVars(
            i++, "progs/v_shot2.mdl", "-3.5", "1", "8.5", "0.8"); // shotgun
        InitWeaponCVars(
            i++, "progs/v_nail.mdl", "-5", "3", "15", "0.5"); // nailgun
        InitWeaponCVars(
            i++, "progs/v_nail2.mdl", "0", "3", "19", "0.5"); // supernailgun
        InitWeaponCVars(
            i++, "progs/v_rock.mdl", "10", "1.5", "13", "0.5"); // grenade
        InitWeaponCVars(
            i++, "progs/v_rock2.mdl", "10", "7", "19", "0.5"); // rocket
        InitWeaponCVars(
            i++, "progs/v_light.mdl", "3", "4", "13", "0.5"); // lightning
        // hipnotic weapons
        InitWeaponCVars(i++, "progs/v_hammer.mdl", "-4", "18", "37",
            "0.33"); // mjolnir hammer
        InitWeaponCVars(
            i++, "progs/v_laserg.mdl", "65", "3.7", "17", "0.33"); // laser
        InitWeaponCVars(i++, "progs/v_prox.mdl", "10", "1.5", "13",
            "0.5"); // proximity - same as grenade
        // rogue weapons
        InitWeaponCVars(i++, "progs/v_lava.mdl", "-5", "3", "15",
            "0.5"); // lava nailgun - same as nailgun
        InitWeaponCVars(i++, "progs/v_lava2.mdl", "0", "3", "19",
            "0.5"); // lava supernailgun - same as supernailgun
        InitWeaponCVars(i++, "progs/v_multi.mdl", "10", "1.5", "13",
            "0.5"); // multigrenade - same as grenade
        InitWeaponCVars(i++, "progs/v_multi2.mdl", "10", "7", "19",
            "0.5"); // multirocket - same as rocket
        InitWeaponCVars(i++, "progs/v_plasma.mdl", "3", "4", "13",
            "0.5"); // plasma - same as lightning
    }

    // TODO VR:
    /*
    auth_mdl
        gun
            vr_wofs_x_02 "-0.5"
            vr_wofs_y_02 "1.5"
            vr_wofs_z_02 "14"
    */

    while(i < MAX_WEAPONS)
    {
        InitWeaponCVars(i++, "-1", "1.5", "1", "10", "0.5");
    }
}

// ----------------------------------------------------------------------------
// Public vars and functions

void VID_VR_Init()
{
    // This is only called once at game start
    Cvar_SetCallback(&vr_enabled, VR_Enabled_f);
    Cvar_SetCallback(&vr_deadzone, VR_Deadzone_f);

    for(cvar_t* c : cvarsToRegister)
    {
        Cvar_RegisterVariable(c);
    }

    InitAllWeaponCVars();

    // Sickness stuff
    Cvar_RegisterVariable(&vr_viewkick);

    VR_Menu_Init();

    // Set the cvar if invoked from a command line parameter
    {
        // int i = COM_CheckParm("-vr");
        // if (i && i < com_argc - 1) {
        Cvar_SetQuick(&vr_enabled, "1");
        //}
    }
}

void VR_InitGame()
{
    InitAllWeaponCVars();
}

bool VR_Enable()
{
    if(vr_initialized) return true;

    vr::EVRInitError eInit = vr::VRInitError_None;
    ovrHMD = vr::VR_Init(&eInit, vr::VRApplication_Scene);

    if(eInit != vr::VRInitError_None)
    {
        Con_Printf("%s\nFailed to Initialize Steam VR",
            VR_GetVRInitErrorAsEnglishDescription(eInit));
        return false;
    }

    if(!InitOpenGLExtensions())
    {
        Con_Printf("Failed to initialize OpenGL extensions");
        return false;
    }

    eyes[0].eye = vr::Eye_Left;
    eyes[1].eye = vr::Eye_Right;

    for(int i = 0; i < 2; i++)
    {
        uint32_t vrwidth, vrheight;
        float LeftTan, RightTan, UpTan, DownTan;

        ovrHMD->GetRecommendedRenderTargetSize(&vrwidth, &vrheight);
        ovrHMD->GetProjectionRaw(
            eyes[i].eye, &LeftTan, &RightTan, &UpTan, &DownTan);

        eyes[i].index = i;
        eyes[i].fbo = CreateFBO(vrwidth, vrheight);
        eyes[i].fov_x = (atan(-LeftTan) + atan(RightTan)) / float(M_PI_DIV_180);
        eyes[i].fov_y = (atan(-UpTan) + atan(DownTan)) / float(M_PI_DIV_180);
    }

    VR_SetTrackingSpace(
        vr::TrackingUniverseStanding); // Put us into standing tracking position
    VR_ResetOrientation();             // Recenter the HMD

    wglSwapIntervalEXT(0); // Disable V-Sync

    Cbuf_AddText(
        "exec vr_autoexec.cfg\n"); // Load the vr autosec config file incase the
                                   // user has settings they want

    vr_initialized = true;
    return true;
}


void VR_PushYaw()
{
    readbackYaw = 1;
}

void VID_VR_Shutdown()
{
    VID_VR_Disable();
}

void VID_VR_Disable()
{
    if(!vr_initialized) return;

    vr::VR_Shutdown();
    ovrHMD = nullptr;

    // Reset the view height
    cl.viewheight = DEFAULT_VIEWHEIGHT;

    // TODO: Cleanup frame buffers

    vr_initialized = false;
}

static void RenderScreenForCurrentEye_OVR()
{
    assert(current_eye != nullptr);

    // Remember the current glwidht/height; we have to modify it here for each
    // eye
    int oldglheight = glheight;
    int oldglwidth = glwidth;

    uint32_t cglwidth = glwidth;
    uint32_t cglheight = glheight;
    ovrHMD->GetRecommendedRenderTargetSize(&cglwidth, &cglheight);
    glwidth = cglwidth;
    glheight = cglheight;

    bool newTextures = glwidth != current_eye->fbo.size.width ||
                       glheight != current_eye->fbo.size.height;
    if(newTextures)
    {
        RecreateTextures(&current_eye->fbo, glwidth, glheight);
    }

    if(newTextures || vr_msaa.value != current_eye->fbo.msaa)
    {
        CreateMSAA(&current_eye->fbo, glwidth, glheight, vr_msaa.value);
    }

    // Set up current FBO
    if(current_eye->fbo.msaa > 0)
    {
        glEnable(GL_MULTISAMPLE);
        glBindFramebufferEXT(
            GL_FRAMEBUFFER_EXT, current_eye->fbo.msaa_framebuffer);
    }
    else
    {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, current_eye->fbo.framebuffer);
    }

    glViewport(0, 0, current_eye->fbo.size.width, current_eye->fbo.size.height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw everything
    srand((int)(cl.time * 1000)); // sync random stuff between eyes

    r_refdef.fov_x = current_eye->fov_x;
    r_refdef.fov_y = current_eye->fov_y;

    SCR_UpdateScreenContent();

    // Generate the eye texture and send it to the HMD

    if(current_eye->fbo.msaa > 0)
    {
        glDisable(GL_MULTISAMPLE);
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, current_eye->fbo.framebuffer);
        glBindFramebufferEXT(
            GL_READ_FRAMEBUFFER, current_eye->fbo.msaa_framebuffer);
        glDrawBuffer(GL_BACK);
        glBlitFramebufferEXT(0, 0, glwidth, glheight, 0, 0, glwidth, glheight,
            GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    vr::Texture_t eyeTexture = {
        reinterpret_cast<void*>(uintptr_t(current_eye->fbo.texture)),
        vr::TextureType_OpenGL, vr::ColorSpace_Gamma};
    vr::VRCompositor()->Submit(current_eye->eye, &eyeTexture);

    // Reset
    glwidth = oldglwidth;
    glheight = oldglheight;

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

void SetHandPos(int index, entity_t* player)
{
    static int foo = 0;


    vec3_t headLocalPreRot;
    _VectorSubtract(controllers[index].position, headOrigin, headLocalPreRot);

    vec3_t headLocal;
    Vec3RotateZ(headLocalPreRot, vrYaw * M_PI_DIV_180, headLocal);
    _VectorAdd(headLocal, headOrigin, headLocal);

    vec3_t final;

    final[0] = -headLocal[0] + player->origin[0];
    final[1] = -headLocal[1] + player->origin[1];
    final[2] = headLocal[2] + player->origin[2] + vr_floor_offset.value +
               vr_gun_z_offset.value;

    // TODO VR:
    // glm::vec3 a{cl.handpos[index][0], cl.handpos[index][1],
    // cl.handpos[index][2]}; glm::vec3 b{final[0], final[1], final[2]}; auto
    // fff = glm::mix(a, b, 0.99f); cl.handpos[index][0] = fff.x;
    // cl.handpos[index][1] = fff.y;
    // cl.handpos[index][2] = fff.z;
    //
    // char msgbuf[100];
    // sprintf(msgbuf, "%f -> %f\n", cl.handpos[index][0], final[0]);
    // OutputDebugStringA(msgbuf);

    // handpos
    auto oldx = cl.handpos[index][0];
    auto oldy = cl.handpos[index][1];
    auto oldz = cl.handpos[index][2];
    VectorCopy(final, cl.handpos[index]);

    // char msgbuf[100];
    // sprintf(msgbuf, "%f -> %f\n", oldz, final[2]);
    // OutputDebugStringA(msgbuf);

    // TODO VR: adjust weight and add cvar, fix movement
    if(false && foo > 500)
    {
        cl.handpos[index][0] = lerp(oldx, final[0], 0.05f);
        cl.handpos[index][1] = lerp(oldy, final[1], 0.05f);
        cl.handpos[index][2] = __lerp(oldz, final[2], 0.05f);
    }
    ++foo;


    // handrot is set with AngleVectorFromRotMat

    // handvel
    VectorCopy(controllers[index].velocity, cl.handvel[index]);

    // handvelmag
    cl.handvelmag[index] = VectorLength(controllers[index].velocity);
}

void IdentifyAxes(int device);

void VR_UpdateScreenContent()
{
    vec3_t orientation;
    GLint w, h;

    // Last chance to enable VR Mode - we get here when the game already start
    // up with vr_enabled 1 If enabling fails, unset the cvar and return.
    if(!vr_initialized && !VR_Enable())
    {
        Cvar_Set("vr_enabled", "0");
        return;
    }

    w = glwidth;
    h = glheight;

    entity_t* player = &cl_entities[cl.viewentity];

    // Update poses
    vr::VRCompositor()->WaitGetPoses(
        ovr_DevicePose, vr::k_unMaxTrackedDeviceCount, nullptr, 0);

    // Get the VR devices' orientation and position
    for(uint32_t iDevice = 0; iDevice < vr::k_unMaxTrackedDeviceCount;
        iDevice++)
    {
        // HMD vectors update
        if(ovr_DevicePose[iDevice].bPoseIsValid &&
            ovrHMD->GetTrackedDeviceClass(iDevice) ==
                vr::TrackedDeviceClass_HMD)
        {
            vr::HmdVector3_t headPos =
                Matrix34ToVector(ovr_DevicePose->mDeviceToAbsoluteTracking);
            headOrigin[0] = headPos.v[2];
            headOrigin[1] = headPos.v[0];
            headOrigin[2] = headPos.v[1];

            vec3_t moveInTracking;
            _VectorSubtract(headOrigin, lastHeadOrigin, moveInTracking);
            moveInTracking[0] *= -meters_to_units;
            moveInTracking[1] *= -meters_to_units;
            moveInTracking[2] = 0;
            Vec3RotateZ(
                moveInTracking, vrYaw * M_PI_DIV_180, vr_room_scale_move);

            _VectorCopy(headOrigin, lastHeadOrigin);
            _VectorSubtract(headOrigin, lastHeadOrigin, headOrigin);
            headPos.v[0] -= lastHeadOrigin[1];
            headPos.v[2] -= lastHeadOrigin[0];

            vr::HmdQuaternion_t headQuat =
                Matrix34ToQuaternion(ovr_DevicePose->mDeviceToAbsoluteTracking);
            vr::HmdVector3_t leyePos =
                Matrix34ToVector(ovrHMD->GetEyeToHeadTransform(eyes[0].eye));
            vr::HmdVector3_t reyePos =
                Matrix34ToVector(ovrHMD->GetEyeToHeadTransform(eyes[1].eye));

            leyePos = RotateVectorByQuaternion(leyePos, headQuat);
            reyePos = RotateVectorByQuaternion(reyePos, headQuat);

            HmdVec3RotateY(&headPos, -vrYaw * M_PI_DIV_180);

            HmdVec3RotateY(&leyePos, -vrYaw * M_PI_DIV_180);
            HmdVec3RotateY(&reyePos, -vrYaw * M_PI_DIV_180);

            eyes[0].position = AddVectors(headPos, leyePos);
            eyes[1].position = AddVectors(headPos, reyePos);
            eyes[0].orientation = headQuat;
            eyes[1].orientation = headQuat;
        }
        // Controller vectors update
        else if(ovr_DevicePose[iDevice].bPoseIsValid &&
                ovrHMD->GetTrackedDeviceClass(iDevice) ==
                    vr::TrackedDeviceClass_Controller)
        {
            vr::HmdVector3_t rawControllerPos = Matrix34ToVector(
                ovr_DevicePose[iDevice].mDeviceToAbsoluteTracking);
            vr::HmdQuaternion_t rawControllerQuat = Matrix34ToQuaternion(
                ovr_DevicePose[iDevice].mDeviceToAbsoluteTracking);
            vr::HmdVector3_t rawControllerVel =
                ovr_DevicePose[iDevice].vVelocity;

            int controllerIndex = -1;

            if(ovrHMD->GetControllerRoleForTrackedDeviceIndex(iDevice) ==
                vr::TrackedControllerRole_LeftHand)
            {
                // Swap controller values for our southpaw players
                controllerIndex = vr_lefthanded.value ? 1 : 0;
            }
            else if(ovrHMD->GetControllerRoleForTrackedDeviceIndex(iDevice) ==
                    vr::TrackedControllerRole_RightHand)
            {
                // Swap controller values for our southpaw players
                controllerIndex = vr_lefthanded.value ? 0 : 1;
            }

            if(controllerIndex != -1)
            {
                vr_controller* controller = &controllers[controllerIndex];

                IdentifyAxes(iDevice);

                controller->lastState = controller->state;
                vr::VRSystem()->GetControllerState(
                    iDevice, &controller->state, sizeof(controller->state));
                controller->rawvector = rawControllerPos;
                controller->raworientation = rawControllerQuat;
                controller->position[0] =
                    (rawControllerPos.v[2] - lastHeadOrigin[0]) *
                    meters_to_units;
                controller->position[1] =
                    (rawControllerPos.v[0] - lastHeadOrigin[1]) *
                    meters_to_units;
                controller->position[2] =
                    (rawControllerPos.v[1]) * meters_to_units;

                controller->velocity[0] = rawControllerVel.v[0];
                controller->velocity[1] = rawControllerVel.v[1];
                controller->velocity[2] = rawControllerVel.v[2];

                QuatToYawPitchRoll(rawControllerQuat, controller->orientation);
            }
        }
    }

    // Reset the aim roll value before calculation, incase the user switches
    // aimmode from 7 to another.
    cl.aimangles[ROLL] = 0.0;

    QuatToYawPitchRoll(eyes[1].orientation, orientation);
    if(readbackYaw)
    {
        vrYaw = cl.viewangles[YAW] - (orientation[YAW] - vrYaw);
        readbackYaw = 0;
    }

    switch((int)vr_aimmode.value)
    {
            // 1: (Default) Head Aiming; View YAW is mouse+head, PITCH is head
        default:
        case VR_AIMMODE_HEAD_MYAW:
            cl.viewangles[PITCH] = cl.aimangles[PITCH] = orientation[PITCH];
            cl.aimangles[YAW] = cl.viewangles[YAW] =
                cl.aimangles[YAW] + orientation[YAW] - lastOrientation[YAW];
            break;

            // 2: Head Aiming; View YAW and PITCH is mouse+head (this is stupid)
        case VR_AIMMODE_HEAD_MYAW_MPITCH:
            cl.viewangles[PITCH] = cl.aimangles[PITCH] = cl.aimangles[PITCH] +
                                                         orientation[PITCH] -
                                                         lastOrientation[PITCH];
            cl.aimangles[YAW] = cl.viewangles[YAW] =
                cl.aimangles[YAW] + orientation[YAW] - lastOrientation[YAW];
            break;

            // 3: Mouse Aiming; View YAW is mouse+head, PITCH is head
        case VR_AIMMODE_MOUSE_MYAW:
            cl.viewangles[PITCH] = orientation[PITCH];
            cl.viewangles[YAW] = cl.aimangles[YAW] + orientation[YAW];
            break;

            // 4: Mouse Aiming; View YAW and PITCH is mouse+head
        case VR_AIMMODE_MOUSE_MYAW_MPITCH:
            cl.viewangles[PITCH] = cl.aimangles[PITCH] + orientation[PITCH];
            cl.viewangles[YAW] = cl.aimangles[YAW] + orientation[YAW];
            break;

        case VR_AIMMODE_BLENDED:
        case VR_AIMMODE_BLENDED_NOPITCH:
        {
            float diffHMDYaw = orientation[YAW] - lastOrientation[YAW];
            float diffHMDPitch = orientation[PITCH] - lastOrientation[PITCH];
            float diffAimYaw = cl.aimangles[YAW] - lastAim[YAW];
            float diffYaw;

            // find new view position based on orientation delta
            cl.viewangles[YAW] += diffHMDYaw;

            // find difference between view and aim yaw
            diffYaw = cl.viewangles[YAW] - cl.aimangles[YAW];

            if(fabs(diffYaw) > vr_deadzone.value / 2.0f)
            {
                // apply the difference from each set of angles to the other
                cl.aimangles[YAW] += diffHMDYaw;
                cl.viewangles[YAW] += diffAimYaw;
            }
            if((int)vr_aimmode.value == VR_AIMMODE_BLENDED)
            {
                cl.aimangles[PITCH] += diffHMDPitch;
            }
            cl.viewangles[PITCH] = orientation[PITCH];
        }
        break;

        // 7: Controller Aiming;
        case VR_AIMMODE_CONTROLLER:
            cl.viewangles[PITCH] = orientation[PITCH];
            cl.viewangles[YAW] = orientation[YAW];

            // TODO VR: this affects aim, not just drawing
            /*
            vec3_t rotOfs = {
                vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 5].value +
            vr_gunangle.value, vr_weapon_offset[weaponCVarEntry *
            VARS_PER_WEAPON + 6].value + vr_gunyaw.value,
                vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 7].value
            };
            */

            vec3_t rotOfs = {vr_gunangle.value, vr_gunyaw.value, 0};

            vec3_t mat[3], matTmp[3], gunMatPitch[3], gunMatYaw[3],
                gunMatRoll[3];
            CreateRotMat(0, rotOfs[0], gunMatPitch); // pitch
            CreateRotMat(1, rotOfs[1], gunMatYaw);   // yaw
            CreateRotMat(2, rotOfs[2], gunMatRoll);  // roll

            for(int i = 0; i < 2; i++)
            {
                RotMatFromAngleVector(controllers[i].orientation, mat);

                R_ConcatRotations(gunMatRoll, mat, matTmp);
                for(int j = 0; j < 3; ++j)
                {
                    VectorCopy(matTmp[j], mat[j]);
                }

                R_ConcatRotations(gunMatPitch, mat, matTmp);
                for(int j = 0; j < 3; ++j)
                {
                    VectorCopy(matTmp[j], mat[j]);
                }

                R_ConcatRotations(gunMatYaw, mat, matTmp);
                for(int j = 0; j < 3; ++j)
                {
                    VectorCopy(matTmp[j], mat[j]);
                }

                vec3_t handrottemp;
                AngleVectorFromRotMat(mat, handrottemp);

                // TODO VR:
                /*
                auto rads = glm::radians(glm::vec3(handrottemp[0],
                handrottemp[1], handrottemp[2])); auto quat = glm::fquat(rads);

                // glm::fquat qa =
                glm::fquat(glm::radians(glm::vec3(cl.handrot[i][0],
                cl.handrot[i][1], cl.handrot[i][2])));
                // glm::fquat qb =
                glm::fquat(glm::radians(glm::vec3(handrottemp[0],
                handrottemp[1], handrottemp[2])));
                // auto qc = glm::mix(qa, qb, 0.02f);
                float t0, t1, t2;
                glm::extractEulerAngleXYZ(glm::toMat4(quat), t2, t1, t0);

                auto qf = glm::degrees(glm::vec3(t0,t1,t2));

                char msgbuf[100];
                sprintf(msgbuf, "%f , %f , %f\n", rads[0], rads[1], rads[2]);
                OutputDebugStringA(msgbuf);
                */

                // glm::fquat a, b;
                //
                // {
                // 	vec3_t forward, right, up;
                // 	AngleVectors(handrottemp, forward, right, up);
                // 	a = glm::conjugate(glm::quatLookAt(toVec3(forward),
                // toVec3(up))); 	a = glm::normalize(a);
                // }
                //
                // {
                // 	vec3_t forward, right, up;
                // 	AngleVectors(cl.handrot[i], forward, right, up);
                // 	b = glm::quatLookAt(toVec3(forward), toVec3(up));
                // 	b = glm::normalize(b);
                // }


                // glm::fquat a(glm::vec3(cl.handrot[i][PITCH],
                // cl.handrot[i][YAW], cl.handrot[i][ROLL])); glm::fquat
                // b(glm::vec3(handrottemp[PITCH], handrottemp[YAW],
                // handrottemp[ROLL])); auto c = glm::mix(b, a, 0.1f); auto ro =
                // controllers[i].raworientation; auto ca =
                // glm::degrees(glm::eulerAngles(glm::conjugate(glm::quat(ro.w,ro.x,ro.y,ro.z))));
                // m = glm::quatLookAt(toVec3(forward), toVec3(up));
                // m = glm::rotate(m, vr_sbar_offset_pitch.value, glm::vec3(1,
                // 0, 0)); m = glm::rotate(m, vr_sbar_offset_yaw.value,
                // glm::vec3(0, 1, 0)); m = glm::rotate(m,
                // vr_sbar_offset_roll.value, glm::vec3(0, 0, 1)); m =
                // glm::normalize(m);

                //
                // cl.handrot[i][0] = ca.x;
                // cl.handrot[i][1] = ca.y;
                // cl.handrot[i][2] = ca.z;

                VectorCopy(handrottemp, cl.handrot[i]);

                // vec3lerp(cl.handrot[i], cl.handrot[i], handrottemp, 0.02f);
            }

            if(cl.viewent.model)
            {
                aliashdr_t* hdr = (aliashdr_t*)Mod_Extradata(cl.viewent.model);
                Mod_Weapon(cl.viewent.model->name, hdr);
            }

            SetHandPos(0, player);
            SetHandPos(1, player);

            // TODO VR: interpolate based on weapon weight?
            VectorCopy(cl.handrot[1], cl.aimangles); // Sets the shooting angle
            // TODO: what sets the shooting origin?

            break;
    }
    cl.viewangles[ROLL] = orientation[ROLL];

    VectorCopy(orientation, lastOrientation);
    VectorCopy(cl.aimangles, lastAim);

    VectorCopy(cl.viewangles, r_refdef.viewangles);
    VectorCopy(cl.aimangles, r_refdef.aimangles);

    // Render the scene for each eye into their FBOs
    for(int i = 0; i < 2; i++)
    {
        current_eye = &eyes[i];

        vec3_t temp, orientation;

        // We need to scale the view offset position to quake units and rotate
        // it by the current input angles (viewangle - eye orientation)
        QuatToYawPitchRoll(current_eye->orientation, orientation);
        temp[0] = -current_eye->position.v[2] * meters_to_units; // X
        temp[1] = -current_eye->position.v[0] * meters_to_units; // Y
        temp[2] = current_eye->position.v[1] * meters_to_units;  // Z
        Vec3RotateZ(temp,
            (r_refdef.viewangles[YAW] - orientation[YAW]) * M_PI_DIV_180,
            vr_viewOffset);
        vr_viewOffset[2] += vr_floor_offset.value;

        RenderScreenForCurrentEye_OVR();
    }

    // Blit mirror texture to backbuffer
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, eyes[0].fbo.framebuffer);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
    glBlitFramebufferEXT(0, eyes[0].fbo.size.width, eyes[0].fbo.size.height, 0,
        0, h, w, 0, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
}

void VR_SetMatrices()
{
    vr::HmdMatrix44_t projection;

    // Calculate HMD projection matrix and view offset position
    projection = TransposeMatrix(
        ovrHMD->GetProjectionMatrix(current_eye->eye, 4.f, gl_farclip.value));

    // Set OpenGL projection and view matrices
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((GLfloat*)projection.m);
}


void VR_AddOrientationToViewAngles(vec3_t angles)
{
    vec3_t orientation;
    QuatToYawPitchRoll(current_eye->orientation, orientation);

    angles[PITCH] = angles[PITCH] + orientation[PITCH];
    angles[YAW] = angles[YAW] + orientation[YAW];
    angles[ROLL] = orientation[ROLL];
}

void VR_ShowCrosshair()
{
    vec3_t forward, up, right;
    vec3_t start, end, impact;
    float size, alpha;

    if(!sv_player) return;

    if((int)(sv_player->v.weapon) == IT_AXE) return;

    size = CLAMP(0.0, vr_crosshair_size.value, 32.0);
    alpha = CLAMP(0.0, vr_crosshair_alpha.value, 1.0);

    if(size <= 0 || alpha <= 0) return;

    // setup gl
    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    GL_PolygonOffset(OFFSET_SHOWTRIS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    // calc the line and draw
    // TODO: Make the laser align correctly
    if(vr_aimmode.value == VR_AIMMODE_CONTROLLER)
    {
        VectorCopy(cl.handpos[1], start);

        // TODO VR:
        vec3_t ofs = {vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON].value,
            vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 1].value,
            vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 2].value +
                vr_gunmodely.value};

        AngleVectors(cl.handrot[1], forward, right, up);
        vec3_t fwd2;
        VectorCopy(forward, fwd2);
        fwd2[0] *= vr_gunmodelscale.value * ofs[2];
        fwd2[1] *= vr_gunmodelscale.value * ofs[2];
        fwd2[2] *= vr_gunmodelscale.value * ofs[2];
        VectorAdd(start, fwd2, start);
    }
    else
    {
        VectorCopy(cl.viewent.origin, start);
        start[2] -= cl.viewheight - 10;
        AngleVectors(cl.aimangles, forward, right, up);
    }


    switch((int)vr_crosshair.value)
    {
        default:
        case VR_CROSSHAIR_POINT:
            if(vr_crosshair_depth.value <= 0)
            {
                // trace to first wall
                VectorMA(start, 4096, forward, end);

                end[2] += vr_crosshairy.value;
                TraceLine(start, end, impact);
            }
            else
            {
                // fix crosshair to specific depth
                VectorMA(start, vr_crosshair_depth.value * meters_to_units,
                    forward, impact);
            }

            glEnable(GL_POINT_SMOOTH);
            glColor4f(1, 0, 0, alpha);
            glPointSize(size * glwidth / vid.width);

            glBegin(GL_POINTS);
            glVertex3f(impact[0], impact[1], impact[2]);
            glEnd();
            glDisable(GL_POINT_SMOOTH);
            break;

        case VR_CROSSHAIR_LINE:
            // trace to first entity
            VectorMA(start, 4096, forward, end);
            TraceLineToEntity(start, end, impact, sv_player);

            glColor4f(1, 0, 0, alpha);
            glLineWidth(size * glwidth / vid.width);
            glBegin(GL_LINES);
            impact[2] += vr_crosshairy.value * 10.f;
            glVertex3f(start[0], start[1], start[2]);
            glVertex3f(impact[0], impact[1], impact[2]);
            glEnd();
            break;
    }

    // cleanup gl
    glColor3f(1, 1, 1);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    GL_PolygonOffset(OFFSET_NONE);
    glEnable(GL_DEPTH_TEST);
}



void VR_Draw2D()
{
    qboolean draw_sbar = false;
    vec3_t menu_angles, forward, right, up, target;
    float scale_hud = vr_menu_scale.value;

    int oldglwidth = glwidth, oldglheight = glheight,
        oldconwidth = vid.conwidth, oldconheight = vid.conheight;

    glwidth = 320;
    glheight = 200;

    vid.conwidth = 320;
    vid.conheight = 200;

    // draw 2d elements 1m from the users face, centered
    glPushMatrix();
    glDisable(GL_DEPTH_TEST); // prevents drawing sprites on sprites from
                              // interferring with one another
    glEnable(GL_BLEND);

    if(vr_aimmode.value == VR_AIMMODE_CONTROLLER)
    {
        AngleVectors(cl.handrot[1], forward, right, up);

        VectorCopy(cl.handrot[1], menu_angles);

        AngleVectors(menu_angles, forward, right, up);

        VectorMA(cl.handpos[1], 48, forward, target);
    }
    else
    {
        // TODO: Make the menus' position sperate from the right hand. Centered
        // on last view dir?
        VectorCopy(r_refdef.aimangles, menu_angles)

            if(vr_aimmode.value == VR_AIMMODE_HEAD_MYAW ||
                vr_aimmode.value == VR_AIMMODE_HEAD_MYAW_MPITCH)
                menu_angles[PITCH] = 0;

        AngleVectors(menu_angles, forward, right, up);

        VectorMA(r_refdef.vieworg, 48, forward, target);
    }

    const auto smoothedTarget =
        glm::mix(lastMenuPosition, toVec3(target), 0.03);
    lastMenuPosition = smoothedTarget;

    glTranslatef(smoothedTarget[0], smoothedTarget[1], smoothedTarget[2]);

    glRotatef(menu_angles[YAW] - 90, 0, 0, 1); // rotate around z
    glRotatef(90 + menu_angles[PITCH], -1, 0,
        0); // keep bar at constant angled pitch towards user
    glTranslatef(-(320.0 * scale_hud / 2), -(200.0 * scale_hud / 2),
        0); // center the status bar
    glScalef(scale_hud, scale_hud, scale_hud);


    if(scr_drawdialog) // new game confirm
    {
        if(con_forcedup)
            Draw_ConsoleBackground();
        else
            draw_sbar = true; // Sbar_Draw ();
        Draw_FadeScreen();
        SCR_DrawNotifyString();
    }
    else if(scr_drawloading) // loading
    {
        SCR_DrawLoading();
        draw_sbar = true; // Sbar_Draw ();
    }
    else if(cl.intermission == 1 && key_dest == key_game) // end of level
    {
        Sbar_IntermissionOverlay();
    }
    else if(cl.intermission == 2 && key_dest == key_game) // end of episode
    {
        Sbar_FinaleOverlay();
        SCR_CheckDrawCenterString();
    }
    else
    {
        // SCR_DrawCrosshair (); //johnfitz
        SCR_DrawRam();
        SCR_DrawNet();
        SCR_DrawTurtle();
        SCR_DrawPause();
        SCR_CheckDrawCenterString();
        draw_sbar = true;   // Sbar_Draw ();
        SCR_DrawDevStats(); // johnfitz
        SCR_DrawFPS();      // johnfitz
        SCR_DrawClock();    // johnfitz
        SCR_DrawConsole();
        M_Draw();
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();

    if(draw_sbar) VR_DrawSbar();

    glwidth = oldglwidth;
    glheight = oldglheight;
    vid.conwidth = oldconwidth;
    vid.conheight = oldconheight;
}


void VR_DrawSbar()
{
    vec3_t sbar_angles, forward, right, up, target;
    float scale_hud = vr_hud_scale.value;

    glPushMatrix();
    glDisable(GL_DEPTH_TEST); // prevents drawing sprites on sprites from
                              // interferring with one another

    if(vr_aimmode.value == VR_AIMMODE_CONTROLLER)
    {
        const auto mode = static_cast<int>(vr_sbar_mode.value);

        if(mode == static_cast<int>(VrSbarMode::MainHand))
        {
            AngleVectors(cl.handrot[1], forward, right, up);
            VectorCopy(cl.handrot[1], sbar_angles);

            AngleVectors(sbar_angles, forward, right, up);
            VectorMA(cl.handpos[1], -5, right, target);
        }
        else
        {
            AngleVectors(cl.handrot[0], forward, right, up);
            VectorCopy(cl.handrot[0], sbar_angles);

            AngleVectors(sbar_angles, forward, right, up);
            VectorMA(cl.handpos[0], 0.f, right, target);
        }
    }
    else
    {
        VectorCopy(cl.aimangles, sbar_angles);

        if(vr_aimmode.value == VR_AIMMODE_HEAD_MYAW ||
            vr_aimmode.value == VR_AIMMODE_HEAD_MYAW_MPITCH)
            sbar_angles[PITCH] = 0;

        AngleVectors(sbar_angles, forward, right, up);

        VectorMA(cl.viewent.origin, 1.0, forward, target);
    }

    // TODO VR: 1.0? Attach to off hand?
    const auto smoothedTarget = glm::mix(lastHudPosition, toVec3(target), 1.0);
    lastHudPosition = smoothedTarget;

    glTranslatef(smoothedTarget[0], smoothedTarget[1], smoothedTarget[2]);

    if(vr_aimmode.value == VR_AIMMODE_CONTROLLER &&
        static_cast<int>(vr_sbar_mode.value) ==
            static_cast<int>(VrSbarMode::OffHand))
    {
        glm::fquat m;
        m = glm::quatLookAt(toVec3(forward), toVec3(up));
        m = glm::rotate(m, vr_sbar_offset_pitch.value, glm::vec3(1, 0, 0));
        m = glm::rotate(m, vr_sbar_offset_yaw.value, glm::vec3(0, 1, 0));
        m = glm::rotate(m, vr_sbar_offset_roll.value, glm::vec3(0, 0, 1));
        m = glm::normalize(m);

        glMultMatrixf(&glm::mat4_cast(m)[0][0]);

        const auto ox = vr_sbar_offset_x.value;
        const auto oy = vr_sbar_offset_y.value;
        const auto oz = vr_sbar_offset_z.value;

        glTranslatef(ox, oy, oz);
    }
    else
    {
        glRotatef(sbar_angles[YAW] - 90, 0, 0, 1); // rotate around z
        glRotatef(90 + 45 + sbar_angles[PITCH], -1, 0,
            0); // keep bar at constant angled pitch towards user

        glTranslatef(-(320.0 * scale_hud / 2), 0, 0); // center the status bar
        glTranslatef(0, 0, 10);                       // move hud down a bit
    }

    glScalef(scale_hud, scale_hud, scale_hud);

    Sbar_Draw();

    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
}

void VR_SetAngles(vec3_t angles)
{
    VectorCopy(angles, cl.aimangles);
    VectorCopy(angles, cl.viewangles);
    VectorCopy(angles, lastAim);
}

void VR_ResetOrientation()
{
    cl.aimangles[YAW] = cl.viewangles[YAW];
    cl.aimangles[PITCH] = cl.viewangles[PITCH];
    if(vr_enabled.value)
    {
        // IVRSystem_ResetSeatedZeroPose(ovrHMD);
        VectorCopy(cl.aimangles, lastAim);
    }
}

void VR_SetTrackingSpace(int n)
{
    if(n >= 0 || n < 3)
        vr::VRCompositor()->SetTrackingSpace((vr::ETrackingUniverseOrigin)n);
}

int axisTrackpad = -1;
int axisJoystick = -1;
int axisTrigger = -1;
bool identified = false;

void IdentifyAxes(int device)
{
    if(identified) return;

    for(uint32_t i = 0; i < vr::k_unControllerStateAxisCount; i++)
    {
        switch(vr::VRSystem()->GetInt32TrackedDeviceProperty(device,
            (vr::ETrackedDeviceProperty)(vr::Prop_Axis0Type_Int32 + i),
            nullptr))
        {
            case vr::k_eControllerAxis_TrackPad:
                if(axisTrackpad == -1) axisTrackpad = i;
                break;
            case vr::k_eControllerAxis_Joystick:
                if(axisJoystick == -1) axisJoystick = i;
                break;
            case vr::k_eControllerAxis_Trigger:
                if(axisTrigger == -1) axisTrigger = i;
                break;
        }
    }

    identified = true;
}

float GetAxis(vr::VRControllerState_t* state, int axis, double deadzoneExtra)
{
    float v = 0;

    if(axis == 0)
    {
        if(axisTrackpad != -1) v += state->rAxis[axisTrackpad].x;
        if(axisJoystick != -1) v += state->rAxis[axisJoystick].x;
    }
    else
    {
        if(axisTrackpad != -1) v += state->rAxis[axisTrackpad].y;
        if(axisJoystick != -1) v += state->rAxis[axisJoystick].y;
    }

    int sign = (v > 0) - (v < 0);
    v = fabsf(v);

    if(v < vr_joystick_axis_deadzone.value + deadzoneExtra)
    {
        return 0.0f;
    }

    if(vr_joystick_deadzone_trunc.value == 0)
    {
        v = (v - vr_joystick_axis_deadzone.value) /
            (1 - vr_joystick_axis_deadzone.value);
    }

    if(vr_joystick_axis_exponent.value >= 0)
    {
        v = powf(v, vr_joystick_axis_exponent.value);
    }

    return sign * v;
}

void DoKey(vr_controller* controller, vr::EVRButtonId vrButton, int quakeKey)
{
    bool wasDown = (controller->lastState.ulButtonPressed &
                       vr::ButtonMaskFromId(vrButton)) != 0;
    bool isDown = (controller->state.ulButtonPressed &
                      vr::ButtonMaskFromId(vrButton)) != 0;
    if(isDown != wasDown)
    {
        Key_Event(quakeKey, isDown);
    }
}

void DoTrigger(vr_controller* controller, int quakeKey)
{
    if(axisTrigger != -1)
    {
        bool triggerWasDown = controller->lastState.rAxis[axisTrigger].x > 0.5f;
        bool triggerDown = controller->state.rAxis[axisTrigger].x > 0.5f;
        if(triggerDown != triggerWasDown)
        {
            Key_Event(quakeKey, triggerDown);
        }
    }
}

void DoAxis(vr_controller* controller, int axis, int quakeKeyNeg,
    int quakeKeyPos, double deadzoneExtra)
{
    float lastVal = GetAxis(&controller->lastState, axis, deadzoneExtra);
    float val = GetAxis(&controller->state, axis, deadzoneExtra);

    bool posWasDown = lastVal > 0.0f;
    bool posDown = val > 0.0f;
    if(posDown != posWasDown)
    {
        Key_Event(quakeKeyPos, posDown);
    }

    bool negWasDown = lastVal < 0.0f;
    bool negDown = val < 0.0f;
    if(negDown != negWasDown)
    {
        Key_Event(quakeKeyNeg, negDown);
    }
}

void VR_Move(usercmd_t* cmd)
{
    if(!vr_enabled.value) return;

    // TODO VR:
    vec3_t adj;
    _VectorCopy(cl.handpos[1], adj);

    vec3_t ofs = {vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON].value,
        vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 1].value,
        vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 2].value +
            vr_gunmodely.value};

    vec3_t fwd2, right, up;
    AngleVectors(cl.handrot[1], fwd2, right, up);
    fwd2[0] *= vr_gunmodelscale.value * ofs[2];
    fwd2[1] *= vr_gunmodelscale.value * ofs[2];
    fwd2[2] *= vr_gunmodelscale.value * ofs[2];
    VectorAdd(adj, fwd2, adj);

    // TODO VR:
    vec3_t adjhandpos;
    VectorCopy(cl.handpos[1], adjhandpos);

    // TODO VR: not needed anymore, changing QC
    // adjhandpos[2] -= vr_projectilespawn_z_offset.value;

    // handpos
    VectorCopy(adjhandpos, cmd->handpos);

    // handvel
    VectorCopy(cl.handvel[1], cmd->handvel);

    // handvelmag
    cmd->handvelmag = cl.handvelmag[1];

    DoTrigger(&controllers[0], K_SPACE);

    // TODO VR:
    // DoKey(&controllers[0], vr::k_EButton_Grip, K_MWHEELUP);
    // DoKey(&controllers[1], vr::k_EButton_Grip, K_MWHEELDOWN);

    DoKey(&controllers[0], vr::k_EButton_SteamVR_Touchpad, K_SHIFT);
    DoKey(&controllers[1], vr::k_EButton_SteamVR_Touchpad, K_ALT);

    DoKey(&controllers[0], vr::k_EButton_ApplicationMenu, '1');
    DoKey(&controllers[0], vr::k_EButton_A, '2');
    DoKey(&controllers[1], vr::k_EButton_A, '3');

    DoKey(&controllers[1], vr::k_EButton_ApplicationMenu, K_ESCAPE);
    if(key_dest == key_menu)
    {
        for(int i = 0; i < 2; i++)
        {
            DoAxis(&controllers[i], 0, K_LEFTARROW, K_RIGHTARROW,
                vr_joystick_axis_menu_deadzone_extra.value);
            DoAxis(&controllers[i], 1, K_DOWNARROW, K_UPARROW,
                vr_joystick_axis_menu_deadzone_extra.value);
            DoTrigger(&controllers[i], K_ENTER);
        }
    }
    else
    {
        DoTrigger(&controllers[1], K_MOUSE1);

        vec3_t lfwd, lright, lup;
        AngleVectors(cl.handrot[0], lfwd, lright, lup);

        if(vr_movement_mode.value == VR_MOVEMENT_MODE_RAW_INPUT)
        {
            cmd->forwardmove +=
                cl_forwardspeed.value * GetAxis(&controllers[0].state, 1, 0.0);
            cmd->sidemove +=
                cl_forwardspeed.value * GetAxis(&controllers[0].state, 0, 0.0);
        }
        else
        {
            vec3_t vfwd, vright, vup;
            vec3_t playerYawOnly = {0, sv_player->v.v_angle[YAW], 0};

            AngleVectors(playerYawOnly, vfwd, vright, vup);

            // avoid gimbal by using up if we are point up/down
            if(fabsf(lfwd[2]) > 0.8f)
            {
                if(lfwd[2] < -0.8f)
                {
                    lfwd[0] *= -1;
                    lfwd[1] *= -1;
                    lfwd[2] *= -1;
                }
                else
                {
                    lup[0] *= -1;
                    lup[1] *= -1;
                    lup[2] *= -1;
                }

                VectorSwap(lup, lfwd);
            }

            // Scale up directions so tilting doesn't affect speed
            float fac = 1.0f / lup[2];
            for(int i = 0; i < 3; i++)
            {
                lfwd[i] *= fac;
                lright[i] *= fac;
            }

            vec3_t move = {0, 0, 0};
            VectorMA(move, GetAxis(&controllers[0].state, 1, 0.0), lfwd, move);
            VectorMA(
                move, GetAxis(&controllers[0].state, 0, 0.0), lright, move);

            float fwd = DotProduct(move, vfwd);
            float right = DotProduct(move, vright);

            // Quake run doesn't affect the value of cl_sidespeed.value, so just
            // use forward speed here for consistency
            cmd->forwardmove += cl_forwardspeed.value * fwd;
            cmd->sidemove += cl_forwardspeed.value * right;
        }

        AngleVectors(cl.handrot[0], lfwd, lright, lup);
        cmd->upmove +=
            cl_upspeed.value * GetAxis(&controllers[0].state, 1, 0.0) * lfwd[2];

        if(cl_forwardspeed.value > 200 && cl_movespeedkey.value)
            cmd->forwardmove /= cl_movespeedkey.value;
        if((cl_forwardspeed.value > 200) ^ (in_speed.state & 1))
        {
            cmd->forwardmove *= cl_movespeedkey.value;
            cmd->sidemove *= cl_movespeedkey.value;
            cmd->upmove *= cl_movespeedkey.value;
        }

        float yawMove = GetAxis(&controllers[1].state, 0, 0.0);

        if(vr_snap_turn.value != 0)
        {
            static int lastSnap = 0;
            int snap = yawMove > 0.0f ? 1 : yawMove < 0.0f ? -1 : 0;
            if(snap != lastSnap)
            {
                vrYaw -= snap * vr_snap_turn.value;
                lastSnap = snap;
            }
        }
        else
        {
            vrYaw -= (yawMove * host_frametime * 100.0f *
                         vr_joystick_yaw_multi.value) *
                     vr_turn_speed.value;
        }
    }
}
