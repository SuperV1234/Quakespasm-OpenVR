cmake_minimum_required(VERSION 3.8)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(quakespasm-sdl2 VERSION 0.0.1 LANGUAGES CXX)

set(source_list "../Quake/bgmusic.c"
                "../Quake/cd_sdl.c"
                "../Quake/cfgfile.c"
                "../Quake/chase.c"
                "../Quake/cl_demo.c"
                "../Quake/cl_input.c"
                "../Quake/cl_main.c"
                "../Quake/cl_parse.c"
                "../Quake/cl_tent.c"
                "../Quake/cmd.c"
                "../Quake/common.c"
                "../Quake/console.c"
                "../Quake/crc.c"
                "../Quake/cvar.c"
                "../Quake/gl_draw.c"
                "../Quake/gl_fog.c"
                "../Quake/gl_mesh.c"
                "../Quake/gl_model.c"
                "../Quake/gl_refrag.c"
                "../Quake/gl_rlight.c"
                "../Quake/gl_rmain.c"
                "../Quake/gl_rmisc.c"
                "../Quake/gl_screen.c"
                "../Quake/gl_sky.c"
                "../Quake/gl_texmgr.c"
                "../Quake/gl_vidsdl.c"
                "../Quake/gl_warp.c"
                "../Quake/host.c"
                "../Quake/host_cmd.c"
                "../Quake/image.c"
                "../Quake/in_sdl.c"
                "../Quake/keys.c"
                "../Quake/main_sdl.c"
                "../Quake/mathlib.c"
                "../Quake/map_menu.c"
                "../Quake/menu.c"
                "../Quake/net_dgrm.c"
                "../Quake/net_loop.c"
                "../Quake/net_main.c"
                "../Quake/net_udp.c"
                "../Quake/net_win.c"
                "../Quake/net_wins.c"
                "../Quake/net_wipx.c"
                "../Quake/pl_win.c"
                "../Quake/pr_cmds.c"
                "../Quake/pr_edict.c"
                "../Quake/pr_exec.c"
                "../Quake/r_alias.c"
                "../Quake/r_brush.c"
                "../Quake/r_part.c"
                "../Quake/r_sprite.c"
                "../Quake/r_world.c"
                "../Quake/sbar.c"
                "../Quake/sbaroffset_menu.c"
                "../Quake/snd_codec.c"
                "../Quake/snd_dma.c"
                "../Quake/snd_flac.c"
                "../Quake/snd_mem.c"
                "../Quake/snd_mikmod.c"
                "../Quake/snd_mix.c"
                "../Quake/snd_modplug.c"
                "../Quake/snd_mp3.c"
                "../Quake/snd_mp3tag.c"
                "../Quake/snd_mpg123.c"
                "../Quake/snd_opus.c"
                "../Quake/snd_sdl.c"
                "../Quake/snd_umx.c"
                "../Quake/snd_vorbis.c"
                "../Quake/snd_wave.c"
                "../Quake/snd_xmp.c"
                "../Quake/strlcat.c"
                "../Quake/strlcpy.c"
                "../Quake/sv_main.c"
                "../Quake/sv_move.c"
                "../Quake/sv_phys.c"
                "../Quake/sv_user.c"
                "../Quake/sys_sdl_win.c"
                "../Quake/view.c"
                "../Quake/vr.c"
                "../Quake/vr_menu.c"
                "../Quake/wad.c"
                "../Quake/world.c"
                "../Quake/wpnoffset_menu.c"
                "../Quake/zone.c"
)

set_source_files_properties(${source_list} PROPERTIES LANGUAGE CXX )

add_executable(quakespasm-sdl2 "${CMAKE_CURRENT_SOURCE_DIR}/../Quake/vr.c" "${source_list}")
set_target_properties(quakespasm-sdl2 PROPERTIES LINKER_LANGUAGE CXX)

target_compile_features(quakespasm-sdl2 PUBLIC cxx_std_17)
target_compile_options(quakespasm-sdl2 PRIVATE -x c++ -Wall -Wextra -Wno-missing-field-initializers -fdiagnostics-color=always -Wpedantic)
target_compile_definitions(quakespasm-sdl2 PRIVATE USE_SDL2=1 _AMD64_=1)

target_include_directories(
    quakespasm-sdl2 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/SDL2/include/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../Quake/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../glm/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

include(FindOpenGL)
set(SDL2_LIBRARIES
    "C:/pers/Quakespasm-OpenVR/Windows/SDL2/lib64/SDL2main.lib"
    "C:/pers/Quakespasm-OpenVR/Windows/SDL2/lib64/SDL2.lib"
    "C:/pers/Quakespasm-OpenVR/Windows/OpenVR/lib/win64/openvr_api.lib")

target_link_libraries(quakespasm-sdl2 ${OPENGL_gl_LIBRARY} ${SDL2_LIBRARIES} wsock32 winmm ws2_32)
