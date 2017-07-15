#pragma once

#include "Emu/Cell/PPUAnalyser.h"
#include "sys_sync.h"

// Return codes
enum CellPrxError : u32
{
	CELL_PRX_ERROR_ERROR                       = 0x80011001, // Error state
	CELL_PRX_ERROR_ILLEGAL_PERM                = 0x800110d1, // No permission to execute API
	CELL_PRX_ERROR_UNKNOWN_MODULE              = 0x8001112e, // Specified PRX could not be found
	CELL_PRX_ERROR_ALREADY_STARTED             = 0x80011133, // Specified PRX is already started
	CELL_PRX_ERROR_NOT_STARTED                 = 0x80011134, // Specified PRX is not started
	CELL_PRX_ERROR_ALREADY_STOPPED             = 0x80011135, // Specified PRX is already stopped
	CELL_PRX_ERROR_CAN_NOT_STOP                = 0x80011136, // Specified PRX must not be stopped
	CELL_PRX_ERROR_NOT_REMOVABLE               = 0x80011138, // Specified PRX must not be deleted
	CELL_PRX_ERROR_LIBRARY_NOT_YET_LINKED      = 0x8001113a, // Called unlinked function
	CELL_PRX_ERROR_LIBRARY_FOUND               = 0x8001113b, // Specified library is already registered
	CELL_PRX_ERROR_LIBRARY_NOTFOUND            = 0x8001113c, // Specified library is not registered
	CELL_PRX_ERROR_ILLEGAL_LIBRARY             = 0x8001113d, // Library structure is invalid
	CELL_PRX_ERROR_LIBRARY_INUSE               = 0x8001113e, // Library cannot be deleted because it is linked
	CELL_PRX_ERROR_ALREADY_STOPPING            = 0x8001113f, // Specified PRX is in the process of stopping
	CELL_PRX_ERROR_UNSUPPORTED_PRX_TYPE        = 0x80011148, // Specified PRX format is invalid and cannot be loaded
	CELL_PRX_ERROR_INVAL                       = 0x80011324, // Argument value is invalid
	CELL_PRX_ERROR_ILLEGAL_PROCESS             = 0x80011801, // Specified process does not exist
	CELL_PRX_ERROR_NO_LIBLV2                   = 0x80011881, // liblv2.sprx does not exist
	CELL_PRX_ERROR_UNSUPPORTED_ELF_TYPE        = 0x80011901, // ELF type of specified file is not supported
	CELL_PRX_ERROR_UNSUPPORTED_ELF_CLASS       = 0x80011902, // ELF class of specified file is not supported
	CELL_PRX_ERROR_UNDEFINED_SYMBOL            = 0x80011904, // References undefined symbols
	CELL_PRX_ERROR_UNSUPPORTED_RELOCATION_TYPE = 0x80011905, // Uses unsupported relocation type
	CELL_PRX_ERROR_ELF_IS_REGISTERED           = 0x80011910, // Fixed ELF is already registered
};

struct sys_prx_get_module_id_by_name_option_t
{
	be_t<u64> size;
	vm::ps3::ptr<void> base;
};

struct sys_prx_load_module_option_t
{
	be_t<u64> size;
	vm::ps3::bptr<void> base_addr;
};

struct sys_prx_segment_info_t;// TODO

struct sys_prx_module_info_t
{
	be_t<u64> size;
	char name[30];
	char version[2];
	be_t<u32> modattribute;
	be_t<u32> start_entry;
	be_t<u32> stop_entry;
	be_t<u32> all_segments_num;
	vm::ps3::bptr<char> filename;
	be_t<u32> filename_size;
	vm::ps3::bptr<sys_prx_segment_info_t> segments;
	be_t<u32> segments_num;
};

struct sys_prx_module_info_option_t
{
	be_t<u64> size; // 0x10
	vm::ps3::bptr<sys_prx_module_info_t> info;
};

struct sys_prx_start_module_option_t
{
	be_t<u64> size;
};

struct sys_prx_stop_module_option_t
{
	be_t<u64> size;
};

struct sys_prx_start_stop_module_option_t
{
	be_t<u64> size;
	be_t<u64> cmd;
	vm::ps3::bptr<s32(u32 argc, vm::ps3::ptr<void> argv), u64> entry;
	be_t<u64> res;
	vm::ps3::bptr<s32(vm::ps3::ptr<s32(u32, vm::ps3::ptr<void>), u64>, u32 argc, vm::ps3::ptr<void> argv), u64> entry2;
};

struct sys_prx_unload_module_option_t
{
	be_t<u64> size;
};

struct sys_prx_get_module_list_t
{
	be_t<u64> size;
	be_t<u32> max;
	be_t<u32> count;
	vm::ps3::bptr<u32> idlist;
};

struct sys_prx_get_module_list_option_t
{
	be_t<u64> size; // 0x20
	be_t<u32> pad;
	be_t<u32> max;
	be_t<u32> count;
	vm::ps3::bptr<u32> idlist;
	be_t<u32> unk; // 0
};

struct lv2_prx final : lv2_obj, ppu_module
{
	static const u32 id_base = 0x23000000;

	bool is_started = false;

	std::unordered_map<u32, u32> specials;
	std::unordered_map<u32, void*> imports;

	vm::ps3::ptr<s32(u32 argc, vm::ps3::ptr<void> argv)> start = vm::null;
	vm::ps3::ptr<s32(u32 argc, vm::ps3::ptr<void> argv)> stop = vm::null;
	vm::ps3::ptr<s32(u64 callback, u64 argc, vm::ps3::ptr<void, u64> argv)> prologue = vm::null;
	vm::ps3::ptr<s32(u64 callback, u64 argc, vm::ps3::ptr<void, u64> argv)> epilogue = vm::null;
	vm::ps3::ptr<s32()> exit = vm::null;
};

// SysCalls

error_code sys_prx_get_ppu_guid();
error_code _sys_prx_load_module_by_fd(s32 fd, u64 offset, u64 flags, vm::ps3::ptr<sys_prx_load_module_option_t> pOpt);
error_code _sys_prx_load_module_on_memcontainer_by_fd(s32 fd, u64 offset, u32 mem_ct, u64 flags, vm::ps3::ptr<sys_prx_load_module_option_t> pOpt);
error_code _sys_prx_load_module_list(s32 count, vm::ps3::cpptr<char, u32, u64> path_list, u64 flags, vm::ps3::ptr<sys_prx_load_module_option_t> pOpt, vm::ps3::ptr<u32> id_list);
error_code _sys_prx_load_module_list_on_memcontainer(s32 count, vm::ps3::cpptr<char, u32, u64> path_list, u32 mem_ct, u64 flags, vm::ps3::ptr<sys_prx_load_module_option_t> pOpt, vm::ps3::ptr<u32> id_list);
error_code _sys_prx_load_module_on_memcontainer(vm::ps3::cptr<char> path, u32 mem_ct, u64 flags, vm::ps3::ptr<sys_prx_load_module_option_t> pOpt);
error_code _sys_prx_load_module(vm::ps3::cptr<char> path, u64 flags, vm::ps3::ptr<sys_prx_load_module_option_t> pOpt);
error_code _sys_prx_start_module(u32 id, u64 flags, vm::ps3::ptr<sys_prx_start_stop_module_option_t> pOpt);
error_code _sys_prx_stop_module(u32 id, u64 flags, vm::ps3::ptr<sys_prx_start_stop_module_option_t> pOpt);
error_code _sys_prx_unload_module(u32 id, u64 flags, vm::ps3::ptr<sys_prx_unload_module_option_t> pOpt);
error_code _sys_prx_register_module();
error_code _sys_prx_query_module();
error_code _sys_prx_register_library(vm::ps3::ptr<void> library);
error_code _sys_prx_unregister_library(vm::ps3::ptr<void> library);
error_code _sys_prx_link_library();
error_code _sys_prx_unlink_library();
error_code _sys_prx_query_library();
error_code _sys_prx_get_module_list(u64 flags, vm::ps3::ptr<sys_prx_get_module_list_option_t> pInfo);
error_code _sys_prx_get_module_info(u32 id, u64 flags, vm::ps3::ptr<sys_prx_module_info_option_t> pOpt);
error_code _sys_prx_get_module_id_by_name(vm::ps3::cptr<char> name, u64 flags, vm::ps3::ptr<sys_prx_get_module_id_by_name_option_t> pOpt);
error_code _sys_prx_get_module_id_by_address(u32 addr);
error_code _sys_prx_start();
error_code _sys_prx_stop();
