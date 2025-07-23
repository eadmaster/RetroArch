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
#include "gfx/video_driver.h"
#include "gfx/video_display_server.h"

#ifdef HAVE_CHEEVOS
#include "cheevos/cheevos.h"
#endif



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

// gameinfo.getboardtype()
// returns identifying information about the 'mapper' or similar capability used for this game. empty if no such useful distinction can be drawn
//int gameinfo_getboardtype(lua_State *L) {
//    return 1;
//}

// gameinfo.getromhash()
// returns the hash of the currently loaded rom, if a rom is loaded
// TODO: currently it is the CRC32, Bizhawk uses MD5 for CD-based systems, SHA1 for ROM-based systems
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
    const char *basename = path ? path_basename(path) : "";
    lua_pushfstring(L, "%s", basename);  // fallback to empty string
    return 1;
}

static const struct luaL_Reg  gameinfolib[] = {
    //{ "getboardtype" , gameinfo_getboardtype },
    { "getromhash" , gameinfo_getromhash }, 
    { "getromname", gameinfo_getromname },
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


// gui.addmessage("test")
// void gui.addmessage(string message)
// Adds a message to the OSD's message area
int gui_addmessage(lua_State *L) {
    const char *msg = luaL_checkstring(L,1);
    runloop_msg_queue_push(msg, strlen(msg), 1, 180, false, NULL,
          MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
    return 0; 
}

    
// void gui.drawPixel(int x, int y, [luacolor color = nil], [string surfacename = nil])
// Draws a single pixel at the given coordinates in the given color. Color is optional (if not specified it will be drawn black)
// Luacolor must be a 32-bit number in the format 0xAARRGGBB;
int gui_drawPixel(lua_State *L) {
   if (lua_gettop(L) < 2)
        return luaL_error(L, "gui.drawPixel(x, y) requires at least 2 arguments");

    unsigned x = luaL_checkinteger(L, 1);
    unsigned y = luaL_checkinteger(L, 2);

    uint32_t color = 0xFF000000; // default black, fully opaque
    if (lua_gettop(L) >= 3 && lua_isnumber(L, 3))
        color = (uint32_t)luaL_checkinteger(L, 3);
    
    /* WIP
    video_driver_state_t *video_st = video_state_get_ptr();    
    //uintptr_t frame = video_driver_get_current_framebuffer();
    const uint32_t *frame = (const uint32_t*)video_st->frame_cache_data;  // remove const
    //unsigned width  = video_st->frame_cache_width;  // 0?
    //unsigned height = video_st->frame_cache_height;  // 0?
    unsigned width  = video_st->width;
    unsigned height = video_st->height;
    size_t pitch    = video_st->frame_cache_pitch;

    printf("%u\n", frame);
    printf("%u %u\n", width, height);
    
    // Bounds-check if desired
    if (!frame || x >= width || y >= height) {
        return luaL_error(L, "invalid coords or frame buffer");
    }

    frame[y * width + x] = color;
    */

    return 0;
}

 
static const struct luaL_Reg  guilib[] = {
    { "addmessage" ,  gui_addmessage },
    { "drawPixel" ,  gui_drawPixel },
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
    { "sleep" , client_sleep },  
    // client.getconfig  // TODO: wrap settings_t *settings           = config_get_ptr();
    // client.openrom(string path)
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

static const struct luaL_Reg  joypadlib [] = {
    { "get" ,  joypad_get },
	{NULL,NULL}
};

// TODO: remove deps
#ifdef HAVE_CHEEVOS

// uint memory.readbyte(long addr, [string domain = nil])
// gets the value from the given address as an unsigned byte
int memory_readbyte(lua_State *L) {
    if (lua_gettop(L) < 1)
        return luaL_error(L, "[Lua] memory_readbyte: argument 1 (addr) is required");


    // Check if an address was passed
    if (!lua_isnumber(L, 1))
    {
        lua_pushstring(L, "memory_readbyte: missing or invalid address argument");
        lua_error(L);
        return 0; // never reached, but good style
    }

    unsigned int address = (unsigned int)luaL_checkinteger(L, 1);

    const uint8_t *data = rcheevos_patch_address(address);
    if (data){
        uint8_t value = *data;
        lua_pushinteger(L, (int)value);
        return 1;
    } else {
        lua_pushstring(L, "memory_readbyte: address out of bounds or memory unavailable");
        lua_error(L);
        return 0;
    }
}
    /* WIP without CHEEVOS

    lua_Integer addr = luaL_checkinteger(L, 1);
    if (addr < 0 || addr > UINT32_MAX) 
    {
        lua_pushstring(L, "Address out of range");
        lua_error(L);
        return 0;
    }

    uint32_t address = (uint32_t)addr;

    runloop_state_t *runloop_st = runloop_state_get_ptr();
    const rarch_system_info_t* sys_info= &runloop_st->system;
    if (!sys_info)
        return 0;

   if (!sys_info || sys_info->mmaps.num_descriptors == 0) {
        lua_pushstring(L, "no memory map defined");
        lua_error(L);
        return 0;
    }
   else
   {
      size_t offset;
      //const rarch_memory_descriptor_t* desc = command_memory_get_descriptor(&sys_info->mmaps, address, &offset);
      const rarch_memory_descriptor_t* desc = sys_info->mmaps.descriptors;
      if (!desc)
        lua_pushstring(L, "no descriptor for address");
      else if (!desc->core.ptr)
         lua_pushstring(L, "no data for descriptor");
      //else if (for_write && (desc->core.flags & RETRO_MEMDESC_CONST))
      //   lua_pushstring(L, "descriptor data is readonly");
      else
      {
         unsigned int *max_bytes = (unsigned int)(desc->core.len - offset);
         uint8_t value =  (uint8_t*)desc->core.ptr + desc->core.offset + offset;
         lua_pushinteger(L, value);
         return 1;
      }
   }
   // else
   lua_error(L);
   return 0;
**/
    
// nluatable memory.readbyterange(long addr, int length, [string domain = nil])
// Reads the address range that starts from address, and is length long. Returns a zero-indexed table containing the read values (an array of bytes.)
int memory_readbyterange(lua_State *L) {
   if (lua_gettop(L) < 2) {
        luaL_error(L, "memory_readbyte: argument 1 (addr) and 2 (length) are required");
        return 0;
    }
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
    {
        lua_pushstring(L, "memory_readbyte: missing or invalid args");
        lua_error(L);
        return 0;
    }
    long address = (long)luaL_checkinteger(L, 1);
    unsigned length = (unsigned)luaL_checkinteger(L, 2);
    // TODO: range check
         
    uint8_t *data = rcheevos_patch_address(address);
    if (data){
        lua_newtable(L);
        for (unsigned int i = 0; i < length; i++) {
            lua_pushfstring(L, "%d", i+1 );
            lua_pushfstring(L, "%d", (int)*(data+i));
            lua_settable(L, -3);
        }
        return 1;
    } else {
        lua_pushstring(L, "memory_readbyte: address out of bounds or memory unavailable");
        lua_error(L);
        return 0;
    }
}

// string memory.hash_region(long addr, int count, [string domain = nil])
// Returns a hash as a string of a region of memory, starting from addr, through count bytes. If the domain is unspecified, it uses the current region.
// Bizhawk currently uses sha256, so we do the same
// TODO: "domain" arg is ignored
int memory_hash_region(lua_State *L) {
   if (lua_gettop(L) < 2) {
        luaL_error(L, "memory_hash_region: argument 1 (addr) and 2 (length) are required");
        return 0;
    }
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
    {
        lua_pushstring(L, "memory_hash_region: missing or invalid args");
        lua_error(L);
        return 0;
    }
    long address = (long)luaL_checkinteger(L, 1);
    unsigned length = (unsigned)luaL_checkinteger(L, 2);
    // TODO: range check
         
    uint8_t *data = rcheevos_patch_address(address);
    if (!data){
        lua_pushstring(L, "memory_hash_region: address out of bounds or memory unavailable");
        lua_error(L);
        return 0;
    }
    // else
    
    char out_hash[256] = {0};
    sha256_hash(out_hash, data, length);
    lua_pushstring(L, out_hash);
    return 1;
}

// void memory.writebyte(long addr, uint value, [string domain = nil])
// Writes the given value to the given address as an unsigned byte
int memory_writebyte(lua_State *L) {
   if (lua_gettop(L) < 2) {
        luaL_error(L, "memory_writebyte: argument 1 (addr) and 2 (value) are required");
        return 0;
    }
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
    {
        lua_pushstring(L, "memory_readbyte: missing or invalid args");
        lua_error(L);
        return 0;
    }
    long address = (long)luaL_checkinteger(L, 1);
    unsigned int value = (unsigned int)luaL_checkinteger(L, 2);
         
    uint8_t *data = rcheevos_patch_address(address);
    if (data){
        
       if (rcheevos_hardcore_active())
       {
          RARCH_LOG("[Lua] Achievements hardcore mode disabled by WRITE_CORE_RAM.\n");
          rcheevos_pause_hardcore();
       }
       *data = (uint8_t)value;
        return 0;
    } else {
        lua_pushstring(L, "memory_readbyte: address out of bounds or memory unavailable");
        lua_error(L);
        return 0;
    }
}

static const struct luaL_Reg  memorylib [] = {
    { "readbyte" ,  memory_readbyte },
    { "readbyteunsigned" ,  memory_readbyte },
    { "readbyterange" ,  memory_readbyterange },
    { "read_bytes_as_array" ,  memory_readbyterange },
    { "hash_region" ,  memory_hash_region },
    { "writebyte" ,  memory_writebyte },
	{NULL,NULL}
};

#endif


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
    if(core_info) sysid = core_info->system_id;
    lua_pushstring(L, sysid ? sysid : "");
    return 1;
}

// emu.getscreenpixel(int x, int y, bool getemuscreen)
// Returns the separate RGB components of the given screen pixel, and the palette. Can be 0-255 by 0-239, but NTSC only displays 0-255 x 8-231 of it. If getemuscreen is false, this gets background colors from either the screen pixel or the LUA pixels set, but LUA data may not match the information used to put the data to the screen.
// TODO (currently ignored): If getemuscreen is true, this gets background colors from anything behind an LUA screen element.
// Usage is local r,g,b,palette = emu.getscreenpixel(5, 5, false) to retrieve the current red/green/blue colors and palette value of the pixel at 5x5.
// The "palette" value is actually the 32-bit pixel value.
int emu_getscreenpixel(lua_State *L) {
    if (lua_gettop(L) < 2)
        return luaL_error(L, "emu.getscreenpixel(x, y) requires 2 arguments");

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
    // emulating
    // loadrom
	{NULL,NULL}
};


// TODO: SAFE FUNCTION SANDBOXING
/*
int safe_io_open(lua_State *L) {
    if (lua_gettop(L) < 1)
        return luaL_error(L, "[Lua] open: argument 1 (addr) is required");

    const char *relpath = luaL_checkstring(L, 1);

    const char *mode = luaL_optstring(L, 2, "r");

    char full_input_path[PATH_MAX];
    char resolved_path[PATH_MAX];

    snprintf(full_input_path, sizeof(full_input_path), "%s%s", CONTENT_DIR, relpath);

    if (!realpath(full_input_path, resolved_path)) {
        return luaL_error(L, "Invalid or inaccessible file");
    }

    if (strncmp(resolved_path, CONTENT_DIR, strlen(CONTENT_DIR)) != 0) {
        return luaL_error(L, "Access denied");
    }

    FILE *fp = fopen(resolved_path, mode);
    if (!fp)
        return luaL_error(L, "Could not open file");

    lua_pushlightuserdata(L, fp);  // you can return a real Lua file object instead
    return 1;
}

int safe_http_get(lua_State *L) {
    const char *url = luaL_checkstring(L, 1);
    if (!strstr(url, "https://api.yoursite.com/")) {
        return luaL_error(L, "URL not allowed");
    }

    // Do the HTTP request using your own HTTP client (e.g., curl)
    // Push result to Lua (response body)
    lua_pushstring(L, "<response>..."); // Replace with actual response
    return 1;
}
*/


lua_State *co = NULL;
char* lua_file = "";
        
void lua_init() {
    lua_State *L = luaL_newstate();
    lua_getglobal(L, "init");
    
    luaL_openlibs(L);   // Load full stdlib
    
    // disable unsafe functions
    /*
    lua_getglobal(L, "os");
    if (lua_istable(L, -1)) {
        lua_pushstring(L, "execute");
        lua_pushcfunction(L, disabled_func); // or lua_pushnil(L);
        lua_settable(L, -3);
    }
    lua_pop(L, 1); // pop os

    // Disable risky loaders
    lua_pushnil(L); lua_setglobal(L, "dofile");
    lua_pushnil(L); lua_setglobal(L, "loadfile");
    lua_pushnil(L); lua_setglobal(L, "require");
    lua_pushnil(L); lua_setglobal(L, "load");  // disables eval-like dynamic code
    */
    
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
#ifdef HAVE_CHEEVOS
    luaL_newlib(L, memorylib);
    lua_setglobal(L, "memory");
#endif
    luaL_newlib(L, guilib);
    lua_setglobal(L, "gui");
    //TODO: lua_register(L, "savestate", savestatelib);
    //TODO: lua_register(L, "movie", movielib);
    //TODO: lua_register(L, "input", inputlib);
    //TODO: lua_register(L, "comm", commlib); // http requests
    lua_settop(L, 0); // clean the stack, because each call to lua_register leaves a table on top

    // build current script name
    char *lua_file = strdup(path_get(RARCH_PATH_BASENAME));
    if (!lua_file) return;
    lua_file = realloc(lua_file, strlen(lua_file) + strlen(".lua") + 1);
    if (!lua_file) return;
    strcat(lua_file, ".lua");
    
    // Create a coroutine (needed to yield)
    co = lua_newthread(L);

    // Store the script in the coroutine
    luaL_loadfile(co, lua_file);

    free(lua_file);
}
    
void lua_loop() {
    //RARCH_LOG("[Lua] main loop\n");

    if (!co) lua_init();  // init once
    
    int status = lua_status(co);
    if(status != LUA_YIELD && status != LUA_OK)
        return;  // error or nothing to execute
    
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
    lua_close(co);
    co = NULL;
}
