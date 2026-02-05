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
#include <compat/strcasestr.h>

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
#ifdef HAVE_MENU
#include "menu/menu_input.h"
#include "menu/menu_driver.h"
#endif


// LUA API based on Bizhawk https://tasvideos.org/Bizhawk/LuaFunctions

// console.log()
// Outputs to the Retroarch debug console
int console_log(lua_State *L) {
    const char *msg = luaL_checkstring(L,1);
    RARCH_LOG("[Lua] %s\n", msg);
    return 0;  // void function
}


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
// returns the full path of the currently loaded rom, if a rom is loaded (can be a relative path)
int gameinfo_getrompath(lua_State *L) {
    const char *path = path_get(RARCH_PATH_CONTENT);
    const char *r = path ? path : "";  // fallback to empty string
    lua_pushfstring(L, "%s", r);
    return 1;
}

// bool savestate.loadslot(int slotnum, [bool suppressosd = False])
// Loads the savestate at the given slot number. Returns true if succeeded. 
int savestate_loadslot(lua_State *L) {
    int slotnum = luaL_checkinteger(L, 1);
    settings_t *settings            = config_get_ptr();
    configuration_set_int(settings, settings->ints.state_slot, slotnum);
    command_event(CMD_EVENT_LOAD_STATE, NULL);
    return 0; 
}

// void savestate.saveslot(int slotnum, [bool suppressosd = False])
// Saves a state at the given save slot. 
int savestate_saveslot(lua_State *L) {
    int slotnum = luaL_checkinteger(L, 1);
    settings_t *settings            = config_get_ptr();
    configuration_set_int(settings, settings->ints.state_slot, slotnum);
    command_event(CMD_EVENT_SAVE_STATE, NULL);
    return 0; 
}

// void savestate.save(string path, [bool suppressosd = False])
// Saves a state at the given path.
int savestate_save(lua_State *L) {
    const char *state_path = luaL_checkstring(L, 1);
    command_event(CMD_EVENT_SAVE_STATE_TO_RAM, NULL);
    command_event(CMD_EVENT_RAM_STATE_TO_FILE, state_path);
    return 0;
}

// bool savestate.load(string path, [bool suppressosd = False])
// Loads a savestate with the given path. Returns true if succeeded.
int savestate_load(lua_State *L) {
    const char *state_path = luaL_checkstring(L, 1);
    bool r = content_load_state(state_path, false, true);  /* Load a state from disk to memory. */
    //command_event(CMD_EVENT_LOAD_STATE_FROM_RAM, NULL);
    lua_pushboolean(L, r);
    return 1;
}


// bool client.ispaused()
// Returns true if emulator is paused, otherwise, false
int client_ispaused(lua_State *L) {
    runloop_state_t *runloop_st = runloop_state_get_ptr();
    int r = (runloop_st->flags & RUNLOOP_FLAG_PAUSED);
    lua_pushboolean(L, r);
    return 1;
}

// bool bizstring.contains(string str, string str2)
// Returns whether or not str contains str2
int bizstring_contains(lua_State *L) {
    const char *str = luaL_checkstring(L, 1);
    const char *str2 = luaL_checkstring(L, 2);
    if ( string_find_index_substring_string( str2, str ) == -1 )
        lua_pushboolean(L, false);
    else
        lua_pushboolean(L, true);   
    return 1;
}

// bool bizstring.endswith(string str, string str2)
// Returns whether str ends wth str2 (case-sensitive)
int bizstring_endswith(lua_State *L) {
    const char *str = luaL_checkstring(L, 1);
    const char *str2 = luaL_checkstring(L, 2);
    bool r = string_ends_with(str, str2);
    lua_pushboolean(L, r);
    return 1;
}

// bool bizstring.startswith(string str, string str2)
// Returns whether str starts with str2
int bizstring_startswith(lua_State *L) {
    const char *str = luaL_checkstring(L, 1);
    const char *str2 = luaL_checkstring(L, 2);
    bool r = string_starts_with(str, str2);
    lua_pushboolean(L, r);
    return 1;
}

// string bizstring.tolower(string str)
// Returns an lowercase version of the given string
int bizstring_tolower(lua_State *L) {
    char *str = luaL_checkstring(L, 1);
    string_to_lower(str);
    lua_pushstring(L, str);
    return 1;
}

// string bizstring.toupper(string str)
// Returns an uppercase version of the given string
int bizstring_toupper(lua_State *L) {
    char *str = luaL_checkstring(L, 1);
    string_to_upper(str);
    lua_pushstring(L, str);
    return 1;
}

// string bizstring.trim(string str)
// returns a string that trims whitespace on the left and right ends of the string
int bizstring_trim(lua_State *L) {
    const char *str = luaL_checkstring(L, 1);
    string_trim_whitespace(str);
    lua_pushstring(L, str);
    return 1;
} 
    

// bool client.emulating()
// Returns true if emulator is paused, otherwise, false
int client_emulating(lua_State *L) {
    runloop_state_t *runloop_st = runloop_state_get_ptr();
    int r = (runloop_st->flags & RUNLOOP_FLAG_PAUSED) ? false : true;
    lua_pushboolean(L, r);
    return 1;
}

// bool client.isturbo()
// Returns true if emulator is in turbo mode, otherwise, false
int client_isturbo(lua_State *L) {
    runloop_state_t *runloop_st = runloop_state_get_ptr();
    int r = (runloop_st->flags & RUNLOOP_FLAG_FASTMOTION);
    lua_pushboolean(L, r);
    return 1;
}

// int client.screenheight()
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

// int client.bufferheight()
// Gets the visible height of the emu display surface (the core video output). This excludes the gameExtraPadding you've set.
int client_bufferheight(lua_State *L) {
    video_driver_state_t *video_st    = video_state_get_ptr();
    unsigned r  = video_st->av_info.geometry.base_height;
    //ALT: unsigned r  = video_st->frame_cache_height;
    lua_pushinteger(L, (lua_Integer)r);
    return 1;
}

// int client.bufferwidth()
// Gets the visible width of the emu display surface (the core video output). This excludes the gameExtraPadding you've set.
int client_bufferwidth(lua_State *L) {
    video_driver_state_t *video_st    = video_state_get_ptr();
    unsigned r  = video_st->av_info.geometry.base_width;
    //ALT: unsigned r  = video_st->frame_cache_width;
    lua_pushinteger(L, (lua_Integer)r);
    return 1;
}

// string client.getversion()
// Returns the current stable Retroarch version
int client_getversion(lua_State *L) {
    lua_pushstring(L, PACKAGE_VERSION);
    return 1; 
}

// void client.pause()
// Pauses the emulator
int client_pause(lua_State *L) {
    command_event(CMD_EVENT_PAUSE, NULL);
    return 0; 
}

// void client.unpause()
// Unpauses the emulator
int client_unpause(lua_State *L) {
    command_event(CMD_EVENT_UNPAUSE, NULL);
    return 0; 
}

// void client.togglepause()
// Toggles the current pause state
int client_togglepause(lua_State *L) {
    command_event(CMD_EVENT_PAUSE_TOGGLE, NULL);
    return 0; 
}

// void client.exit()
// Closes the emulator
int client_exit(lua_State *L) {
    command_event(CMD_EVENT_QUIT, NULL);
    return 0; 
}

// void client.reboot_core()
// Reboots the currently loaded core
int client_reboot_core(lua_State *L) {
    command_event(CMD_EVENT_RESET, NULL);
    return 0; 
}

// void client.closerom()
// Closes the loaded Rom
int client_closerom(lua_State *L) {
    command_event(CMD_EVENT_CLOSE_CONTENT, NULL);
    return 0; 
}


// void client.screenshot([string path = nil])
// TODO: allow passing path
int client_screenshot(lua_State *L) {
    const char *path = luaL_optstring(L, 1, NULL); // optional first argument, defaults to NULL
    if(!path)
        command_event(CMD_EVENT_TAKE_SCREENSHOT, NULL);
    /*
    else
        take_screenshot(
          const char *screenshot_dir,
          const char *path, bool silence,
          bool has_valid_framebuffer, bool fullpath, true);
        */
    return 0; 
}

// void client.sleep(int millis)
// sleeps for n milliseconds
int client_sleep(lua_State *L) {
    lua_Integer ms = luaL_checkinteger(L, 1);
    if (ms < 0) return luaL_error(L, "emulation_sleep: time must be >= 0");
    retro_sleep((uint64_t)ms);
    return 0; 
}

// object client.getconfig()
// gets the current config settings object
// TODO: add more fields, should match the names used here https://github.com/TASEmulators/BizHawk/blob/master/src/BizHawk.Client.Common/config/Config.cs
int client_getconfig(lua_State *L) {
    settings_t *settings            = config_get_ptr();

    lua_newtable(L);

    lua_pushinteger(L, settings->ints.state_slot);
    lua_setfield(L, -2, "SaveSlot");

    //lua_pushnumber(L, settings->floats.video_font_size);
    //lua_setfield(L, -2, "FontSize");
    
    // TODO: more settings
    /*
    // Set another field, e.g., PauseOnFrame = true
    lua_pushboolean(L, 0);              // false
    lua_setfield(L, -2, "PauseOnFrame"); 
    */

    return 1;
}


// nluatable joypad.get([int? controller = nil])
// returns a lua table of the controller buttons pressed. If supplied, it will only return a table of buttons for the given controller
int joypad_get(lua_State *L) {
    // TODO: handle "controller" arg
    
    input_driver_state_t *input_st    = input_state_get_ptr();
    settings_t *settings = config_get_ptr();
    input_bits_t current_bits;
    BIT256_CLEAR_ALL_PTR(&current_bits);
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
    input_driver_t *current_input           = input_st->current_driver;
    
    char curr_keyname[64] = {0};

   // Keyboard scan
   for (unsigned key = 0; key < RETROK_LAST; key++)
   {
      input_keymaps_translate_rk_to_str(key, curr_keyname, 64);
      string_to_upper(curr_keyname);
  
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


bool using_hw_framebuffer() {
    /*
    video_driver_state_t *video_st = video_state_get_ptr();  
    if(video_st->frame_cache_data && (video_st->frame_cache_data == RETRO_HW_FRAME_BUFFER_VALID))
        return true;
    else
        return false;
        */

    // ALT from video_driver.h: video_driver_is_hw_context()
    //bool is_hw_context  = (video_st->hw_render.context_type != RETRO_HW_CONTEXT_NONE);
    //return is_hw_context;
    return video_driver_is_hw_context();  // TODO: replace all calls?
}


size_t check_memory_range(lua_State *L, const size_t start_address, const size_t len, const unsigned int domain) {

    runloop_state_t *runloop_st = runloop_state_get_ptr(); 
    size_t memsize = runloop_st->current_core.retro_get_memory_size(domain);
    
    if(memsize == 0 && domain == RETRO_MEMORY_ROM)
    {
        // fallback to frontend buffer (read-only)
        content_state_t *p_content = content_state_get_ptr();
        memsize = p_content->content_list->entries[0].data_size;    
    }

    if((start_address + len - 1) > memsize) {
        //RARCH_ERR("address out of bounds: %d > %d\n", address, memsize );
        return luaL_error(L, "memory address out of bounds");
    }
    
    return memsize;  // no error
}

size_t get_memory_address_arg(lua_State *L, const size_t BYTES_TO_READ, const unsigned int domain) {    
    size_t address = (size_t)luaL_checkinteger(L, 1);
    
    // check if the address is valid for the current core
    if(domain == RETRO_MEMORY_ROM)
    {
        content_state_t *p_content = content_state_get_ptr();
        if (!p_content || !p_content->content_list || p_content->content_list->size == 0)
            return luaL_error(L, "Content is not loaded in RAM");
    }
        
    check_memory_range(L, address, BYTES_TO_READ, domain);
    
    // else 
    return address;
}

uint8_t* get_memory_ptr(lua_State *L, const unsigned int domain) {
    uint8_t *data = (uint8_t *) runloop_state_get_ptr()->current_core.retro_get_memory_data(domain);  // core-provided pointer
        
    if(!data && domain == RETRO_MEMORY_ROM) {   
        // fallback to frontend buffer (read-only)
        data = (uint8_t *) content_state_get_ptr()->content_list->entries[0].data;
    }

    /* TODO: try retro_memory_map
    // RC_MEMORY_TYPE_READONLY, "Cartridge ROM"
      for (i = 0; i < console_regions->num_regions; ++i) {
    const rc_memory_region_t* console_region = &console_regions->region[i];
    const size_t console_region_size = console_region->end_address - console_region->start_address + 1;
    const uint32_t type = rc_libretro_memory_console_region_to_ram_type(console_region->type);
    uint32_t base_address = 0;
    #endif
    */
    
    if (!data) luaL_error(L, "unable to access memory");
    return data;
}


unsigned int get_memory_domain_arg(lua_State *L, const int DOMAIN_ARG_POS) {
    unsigned int domain = RETRO_MEMORY_SYSTEM_RAM;  // default
    if (lua_gettop(L) >= DOMAIN_ARG_POS ) {  // 3 for write functions, 2 for read functions
            const char *domain_str = luaL_checkstring(L, DOMAIN_ARG_POS);  // domain arg passed
            if(strcasecmp(domain_str, "RAM")==0 || strcasecmp(domain_str, "WRAM")==0 || strcasecmp(domain_str, "Main Memory")==0) {
                domain = RETRO_MEMORY_SYSTEM_RAM;
            }
            else if(strcasestr(domain_str, "VRAM") != NULL) {  // also matches "VRAM1"
                domain = RETRO_MEMORY_VIDEO_RAM;
            }
            else if(strcasecmp(domain_str, "ROM")==0 || strcasecmp(domain_str, "CARTROM")==0) {
                domain = RETRO_MEMORY_ROM;
            }
            else if(strcasecmp(domain_str, "SaveRAM")==0 || strcasecmp(domain_str, "Battery RAM")==0 || strcasecmp(domain_str, "CARTRAM")==0) {
                domain = RETRO_MEMORY_SAVE_RAM;
            }
            else if(strcasecmp(domain_str, "RTC")==0) {
                domain = RETRO_MEMORY_RTC;
            } else {
                return luaL_error(L, "unsupported memory domain");
            }
    }
    if(domain == RETRO_MEMORY_VIDEO_RAM && using_hw_framebuffer()) {
        return luaL_error(L, "cannot access hardware framebuffer");
    }
    return domain;
}

int get_memory_value(lua_State *L, const int BYTES_TO_READ, bool with_sign, bool big_endian) {
    unsigned int domain = get_memory_domain_arg(L, 2);
    size_t address = get_memory_address_arg(L, BYTES_TO_READ, domain);
    uint8_t *data = get_memory_ptr(L, domain);
    
    int value = 0;
    
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


// rom.readbyte(int address)
// Get an unsigned byte from the actual ROM file at the given address.  
// TODO: handle content with multiple entries?
int rom_readbyte(lua_State *L) {
    lua_pushstring(L, "ROM");  // add domain arg
    return get_memory_value(L, 1, false, false);
}

// void memory.writebyte(long addr, uint value, [string domain = nil])
// Writes the given value to the given address as an unsigned byte
int memory_writebyte(lua_State *L) {
    const unsigned int domain = get_memory_domain_arg(L, 3);
    const size_t address = get_memory_address_arg(L, 1, domain);
    unsigned int value = (unsigned int)luaL_checkinteger(L, 2);
    uint8_t *data = get_memory_ptr(L, domain);
    
    *(data + address) = (uint8_t)value;
    return 0;
}

// void memory.write_bytes_as_array(long addr, nluatable bytes, [string domain = nil])
// Writes sequential bytes starting at addr.
int memory_write_bytes_as_array(lua_State *L) {
    const unsigned int domain = get_memory_domain_arg(L, 3);
    const size_t address = get_memory_address_arg(L, 1, domain);
    uint8_t *data = get_memory_ptr(L, domain);
    
    luaL_checktype(L, 2, LUA_TTABLE);
    size_t len = lua_rawlen(L, 2); // Number of elements in the table
    check_memory_range(L, address, len, domain);  // abort if too long
    
    for (size_t i = 1; i <= len; i++)
    {
        lua_rawgeti(L, 2, i); // Push table[i] on the stack
        int byte = luaL_checkinteger(L, -1);
        lua_pop(L, 1);
        data[address + i - 1] = (uint8_t)byte;  // truncate if necessary
    }

    return 0;
}

// nluatable memory.readbyterange(long addr, int length, [string domain = nil])
// Reads the address range that starts from address, and is length long. Returns a zero-indexed table containing the read values (an array of bytes.)
int memory_readbyterange(lua_State *L) {
    const unsigned int domain = get_memory_domain_arg(L, 3);
    unsigned length = (unsigned)luaL_checkinteger(L, 2);
    const size_t address = get_memory_address_arg(L, length, domain);      
    const uint8_t *data = get_memory_ptr(L, domain);
    
    check_memory_range(L, address, length, domain);
    
    lua_newtable(L);
    for (unsigned int i = 0; i < length; i++) {
        lua_pushfstring(L, "%d", i );
        lua_pushfstring(L, "%d", (uint8_t)*(data+address+i));
        lua_settable(L, -3);
    }
    return 1;
}


// nluatable memory.getmemorydomainlist()
// Returns a string of the memory domains for the loaded platform core. List will be a single string delimited by line feeds
int memory_getmemorydomainlist(lua_State *L) {
    const unsigned int domains_list_values[] = { RETRO_MEMORY_SAVE_RAM, RETRO_MEMORY_RTC, RETRO_MEMORY_SYSTEM_RAM, RETRO_MEMORY_VIDEO_RAM, RETRO_MEMORY_ROM  };
    const char* domains_list_names[] = { "Battery RAM", "RTC", "RAM", "VRAM", "ROM" };
    const unsigned domain_count = sizeof(domains_list_values) / sizeof(domains_list_values[0]);
    // create return table
    lua_newtable(L);
    int table_index = 1;

    for (unsigned int i = 0; i < domain_count; i++)
    {
        unsigned int domain = domains_list_values[i];

        //if( get_memory_ptr(L, domain) != NULL ) // throw errors
        if( runloop_state_get_ptr()->current_core.retro_get_memory_data(domain) )
        {
            // current domain is supported for current core
            lua_pushinteger(L, table_index++);
            lua_pushstring(L, domains_list_names[i]);
            lua_settable(L, -3);
        }
    }
    
   return 1;
}


// uint memory.getmemorydomainsize([string name = ])
// Returns the number of bytes of the specified memory domain. If no domain is specified, or the specified domain doesn't exist, returns the current domain size
int memory_getmemorydomainsize(lua_State *L) {
   const unsigned int domain = get_memory_domain_arg(L, 1);
   lua_pushinteger(L, (lua_Integer)check_memory_range(L, 1, 1, domain));  // lua_pushunsigned is deprecated
   return 1;
}


// string memory.hash_region(long addr, int count, [string domain = nil])
// Returns a hash as a string of a region of memory, starting from addr, through count bytes. If the domain is unspecified, it uses the current region.
// Bizhawk currently uses sha256, so we do the same
int memory_hash_region(lua_State *L) {
    const unsigned int domain = get_memory_domain_arg(L, 3);
    unsigned length = (unsigned)luaL_checkinteger(L, 2);
    const size_t address = get_memory_address_arg(L, length, domain);
    uint8_t *data = get_memory_ptr(L, domain);
    
    check_memory_range(L, address, length, domain);
    
    char out_hash[256] = {0};
    sha256_hash(out_hash, data+address, length);
    string_to_upper(out_hash);
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
int emu_getsystemid(lua_State *L) {
    core_info_t *core_info      = NULL;
    core_info_get_current_core(&core_info);
    char* sysid = core_info->system_id;  // read only, could be NULL
    if(sysid) {
        sysid = strdup(sysid);  // TODO: never freed
        // make sure it is in uppercase ("nes"->"NES")
        string_to_upper(sysid);
        // try to match Bizhawk names
        if (string_is_equal(sysid, "super_nes")) sysid = "SNES";
        if (string_is_equal(sysid, "mega_drive")) sysid = "GEN";
        if (string_is_equal(sysid, "master_system")) sysid = "SMS";
        if (string_is_equal(sysid, "game_boy")) sysid = "GB";
        if (string_is_equal(sysid, "game_boy_advance")) sysid = "GBA";
        if (string_is_equal(sysid, "pc_engine")) sysid = "PCE";
        if (string_is_equal(sysid, "sega_saturn")) sysid = "SAT";
        if (string_is_equal(sysid, "playstation")) sysid = "PSX";
        if (string_is_equal(sysid, "commodore_64")) sysid = "C64";
        if (string_is_equal(sysid, "nintendo_64")) sysid = "N64";
        if (string_is_equal(sysid, "virtual_boy")) sysid = "VB";
        if (string_is_equal(sysid, "wonderswan")) sysid = "WSWAN";
        if (string_is_equal(sysid, "neo_geo_pocket")) sysid = "NGP";
        // TODO: more matches
    }
    lua_pushstring(L, sysid ? sysid : "");
    return 1;
}

#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
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
#endif

// emu.getscreenpixel(int x, int y, bool getemuscreen)
// Returns the separate RGB components of the given screen pixel, and the palette. Can be 0-255 by 0-239, but NTSC only displays 0-255 x 8-231 of it.
// TODO (currently ignored): If getemuscreen is false, this gets background colors from either the screen pixel or the LUA pixels set, but LUA data may not match the information used to put the data to the screen. If getemuscreen is true, this gets background colors from anything behind an LUA screen element.
// Usage is local r,g,b,palette = emu.getscreenpixel(5, 5, false) to retrieve the current red/green/blue colors and palette value of the pixel at 5x5.
// The "palette" value is actually the 32-bit pixel value.
int emu_getscreenpixel(lua_State *L) {
    if(using_hw_framebuffer()) 
        return luaL_error(L, "cannot read hardware framebuffer");
        
    unsigned x = luaL_checkinteger(L, 1);
    unsigned y = luaL_checkinteger(L, 2);

    video_driver_state_t *video_st = video_state_get_ptr();
    const void *frame = (const void*)video_st->frame_cache_data;
    unsigned width  = video_st->frame_cache_width;
    unsigned height = video_st->frame_cache_height;
    size_t pitch    = video_st->frame_cache_pitch;
    
    // Bounds-check
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


// void gui.addmessage(string message)
// Adds a message to the OSD's message area
int gui_addmessage(lua_State *L) {
    const char *msg = luaL_checkstring(L,1);
    runloop_msg_queue_push(msg, strlen(msg), 1, 180, false, NULL,
          MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
    return 0; 
}


#ifdef HAVE_GFX_WIDGETS

enum gui_shape_type
{
   SHAPE_UNUSED = 0,
   SHAPE_TEXT,
   SHAPE_PIXELTEXT,
   SHAPE_RECT
   // TODO: line, circle, image, etc.
};

/* TODO: IMAGE
            image_type = IMAGE_TYPE_PNG;

         if (!gfx_widgets_ai_service_overlay_load(
               raw_image_file_data, (unsigned)new_image_size,
               image_type))
*/

typedef struct gui_shape
{
    enum gui_shape_type type;
    unsigned x;
    unsigned y;
    uint32_t color;
    uint32_t bg_color;
    unsigned width;
    unsigned height;
    char *text;
    //font_data_t *font;
    gfx_widget_font_data_t font;
    bool convert_coords;
} gui_shape_t;

#define LUA_MAX_SHAPES_ONSCREEN 256
gui_shape_t gui_shapes[ LUA_MAX_SHAPES_ONSCREEN ] = {0};  // static memory buffer of shapes currently onscreen

unsigned gui_shapes_curr_index = 0;


// void gui.clearGraphics([string surfacename = nil])
// clears all lua drawn graphics from the screen
int gui_clearGraphics(lua_State *L) {
    gui_shapes_curr_index = 0;  // reset the index
    for(int i=0; i<LUA_MAX_SHAPES_ONSCREEN; i++)
        gui_shapes[i].type = SHAPE_UNUSED;  // set as unused
    return 0; 
}


static unsigned convert_to_screen_space(unsigned x, unsigned y,
                                        unsigned width, unsigned height )
{
    
    struct video_viewport vp = {0};
    video_driver_get_viewport_info(&vp);
    //unsigned SCREEN_PADDING_X = 3;
    //const unsigned SCREEN_PADDING_X   = dispwidget_get_ptr()->msg_queue_rect_start_x;
    //const unsigned SCREEN_PADDING_X  = dispwidget_get_ptr()->simple_widget_padding;

    video_driver_state_t *video_st = video_state_get_ptr();
    unsigned fb_w = video_st->av_info.geometry.base_width;
    unsigned fb_h = video_st->av_info.geometry.base_height;

    //RARCH_LOG("screen: %u %u \n", fb_w, fb_h ) ; 
    //RARCH_LOG("viewport: %u %u %u %u %u %u \n", vp.x , vp.y, vp.width , vp.height,vp.full_width, vp.full_height ) ; 

/*
   gfx_display_t *p_disp      = disp_get_ptr();
      unsigned fb_width            = p_disp->framebuf_width;
      unsigned fb_height           = p_disp->framebuf_height;
  */
  
  // TODO: try as alternative:
  /*
  video_driver_translate_coord_viewport(
      struct video_viewport *vp,
      int mouse_x,           int mouse_y,
      int16_t *res_x,        int16_t *res_y,
      int16_t *res_screen_x, int16_t *res_screen_y,
      bool report_oob)
    */
          
    if (x)
        return vp.x + (unsigned)((float)x / fb_w * vp.width);
    if (y)
        return vp.y + (unsigned)((float)y / fb_h * vp.height);  // sy -= curr_shape->font->ascent
    if (width)
        return (unsigned)((float)width / fb_w * vp.width);
    if (height)
        return (unsigned)((float)height / fb_h * vp.height);

    return 0;
}


// nluatable client.transformPoint(int x, int y)
// Transforms a point (x, y) in emulator space to a point in client space
int client_transformPoint(lua_State *L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);

    unsigned video_width;  // video_st->width;
    unsigned video_height;  // video_st->height;
    video_driver_get_size(&video_width, &video_height);
   
    video_driver_state_t *video_st = video_state_get_ptr();  
    const unsigned buffer_width = video_st->av_info.geometry.base_width;
    const unsigned buffer_height = video_st->av_info.geometry.base_height;
    
    //const float aspect_ratio = video_driver_get_aspect_ratio();
    video_viewport_t vp = {0}; 
    video_driver_get_viewport_info(&vp);
    
    // update coords
    x = convert_to_screen_space(1+x, 0, 0, 0);
    y = convert_to_screen_space(0, 1+y, 0, 0);
    
    // populate return table
    lua_newtable(L);
    
    char coord[16];
    snprintf(coord, sizeof(coord), "%d", x);
    lua_pushstring(L, "x");
    lua_pushstring(L, coord);
    lua_settable(L, -3);
    
    snprintf(coord, sizeof(coord), "%d", y);
    lua_pushstring(L, "y");
    lua_pushstring(L, coord);
    lua_settable(L, -3);

    return 1;
}


void lua_draw_gfxs_loop() {
    // disable drawing when inside the menu
#ifdef HAVE_MENU
      bool menu_open = menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE;
      if (menu_open) return;
#endif
    
    //dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();
    //bool widgets_active            = p_dispwidget->active;
    // TODO :Check if active
    
    //video_driver_state_t *video_st = video_state_get_ptr();     
    void *userdata                   = video_driver_get_ptr();
    //void *userdata = VIDEO_DRIVER_GET_PTR_INTERNAL(video_st);
    gfx_display_t *p_disp      = disp_get_ptr();
   
    unsigned video_width;  // video_st->width;
    unsigned video_height;  // video_st->height;
    video_driver_get_size(&video_width, &video_height);
   
    //const unsigned buffer_width = video_st->av_info.geometry.base_width;
    //const unsigned buffer_height = video_st->av_info.geometry.base_height;
    
    //const float aspect_ratio = video_driver_get_aspect_ratio();
    
    //video_viewport_t vp = {0}; 
    //video_driver_get_viewport_info(&vp);
    
    // temp workaround to make sure the viewport offset is reported correctly on the 1st call (needs a better solution) https://github.com/libretro/RetroArch/issues/6454#issuecomment-3460354990
    gfx_display_draw_text(
        NULL,
        ".",
        convert_to_screen_space(10, 0, 0, 0),
        convert_to_screen_space(0, 10, 0, 0), //  - (2 * curr_shape->font->size)
        video_width, video_height,
        0xFFFFFF11,  // semi-invisible via alpha
        TEXT_ALIGN_LEFT,
        1.0f, false, 0.0f, true);
    
    const float DEFAULT_FONT_SIZE = config_get_ptr()->floats.video_font_size;
    
    // Iterate over the shapes to draw
    for(int i=0; i<LUA_MAX_SHAPES_ONSCREEN; i++) {
        
        gui_shape_t* curr_shape = &gui_shapes[i];
        
        switch (curr_shape->type)
        {
            case SHAPE_UNUSED:
            {
                //return;  // fastest, but may skip some shapes if the buffer is full
                continue;
            }
            
            case SHAPE_TEXT:
            case SHAPE_PIXELTEXT:
            {
                if(string_is_empty(curr_shape->text)) // empty string
                    continue;
                   
                gfx_display_draw_text(
                    curr_shape->font.font,
                    curr_shape->text,
                    curr_shape->convert_coords ? convert_to_screen_space(1+curr_shape->x, 0, 0, 0) : (curr_shape->x),
                    (curr_shape->convert_coords ? convert_to_screen_space(0, 1+curr_shape->y, 0, 0) : (curr_shape->y)) 
                        + (curr_shape->font.font ? curr_shape->font.font->size : DEFAULT_FONT_SIZE),
                    video_width, video_height,
                    curr_shape->color,
                    TEXT_ALIGN_LEFT,
                    1.0f, false, 0.0f, true);
                                
                /*
                gfx_widgets_draw_text(
                     &(curr_shape->font),
                     curr_shape->text,
                     curr_shape->convert_coords ? convert_to_screen_space(1+curr_shape->x, 0, 0, 0) : (curr_shape->x),
                     (curr_shape->convert_coords ? convert_to_screen_space(0, 1+curr_shape->y, 0, 0) : (curr_shape->y))
                        + (curr_shape->font.font ? curr_shape->font.font->size : DEFAULT_FONT_SIZE),
                     video_width, video_height,
                     curr_shape->color,
                     TEXT_ALIGN_LEFT,
                     true);  // draw_outside
                */

                break;
            }
            
            case SHAPE_RECT:
            {
                if(curr_shape->width==0 || curr_shape->height==0)  // nothing to draw
                    continue;
                    
                // color conversion
                uint32_t rgb = curr_shape->bg_color >> 8;  // shift out the last byte (alpha)
                float alpha = (curr_shape->bg_color & 0xFF) / 255.0f; // extract alpha and normalize
                float curr_quad_bg_color[16] = COLOR_HEX_TO_FLOAT(rgb, alpha);
                
                if(alpha == 0.0f) { // nothing to draw
                    //RARCH_LOG("rect alpha is 0\n");
                    continue;
                }
                
                // TODO: handle the "line" color
        
                gfx_display_draw_quad(
                     p_disp,
                     userdata,
                     video_width, video_height,
                     curr_shape->convert_coords ? convert_to_screen_space(1+curr_shape->x, 0, 0, 0) : (curr_shape->x),
                     curr_shape->convert_coords ? convert_to_screen_space(0, 1+curr_shape->y, 0, 0) : (curr_shape->y),
                     convert_to_screen_space(0, 0, 1+curr_shape->width, 0),
                     convert_to_screen_space(0, 0, 0, 1+curr_shape->height),
                     video_width, video_height,
                     curr_quad_bg_color,
                     NULL);
                     
                break;
            }
        }
    }
}


uint32_t read_color_arg(lua_State *L, const int ARG_NO, const uint32_t DEFAULT_COLOR) {
    if (lua_isnoneornil(L, ARG_NO))
    {
        // Argument is missing, using default value
        return DEFAULT_COLOR;
    }   
    else if (lua_type(L, ARG_NO) == LUA_TNUMBER)
    {
        // Integer argument received
        // convert 0xAARRGGBB → 0xRRGGBBAA
        uint32_t i = lua_tointeger(L, 1);
        uint8_t a = (i >> 24) & 0xFF;
        uint8_t r = (i >> 16) & 0xFF;
        uint8_t g = (i >> 8)  & 0xFF;
        uint8_t b = i & 0xFF;

        //if (a == 0) RARCH_LOG("WARNING: passed alpha is 0\n");

        // reorder: RRGGBBAA
        return (r << 24) | (g << 16) | (b << 8) | a;
    }
    else if (lua_type(L, ARG_NO) == LUA_TSTRING)
    {
        const char *color_str = lua_tostring(L, ARG_NO);
        
        switch (tolower(color_str[0])) // switch on first character
        {
            case 'b':
                if (strcasecmp(color_str, "black") == 0) return 0x000000FF;
                else if (strcasecmp(color_str, "blue") == 0) return 0x0000FFFF;
                break;
            case 'w':
                if (strcasecmp(color_str, "white") == 0) return 0xFFFFFFFF;
                break;
            case 'r':
                if (strcasecmp(color_str, "red") == 0) return 0xFF0000FF;
                break;
            case 'g':
                if (strcasecmp(color_str, "green") == 0) return 0x00FF00FF;
                break;
            case 'p':
                if (strcasecmp(color_str, "pink") == 0) return 0xFFC0CBFF;
                break;
            case 'y':
                if (strcasecmp(color_str, "yellow") == 0) return 0xFFFF00FF;
                break;
            case '#':
            {
                // parse html-style color:  "#RRGGBB" or "#AARRGGBB";
                size_t len = strlen(color_str);
                unsigned int a=0,r=0,g=0,b=0;

                if (len == 7)
                {
                    if (sscanf(color_str+1,"%02x%02x%02x",&r,&g,&b)!=3)
                        return luaL_error(L,"invalid hex color '%s'",color_str);
                    a = 0xFF;
                }
                else if (len == 9)
                {
                    if (sscanf(color_str+1,"%02x%02x%02x%02x",&a,&r,&g,&b)!=4)
                        return luaL_error(L,"invalid hex color '%s'",color_str);
                }
                else
                {
                    return luaL_error(L,"invalid hex color length '%s'", color_str);
                }

                //if (a==0) RARCH_LOG("WARNING: passed alpha is 0\n");
                return (r<<24)|(g<<16)|(b<<8)|a;
            }
            default:
            {
                return luaL_error(L, "invalid color string arg");
            }
        }
    }
    
    // else
    return luaL_error(L, "invalid color arg type");
}


int gui_drawPixelText_impl(lua_State *L, bool convert_coords) {
    if(using_hw_framebuffer()) 
        return luaL_error(L, "cannot draw on hardware framebuffer");
    
    gui_shape_t* curr_shape = &gui_shapes[gui_shapes_curr_index];
    
    curr_shape->type = SHAPE_PIXELTEXT;
    
    curr_shape->x = luaL_checkinteger(L, 1);
    curr_shape->y = luaL_checkinteger(L, 2);
    
    if(curr_shape->text) { // free prev string
        free(curr_shape->text);
        curr_shape->text = NULL;
    }
    curr_shape->text = strdup(luaL_checkstring(L, 3));

    curr_shape->color = read_color_arg(L, 4, 0xFFFFFFFF); // default white, fully opaque
    curr_shape->bg_color = read_color_arg(L, 5, 0x000000FF); // default black, fully opaque
    
    //curr_shape->font_face = luaL_optstring(L, 6, "");  // unused for drawPixelText
    if(curr_shape->font.font && curr_shape->font.font != dispwidget_get_ptr()->gfx_widget_fonts.regular.font) {  // TODO: better comparison
        free(curr_shape->font.font);  // free custom font
    }
    //curr_shape->font = dispwidget_get_ptr()->gfx_widget_fonts.regular;
    curr_shape->font.font = NULL;  // TODO: force using bitmap font
    
    curr_shape->convert_coords = convert_coords;
    
    // increase curr shape index
    gui_shapes_curr_index += 1;
    if( gui_shapes_curr_index == LUA_MAX_SHAPES_ONSCREEN) 
        gui_shapes_curr_index = 0;  // cycle back to 0

    return 0;
}

// void gui.pixelText(int x, int y, string message, [luacolor forecolor = nil], [luacolor backcolor = nil], [string fontfamily = nil], [string surfacename = nil])
// Draws the given message in the emulator screen space (like all draw functions) at the given x,y coordinates and the given color. The default color is white. Two font families are available, "fceux" and "gens" (or "0" and "1" respectively), both are monospace and have the same size as in the emulators they've been taken from.
// NOTE: multiple strings can be onscreen at a time.
int gui_drawPixelText(lua_State *L) {
    return gui_drawPixelText_impl(L, true);
}
int gui_drawPixelTextO(lua_State *L) {
    return gui_drawPixelText_impl(L, false);
}


int gui_drawString_impl(lua_State *L, bool convert_coords) {
    if(using_hw_framebuffer()) 
        return luaL_error(L, "cannot draw on hardware framebuffer");
    
    gui_shape_t* curr_shape = &gui_shapes[gui_shapes_curr_index];
    
    curr_shape->type = SHAPE_TEXT;
        
    curr_shape->x = luaL_checkinteger(L, 1);
    curr_shape->y = luaL_checkinteger(L, 2);
    
    if(curr_shape->text) { // free prev string
        free(curr_shape->text);
        curr_shape->text = NULL;
    }
    curr_shape->text = strdup(luaL_checkstring(L, 3));

    curr_shape->color = read_color_arg(L, 4, 0xFFFFFFFF); // default white, fully opaque
    curr_shape->bg_color = read_color_arg(L, 5, 0x000000FF); // default black, fully opaque
     
    // default font
    dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();

    // apply font and scaling
    settings_t *settings            = config_get_ptr();
    //const float DEFAULT_FONT_SIZE = 32.0f;  // BASE_FONT_SIZE as defined in gfx_widgets.c
    const float DEFAULT_FONT_SIZE = settings->floats.video_font_size;
    //RARCH_LOG("DEFAULT_FONT_SIZE: %f\n", DEFAULT_FONT_SIZE);
    const char* DEFAULT_FONT_FACE = settings->paths.path_font;  // defaults to ""
    //RARCH_LOG("DEFAULT_FONT_FACE: %s\n", DEFAULT_FONT_FACE);
    
    float font_size = luaL_optinteger(L, 6, DEFAULT_FONT_SIZE);
    char* font_face = luaL_optstring(L, 7, DEFAULT_FONT_FACE);
    //char* font_face = luaL_optstring(L, 7, "");
    
    if(curr_shape->font.font && curr_shape->font.font != p_dispwidget->gfx_widget_fonts.regular.font) {  // TODO: better comparison
        free(curr_shape->font.font);  // free custom font
        //gfx_widgets_font_free(curr_shape->font);
    }
    curr_shape->font.font = NULL;
    
    // font size dpi-aware
    // MEMO: scale_factor = dpi / REFERENCE_DPI = dpi / 96.0f;
    //float dpi_scale             = dispwidget_get_ptr()->last_scale_factor;
    //RARCH_LOG("detected dpi: %f\n", dpi_scale);   
    //menu_handle_t *menu  = menu_state_get_ptr()->driver_data;
    //video_driver_state_t *video_st = video_state_get_ptr();     
    //float dpi = menu_input_get_dpi(menu, disp_get_ptr(), video_st->width, video_st->height);
    //float last_scale_factor                     = gfx_display_get_dpi_scale(disp_get_ptr(), config_get_ptr(), video_st->width, video_st->height, false, false);
    //RARCH_LOG("last_scale_factor: %f\n", last_scale_factor);   
         
    //RARCH_LOG("path_font: %s\n", font_face);
    //RARCH_LOG("video_font_size: %f\n", font_size);
    
    //gfx_widget_font_data_t *font_regular
    
    //if(!string_is_empty(font_face) || font_size!=DEFAULT_FONT_SIZE) {
    if(strcasecmp(font_face, DEFAULT_FONT_FACE)!=0 || font_size!=DEFAULT_FONT_SIZE) {
        char fontpath[PATH_MAX_LENGTH] = {0};
        if(!string_is_empty(font_face))
        {
            static const char *font_path_prefix = {
                #if defined(_WIN32)
                   "C:\\Windows\\Fonts\\"
                #elif defined(__APPLE__)
                   "/Library/Fonts/"
                #elif defined(__ANDROID_API__)
                   "/system/fonts/"
                #elif defined(WEBOS)
                  "/usr/share/fonts/"
                #else
                  "/usr/share/fonts/TTF/"
                #endif
                   ""
            };
            fill_pathname_join_special(fontpath, font_path_prefix, font_face, sizeof(fontpath));
            strcat(fontpath, ".ttf");
        }

        curr_shape->font.font = gfx_display_font_file(disp_get_ptr(), fontpath, font_size, video_driver_is_threaded());
        if(curr_shape->font.font == NULL) RARCH_ERR("cannot load font: %s\n", font_face);
        // TODO: need to init other fields?
    }
    
    if(curr_shape->font.font == NULL) {
        // fallback to the default font
        curr_shape->font = p_dispwidget->gfx_widget_fonts.regular;  // copy all struct fields
    }

    // adjust y coord padding?
    //unsigned widget_padding = dispwidget_get_ptr()->simple_widget_padding;
    //curr_shape->y += widget_padding;
    //curr_shape->y += (curr_shape->font_size);
    //curr_shape->y += DEFAULT_FONT_SIZE;
    
    curr_shape->convert_coords = convert_coords;
    
    // increase curr shape index
    gui_shapes_curr_index += 1;
    if( gui_shapes_curr_index == LUA_MAX_SHAPES_ONSCREEN) 
        gui_shapes_curr_index = 0;  // cycle back to 0

    return 0;
}


// void gui.drawString(int x, int y, string message, [luacolor forecolor = nil], [luacolor backcolor = nil], [int? fontsize = nil], [string fontfamily = nil]
//      , [string fontstyle = nil], [string horizalign = nil], [string vertalign = nil], [string surfacename = nil])
// Draws the given message in the emulator screen space (like all draw functions) at the given x,y coordinates and the given color. The default color is white. A fontfamily can be specified and is monospace generic if none is specified (font family options are the same as the .NET FontFamily class). The fontsize default is 12. The default font style is regular. Font style options are regular, bold, italic, strikethrough, underline. Horizontal alignment options are left (default), center, or right. Vertical alignment options are bottom (default), middle, or top. Alignment options specify which ends of the text will be drawn at the x and y coordinates. For pixel-perfect font look, make sure to disable aspect ratio correction.
int gui_drawString(lua_State *L) {
    return gui_drawString_impl(L, true);
}
int gui_drawStringO(lua_State *L) {
    return gui_drawString_impl(L, false);
}


int gui_drawRectangle_impl(lua_State *L, bool convert_coords) {
    if(using_hw_framebuffer()) 
        return luaL_error(L, "cannot draw on hardware framebuffer");
        
    gui_shape_t* curr_shape = &gui_shapes[gui_shapes_curr_index];
    
    curr_shape->type = SHAPE_RECT;
        
    curr_shape->x = luaL_checkinteger(L, 1);
    curr_shape->y = luaL_checkinteger(L, 2);
    curr_shape->width = luaL_checkinteger(L, 3);
    curr_shape->height = luaL_checkinteger(L, 4);
    
    if(curr_shape->text) { // free prev string
        free(curr_shape->text);
        curr_shape->text = NULL;
    }
    
    curr_shape->color = read_color_arg(L, 5, 0xFFFFFFFF); // default white, fully opaque
    curr_shape->bg_color = read_color_arg(L, 6, 0x000000FF); // default black, fully opaque
    
    curr_shape->convert_coords = convert_coords;
    
    // increase curr shape index
    gui_shapes_curr_index += 1;
    if( gui_shapes_curr_index == LUA_MAX_SHAPES_ONSCREEN) 
        gui_shapes_curr_index = 0;  // cycle back to 0

    return 0;
}


// void gui.drawRectangle(int x, int y, int width, int height, [luacolor line = nil], [luacolor background = nil], [string surfacename = nil])
// Draws a rectangle at the given coordinate and the given width and height. Line is the color of the box. Background is the optional fill color
int gui_drawRectangle(lua_State *L) {
    return gui_drawRectangle_impl(L, true);
}

int gui_drawRectangleO(lua_State *L) {
    return gui_drawRectangle_impl(L, false);
}


// void gui.drawPixel(int x, int y, [luacolor color = nil], [string surfacename = nil])
// Draws a single pixel at the given coordinates in the given color. Color is optional (if not specified it will be drawn black)
// Luacolor must be a 32-bit number in the format 0xAARRGGBB;
int gui_drawPixel(lua_State *L) {
    // this is just a wrapper to gui_drawRectangle

    unsigned x = luaL_checkinteger(L, 1);
    unsigned y = luaL_checkinteger(L, 2);
    uint32_t bg_color = read_color_arg(L, 3, 0xFFFFFFFF);
    
    lua_settop(L, 0);  // Clear stack completely

    // Push arguments for gui_drawRectangle
    lua_pushinteger(L, x);        // x
    lua_pushinteger(L, y);        // y
    lua_pushinteger(L, 1);        // width  // TODO: need to adjust to screen size
    lua_pushinteger(L, 1);        // height  // TODO: need to adjust to screen size
    lua_pushnil(L);             // color = nil
    lua_pushinteger(L, bg_color);    // default white, fully opaque

    // Call rectangle drawer
    gui_drawRectangle(L);
    
    return 0;
}

#endif


#ifdef LUA_SCRIPTS_SANDBOXED
void check_safe_url(lua_State *L, url) {
    if (!string_starts_with("http://localhost") && !string_starts_with("https://localhost"))
        return luaL_error(L, "cannot send HTTP request to remote domain due to sandboxing enabled");
}
#endif


// string comm.httpGet(string url)
// makes a HTTP GET request
int comm_httpget(lua_State *L) {
    const char *url = luaL_checkstring(L,1);
#ifdef LUA_SCRIPTS_SANDBOXED 
    check_safe_url(L, url);
#endif
    // TODO: allow passing headers: task_push_http_transfer_with_headers(...)
    void* t = task_push_http_transfer(url, true, "GET", NULL, NULL);
    if(!t) return luaL_error(L, "cannot send HTTP request");
    // TODO: blocking request, read the response body and return as a string, see in task_core_updater.c
    lua_pushstring(L, "OK");
    return 1;
}

// string comm.httpPost(string url, string payload)
// makes a HTTP POST request
int comm_httppost(lua_State *L) {
    const char *url = luaL_checkstring(L,1);
#ifdef LUA_SCRIPTS_SANDBOXED 
    check_safe_url(L, url);
#endif
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
#ifdef LUA_SCRIPTS_SANDBOXED 
    check_safe_url(L, url);
#endif
    const char *payload = luaL_checkstring(L,2);
    void* t = task_push_http_post_transfer(url, payload, true, "PUT", NULL, NULL);
    if(!t) return luaL_error(L, "cannot send HTTP request");
    lua_pushstring(L, "OK");  // TODO: return body to the caller?   
    return 1;
}


static const struct luaL_Reg  consolelib[] = {
    { "log" , console_log },
    { "writeline" , console_log },
 	{NULL,NULL}
};

static const struct luaL_Reg  gameinfolib[] = {
    //{ "getboardtype" , gameinfo_getboardtype },
    { "getromhash" , gameinfo_getromhash }, 
    { "getromname", gameinfo_getromname },
    { "getrompath", gameinfo_getrompath },  // retroarch-only
    //{ "getstatus" , gameinfo_getstatus },  // returns the game database status of the currently loaded rom. Statuses are for example: GoodDump, BadDump, Hack, Unknown, NotInDatabase
    //{ "indatabase", gameinfo_indatabase }, // returns whether or not the currently loaded rom is in the game database
    //{ "isstatusbad", gameinfo_isstatusbad }  // returns the currently loaded rom's game database status is considered 'bad'
	{NULL,NULL}
};

static const struct luaL_Reg  romlib[] = {
    { "gethash" , gameinfo_getromhash }, 
    { "getfilename", gameinfo_getromname },
    { "readbyte", rom_readbyte },  // fceux-only
	{NULL,NULL}
};

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
    { "getconfig" , client_getconfig },  
    { "transformPoint" , client_transformPoint },  
    // client.openrom(string path)  -> core_load_game
	{NULL,NULL}
};

static const struct luaL_Reg  guilib[] = {
    { "addmessage" ,  gui_addmessage },
#ifdef HAVE_GFX_WIDGETS
    { "drawString" ,  gui_drawString },
    { "drawStringO" ,  gui_drawStringO },
    { "drawText" ,  gui_drawString },
    { "pixelText" ,  gui_drawPixelText },
    { "pixelTextO" ,  gui_drawPixelTextO },
    { "text" ,  gui_drawPixelText },
    { "drawRectangle" ,  gui_drawRectangle },
    { "drawRectangleO" ,  gui_drawRectangleO },
    { "drawPixel" ,  gui_drawPixel },
    { "clearGraphics" ,  gui_clearGraphics },
    { "cleartext" ,  gui_clearGraphics },
    //TODO: drawLine
    //TODO: drawImage
    //TODO: drawBox
    // FCEUX-aliases
    { "text", gui_drawString },
    { "drawtext", gui_drawString },
    { "setpixel", gui_drawPixel },
#endif
    { "getpixel", emu_getscreenpixel },
    //TODO: gui.parsecolor(color)
    { "savescreenshot", client_screenshot },
    { "savescreenshotas", client_screenshot },
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
#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
    { "getdir", emu_getdir },
#endif
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
    // TODO: memory.getcurrentmemorydomain
    // TODO:memory.getcurrentmemorydomainsize
    { "getmemorydomainlist" ,  memory_getmemorydomainlist },
    { "getmemorydomainsize" ,  memory_getmemorydomainsize },
    { "hash_region" ,  memory_hash_region },
    { "writebyte" ,  memory_writebyte },
    { "write_u8" ,  memory_writebyte },
    // TODO: write_s8 / 16/ 32
    { "write_bytes_as_array" ,  memory_write_bytes_as_array },
    //TODO: { "write_bytes_as_dict" ,  memory_write_bytes_as_dict },
    // FCEUX-aliases
    { "readbyteunsigned" ,  memory_readbyte },
    { "readbytesigned" ,  memory_readbytesigned },
    { "readwordsigned" ,  memory_read_s16_le },
    { "readword" ,  memory_read_u16_le },
    //TODO: { "readfloat" ,  memory_readfloat },
	{NULL,NULL}
};

static const struct luaL_Reg  joypadlib [] = {
    { "get" ,  joypad_get },
    { "read" ,  joypad_get },
    // TODO: { "rumble" ,  joypad_rumble },
	{NULL,NULL}
};

static const struct luaL_Reg  inputlib [] = {
    { "get" ,  input_get },
    { "read" ,  input_get },
	{NULL,NULL}
};

static const struct luaL_Reg  commlib[] = {
    { "httpGet", comm_httpget },
    { "httpPost", comm_httppost },
    { "httpPut", comm_httpput },
    // TODO: more functions
	{NULL,NULL}
};

static const struct luaL_Reg  savestatelib[] = {
    { "loadslot", savestate_loadslot },
    { "saveslot", savestate_saveslot },   
    { "save", savestate_save },   
    { "load", savestate_load },   
	{NULL,NULL}
};


static const struct luaL_Reg  bizstringlib[] = {
    { "contains", bizstring_contains },
    { "endswith", bizstring_endswith },
    { "startswith", bizstring_startswith },
    { "tolower", bizstring_tolower },
    { "toupper", bizstring_toupper },
    { "trim", bizstring_trim },
    //TODO: more functions
	{NULL,NULL}
};


// stdlib function sandboxing
int safe_io_open(lua_State *L) {
    const char *user_path = luaL_checkstring(L, 1);
    const char *mode = luaL_optstring(L, 2, "r");

    if (path_is_absolute(user_path)) {
        return luaL_error(L, "Access denied: file path is absolute, must be relative. Disable sandboxing to bypass.");
    }
    if (string_starts_with(user_path, "..")) {
        return luaL_error(L, "Access denied: file path cannot access parent. Disable sandboxing to bypass.");
    }
    // TODO: allow subdirs of
    //const char* retroarch_system_dir = path_get(RARCH_PATH_CONFIG);  // /system
    
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

#ifdef LUA_SCRIPTS_SANDBOXED
    // TODO: turn into a user setting
    // override unsafe functions
    // io.open
    lua_getglobal(L, "io");
    lua_getfield(L, -1, "open");       // push io.open
    lua_setfield(L, LUA_REGISTRYINDEX, "original_io_open"); // registry["original_io_open"] = io.open
    lua_pop(L, 1);                     // pop io table
    lua_getglobal(L, "io");            // push io table
    lua_pushstring(L, "open");         // push key "open"
    lua_pushcfunction(L, safe_io_open); // push our safe_io_open function
    lua_settable(L, -3);               // io.open = safe_io_open
    lua_pop(L, 1);                    // pop io table
    
    // disable unsafe functions
    lua_getglobal(L, "os");
    if (lua_istable(L, -1)) {
        lua_pushstring(L, "execute");
        lua_pushnil(L);
        lua_settable(L, -3);
        // Not needed? os.remove(filename), os.rename(old, new)
    }
    lua_pop(L, 1);

    // Disable risky loaders
    //lua_pushnil(L); lua_setglobal(L, "dofile");
    //lua_pushnil(L); lua_setglobal(L, "loadfile");
    //lua_pushnil(L); lua_setglobal(L, "require");
    //lua_pushnil(L); lua_setglobal(L, "load");  // disables eval-like dynamic code
#endif

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
    luaL_newlib(L, savestatelib);
    lua_setglobal(L, "savestate");
    luaL_newlib(L, bizstringlib);
    lua_setglobal(L, "bizstring");
    //TODO: lua_register(L, "bit", bitlib);
    
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

    // needed on some lua versions?
    //if LUA_VERSION_NUM == 502
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
    
    // clear all gfx shapes
    gui_clearGraphics(NULL);
    
    lua_close(co);
    co = NULL;
}
