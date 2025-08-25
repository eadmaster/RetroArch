/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025 - eadmaster
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <locale.h>
#include <lists/dir_list.h>
#include <file/file_path.h>
#include <streams/stdin_stream.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <retro_timers.h>
#include <lrc_hash.h>
#include <lrc_hash.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "lua_manager.h"
#include "runloop.h"
#include "verbosity.h"
#include "paths.h"
#include "core_info.h"
#include "content.h"
#include "version.h"
#include "command.h"
#include "input/input_driver.h"
#include "input/input_keymaps.h"
#include "gfx/video_driver.h"
#include "gfx/video_display_server.h"
#include "gfx/gfx_widgets.h"
#include "tasks/tasks_internal.h"


// LUA API based on Bizhawk https://tasvideos.org/Bizhawk/LuaFunctions

// console.log()
// Outputs to the Retroarch debug console
int console_log(lua_State *L) {
    const char *msg = luaL_checkstring(L,1);
    RARCH_LOG("[Lua] %s\n", msg);
    return 0;  // void function
}

static const struct luaL_Reg  consolelib[] = {
    { "log" , console_log },
    { "writeline" , console_log },
 	{NULL,NULL}
};


// gameinfo.getromhash()
// returns the hash of the currently loaded rom, if a rom is loaded
// TODO: currently it is the CRC32, Bizhawk uses MD5 for CD-based systems, SHA1 for ROM-based systems
// TODO: fceux allows passing "string type" arg like "md5"
int gameinfo_getromhash(lua_State *L) {
    char reply[40] = {0};
    snprintf(reply, sizeof(reply), "%X", content_get_crc());
    lua_pushstring(L, reply);
    return 1;
}

// gameinfo.getromname()
// returns the name of the currently loaded rom, if a rom is loaded
int gameinfo_getromname(lua_State *L) {
    const char *path = path_get(RARCH_PATH_BASENAME);
    const char *basename = path ? path_basename(path) : ""; // fallback to empty string
    lua_pushfstring(L, "%s", basename); 
    return 1;
}

// gameinfo_getrompath()
// returns the full path of the currently loaded rom, if a rom is loaded
int gameinfo_getrompath(lua_State *L) {
    const char *path = path_get(RARCH_PATH_CONTENT);
    const char *r = path ? path : "";  // fallback to empty string
    lua_pushfstring(L, "%s", r);
    return 1;
}

static const struct luaL_Reg  gameinfolib[] = {
    //{ "getboardtype" , gameinfo_getboardtype },
    { "getromhash" , gameinfo_getromhash }, 
    { "getromname", gameinfo_getromname },
    { "getrompath", gameinfo_getrompath },
    //{ "getstatus" , gameinfo_getstatus },  // returns the game database status of the currently loaded rom. Statuses are for example: GoodDump, BadDump, Hack, Unknown, NotInDatabase
    //{ "indatabase", gameinfo_indatabase }, // returns whether or not the currently loaded rom is in the game database
    //{ "isstatusbad", gameinfo_isstatusbad }  // returns the currently loaded rom's game database status is considered 'bad'
	{NULL,NULL}
};

static const struct luaL_Reg  romlib[] = {
    { "gethash" , gameinfo_getromhash }, 
    { "getfilename", gameinfo_getromname },
    //TODO: rom.readbyte(int address)
	{NULL,NULL}
};


// client.ispaused()
// Returns true if emulator is paused, otherwise, false
int client_ispaused(lua_State *L) {
    runloop_state_t *runloop_st = runloop_state_get_ptr();
    int r = (runloop_st->flags & RUNLOOP_FLAG_PAUSED);
    lua_pushboolean(L, r);
    return 1;
}

// client.emulating()
// Returns true if emulator is paused, otherwise, false
int client_emulating(lua_State *L) {
    runloop_state_t *runloop_st = runloop_state_get_ptr();
    int r = (runloop_st->flags & RUNLOOP_FLAG_PAUSED) ? false : true;
    lua_pushboolean(L, r);
    return 1;
}

// client.isturbo()
// Returns true if emulator is in turbo mode, otherwise, false
int client_isturbo(lua_State *L) {
    runloop_state_t *runloop_st = runloop_state_get_ptr();
    int r = (runloop_st->flags & RUNLOOP_FLAG_FASTMOTION);
    lua_pushboolean(L, r);
    return 1;
}

// client.screenheight()
// Gets the current height in pixels of the emulator's drawing area
int client_screenheight(lua_State *L) {
    video_driver_state_t *video_st    = video_state_get_ptr();
    unsigned r = video_st->height;
    lua_pushinteger(L, (lua_Integer)r);
    return 1;
}

// client.screenwidth()
// Gets the current height in pixels of the emulator's drawing area
int client_screenwidth(lua_State *L) {
    video_driver_state_t *video_st    = video_state_get_ptr();
    unsigned r = video_st->width;
    lua_pushinteger(L, (lua_Integer)r);
    return 1;
}

// client.bufferheight
// Gets the visible height of the emu display surface (the core video output). This excludes the gameExtraPadding you've set.
int client_bufferheight(lua_State *L) {
    video_driver_state_t *video_st    = video_state_get_ptr();
    unsigned r  = video_st->av_info.geometry.base_height;
    lua_pushinteger(L, (lua_Integer)r);
    return 1;
}

// client.bufferwidth()
// Gets the visible width of the emu display surface (the core video output). This excludes the gameExtraPadding you've set.
int client_bufferwidth(lua_State *L) {
    video_driver_state_t *video_st    = video_state_get_ptr();
    unsigned r  = video_st->av_info.geometry.base_width;
    lua_pushinteger(L, (lua_Integer)r);
    return 1;
}

// client.getversion()
// Returns the current stable Retroarch version
int client_getversion(lua_State *L) {
    lua_pushstring(L, PACKAGE_VERSION);
    return 1; 
}

// client.pause()
// Pauses the emulator
int client_pause(lua_State *L) {
    command_event(CMD_EVENT_PAUSE, NULL);
    return 0; 
}

// client.unpause()
// Unpauses the emulator
int client_unpause(lua_State *L) {
    command_event(CMD_EVENT_UNPAUSE, NULL);
    return 0; 
}

// client.togglepause()
// Toggles the current pause state
int client_togglepause(lua_State *L) {
    command_event(CMD_EVENT_PAUSE_TOGGLE, NULL);
    return 0; 
}

// client.exit()
// Closes the emulator
int client_exit(lua_State *L) {
    command_event(CMD_EVENT_QUIT, NULL);
    return 0; 
}

// client.reboot_core()
// Reboots the currently loaded core
int client_reboot_core(lua_State *L) {
    command_event(CMD_EVENT_RESET, NULL);
    return 0; 
}

// client.closerom()
// Closes the loaded Rom
int client_closerom(lua_State *L) {
    command_event(CMD_EVENT_CLOSE_CONTENT, NULL);
    return 0; 
}


// void client.screenshot([string path = nil])
// TODO: if a parameter is passed it will function as the Screenshot As menu item of EmuHawk, else it will function as the Screenshot menu item
int client_screenshot(lua_State *L) {
    command_event(CMD_EVENT_TAKE_SCREENSHOT, NULL);
    return 0; 
}

// client.sleep(int millis)
// sleeps for n milliseconds
int client_sleep(lua_State *L) {
    if (lua_gettop(L) < 1) return luaL_error(L, "emulation_sleep: expected 1 argument (milliseconds)");
    lua_Integer ms = luaL_checkinteger(L, 1);
    if (ms < 0) return luaL_error(L, "emulation_sleep: time must be >= 0");
    retro_sleep((uint64_t)ms);
    return 0; 
}

static const struct luaL_Reg  clientlib [] = {
    { "ispaused" ,  client_ispaused },
    { "isturbo" ,  client_isturbo },
    { "screenheight" , client_screenheight },
    { "screenwidth" , client_screenwidth },
    { "bufferheight" , client_bufferheight },
    { "bufferwidth" , client_bufferwidth },
    { "getversion" , client_getversion },
    { "pause" , client_pause },
    { "unpause" , client_unpause },
    { "togglepause" , client_togglepause },
    { "exit" , client_exit },
    { "reboot_core" , client_reboot_core },
    { "closerom" , client_closerom },  
    { "screenshot", client_screenshot },
    { "sleep" , client_sleep },  
    // client.getconfig  // TODO: wrap settings_t *settings           = config_get_ptr();
    // client.openrom(string path)  -> core_load_game
	{NULL,NULL}
};


// nluatable joypad.get([int? controller = nil])
// returns a lua table of the controller buttons pressed. If supplied, it will only return a table of buttons for the given controller
int joypad_get(lua_State *L) {
    // TODO: handle "controller" arg
    
    input_driver_state_t *input_st    = input_state_get_ptr();
    settings_t *settings = config_get_ptr();
    input_bits_t current_bits;
    input_driver_collect_system_input(input_st, settings, &current_bits);

    static const char* button_names[] = {
        "A", "B", "Select", "Start", "Up", "Down", "Left", "Right",
        "L", "R", "X", "Y", "L2", "R2", "L3", "R3"
    };
    static const int button_codes[] = {
        RETRO_DEVICE_ID_JOYPAD_A, 
        RETRO_DEVICE_ID_JOYPAD_B, 
        RETRO_DEVICE_ID_JOYPAD_SELECT, 
        RETRO_DEVICE_ID_JOYPAD_START,
        RETRO_DEVICE_ID_JOYPAD_UP,
        RETRO_DEVICE_ID_JOYPAD_DOWN,
        RETRO_DEVICE_ID_JOYPAD_LEFT,
        RETRO_DEVICE_ID_JOYPAD_RIGHT,
        RETRO_DEVICE_ID_JOYPAD_L,
        RETRO_DEVICE_ID_JOYPAD_R,
        RETRO_DEVICE_ID_JOYPAD_X,
        RETRO_DEVICE_ID_JOYPAD_Y,
        RETRO_DEVICE_ID_JOYPAD_L2,
        RETRO_DEVICE_ID_JOYPAD_R2,
        RETRO_DEVICE_ID_JOYPAD_L3,
        RETRO_DEVICE_ID_JOYPAD_R3,
    };

    lua_newtable(L);
    
    for (unsigned i = 0; i < 16 ; i++)
    {
        bool pressed = BIT256_GET(current_bits, button_codes[i]);

        char key[16];
        snprintf(key, sizeof(key), "P1 %s", button_names[i]);
        lua_pushstring(L, key);
        lua_pushstring(L, pressed ? "True" : "False");
        lua_settable(L, -3);
    }
    
    return 1;
}

// nluatable input.get()
// Returns a dict-like table of key/button names (of host). Only pressed buttons will appear (with a value of true); unpressed buttons are omitted. I
int input_get(lua_State *L) {
   lua_newtable(L);

    input_driver_state_t *input_st    = input_state_get_ptr();
    settings_t *settings = config_get_ptr();
    //input_bits_t current_bits;
    //input_driver_collect_system_input(input_st, settings, &current_bits);
    input_driver_t *current_input           = input_st->current_driver;
    
    char curr_keyname[64] = {0};

   // Keyboard scan
   for (unsigned key = 0; key < RETROK_LAST; key++)
   {
      input_keymaps_translate_rk_to_str(key, curr_keyname, 64);
  
     bool curr_pressed = current_input->input_state(
              input_st->current_data,
              0,
              0,
              0,
              0,
              0,
              0,
              RETRO_DEVICE_KEYBOARD, 0, key);
              
      if (*curr_keyname && curr_pressed )
      {
          // key is pressed
         lua_pushboolean(L, 1);  // true
         lua_setfield(L, -2, curr_keyname);
      }
   }
   return 1;
}


static const struct luaL_Reg  joypadlib [] = {
    { "get" ,  joypad_get },
    { "read" ,  joypad_get },
	{NULL,NULL}
};

static const struct luaL_Reg  inputlib [] = {
    { "get" ,  input_get },
    { "read" ,  input_get },
	{NULL,NULL}
};

bool using_hw_framebuffer() {
    video_driver_state_t *video_st = video_state_get_ptr();  
    if(video_st->frame_cache_data && (video_st->frame_cache_data == RETRO_HW_FRAME_BUFFER_VALID))
        return true;
    else
        return false;
}


size_t get_memory_address_arg(lua_State *L, const int BYTES_TO_READ, const unsigned int domain) {
    if (lua_gettop(L) < 1)
        return luaL_error(L, "missing address");
        
    if (!lua_isnumber(L, 1))
        return luaL_error(L, "invalid address");
    
    const unsigned int address = (unsigned int)luaL_checkinteger(L, 1);
    
    // check if the address is valid for the current core
    runloop_state_t *runloop_st = runloop_state_get_ptr(); 
    const size_t memsize = runloop_st->current_core.retro_get_memory_size(domain);
    if((address + BYTES_TO_READ - 1) > memsize)
        return luaL_error(L, "address out of bounds");
    // else 
    return address;
}


unsigned int get_memory_domain_arg(lua_State *L, const int DOMAIN_ARG_POS) {
    unsigned int domain = RETRO_MEMORY_SYSTEM_RAM;  // default

    if (lua_gettop(L) >= DOMAIN_ARG_POS ) {  // 3 for write functions, 2 for read functions
            // domain arg passed
            const char *domain_str = luaL_checkstring(L, DOMAIN_ARG_POS);
            if(strcasestr(domain_str, "vram") != NULL) {  // case-insensitive substring check
                // only vram domain is supported by now
                domain = RETRO_MEMORY_VIDEO_RAM;
            }
            // TODO: add more memory domains
    }
    if(domain == RETRO_MEMORY_VIDEO_RAM && using_hw_framebuffer()) {
        return luaL_error(L, "cannot access hardware framebuffer");
    }
    return domain;
}

int get_memory_value(lua_State *L, const int BYTES_TO_READ, bool with_sign, bool big_endian) {
    const unsigned int domain = get_memory_domain_arg(L, 2);
    const size_t address = get_memory_address_arg(L, BYTES_TO_READ, domain);    
    const uint8_t *data = (const uint8_t *) runloop_state_get_ptr()->current_core.retro_get_memory_data(domain);
    if (!data) return luaL_error(L, "unable to access memory");
    
    int value;
    
    if(BYTES_TO_READ == 1 && with_sign == false)
        value = (uint8_t) *(data+address);
    else if(BYTES_TO_READ == 1 && with_sign == true)
        value = (int8_t) *(data+address);
    else if(BYTES_TO_READ == 2 && with_sign == false && big_endian == false) // u16_le
        value = (uint16_t)((data[address]) | (data[address + 1] << 8));
    else if(BYTES_TO_READ == 2 && with_sign == false && big_endian == true) // u16_be
        value = (uint16_t)((data[address] << 8) | data[address + 1]);
    else if(BYTES_TO_READ == 2 && with_sign == true && big_endian == false) // s16_le
        value = (int16_t)((data[address]) | (data[address + 1] << 8));
    else if(BYTES_TO_READ == 2 && with_sign == true && big_endian == true) // s16_be
        value = (int16_t)((data[address] << 8) | data[address + 1]);
    // TODO:BYTES_TO_READ == 3, 4
    
    lua_pushinteger(L, value);
    return 1;
}


// uint memory.readbyte(long addr, [string domain = nil])
// gets the value from the given address as an unsigned byte
int memory_readbyte(lua_State *L) {
    return get_memory_value(L, 1, false, false);
}

int memory_readbytesigned(lua_State *L) {  
    return get_memory_value(L, 1, true, false);
}

int memory_read_u16_le(lua_State *L) {
    return get_memory_value(L, 2, false, false);
}

int memory_read_u16_be(lua_State *L) {
    return get_memory_value(L, 2, false, true);
}

int memory_read_s16_le(lua_State *L) {
    return get_memory_value(L, 2, true, false);
}

int memory_read_s16_be(lua_State *L) {
    return get_memory_value(L, 2, true, true);
}


// void memory.writebyte(long addr, uint value, [string domain = nil])
// Writes the given value to the given address as an unsigned byte
int memory_writebyte(lua_State *L) {
    const unsigned int domain = get_memory_domain_arg(L, 3);
    const size_t address = get_memory_address_arg(L, 1, domain);
    
    unsigned int value = (unsigned int)luaL_checkinteger(L, 2);

    uint8_t *data = runloop_state_get_ptr()->current_core.retro_get_memory_data(domain);  
    if (!data) return luaL_error(L, "unable to access memory");
    // else
    
    *(data + address) = (uint8_t)value;
    return 0;
}

// nluatable memory.readbyterange(long addr, int length, [string domain = nil])
// Reads the address range that starts from address, and is length long. Returns a zero-indexed table containing the read values (an array of bytes.)
int memory_readbyterange(lua_State *L) {
    if (lua_gettop(L) < 2 || !lua_isnumber(L, 2) )
        return luaL_error(L, "memory.readbyterange: argument 1 (addr) and 2 (length) are required");
    
    const unsigned int domain = get_memory_domain_arg(L, 3);
    unsigned length = (unsigned)luaL_checkinteger(L, 2);
    const size_t address = get_memory_address_arg(L, length, domain);      
    const uint8_t *data = runloop_state_get_ptr()->current_core.retro_get_memory_data(domain);  
    if (!data) return luaL_error(L, "unable to access memory");
    // else
    
    lua_newtable(L);
    for (unsigned int i = 0; i < length; i++) {
        lua_pushfstring(L, "%d", i );
        lua_pushfstring(L, "%d", (uint8_t)*(data+address+i));
        lua_settable(L, -3);
    }
    return 1;
}

// string memory.hash_region(long addr, int count, [string domain = nil])
// Returns a hash as a string of a region of memory, starting from addr, through count bytes. If the domain is unspecified, it uses the current region.
// Bizhawk currently uses sha256, so we do the same
int memory_hash_region(lua_State *L) {
    if (lua_gettop(L) < 2 || !lua_isnumber(L, 2) )
        return luaL_error(L, "memory.hash_region: argument 1 (addr) and 2 (length) are required");
    
    const unsigned int domain = get_memory_domain_arg(L, 3);
    unsigned length = (unsigned)luaL_checkinteger(L, 2);
    const size_t address = get_memory_address_arg(L, length, domain);
      
    const uint8_t *data = runloop_state_get_ptr()->current_core.retro_get_memory_data(domain);  
    if (!data) return luaL_error(L, "unable to access memory");
    // else
    
    char out_hash[256] = {0};
    sha256_hash(out_hash, data+address, length);
    lua_pushstring(L, out_hash);
    return 1;
}


// emu.frameadvance()
// Signals to the emulator to resume emulation. Necessary for any lua script while loop or else the emulator will freeze!
int emu_frameadvance(lua_State *L) {
	return lua_yield(L, 0);
}

// emu.framecount()
// Returns the current frame count
int emu_framecount(lua_State *L) {
   video_driver_state_t *video_st = video_state_get_ptr();
   uint64_t frame_count  = video_st->frame_count;
   lua_pushinteger(L, (lua_Integer)frame_count);
   return 1;
}

// emu.getsystemid()
// returns (if available) the board name of the loaded ROM
// TODO: match Bizhaws strings: "pc_engine" -> "PCE", ...
int emu_getsystemid(lua_State *L) {
    core_info_t *core_info      = NULL;
    core_info_get_current_core(&core_info);
    char* sysid = NULL;
    sysid = core_info->system_id;
    if(sysid) {
        // try to match Bizhawk names
        if (strncmp(sysid, "super_nes", strlen(sysid)) == 0) sysid = "SNES" ;
        if (strncmp(sysid, "mega_drive", strlen(sysid)) == 0) sysid = "GEN" ;
        if (strncmp(sysid, "master_system", strlen(sysid)) == 0) sysid = "SMS" ;
        if (strncmp(sysid, "game_boy", strlen(sysid)) == 0) sysid = "GB" ;
        if (strncmp(sysid, "game_boy_advance", strlen(sysid)) == 0) sysid = "GBA" ;
        if (strncmp(sysid, "pc_engine", strlen(sysid)) == 0) sysid = "PCE" ;
        if (strncmp(sysid, "sega_saturn", strlen(sysid)) == 0) sysid = "SAT" ;
        if (strncmp(sysid, "playstation", strlen(sysid)) == 0) sysid = "PSX" ;
        if (strncmp(sysid, "commodore_64", strlen(sysid)) == 0) sysid = "C64" ;
        if (strncmp(sysid, "nintendo_64", strlen(sysid)) == 0) sysid = "N64" ;
        if (strncmp(sysid, "virtual_boy", strlen(sysid)) == 0) sysid = "VB" ;
        if (strncmp(sysid, "wonderswan", strlen(sysid)) == 0) sysid = "WSWAN" ;
        if (strncmp(sysid, "neo_geo_pocket", strlen(sysid)) == 0) sysid = "NGP" ;
    }
    lua_pushstring(L, sysid ? sysid : "");
    return 1;
}

// emu.getdir()
// Returns the path of retroarch.exe as a string.
int emu_getdir(lua_State *L) {
    settings_t *settings            = config_get_ptr();
    //const char* r = strdup(settings->paths.directory_system);  // /system
    //const char* r = path_get(RARCH_PATH_CONFIG);  // /system
    char app_path[PATH_MAX_LENGTH]         = {0};
    fill_pathname_application_dir(app_path, sizeof(app_path));  // main exe    
    lua_pushstring(L, strdup(app_path));
    return 1;
}

// emu.getscreenpixel(int x, int y, bool getemuscreen)
// Returns the separate RGB components of the given screen pixel, and the palette. Can be 0-255 by 0-239, but NTSC only displays 0-255 x 8-231 of it.
// TODO (currently ignored): If getemuscreen is false, this gets background colors from either the screen pixel or the LUA pixels set, but LUA data may not match the information used to put the data to the screen. If getemuscreen is true, this gets background colors from anything behind an LUA screen element.
// Usage is local r,g,b,palette = emu.getscreenpixel(5, 5, false) to retrieve the current red/green/blue colors and palette value of the pixel at 5x5.
// The "palette" value is actually the 32-bit pixel value.
int emu_getscreenpixel(lua_State *L) {
    if (lua_gettop(L) < 2)
        return luaL_error(L, "emu.getscreenpixel(x, y) requires 2 arguments");

    if(using_hw_framebuffer()) 
        return luaL_error(L, "cannot read hardware framebuffer");
        
    unsigned x = luaL_checkinteger(L, 1);
    unsigned y = luaL_checkinteger(L, 2);

    video_driver_state_t *video_st = video_state_get_ptr();
    const void *frame = (const void*)video_st->frame_cache_data;
    unsigned width  = video_st->frame_cache_width;
    unsigned height = video_st->frame_cache_height;
    size_t pitch    = video_st->frame_cache_pitch;
    
    // Bounds-check if desired
    if (!frame || x >= width || y >= height) {
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
        return 4;
    }
    
    // TODO: detect hardware-rendered cores and abort (segfault otherwise)

    // Locate the pixel (assumes 32bpp XRGB8888)
    uint32_t *pixels = (uint32_t*)frame;
    unsigned pitch_pixels = pitch / sizeof(uint32_t);
    uint32_t pixel = pixels[y * pitch_pixels + x];

    // Extract RGB (R = high byte, B = low byte)
    uint8_t r = (pixel >> 16) & 0xFF;
    uint8_t g = (pixel >>  8) & 0xFF;
    uint8_t b = (pixel      ) & 0xFF;

    lua_pushinteger(L, r);
    lua_pushinteger(L, g);
    lua_pushinteger(L, b);
    lua_pushinteger(L, pixel);
    return 4;
}


// void gui.addmessage(string message)
// Adds a message to the OSD's message area
int gui_addmessage(lua_State *L) {
    const char *msg = luaL_checkstring(L,1);
    runloop_msg_queue_push(msg, strlen(msg), 1, 180, false, NULL,
          MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
    return 0; 
}

#ifdef HAVE_GFX_WIDGETS
// WIP:
unsigned gui_drawString_x;
unsigned gui_drawString_y;
char *gui_drawString_msg = NULL;
uint32_t gui_drawString_color = 0x000000FF; // default black, fully opaque
uint32_t gui_drawString_bg_color = 0xFFFFFFFF; // default white, fully opaque  (alpha is the last byte)

void draw_strings_loop() {

    if(!gui_drawString_msg) return;

    // TODO: iterate and read from a container
    unsigned x = gui_drawString_x;
    unsigned y = gui_drawString_y;
    const char *msg = gui_drawString_msg;
    uint32_t color = gui_drawString_color;
    uint32_t bg_color = gui_drawString_bg_color;
        
    dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();
    void *font  = &p_dispwidget->gfx_widget_fonts.regular.font;
    if (!font) puts("empty font data");

    //bool widgets_active            = p_dispwidget->active;
    // TODO :Check if active

   //video_driver_state_t *video_st = video_state_get_ptr();     
   //unsigned video_width          = video_st->current_video.width;
   //unsigned video_width             = video_info->width;
   unsigned video_width             = p_dispwidget->last_video_width;
   unsigned video_height            = p_dispwidget->last_video_height;
   unsigned font_scale            = 1;
   unsigned font_size            = 24;  // TODO: read from the caller
   
   /*
    unsigned height       = p_dispwidget->simple_widget_height;
      size_t _len = strlcpy(txt, msg_hash_to_str(msg), sizeof(txt));

      width = font_driver_get_message_width(
            p_dispwidget->gfx_widget_fonts.regular.font,
            txt, _len, 1.0f)
         + p_dispwidget->simple_widget_padding * 2;
    */


    video_driver_state_t *video_st = video_state_get_ptr();
    /*
    struct font_params params;
   
   params.x           = x / video_width;
   params.y           = 1.0f - y / video_height;
   params.scale       = font_scale;
   params.drop_mod    = 0.0f;
   params.drop_x      = 0.0f;
   params.drop_y      = 0.0f;
   params.color       = color;
   params.full_screen = true;
   params.text_align  = TEXT_ALIGN_CENTER;
*/
  /*
   if (shadows_enable)
   {
      params.drop_x      = shadow_offset;
      params.drop_y      = -shadow_offset;
      params.drop_alpha  = GFX_SHADOW_ALPHA;
   }

video_frame_info_t video;
video_driver_build_info(&video);
 gfx_display_t *p_disp      = (gfx_display_t*)video.disp_userdata;
   dispgfx_widget_t *p_widget = (dispgfx_widget_t*)video.widgets_userdata;
   void *userdata             = video.userdata;
    */
  //gfx_display_set_alpha(p_widget->backdrop_orig, DEFAULT_BACKDROP);
  
   void *userdata                   = video_driver_get_ptr();
   //void *userdata = VIDEO_DRIVER_GET_PTR_INTERNAL(video_st);
   gfx_display_t *p_disp      = disp_get_ptr();
   
   float quad_color[4] = {0.0f, 1.0f, 0.0f, 1.0f};  // RGBA

   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width, video_height,
         x, y,
         100, 100,
         video_width, video_height,
         quad_color,
         NULL);   

   font_data_impl_t font_data;
   font_data.line_height        = (int)(font_size + 0.5f);
   font_data.glyph_width        = (int)((font_size * (3.0f / 4.0f)) + 0.5f);

   // Load a custom font -- TODO: only if specified
   /*
   char font_path[PATH_MAX_LENGTH];
   if (!(font_data.font = gfx_display_font_file(p_disp, "Vera.ttf", font_size, false))) {
      puts("cannot load font");
      return;
   }*/
   // Get font metadata
   //if ((glyph_width = font_driver_get_message_width(font_data->font, "a", 1, 1.0f)) > 0)

    gfx_display_draw_text(
        font_data.font,
        msg,
        x, y,
        video_width, video_height,
        color,
        TEXT_ALIGN_LEFT,
        1.0f, false, 0.0f, true);

    /* ALT:
      gfx_widgets_draw_text(
         font,
         msg,
         x, y,
         video_width, video_height,
         0xFFFFFFFF,
         TEXT_ALIGN_LEFT,
         true);
        */
}

void lua_draw_gfxs_loop() {
    draw_strings_loop();
}

// void gui.drawString(int x, int y, string message, [luacolor forecolor = nil], [luacolor backcolor = nil], [int? fontsize = nil], [string fontfamily = nil], [string fontstyle = nil], [string horizalign = nil], [string vertalign = nil], [string surfacename = nil])
// Draws the given message in the emulator screen space (like all draw functions) at the given x,y coordinates and the given color. The default color is white. A fontfamily can be specified and is monospace generic if none is specified (font family options are the same as the .NET FontFamily class). The fontsize default is 12. The default font style is regular. Font style options are regular, bold, italic, strikethrough, underline. Horizontal alignment options are left (default), center, or right. Vertical alignment options are bottom (default), middle, or top. Alignment options specify which ends of the text will be drawn at the x and y coordinates. For pixel-perfect font look, make sure to disable aspect ratio correction.
int gui_drawString(lua_State *L) {
   if (lua_gettop(L) < 3)
        return luaL_error(L, "gui.drawString(x, y, message) requires at least 3 arguments");

    if(using_hw_framebuffer()) 
        return luaL_error(L, "cannot draw on hardware framebuffer");
        
    unsigned x = luaL_checkinteger(L, 1);
    unsigned y = luaL_checkinteger(L, 2);
    const char *msg = luaL_checkstring(L, 3);

    uint32_t color = 0xFFFFFFFF; // default white, fully opaque
    if (lua_gettop(L) >= 4 && lua_isnumber(L, 4))
        color = (uint32_t)luaL_checkinteger(L, 4);
    
    uint32_t bg_color = 0x000000FF; // default black, fully opaque
    //uint32_t bg_color = p_widget->backdrop_orig;
    if (lua_gettop(L) >= 5 && lua_isnumber(L, 5))
        bg_color = (uint32_t)luaL_checkinteger(L, 5);
    
    // TODO: use a container for multiple shapes
    gui_drawString_x = x;
    gui_drawString_y = y;
    if(gui_drawString_msg) free(gui_drawString_msg);
    gui_drawString_msg = strdup(msg);
    gui_drawString_color = color;
    gui_drawString_bg_color = bg_color;
    
    return 0;
}

// void gui.drawPixel(int x, int y, [luacolor color = nil], [string surfacename = nil])
// Draws a single pixel at the given coordinates in the given color. Color is optional (if not specified it will be drawn black)
// Luacolor must be a 32-bit number in the format 0xAARRGGBB;
int gui_drawPixel(lua_State *L) {
   if (lua_gettop(L) < 2)
        return luaL_error(L, "gui.drawPixel(x, y) requires at least 2 arguments");

    if(using_hw_framebuffer()) 
        return luaL_error(L, "cannot draw on hardware framebuffer");

    unsigned x = luaL_checkinteger(L, 1);
    unsigned y = luaL_checkinteger(L, 2);

    uint32_t color = 0xFF000000; // default black, fully opaque
    if (lua_gettop(L) >= 3 && lua_isnumber(L, 3))
        color = (uint32_t)luaL_checkinteger(L, 3);

    /* WIP
    video_driver_state_t *video_st = video_state_get_ptr();    
    //uintptr_t frame = video_driver_get_current_framebuffer();
    uint32_t *frame = (uint32_t*)video_st->frame_cache_data;  // remove const
    unsigned width  = video_st->frame_cache_width;  // 0?
    unsigned height = video_st->frame_cache_height;  // 0?
    size_t pitch    = video_st->frame_cache_pitch;

    printf("%u\n", frame);
    printf("%u %u\n", width, height);
    
    // Bounds-check if desired
    if (!frame || x >= width || y >= height) {
        return luaL_error(L, "invalid coords or frame buffer");
    }

    frame[y * width + x] = color;
    */
    
    dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();
    gfx_widget_font_data_t *font  = &p_dispwidget->gfx_widget_fonts.regular;
   
    bool widgets_active            = p_dispwidget->active;
    // TODO :Check if active
    
    //unsigned width  = video_st->frame_cache_width;
    //unsigned width  = video_st->width;
    //unsigned height = video_st->frame_cache_height;
    //unsigned height = video_st->height;
   
   //video_driver_state_t *video_st = video_state_get_ptr();     
   //unsigned video_width          = video_st->current_video.width;
   unsigned video_width             = p_dispwidget->last_video_width;
   unsigned video_height            = p_dispwidget->last_video_height;
   video_driver_get_size(&video_width, &video_height);
   void *userdata                   = video_driver_get_ptr();
   gfx_display_t *p_disp      = disp_get_ptr();
      
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width, video_height,
            x, y,
            100, 100,
            video_width, video_height,
            p_dispwidget->backdrop_orig,
            NULL
      );
    return 0;
}
#endif

// string comm.httpGet(string url)
// makes a HTTP GET request
int comm_httpget(lua_State *L) {
    const char *url = luaL_checkstring(L,1);  
    void* t = task_push_http_transfer(url, true, "GET", NULL, NULL);
    if(!t) return luaL_error(L, "cannot send HTTP request");
    lua_pushstring(L, "OK");  // TODO: return body to the caller?   
    return 1;
}

// string comm.httpPost(string url, string payload)
// makes a HTTP POST request
int comm_httppost(lua_State *L) {
    const char *url = luaL_checkstring(L,1);  
    const char *payload = luaL_checkstring(L,2);
    void* t = task_push_http_post_transfer(url, payload, true, "POST", NULL, NULL);
    if(!t) return luaL_error(L, "cannot send HTTP request");
    lua_pushstring(L, "OK");  // TODO: return body to the caller?   
    return 1;
}

// string comm.httpPost(string url, string payload)
// makes a HTTP PUT request
int comm_httpput(lua_State *L) {
    const char *url = luaL_checkstring(L,1);  
    const char *payload = luaL_checkstring(L,2);
    void* t = task_push_http_post_transfer(url, payload, true, "PUT", NULL, NULL);
    if(!t) return luaL_error(L, "cannot send HTTP request");
    lua_pushstring(L, "OK");  // TODO: return body to the caller?   
    return 1;
}


static const struct luaL_Reg  guilib[] = {
    { "addmessage" ,  gui_addmessage },
#ifdef HAVE_GFX_WIDGETS
    { "drawString" ,  gui_drawString },
    { "drawText" ,  gui_drawString },
    { "drawPixel" ,  gui_drawPixel },
    //TODO: drawLine
    //TODO: drawImage
    //TODO: drawRectangle
    //TODO: drawBox
#endif
    // FCEUX-aliases
    { "text", gui_drawString },
    { "drawtext", gui_drawString },
    { "getpixel", emu_getscreenpixel },
    { "setpixel", gui_drawPixel },
    //TODO: gui.parsecolor(color)
    { "savescreenshot", client_screenshot },
	{NULL,NULL}
};

static const struct luaL_Reg  emulib[] = {
    { "frameadvance" , emu_frameadvance } ,
    { "framecount" ,  emu_framecount },
    { "getsystemid" ,  emu_getsystemid },
    // FCEUX compatible functions  https://fceux.com/web/help/LuaFunctionsList.html
    { "getscreenpixel", emu_getscreenpixel },  // FCEUX-only, sometimes used for scene detection
    { "exit" ,  client_exit },  
    { "paused" ,  client_ispaused },
    { "pause" ,  client_pause },
    { "unpause" ,  client_unpause },
    { "softreset" ,  client_reboot_core },
    { "message" ,  gui_addmessage },
    { "print" ,  console_log },
    { "emulating", client_emulating },
    { "getdir", emu_getdir },
    // TODO: "poweron"
    // TODO: "loadrom"
	{NULL,NULL}
};


static const struct luaL_Reg  memorylib [] = {
    { "readbyte" ,  memory_readbyte },
    { "read_u8" ,  memory_readbyte },
    { "read_s8" ,  memory_readbytesigned },
    { "read_s16_be" ,  memory_read_s16_be },
    { "read_s16_le" ,  memory_read_s16_le },
    { "read_u16_be" ,  memory_read_u16_be },
    { "read_u16_le" ,  memory_read_u16_le },
    // TODO: read_u/s24_be(le
    // TODO: read_u/s32_be(le
    { "readbyterange" ,  memory_readbyterange },
    { "read_bytes_as_array" ,  memory_readbyterange },
    { "hash_region" ,  memory_hash_region },
    { "writebyte" ,  memory_writebyte },
    { "write_u8" ,  memory_writebyte },
    // TODO: write_s8 / 16/ 32
    //TODO: { "write_bytes_as_array" ,  memory_write_bytes_as_array },
    //TODO: { "write_bytes_as_dict" ,  memory_write_bytes_as_dict },
    // FCEUX-aliases
    { "readbyteunsigned" ,  memory_readbyte },
    { "readbytesigned" ,  memory_readbytesigned },
    { "readwordsigned" ,  memory_read_s16_le },
    { "readword" ,  memory_read_u16_le },
    //TODO: { "readfloat" ,  memory_readfloat },
    //TODO: { "writebyterange" ,  memory_writebyterange },
	{NULL,NULL}
};


static const struct luaL_Reg  commlib[] = {
    { "httpGet", comm_httpget },
    { "httpPost", comm_httppost },
    { "httpPut", comm_httpput },
    // TODO: more functions
	{NULL,NULL}
};

// stdlib function sandboxing
int safe_io_open(lua_State *L) {
    if (lua_gettop(L) < 1)
        return luaL_error(L, "[Lua] open: argument 1 (addr) is required");

    const char *user_path = luaL_checkstring(L, 1);
    const char *mode = luaL_optstring(L, 2, "r");

    /* TOO RESTRICTIVE:
    if (path_is_absolute(user_path)) {
        return luaL_error(L, "Access denied: file path is absolute, must be relative");
    }*/
    // TODO: allow subdirs of
    //const char* r = path_get(RARCH_PATH_CONFIG);  // /system
    
    // Retrieve original io.open from registry
    lua_getfield(L, LUA_REGISTRYINDEX, "original_io_open");
    if (!lua_isfunction(L, -1)) {
        return luaL_error(L, "Original io.open not found");
    }

    // Push arguments for original io.open
    lua_pushstring(L, user_path);
    lua_pushstring(L, mode);

    // Call original io.open(path, mode)
    lua_call(L, 2, 1);

    return 1;
}


lua_State *co = NULL;
char* lua_file = "";
        
void lua_init() {

    // build current script name
    char *lua_file = strdup(path_get(RARCH_PATH_BASENAME));
    if (!lua_file) return;
    lua_file = realloc(lua_file, strlen(lua_file) + strlen(".lua") + 1);
    if (!lua_file) return;
    strcat(lua_file, ".lua");
    
    if (!path_is_valid(lua_file)) {
        RARCH_LOG("[Lua] %s not found\n", lua_file);
        free(lua_file);
        // try a global alternative
        settings_t *settings            = config_get_ptr();
        lua_file = strdup(settings->paths.directory_system);
        if (!lua_file) return;
        lua_file = realloc(lua_file, strlen(lua_file) + strlen("/autostart.lua") + 1);
        if (!lua_file) return;
        strcat(lua_file, "/autostart.lua");
        
        if (!path_is_valid(lua_file)) {
            RARCH_LOG("[Lua] %s not found\n", lua_file);
            free(lua_file);
            return;
        }
    }
    
    lua_State *L = luaL_newstate();
    lua_getglobal(L, "init");
    
    // Load full stdlib
    luaL_openlibs(L);
    
    // override unsafe functions
    // io.open
    /*
    lua_getglobal(L, "io");
    lua_getfield(L, -1, "open");       // push io.open
    lua_setfield(L, LUA_REGISTRYINDEX, "original_io_open"); // registry["original_io_open"] = io.open
    lua_pop(L, 1);                     // pop io table
    lua_getglobal(L, "io");            // push io table
    lua_pushstring(L, "open");         // push key "open"
    lua_pushcfunction(L, safe_io_open); // push our safe_io_open function
    lua_settable(L, -3);               // io.open = safe_io_open
    lua_pop(L, 1);                    // pop io table
    */
    
    // disable unsafe functions
    lua_getglobal(L, "os");
    if (lua_istable(L, -1)) {
        lua_pushstring(L, "execute");
        lua_pushnil(L);
        lua_settable(L, -3);
        // TODO: wrappers for : os.remove(filename), os.rename(old, new)
    }
    lua_pop(L, 1);

    // Disable risky loaders
    lua_pushnil(L); lua_setglobal(L, "dofile");
    lua_pushnil(L); lua_setglobal(L, "loadfile");
    lua_pushnil(L); lua_setglobal(L, "require");
    lua_pushnil(L); lua_setglobal(L, "load");  // disables eval-like dynamic code
    
    // register custom C functions
    // MEMO: luaL_register is deprecated as of Lua 5.2 and later. It still exists in Lua 5.1.
    luaL_newlib(L, consolelib);
    lua_setglobal(L, "console");
    luaL_newlib(L, gameinfolib);
    lua_setglobal(L, "gameinfo");
    luaL_newlib(L, romlib);
    lua_setglobal(L, "rom");  // fceux alternative
    luaL_newlib(L, emulib);
    lua_setglobal(L, "emu");
    luaL_newlib(L, clientlib);
    lua_setglobal(L, "client");
    luaL_newlib(L, joypadlib);
    lua_setglobal(L, "joypad");
    luaL_newlib(L, inputlib);
    lua_setglobal(L, "input");
    luaL_newlib(L, memorylib);
    lua_setglobal(L, "memory");
    luaL_newlib(L, guilib);
    lua_setglobal(L, "gui");
    luaL_newlib(L, commlib);
    lua_setglobal(L, "comm");
    //TODO: lua_register(L, "savestate", savestatelib);
    //TODO: lua_register(L, "movie", movielib);    
    lua_settop(L, 0); // clean the stack, because each call to lua_register leaves a table on top

    // Create a coroutine (needed to yield)
    co = lua_newthread(L);

    // Store the script in the coroutine
    luaL_loadfile(co, lua_file);

    free(lua_file);
}


void lua_loop() {
    //RARCH_LOG("[Lua] main loop\n");

    //if (!co) lua_init();  // lazy-init
    if (!co) return;  // init failed (no script file found)
    
    int status = lua_status(co);
    if(status != LUA_YIELD && status != LUA_OK)
        return;  // error or nothing to execute

    // maybe needed on mac?
    //int nres;
    //status = lua_resume(co, NULL, 0, &nres);
    //#else
    status = lua_resume(co, NULL, 0);

    if (status == LUA_YIELD) {
        // Successfully yielded (from emu.frameadvance)
    } else if (status == LUA_OK) {
        
    } else {
        // An error occurred
        const char *error_msg = lua_tostring(co, -1);
        if(error_msg) RARCH_ERR("[Lua] %s\n", error_msg);
    }
} 


void lua_deinit() {
    if (!co) return;  // init failed (no script file found)
    lua_close(co);
    co = NULL;
}
