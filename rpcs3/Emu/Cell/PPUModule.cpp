#include "stdafx.h"
#include "Emu/Cell/PPUModule.h"

#include "Utilities/bin_patch.h"
#include "Utilities/StrUtil.h"
#include "Utilities/address_range.h"
#include "util/serialization.hpp"
#include "Crypto/sha1.h"
#include "Crypto/unself.h"
#include "Loader/ELF.h"
#include "Emu/System.h"
#include "Emu/VFS.h"

#include "Emu/Cell/PPUOpcodes.h"
#include "Emu/Cell/PPUAnalyser.h"

#include "Emu/Cell/lv2/sys_process.h"
#include "Emu/Cell/lv2/sys_prx.h"
#include "Emu/Cell/lv2/sys_memory.h"
#include "Emu/Cell/lv2/sys_overlay.h"

#include "Emu/Cell/Modules/StaticHLE.h"

#include <map>
#include <span>
#include <set>
#include <algorithm>
#include <shared_mutex>
#include "util/asm.hpp"

LOG_CHANNEL(ppu_loader);

extern std::string ppu_get_function_name(const std::string& _module, u32 fnid);
extern std::string ppu_get_variable_name(const std::string& _module, u32 vnid);
extern void ppu_register_range(u32 addr, u32 size);
extern void ppu_register_function_at(u32 addr, u32 size, ppu_intrp_func_t ptr);

extern void sys_initialize_tls(ppu_thread&, u64, u32, u32, u32);

std::unordered_map<std::string, ppu_static_module*>& ppu_module_manager::get()
{
	// In C++ the order of static initialization is undefined if it happens in
	// separate compilation units, therefore we have to initialize the map on first use.
	static std::unordered_map<std::string, ppu_static_module*> s_module_map;
	return s_module_map;
}

// HLE function name cache
std::vector<std::string> g_ppu_function_names;

atomic_t<u32> liblv2_begin = 0, liblv2_end = 0;

extern u32 ppu_generate_id(std::string_view name)
{
	// Symbol name suffix
	constexpr auto suffix = "\x67\x59\x65\x99\x04\x25\x04\x90\x56\x64\x27\x49\x94\x89\x74\x1A"sv;

	sha1_context ctx;
	u8 output[20];

	// Compute SHA-1 hash
	sha1_starts(&ctx);
	sha1_update(&ctx, reinterpret_cast<const u8*>(name.data()), name.size());
	sha1_update(&ctx, reinterpret_cast<const u8*>(suffix.data()), suffix.size());
	sha1_finish(&ctx, output);

	le_t<u32> result = 0;
	std::memcpy(&result, output, sizeof(result));
	return result;
}

ppu_static_module::ppu_static_module(const char* name)
	: name(name)
{
	ppu_module_manager::register_module(this);
}

void ppu_static_module::add_init_func(void(*func)(ppu_static_module*))
{
	m_on_init.emplace_back(func);
}

void ppu_static_module::initialize()
{
	for (auto func : m_on_init)
	{
		func(this);
	}
}

void ppu_module_manager::register_module(ppu_static_module* _module)
{
	ppu_module_manager::get().emplace(_module->name, _module);
}

ppu_static_function& ppu_module_manager::access_static_function(const char* _module, u32 fnid)
{
	auto& res = ::at32(ppu_module_manager::get(), _module)->functions[fnid];

	if (res.name)
	{
		fmt::throw_exception("PPU FNID duplication in module %s (%s, 0x%x)", _module, res.name, fnid);
	}

	return res;
}

ppu_static_variable& ppu_module_manager::access_static_variable(const char* _module, u32 vnid)
{
	auto& res = ::at32(ppu_module_manager::get(), _module)->variables[vnid];

	if (res.name)
	{
		fmt::throw_exception("PPU VNID duplication in module %s (%s, 0x%x)", _module, res.name, vnid);
	}

	return res;
}

const ppu_static_module* ppu_module_manager::get_module(const std::string& name)
{
	const auto& map = ppu_module_manager::get();
	const auto found = map.find(name);
	return found != map.end() ? found->second : nullptr;
}

void ppu_module_manager::initialize_modules()
{
	for (auto& _module : ppu_module_manager::get())
	{
		_module.second->initialize();
	}
}

// Global linkage information
struct ppu_linkage_info
{
	ppu_linkage_info() = default;

	ppu_linkage_info(const ppu_linkage_info&) = delete;

	ppu_linkage_info& operator=(const ppu_linkage_info&) = delete;

	struct module_data
	{
		struct info
		{
			ppu_static_function* static_func = nullptr;
			ppu_static_variable* static_var = nullptr;
			u32 export_addr = 0;
			std::set<u32> imports{};
			std::set<u32> frefss{};
		};

		// FNID -> (export; [imports...])
		std::map<u32, info> functions{};
		std::map<u32, info> variables{};

		// Obsolete
		bool imported = false;
	};

	// Module map
	std::map<std::string, module_data> modules{};
	std::map<std::string, atomic_t<bool>, std::less<>> lib_lock;
	shared_mutex lib_lock_mutex;
	shared_mutex mutex;
};

// Initialize static modules.
static void ppu_initialize_modules(ppu_linkage_info* link, utils::serial* ar = nullptr)
{
	if (!link->modules.empty())
	{
		return;
	}

	ppu_module_manager::initialize_modules();

	const std::initializer_list<const ppu_static_module*> registered
	{
		&ppu_module_manager::cellAdec,
		&ppu_module_manager::cellAtrac,
		&ppu_module_manager::cellAtracMulti,
		&ppu_module_manager::cellAudio,
		&ppu_module_manager::cellAvconfExt,
		&ppu_module_manager::cellAuthDialogUtility,
		&ppu_module_manager::cellBGDL,
		&ppu_module_manager::cellCamera,
		&ppu_module_manager::cellCelp8Enc,
		&ppu_module_manager::cellCelpEnc,
		&ppu_module_manager::cellCrossController,
		&ppu_module_manager::cellDaisy,
		&ppu_module_manager::cellDmux,
		&ppu_module_manager::cellDtcpIpUtility,
		&ppu_module_manager::cellFiber,
		&ppu_module_manager::cellFont,
		&ppu_module_manager::cellFontFT,
		&ppu_module_manager::cell_FreeType2,
		&ppu_module_manager::cellFs,
		&ppu_module_manager::cellGame,
		&ppu_module_manager::cellGameExec,
		&ppu_module_manager::cellGcmSys,
		&ppu_module_manager::cellGem,
		&ppu_module_manager::cellGifDec,
		&ppu_module_manager::cellHttp,
		&ppu_module_manager::cellHttps,
		&ppu_module_manager::cellHttpUtil,
		&ppu_module_manager::cellImeJp,
		&ppu_module_manager::cellJpgDec,
		&ppu_module_manager::cellJpgEnc,
		&ppu_module_manager::cellKey2char,
		&ppu_module_manager::cellL10n,
		&ppu_module_manager::cellLibprof,
		&ppu_module_manager::cellMic,
		&ppu_module_manager::cellMusic,
		&ppu_module_manager::cellMusicDecode,
		&ppu_module_manager::cellMusicExport,
		&ppu_module_manager::cellNetAoi,
		&ppu_module_manager::cellNetCtl,
		&ppu_module_manager::cellOskDialog,
		&ppu_module_manager::cellOvis,
		&ppu_module_manager::cellPamf,
		&ppu_module_manager::cellPesmUtility,
		&ppu_module_manager::cellPhotoDecode,
		&ppu_module_manager::cellPhotoExport,
		&ppu_module_manager::cellPhotoImportUtil,
		&ppu_module_manager::cellPngDec,
		&ppu_module_manager::cellPngEnc,
		&ppu_module_manager::cellPrint,
		&ppu_module_manager::cellRec,
		&ppu_module_manager::cellRemotePlay,
		&ppu_module_manager::cellResc,
		&ppu_module_manager::cellRtc,
		&ppu_module_manager::cellRtcAlarm,
		&ppu_module_manager::cellRudp,
		&ppu_module_manager::cellSail,
		&ppu_module_manager::cellSailRec,
		&ppu_module_manager::cellSaveData,
		&ppu_module_manager::cellMinisSaveData,
		&ppu_module_manager::cellScreenShot,
		&ppu_module_manager::cellSearch,
		&ppu_module_manager::cellSheap,
		&ppu_module_manager::cellSpudll,
		&ppu_module_manager::cellSpurs,
		&ppu_module_manager::cellSpursJq,
		&ppu_module_manager::cellSsl,
		&ppu_module_manager::cellSubDisplay,
		&ppu_module_manager::cellSync,
		&ppu_module_manager::cellSync2,
		&ppu_module_manager::cellSysconf,
		&ppu_module_manager::cellSysmodule,
		&ppu_module_manager::cellSysutil,
		&ppu_module_manager::cellSysutilAp,
		&ppu_module_manager::cellSysutilAvc2,
		&ppu_module_manager::cellSysutilAvcExt,
		&ppu_module_manager::cellSysutilNpEula,
		&ppu_module_manager::cellSysutilMisc,
		&ppu_module_manager::cellUsbd,
		&ppu_module_manager::cellUsbPspcm,
		&ppu_module_manager::cellUserInfo,
		&ppu_module_manager::cellVdec,
		&ppu_module_manager::cellVideoExport,
		&ppu_module_manager::cellVideoPlayerUtility,
		&ppu_module_manager::cellVideoUpload,
		&ppu_module_manager::cellVoice,
		&ppu_module_manager::cellVpost,
		&ppu_module_manager::libad_async,
		&ppu_module_manager::libad_core,
		&ppu_module_manager::libfs_utility_init,
		&ppu_module_manager::libmedi,
		&ppu_module_manager::libmixer,
		&ppu_module_manager::libsnd3,
		&ppu_module_manager::libsynth2,
		&ppu_module_manager::sceNp,
		&ppu_module_manager::sceNp2,
		&ppu_module_manager::sceNpClans,
		&ppu_module_manager::sceNpCommerce2,
		&ppu_module_manager::sceNpMatchingInt,
		&ppu_module_manager::sceNpSns,
		&ppu_module_manager::sceNpTrophy,
		&ppu_module_manager::sceNpTus,
		&ppu_module_manager::sceNpUtil,
		&ppu_module_manager::sys_crashdump,
		&ppu_module_manager::sys_io,
		&ppu_module_manager::sys_net,
		&ppu_module_manager::sysPrxForUser,
		&ppu_module_manager::sys_libc,
		&ppu_module_manager::sys_lv2dbg,
		&ppu_module_manager::static_hle,
		&ppu_module_manager::hle_patches,
	};

	// Initialize double-purpose fake OPD array for HLE functions
	const auto& hle_funcs = ppu_function_manager::get(g_cfg.core.ppu_decoder != ppu_decoder_type::_static);

	u32& hle_funcs_addr = g_fxo->get<ppu_function_manager>().addr;

	// Allocate memory for the array (must be called after fixed allocations)
	if (!hle_funcs_addr)
		hle_funcs_addr = vm::alloc(::size32(hle_funcs) * 8, vm::main);
	else
		vm::page_protect(hle_funcs_addr, utils::align(::size32(hle_funcs) * 8, 0x1000), 0, vm::page_writable);

	// Initialize as PPU executable code
	ppu_register_range(hle_funcs_addr, ::size32(hle_funcs) * 8);

	// Fill the array (visible data: self address and function index)
	for (u32 addr = hle_funcs_addr, index = 0; index < hle_funcs.size(); addr += 8, index++)
	{
		// Function address = next CIA, RTOC = 0 (vm::null)
		vm::write32(addr + 0, addr + 4);
		vm::write32(addr + 4, 0);

		// Register the HLE function directly
		ppu_register_function_at(addr + 0, 4, nullptr);
		ppu_register_function_at(addr + 4, 4, hle_funcs[index]);
	}

	// Set memory protection to read-only
	vm::page_protect(hle_funcs_addr, utils::align(::size32(hle_funcs) * 8, 0x1000), 0, 0, vm::page_writable);

	// Initialize function names
	const bool is_first = g_ppu_function_names.empty();

	if (is_first)
	{
		g_ppu_function_names.resize(hle_funcs.size());
		g_ppu_function_names[0] = "INVALID";
		g_ppu_function_names[1] = "HLE RETURN";
	}

	// For HLE variable allocation
	u32 alloc_addr = 0;

	// "Use" all the modules for correct linkage
	for (auto& _module : registered)
	{
		ppu_loader.trace("Registered static module: %s", _module->name);
	}

	struct hle_vars_save
	{
		hle_vars_save() = default;

		hle_vars_save(const hle_vars_save&) = delete;

		hle_vars_save& operator =(const hle_vars_save&) = delete;

		hle_vars_save(utils::serial& ar)
		{
			auto& manager = ppu_module_manager::get();

			while (true)
			{
				const std::string name = ar.operator std::string();

				if (name.empty())
				{
					// Null termination
					break;
				}

				const auto _module = ::at32(manager, name);

				auto& variable = _module->variables;

				for (u32 i = 0, end = ar.operator usz(); i < end; i++)
				{
					auto* ptr = &::at32(variable, ar.operator u32());
					ptr->addr = ar.operator u32();
					ensure(!!ptr->var);
				}
			}
		}

		void save(utils::serial& ar)
		{
			for (auto& pair : ppu_module_manager::get())
			{
				const auto _module = pair.second;

				if (_module->variables.empty())
				{
					continue;
				}

				ar(_module->name);

				ar(_module->variables.size());

				for (auto& variable : _module->variables)
				{
					ar(variable.first, variable.second.addr);
				}
			}

			// Null terminator
			ar(std::string{});
		}
	};

	if (ar)
	{
		g_fxo->init<hle_vars_save>(*ar);
	}
	else
	{
		g_fxo->init<hle_vars_save>();
	}

	for (auto& pair : ppu_module_manager::get())
	{
		const auto _module = pair.second;
		auto& linkage = link->modules[_module->name];

		for (auto& function : _module->functions)
		{
			ppu_loader.trace("** 0x%08X: %s", function.first, function.second.name);

			if (is_first)
			{
				g_ppu_function_names[function.second.index] = fmt::format("%s:%s", function.second.name, _module->name);
			}

			auto& flink = linkage.functions[function.first];

			flink.static_func = &function.second;
			flink.export_addr = g_fxo->get<ppu_function_manager>().func_addr(function.second.index);
			function.second.export_addr = &flink.export_addr;
		}

		for (auto& variable : _module->variables)
		{
			ppu_loader.trace("** &0x%08X: %s (size=0x%x, align=0x%x)", variable.first, variable.second.name, variable.second.size, variable.second.align);

			// Allocate HLE variable
			if (ar)
			{
				// Already loaded
			}
			else if (variable.second.size >= 0x10000 || variable.second.align >= 0x10000)
			{
				variable.second.addr = vm::alloc(variable.second.size, vm::main, std::max<u32>(variable.second.align, 0x10000));
			}
			else
			{
				const u32 next = utils::align(alloc_addr, variable.second.align);
				const u32 end = next + variable.second.size - 1;

				if (!next || (end >> 16 != alloc_addr >> 16))
				{
					alloc_addr = vm::alloc(0x10000, vm::main);
				}
				else
				{
					alloc_addr = next;
				}

				variable.second.addr = alloc_addr;
				alloc_addr += variable.second.size;
			}

			*variable.second.var = variable.second.addr;
			ppu_loader.trace("Allocated HLE variable %s.%s at 0x%x", _module->name, variable.second.name, *variable.second.var);

			// Initialize HLE variable
			if (variable.second.init)
			{
				variable.second.init();
			}

			if ((variable.second.flags & MFF_HIDDEN) == 0)
			{
				auto& vlink = linkage.variables[variable.first];

				vlink.static_var = &variable.second;
				vlink.export_addr = variable.second.addr;
				variable.second.export_addr = &vlink.export_addr;
			}
		}
	}
}

// For the debugger (g_ppu_function_names shouldn't change, string_view should suffice)
extern const std::unordered_map<u32, std::string_view>& get_exported_function_names_as_addr_indexed_map()
{
	struct info_t
	{
		std::unordered_map<u32, std::string_view> res;
		u64 update_time = 0;
	};

	static thread_local std::unique_ptr<info_t> info;

	if (!info)
	{
		info = std::make_unique<info_t>();
		info->res.reserve(ppu_module_manager::get().size());
	}

	auto& [res, update_time] = *info;

	const auto link = g_fxo->try_get<ppu_linkage_info>();
	const auto hle_funcs = g_fxo->try_get<ppu_function_manager>();

	if (!link || !hle_funcs)
	{
		res.clear();
		return res;
	}

	const u64 current_time = get_system_time();

	// Update list every >=0.1 seconds
	if (current_time - update_time < 100'000)
	{
		return res;
	}

	update_time = current_time;

	res.clear();

	for (auto& pair : ppu_module_manager::get())
	{
		const auto _module = pair.second;
		auto& linkage = link->modules[_module->name];

		for (auto& function : _module->functions)
		{
			auto& flink = linkage.functions[function.first];
			u32 addr = flink.export_addr;

			if (vm::check_addr<4>(addr, vm::page_readable) && addr != hle_funcs->func_addr(function.second.index))
			{
				addr = vm::read32(addr);

				if (!(addr % 4) && vm::check_addr<4>(addr, vm::page_executable))
				{
					res.try_emplace(addr, g_ppu_function_names[function.second.index]);
				}
			}
		}
	}

	return res;
}

// Resolve relocations for variable/function linkage.
static void ppu_patch_refs(std::vector<ppu_reloc>* out_relocs, u32 fref, u32 faddr)
{
	struct ref_t
	{
		be_t<u32> type;
		be_t<u32> addr;
		be_t<u32> addend; // Note: Treating it as addend seems to be correct for now, but still unknown if theres more in this variable
	};

	for (auto ref = vm::ptr<ref_t>::make(fref); ref->type; ref++)
	{
		if (ref->addend) ppu_loader.warning("**** REF(%u): Addend value(0x%x, 0x%x)", ref->type, ref->addr, ref->addend);

		const u32 raddr = ref->addr;
		const u32 rtype = ref->type;
		const u32 rdata = faddr + ref->addend;

		if (out_relocs)
		{
			// Register relocation with unpredictable target (data=0)
			ppu_reloc _rel;
			_rel.addr = raddr;
			_rel.type = rtype;
			_rel.data = 0;
			out_relocs->emplace_back(_rel);
		}

		// OPs must be similar to relocations
		switch (rtype)
		{
		case 1:
		{
			const u32 value = vm::_ref<u32>(ref->addr) = rdata;
			ppu_loader.trace("**** REF(1): 0x%x <- 0x%x", ref->addr, value);
			break;
		}

		case 4:
		{
			const u16 value = vm::_ref<u16>(ref->addr) = static_cast<u16>(rdata);
			ppu_loader.trace("**** REF(4): 0x%x <- 0x%04x (0x%llx)", ref->addr, value, faddr);
			break;
		}

		case 6:
		{
			const u16 value = vm::_ref<u16>(ref->addr) = static_cast<u16>(rdata >> 16) + (rdata & 0x8000 ? 1 : 0);
			ppu_loader.trace("**** REF(6): 0x%x <- 0x%04x (0x%llx)", ref->addr, value, faddr);
			break;
		}

		case 57:
		{
			const u16 value = vm::_ref<ppu_bf_t<be_t<u16>, 0, 14>>(ref->addr) = static_cast<u16>(rdata) >> 2;
			ppu_loader.trace("**** REF(57): 0x%x <- 0x%04x (0x%llx)", ref->addr, value, faddr);
			break;
		}

		default: ppu_loader.error("**** REF(%u): Unknown/Illegal type (0x%x, 0x%x)", rtype, raddr, ref->addend);
		}
	}
}

enum PRX_EXPORT_ATTRIBUTES : u16
{
	PRX_EXPORT_LIBRARY_FLAG = 1,
	PRX_EXPORT_PRX_MANAGEMENT_FUNCTIONS_FLAG = 0x8000,
};

// Export or import module struct
struct ppu_prx_module_info
{
	u8 size;
	u8 unk0;
	be_t<u16> version;
	be_t<u16> attributes;
	be_t<u16> num_func;
	be_t<u16> num_var;
	be_t<u16> num_tlsvar;
	u8 info_hash;
	u8 info_tlshash;
	u8 unk1[2];
	vm::bcptr<char> name;
	vm::bcptr<u32> nids; // Imported FNIDs, Exported NIDs
	vm::bptr<u32> addrs;
	vm::bcptr<u32> vnids; // Imported VNIDs
	vm::bcptr<u32> vstubs;
	be_t<u32> unk4;
	be_t<u32> unk5;
};

bool ppu_form_branch_to_code(u32 entry, u32 target);

extern u32 ppu_get_exported_func_addr(u32 fnid, const std::string& module_name)
{
	return g_fxo->get<ppu_linkage_info>().modules[module_name].functions[fnid].export_addr;
}

extern bool ppu_register_library_lock(std::string_view libname, bool lock_lib)
{
	auto link = g_fxo->try_get<ppu_linkage_info>();

	if (!link || libname.empty())
	{
		return false;
	}

	reader_lock lock(link->lib_lock_mutex);

	if (auto it = link->lib_lock.find(libname); it != link->lib_lock.cend())
	{
		return lock_lib ? !it->second.test_and_set() : it->second.test_and_reset();
	}

	if (!lock_lib)
	{
		// If lock hasn't been installed it wasn't locked in the first place
		return false;
	}

	lock.upgrade();

	auto& lib_lock = link->lib_lock.emplace(std::string{libname}, false).first->second;

	return !lib_lock.test_and_set();
}

// Load and register exports; return special exports found (nameless module)
static auto ppu_load_exports(ppu_linkage_info* link, u32 exports_start, u32 exports_end, bool for_observing_callbacks = false, std::basic_string<bool>* loaded_flags = nullptr)
{
	std::unordered_map<u32, u32> result;

	// Flags were already provided meaning it's an unload operation
	const bool unload_exports = loaded_flags && !loaded_flags->empty();

	std::lock_guard lock(link->mutex);

	usz unload_index = 0;
	ppu_prx_module_info lib{};

	for (u32 addr = exports_start; addr < exports_end; unload_index++, addr += lib.size ? lib.size : sizeof(ppu_prx_module_info))
	{
		std::memcpy(&lib, vm::base(addr), sizeof(lib));

		const bool is_library = !!(lib.attributes & PRX_EXPORT_LIBRARY_FLAG);
		const bool is_management = !is_library && !!(lib.attributes & PRX_EXPORT_PRX_MANAGEMENT_FUNCTIONS_FLAG);

		if (loaded_flags && !unload_exports)
		{
			loaded_flags->push_back(false);
		}

		if (is_management)
		{
			// Set special exports
			for (u32 i = 0, end = lib.num_func + lib.num_var; i < end; i++)
			{
				const u32 nid = lib.nids[i];
				const u32 addr = lib.addrs[i];

				if (i < lib.num_func)
				{
					ppu_loader.notice("** Special: [%s] at 0x%x [0x%x, 0x%x]", ppu_get_function_name({}, nid), addr, vm::_ref<u32>(addr), vm::_ref<u32>(addr + 4));
				}
				else
				{
					ppu_loader.notice("** Special: &[%s] at 0x%x", ppu_get_variable_name({}, nid), addr);
				}

				result.emplace(nid, addr);
			}

			continue;
		}

		if (!is_library)
		{
			// Skipped if none of the flags is set
			continue;
		}

		if (for_observing_callbacks)
		{
			continue;
		}

		const std::string module_name(lib.name.get_ptr());

		if (unload_exports)
		{
			if (::at32(*loaded_flags, unload_index))
			{
				ppu_register_library_lock(module_name, false);
			}

			continue;
		}

		ppu_loader.notice("** Exported module '%s' (vnids=0x%x, vstubs=0x%x, version=0x%x, attributes=0x%x, unk4=0x%x, unk5=0x%x)", module_name, lib.vnids, lib.vstubs, lib.version, lib.attributes, lib.unk4, lib.unk5);

		if (lib.num_tlsvar)
		{
			ppu_loader.fatal("Unexpected num_tlsvar (%u)!", lib.num_tlsvar);
		}

		const bool should_load = ppu_register_library_lock(module_name, true);

		if (loaded_flags)
		{
			loaded_flags->back() = should_load;
		}

		if (!should_load)
		{
			ppu_loader.notice("** Skipped module '%s' (already loaded)", module_name);
			continue;
		}

		// Static module
		const auto _sm = ppu_module_manager::get_module(module_name);

		// Module linkage
		auto& mlink = link->modules[module_name];

		const auto fnids = +lib.nids;
		const auto faddrs = +lib.addrs;

		// Get functions
		for (u32 i = 0, end = lib.num_func; i < end; i++)
		{
			const u32 fnid = fnids[i];
			const u32 faddr = faddrs[i];
			ppu_loader.notice("**** %s export: [%s] (0x%08x) at 0x%x [at:0x%x]", module_name, ppu_get_function_name(module_name, fnid), fnid, faddr, vm::read32(faddr));

			// Function linkage info
			auto& flink = mlink.functions[fnid];

			if (flink.static_func && flink.export_addr == g_fxo->get<ppu_function_manager>().func_addr(flink.static_func->index))
			{
				flink.export_addr = 0;
			}

			if (flink.export_addr)
			{
				ppu_loader.notice("Already linked function '%s' in module '%s'", ppu_get_function_name(module_name, fnid), module_name);
			}
			//else
			{
				// Static function
				const auto _sf = _sm && _sm->functions.count(fnid) ? &::at32(_sm->functions, fnid) : nullptr;

				if (_sf && (_sf->flags & MFF_FORCED_HLE))
				{
					// Inject a branch to the HLE implementation
					const u32 target = g_fxo->get<ppu_function_manager>().func_addr(_sf->index, true);

					// Set exported function
					flink.export_addr = target - 4;

					ppu_form_branch_to_code(vm::read32(faddr), target);
				}
				else
				{
					// Set exported function
					flink.export_addr = faddr;

					// Fix imports
					for (const u32 addr : flink.imports)
					{
						vm::write32(addr, faddr);
						//ppu_loader.warning("Exported function '%s' in module '%s'", ppu_get_function_name(module_name, fnid), module_name);
					}

					for (const u32 fref : flink.frefss)
					{
						ppu_patch_refs(nullptr, fref, faddr);
					}
				}
			}
		}

		const auto vnids = lib.nids + lib.num_func;
		const auto vaddrs = lib.addrs + lib.num_func;

		// Get variables
		for (u32 i = 0, end = lib.num_var; i < end; i++)
		{
			const u32 vnid = vnids[i];
			const u32 vaddr = vaddrs[i];
			ppu_loader.notice("**** %s export: &[%s] at 0x%x", module_name, ppu_get_variable_name(module_name, vnid), vaddr);

			// Variable linkage info
			auto& vlink = mlink.variables[vnid];

			if (vlink.static_var && vlink.export_addr == vlink.static_var->addr)
			{
				vlink.export_addr = 0;
			}

			if (vlink.export_addr)
			{
				ppu_loader.error("Already linked variable '%s' in module '%s'", ppu_get_variable_name(module_name, vnid), module_name);
			}
			//else
			{
				// Set exported variable
				vlink.export_addr = vaddr;

				// Fix imports
				for (const auto vref : vlink.imports)
				{
					ppu_patch_refs(nullptr, vref, vaddr);
					//ppu_loader.warning("Exported variable '%s' in module '%s'", ppu_get_variable_name(module_name, vnid), module_name);
				}
			}
		}
	}

	return result;
}

static auto ppu_load_imports(std::vector<ppu_reloc>& relocs, ppu_linkage_info* link, u32 imports_start, u32 imports_end)
{
	std::unordered_map<u32, void*> result;

	reader_lock lock(link->mutex);

	for (u32 addr = imports_start; addr < imports_end;)
	{
		const auto& lib = vm::_ref<const ppu_prx_module_info>(addr);

		const std::string module_name(lib.name.get_ptr());

		ppu_loader.notice("** Imported module '%s' (ver=0x%x, attr=0x%x, 0x%x, 0x%x) [0x%x]", module_name, lib.version, lib.attributes, lib.unk4, lib.unk5, addr);

		if (lib.num_tlsvar)
		{
			ppu_loader.fatal("Unexpected num_tlsvar (%u)!", lib.num_tlsvar);
		}

		// Static module
		//const auto _sm = ppu_module_manager::get_module(module_name);

		// Module linkage
		auto& mlink = link->modules[module_name];

		const auto fnids = +lib.nids;
		const auto faddrs = +lib.addrs;

		for (u32 i = 0, end = lib.num_func; i < end; i++)
		{
			const u32 fnid = fnids[i];
			const u32 fstub = faddrs[i];
			const u32 faddr = (faddrs + i).addr();
			ppu_loader.notice("**** %s import: [%s] (0x%08x) -> 0x%x", module_name, ppu_get_function_name(module_name, fnid), fnid, fstub);

			// Function linkage info
			auto& flink = link->modules[module_name].functions[fnid];

			// Add new import
			result.emplace(faddr, &flink);
			flink.imports.emplace(faddr);
			mlink.imported = true;

			// Link address (special HLE function by default)
			const u32 link_addr = flink.export_addr ? flink.export_addr : g_fxo->get<ppu_function_manager>().addr;

			// Write import table
			vm::write32(faddr, link_addr);

			// Patch refs if necessary (0x2000 seems to be correct flag indicating the presence of additional info)
			if (const u32 frefs = (lib.attributes & 0x2000) ? +fnids[i + lib.num_func] : 0)
			{
				result.emplace(frefs, &flink);
				flink.frefss.emplace(frefs);
				ppu_patch_refs(&relocs, frefs, link_addr);
			}

			//ppu_loader.warning("Imported function '%s' in module '%s' (0x%x)", ppu_get_function_name(module_name, fnid), module_name, faddr);
		}

		const auto vnids = +lib.vnids;
		const auto vstubs = +lib.vstubs;

		for (u32 i = 0, end = lib.num_var; i < end; i++)
		{
			const u32 vnid = vnids[i];
			const u32 vref = vstubs[i];
			ppu_loader.notice("**** %s import: &[%s] (ref=*0x%x)", module_name, ppu_get_variable_name(module_name, vnid), vref);

			// Variable linkage info
			auto& vlink = link->modules[module_name].variables[vnid];

			// Add new import
			result.emplace(vref, &vlink);
			vlink.imports.emplace(vref);
			mlink.imported = true;

			// Link if available
			ppu_patch_refs(&relocs, vref, vlink.export_addr);

			//ppu_loader.warning("Imported variable '%s' in module '%s' (0x%x)", ppu_get_variable_name(module_name, vnid), module_name, vlink.first);
		}

		addr += lib.size ? lib.size : sizeof(ppu_prx_module_info);
	}

	return result;
}

// For _sys_prx_register_module
void ppu_manual_load_imports_exports(u32 imports_start, u32 imports_size, u32 exports_start, u32 exports_size, std::basic_string<bool>& loaded_flags)
{
	auto& _main = g_fxo->get<main_ppu_module>();
	auto& link = g_fxo->get<ppu_linkage_info>();

	ppu_load_exports(&link, exports_start, exports_start + exports_size, false, &loaded_flags);

	if (!imports_size)
	{
		return;
	}

	ppu_load_imports(_main.relocs, &link, imports_start, imports_start + imports_size);
}

// For savestates
extern bool is_memory_compatible_for_copy_from_executable_optimization(u32 addr, u32 size)
{
	if (g_cfg.savestate.state_inspection_mode)
	{
		return false;
	}

	static ppu_exec_object s_ppu_exec;
	static std::vector<char> zeroes;

	if (!addr)
	{
		// A call for cleanup
		s_ppu_exec.clear();
		zeroes = {};
		return false;
	}

	if (s_ppu_exec != elf_error::ok)
	{
		if (s_ppu_exec != elf_error::stream)
		{
			// Failed before
			return false;
		}

		s_ppu_exec.open(decrypt_self(fs::file(Emu.GetBoot()), Emu.klic.empty() ? nullptr : reinterpret_cast<u8*>(&Emu.klic[0])));

		if (s_ppu_exec != elf_error::ok)
		{
			return false;
		}
	}

	for (const auto& prog : s_ppu_exec.progs)
	{
		const u32 vaddr = static_cast<u32>(prog.p_vaddr);
		const u32 seg_size = static_cast<u32>(prog.p_filesz);
		const u32 aligned_vaddr = vaddr & -0x10000;
		const u32 vaddr_offs = vaddr & 0xffff;

		// Check if the address is a start of segment within the executable
		if (prog.p_type == 0x1u /* LOAD */ && seg_size && aligned_vaddr == addr && prog.p_vaddr == prog.p_paddr && vaddr_offs + seg_size <= size)
		{
			zeroes.resize(std::max<usz>({zeroes.size(), usz{addr + size - (vaddr + seg_size)}, usz{vaddr_offs}}));

			// Check if gaps between segment and allocation bounds are still zeroes-only
			if (!std::memcmp(vm::_ptr<char>(aligned_vaddr), zeroes.data(), vaddr_offs) &&
				!std::memcmp(vm::_ptr<char>(vaddr + seg_size), zeroes.data(), (addr + size - (vaddr + seg_size))))
			{
				// Test memory equality
				return !std::memcmp(prog.bin.data(), vm::base(vaddr), seg_size);
			}
		}
	}

	return false;
}

void init_ppu_functions(utils::serial* ar, bool full = false)
{
	g_fxo->need<ppu_linkage_info>();

	if (ar)
	{
		ensure(vm::check_addr(g_fxo->init<ppu_function_manager>(*ar)->addr));
	}
	else
		g_fxo->init<ppu_function_manager>();

	if (full)
	{
		ensure(ar);

		// Initialize HLE modules
		ppu_initialize_modules(&g_fxo->get<ppu_linkage_info>(), ar);
	}
}

static void ppu_check_patch_spu_images(const ppu_segment& seg)
{
	const std::string_view seg_view{vm::get_super_ptr<char>(seg.addr), seg.size};

	for (usz i = seg_view.find("\177ELF"); i < seg.size; i = seg_view.find("\177ELF", i + 4))
	{
		const auto elf_header = vm::get_super_ptr<u8>(seg.addr + i);

		// Try to load SPU image
		const spu_exec_object obj(fs::file(elf_header, seg.size - i));

		if (obj != elf_error::ok)
		{
			// This address does not have an SPU elf
			continue;
		}

		// Segment info dump
		std::string name;
		std::string dump;

		std::basic_string<u32> applied;

		// Executable hash
		sha1_context sha2;
		sha1_starts(&sha2);
		u8 sha1_hash[20];

		for (const auto& prog : obj.progs)
		{
			// Only hash the data, we are not loading it
			sha1_update(&sha2, reinterpret_cast<const uchar*>(&prog.p_vaddr), sizeof(prog.p_vaddr));
			sha1_update(&sha2, reinterpret_cast<const uchar*>(&prog.p_memsz), sizeof(prog.p_memsz));
			sha1_update(&sha2, reinterpret_cast<const uchar*>(&prog.p_filesz), sizeof(prog.p_filesz));

			fmt::append(dump, "\n\tSegment: p_type=0x%x, p_vaddr=0x%llx, p_filesz=0x%llx, p_memsz=0x%llx, p_offset=0x%llx", prog.p_type, prog.p_vaddr, prog.p_filesz, prog.p_memsz, prog.p_offset);

			if (prog.p_type == 0x1u /* LOAD */ && prog.p_filesz > 0u)
			{
				sha1_update(&sha2, (elf_header + prog.p_offset), prog.p_filesz);
			}

			else if (prog.p_type == 0x4u /* NOTE */ && prog.p_filesz > 0u)
			{
				sha1_update(&sha2, (elf_header + prog.p_offset), prog.p_filesz);

				// We assume that the string SPUNAME exists 0x14 bytes into the NOTE segment
				name = reinterpret_cast<char*>(elf_header + prog.p_offset + 0x14);

				if (!name.empty())
				{
					fmt::append(dump, "\n\tSPUNAME: '%s'", name);
				}
			}
		}

		sha1_finish(&sha2, sha1_hash);

		// Format patch name
		std::string hash("SPU-0000000000000000000000000000000000000000");
		for (u32 i = 0; i < sizeof(sha1_hash); i++)
		{
			constexpr auto pal = "0123456789abcdef";
			hash[4 + i * 2] = pal[sha1_hash[i] >> 4];
			hash[5 + i * 2] = pal[sha1_hash[i] & 15];
		}

		if (g_cfg.core.spu_debug)
		{
			fs::file temp(fs::get_cache_dir() + "/spu_progs/" + vfs::escape(name.substr(name.find_last_of('/') + 1)) + '_' + hash.substr(4) + ".elf", fs::rewrite);

			if (!temp || !temp.write(obj.save()))
			{
				ppu_loader.error("Failed to dump SPU program from PPU executable: name='%s', hash=%s", name, hash);
			}
		}

		// Try to patch each segment, will only succeed if the address exists in SPU local storage
		for (const auto& prog : obj.progs)
		{
			// Apply the patch
			applied += g_fxo->get<patch_engine>().apply(hash, (elf_header + prog.p_offset), prog.p_filesz, prog.p_vaddr);

			if (!Emu.GetTitleID().empty())
			{
				// Alternative patch
				applied += g_fxo->get<patch_engine>().apply(Emu.GetTitleID() + '-' + hash, (elf_header + prog.p_offset), prog.p_filesz, prog.p_vaddr);
			}
		}

		if (applied.empty())
		{
			ppu_loader.warning("SPU executable hash: %s%s", hash, dump);
		}
		else
		{
			ppu_loader.success("SPU executable hash: %s (<- %u)%s", hash, applied.size(), dump);
		}
	}
}

void try_spawn_ppu_if_exclusive_program(const ppu_module& m)
{
	// If only PRX/OVL has been loaded at Emu.BootGame(), launch a single PPU thread so its memory can be viewed
	if (Emu.IsReady() && g_fxo->get<main_ppu_module>().segs.empty() && !Emu.DeserialManager())
	{
		ppu_thread_params p
		{
			.stack_addr = vm::cast(vm::alloc(SYS_PROCESS_PARAM_STACK_SIZE_MAX, vm::stack, 4096)),
			.stack_size = SYS_PROCESS_PARAM_STACK_SIZE_MAX,
		};

		auto ppu = idm::make_ptr<named_thread<ppu_thread>>(p, "test_thread", 0);

		ppu->cia = m.funcs.empty() ? m.secs[0].addr : m.funcs[0].addr;

		// For kernel explorer
		g_fxo->init<lv2_memory_container>(4096);
	}
}

struct prx_names_table
{
	shared_mutex mutex;
	std::set<std::string, std::less<>> registered;
	atomic_t<const char*> lut[0x1000'0000 / 0x1'0000]{};

	SAVESTATE_INIT_POS(4.1); // Dependency on lv2_obj

	prx_names_table() noexcept
	{
		idm::select<lv2_obj, lv2_prx>([this](u32, lv2_prx& prx)
		{
			install(prx.name, prx);
		});
	}

	void install(std::string_view name, lv2_prx& prx)
	{
		if (name.empty())
		{
			return;
		}

		if (name.ends_with(".sprx"sv) && name.size() > (".sprx"sv).size())
		{
			name = name.substr(0, name.size() - (".sprx"sv).size());
		}

		std::lock_guard lock(mutex);

		const auto ptr = registered.emplace(name).first->c_str();

		for (auto& seg : prx.segs)
		{
			if (!seg.size)
			{
				continue;
			}

			// Doesn't support addresses above 256MB because it wastes memory and is very unlikely (if somehow does occur increase it)
			const u32 max0 = (seg.addr + seg.size - 1) >> 16;
			const u32 max = std::min<u32>(std::size(lut), max0);

			if (max0 > max)
			{
				ppu_loader.error("Skipping PRX name registeration: %s, max=0x%x", name, max0 << 16);
			}

			for (u32 i = seg.addr >> 16; i <= max; i++)
			{
				lut[i].release(ptr);
			}
		}
	}
};

const char* get_prx_name_by_cia(u32 addr)
{
	if (auto t = g_fxo->try_get<prx_names_table>())
	{
		addr >>= 16;

		if (addr < std::size(t->lut))
		{
			return t->lut[addr];
		}
	}

	return nullptr;
}

std::shared_ptr<lv2_prx> ppu_load_prx(const ppu_prx_object& elf, const std::string& path, s64 file_offset, utils::serial* ar)
{
	if (elf != elf_error::ok)
	{
		return nullptr;
	}

	// Create new PRX object
	const auto prx = !ar ? idm::make_ptr<lv2_obj, lv2_prx>() : std::make_shared<lv2_prx>();

	// Access linkage information object
	auto& link = g_fxo->get<ppu_linkage_info>();

	// Initialize HLE modules
	ppu_initialize_modules(&link);

	// Library hash
	sha1_context sha;
	sha1_starts(&sha);

	u32 end = 0;
	u32 toc = 0;

	for (const auto& prog : elf.progs)
	{
		ppu_loader.notice("** Segment: p_type=0x%x, p_vaddr=0x%llx, p_filesz=0x%llx, p_memsz=0x%llx, flags=0x%x", prog.p_type, prog.p_vaddr, prog.p_filesz, prog.p_memsz, prog.p_flags);

		// Hash big-endian values
		sha1_update(&sha, reinterpret_cast<const uchar*>(&prog.p_type), sizeof(prog.p_type));
		sha1_update(&sha, reinterpret_cast<const uchar*>(&prog.p_flags), sizeof(prog.p_flags));

		switch (const u32 p_type = prog.p_type)
		{
		case 0x1: // LOAD
		{
			auto& _seg = prx->segs.emplace_back();
			_seg.flags = prog.p_flags;
			_seg.type = p_type;

			if (prog.p_memsz)
			{
				const u32 mem_size = ::narrow<u32>(prog.p_memsz);
				const u32 file_size = ::narrow<u32>(prog.p_filesz);
				//const u32 init_addr = ::narrow<u32>(prog.p_vaddr);

				// Alloc segment memory
				// Or use saved address
				const u32 addr = !ar ? vm::alloc(mem_size, vm::main) : ar->operator u32();

				if (!vm::check_addr(addr))
				{
					fmt::throw_exception("vm::alloc() failed (size=0x%x)", mem_size);
				}

				// Copy segment data
				if (!ar) std::memcpy(vm::base(addr), prog.bin.data(), file_size);
				ppu_loader.warning("**** Loaded to 0x%x...0x%x (size=0x%x)", addr, addr + mem_size - 1, mem_size);

				// Hash segment
				sha1_update(&sha, reinterpret_cast<const uchar*>(&prog.p_vaddr), sizeof(prog.p_vaddr));
				sha1_update(&sha, reinterpret_cast<const uchar*>(&prog.p_memsz), sizeof(prog.p_memsz));
				sha1_update(&sha, prog.bin.data(), prog.bin.size());

				// Initialize executable code if necessary
				if (prog.p_flags & 0x1)
				{
					if (ar)
					{
						// Disable analysis optimization for savestates (it's not compatible with savestate with patches applied)
						end = std::max(end, utils::align<u32>(addr + mem_size, 0x10000));
					}

					ppu_register_range(addr, mem_size);
				}

				_seg.addr = addr;
				_seg.size = mem_size;
				_seg.filesz = file_size;
			}

			break;
		}

		case 0x700000a4: break; // Relocations

		default: ppu_loader.error("Unknown segment type! 0x%08x", p_type);
		}
	}

	for (const auto& s : elf.shdrs)
	{
		ppu_loader.notice("** Section: sh_type=0x%x, addr=0x%llx, size=0x%llx, flags=0x%x", std::bit_cast<u32>(s.sh_type), s.sh_addr, s.sh_size, s._sh_flags);

		if (s.sh_type != sec_type::sht_progbits) continue;

		const u32 addr = vm::cast(s.sh_addr);
		const u32 size = vm::cast(s.sh_size);

		if (addr && size) // TODO: some sections with addr=0 are valid
		{
			for (usz i = 0; i < prx->segs.size(); i++)
			{
				const u32 saddr = static_cast<u32>(elf.progs[i].p_vaddr);
				if (addr >= saddr && addr < saddr + elf.progs[i].p_memsz)
				{
					// "Relocate" section
					ppu_segment _sec;
					_sec.addr = addr - saddr + prx->segs[i].addr;
					_sec.size = size;
					_sec.type = std::bit_cast<u32>(s.sh_type);
					_sec.flags = static_cast<u32>(s._sh_flags & 7);
					_sec.filesz = 0;
					prx->secs.emplace_back(_sec);

					if (_sec.flags & 0x4 && i == 0)
					{
						end = std::max<u32>(end, _sec.addr + _sec.size);
					}
					break;
				}
			}
		}
	}

	// Do relocations
	for (auto& prog : elf.progs)
	{
		switch (prog.p_type)
		{
		case 0x700000a4:
		{
			// Relocation information of the SCE_PPURELA segment
			struct ppu_prx_relocation_info
			{
				be_t<u64> offset;
				be_t<u16> unk0;
				u8 index_value;
				u8 index_addr;
				be_t<u32> type;
				vm::bptr<void, u64> ptr;
			};

			for (uint i = 0; i < prog.p_filesz; i += sizeof(ppu_prx_relocation_info))
			{
				const auto& rel = reinterpret_cast<const ppu_prx_relocation_info&>(prog.bin[i]);

				if (rel.offset >= ::at32(prx->segs, rel.index_addr).size)
				{
					fmt::throw_exception("Relocation offset out of segment memory! (offset=0x%x, index_addr=%u)", rel.offset, rel.index_addr);
				}

				const u32 data_base = rel.index_value == 0xFF ? 0 : ::at32(prx->segs, rel.index_value).addr;

				if (rel.index_value != 0xFF && !data_base)
				{
					fmt::throw_exception("Empty segment has been referenced for relocation data! (reloc_offset=0x%x, index_value=%u)", i, rel.index_value);
				}

				ppu_reloc _rel;
				const u32 raddr = _rel.addr = vm::cast(::at32(prx->segs, rel.index_addr).addr + rel.offset);
				const u32 rtype = _rel.type = rel.type;
				const u64 rdata = _rel.data = data_base + rel.ptr.addr();
				prx->relocs.emplace_back(_rel);

				if (ar)
				{
					break;
				}

				switch (rtype)
				{
				case 1: // R_PPC64_ADDR32
				{
					const u32 value = vm::_ref<u32>(raddr) = static_cast<u32>(rdata);
					ppu_loader.trace("**** RELOCATION(1): 0x%x <- 0x%08x (0x%llx)", raddr, value, rdata);
					break;
				}

				case 4: //R_PPC64_ADDR16_LO
				{
					const u16 value = vm::_ref<u16>(raddr) = static_cast<u16>(rdata);
					ppu_loader.trace("**** RELOCATION(4): 0x%x <- 0x%04x (0x%llx)", raddr, value, rdata);
					break;
				}

				case 5: //R_PPC64_ADDR16_HI
				{
					const u16 value = vm::_ref<u16>(raddr) = static_cast<u16>(rdata >> 16);
					ppu_loader.trace("**** RELOCATION(5): 0x%x <- 0x%04x (0x%llx)", raddr, value, rdata);
					break;
				}

				case 6: //R_PPC64_ADDR16_HA
				{
					const u16 value = vm::_ref<u16>(raddr) = static_cast<u16>(rdata >> 16) + (rdata & 0x8000 ? 1 : 0);
					ppu_loader.trace("**** RELOCATION(6): 0x%x <- 0x%04x (0x%llx)", raddr, value, rdata);
					break;
				}

				case 10: //R_PPC64_REL24
				{
					const u32 value = vm::_ref<ppu_bf_t<be_t<u32>, 6, 24>>(raddr) = static_cast<u32>(rdata - raddr) >> 2;
					ppu_loader.warning("**** RELOCATION(10): 0x%x <- 0x%06x (0x%llx)", raddr, value, rdata);
					break;
				}

				case 11: //R_PPC64_REL14
				{
					const u32 value = vm::_ref<ppu_bf_t<be_t<u32>, 16, 14>>(raddr) = static_cast<u32>(rdata - raddr) >> 2;
					ppu_loader.warning("**** RELOCATION(11): 0x%x <- 0x%06x (0x%llx)", raddr, value, rdata);
					break;
				}

				case 38: //R_PPC64_ADDR64
				{
					const u64 value = vm::_ref<u64>(raddr) = rdata;
					ppu_loader.trace("**** RELOCATION(38): 0x%x <- 0x%016llx (0x%llx)", raddr, value, rdata);
					break;
				}

				case 44: //R_PPC64_REL64
				{
					const u64 value = vm::_ref<u64>(raddr) = rdata - raddr;
					ppu_loader.trace("**** RELOCATION(44): 0x%x <- 0x%016llx (0x%llx)", raddr, value, rdata);
					break;
				}

				case 57: //R_PPC64_ADDR16_LO_DS
				{
					const u16 value = vm::_ref<ppu_bf_t<be_t<u16>, 0, 14>>(raddr) = static_cast<u16>(rdata) >> 2;
					ppu_loader.trace("**** RELOCATION(57): 0x%x <- 0x%04x (0x%llx)", raddr, value, rdata);
					break;
				}

				default: ppu_loader.error("**** RELOCATION(%u): Illegal/Unknown type! (addr=0x%x; 0x%llx)", rtype, raddr, rdata);
				}

				if (rdata == 0)
				{
					ppu_loader.todo("**** RELOCATION(%u): 0x%x <- (zero-based value)", rtype, raddr);
				}
			}

			break;
		}
		default : break;
		}
	}

	if (!elf.progs.empty() && elf.progs[0].p_paddr)
	{
		struct ppu_prx_library_info
		{
			be_t<u16> attributes;
			u8 version[2];
			char name[28];
			be_t<u32> toc;
			be_t<u32> exports_start;
			be_t<u32> exports_end;
			be_t<u32> imports_start;
			be_t<u32> imports_end;
		};

		// Access library information (TODO)
		const vm::cptr<ppu_prx_library_info> lib_info = vm::cast(prx->segs[0].addr + elf.progs[0].p_paddr - elf.progs[0].p_offset);
		const std::string lib_name = lib_info->name;

		strcpy_trunc(prx->module_info_name, lib_name);
		prx->module_info_version[0] = lib_info->version[0];
		prx->module_info_version[1] = lib_info->version[1];
		prx->module_info_attributes = lib_info->attributes;

		prx->exports_start = lib_info->exports_start;
		prx->exports_end = lib_info->exports_end;

		for (usz start = prx->exports_start, size = 0;; size++, start += vm::read8(start) ? vm::read8(start) : sizeof(ppu_prx_module_info))
		{
			if (start >= prx->exports_end)
			{
				// Preallocate storage
				prx->m_external_loaded_flags.resize(size);
				break;
			}
		}

		ppu_loader.warning("Library %s (rtoc=0x%x):", lib_name, lib_info->toc);

		prx->specials = ppu_load_exports(&link, prx->exports_start, prx->exports_end, true);
		prx->imports = ppu_load_imports(prx->relocs, &link, lib_info->imports_start, lib_info->imports_end);
		std::stable_sort(prx->relocs.begin(), prx->relocs.end());
		toc = lib_info->toc;
	}
	else
	{
		ppu_loader.fatal("Library %s: PRX library info not found");
	}

	prx->start.set(prx->specials[0xbc9a0086]);
	prx->stop.set(prx->specials[0xab779874]);
	prx->exit.set(prx->specials[0x3ab9a95e]);
	prx->prologue.set(prx->specials[0x0d10fd3f]);
	prx->epilogue.set(prx->specials[0x330f7005]);
	prx->name = path.substr(path.find_last_of('/') + 1);
	prx->path = path;
	prx->offset = file_offset;

	g_fxo->need<prx_names_table>();
	g_fxo->get<prx_names_table>().install(prx->name, *prx);

	sha1_finish(&sha, prx->sha1);

	// Format patch name
	std::string hash = fmt::format("PRX-%s", fmt::base57(prx->sha1));

	if (prx->path.ends_with("sys/external/liblv2.sprx"sv))
	{
		liblv2_begin = prx->segs[0].addr;
		liblv2_end = prx->segs[0].addr + prx->segs[0].size;
	}

	std::basic_string<u32> applied;

	for (usz i = 0; i < prx->segs.size(); i++)
	{
		const auto& seg = prx->segs[i];

		if (!seg.size) continue;

		const std::string hash_seg = fmt::format("%s-%u", hash, i);

		// Apply the patch
		auto _applied = g_fxo->get<patch_engine>().apply(hash_seg, vm::get_super_ptr(seg.addr), seg.size);

		if (!Emu.GetTitleID().empty())
		{
			// Alternative patch
			_applied += g_fxo->get<patch_engine>().apply(Emu.GetTitleID() + '-' + hash_seg, vm::get_super_ptr(seg.addr), seg.size);
		}

		// Rebase patch offsets
		std::for_each(_applied.begin(), _applied.end(), [&](u32& res) { if (res != umax) res += seg.addr; });

		applied += _applied;

		if (_applied.empty())
		{
			ppu_loader.warning("PRX hash of %s[%u]: %s", prx->name, i, hash_seg);
		}
		else
		{
			ppu_loader.success("PRX hash of %s[%u]: %s (<- %u)", prx->name, i, hash_seg, _applied.size());
		}
	}

	// Embedded SPU elf patching
	for (const auto& seg : prx->segs)
	{
		ppu_check_patch_spu_images(seg);
	}

	prx->analyse(toc, 0, end, applied);

	try_spawn_ppu_if_exclusive_program(*prx);

	return prx;
}

void ppu_unload_prx(const lv2_prx& prx)
{
	std::unique_lock lock(g_fxo->get<ppu_linkage_info>().mutex, std::defer_lock);

	// Clean linkage info
	for (auto& imp : prx.imports)
	{
		if (!lock)
		{
			lock.lock();
		}

		auto pinfo = static_cast<ppu_linkage_info::module_data::info*>(imp.second);
		pinfo->frefss.erase(imp.first);
		pinfo->imports.erase(imp.first);
	}

	//for (auto& exp : prx.exports)
	//{
	//	auto pinfo = static_cast<ppu_linkage_info::module_data::info*>(exp.second);
	//	if (pinfo->static_func)
	//	{
	//		pinfo->export_addr = g_fxo->get<ppu_function_manager>().func_addr(pinfo->static_func->index);
	//	}
	//	else if (pinfo->static_var)
	//	{
	//		pinfo->export_addr = pinfo->static_var->addr;
	//	}
	//	else
	//	{
	//		pinfo->export_addr = 0;
	//	}
	//}

	if (lock)
	{
		lock.unlock();
	}

	if (prx.path.ends_with("sys/external/liblv2.sprx"sv))
	{
		liblv2_begin = 0;
		liblv2_end = 0;
	}

	// Format patch name
	std::string hash = fmt::format("PRX-%s", fmt::base57(prx.sha1));

	for (auto& seg : prx.segs)
	{
		if (!seg.size) continue;

		vm::dealloc(seg.addr, vm::main);

		const std::string hash_seg = fmt::format("%s-%u", hash, &seg - prx.segs.data());

		// Deallocatte memory used for patches
		g_fxo->get<patch_engine>().unload(hash_seg);

		if (!Emu.GetTitleID().empty())
		{
			// Alternative patch
			g_fxo->get<patch_engine>().unload(Emu.GetTitleID() + '-' + hash_seg);
		}
	}
}

bool ppu_load_exec(const ppu_exec_object& elf, utils::serial* ar)
{
	if (elf != elf_error::ok)
	{
		return false;
	}

	// Check if it is a standalone executable first
	for (const auto& prog : elf.progs)
	{
		if (prog.p_type == 0x1u /* LOAD */ && prog.p_memsz)
		{
			using addr_range = utils::address_range;

			const addr_range r = addr_range::start_length(static_cast<u32>(prog.p_vaddr), static_cast<u32>(prog.p_memsz));

			if ((prog.p_vaddr | prog.p_memsz) > u32{umax} || !r.valid() || !r.inside(addr_range::start_length(0x00000000, 0x30000000)))
			{
				return false;
			}
		}
	}

	init_ppu_functions(ar, false);

	// Set for delayed initialization in ppu_initialize()
	auto& _main = g_fxo->get<main_ppu_module>();

	// Access linkage information object
	auto& link = g_fxo->get<ppu_linkage_info>();

	// TLS information
	u32 tls_vaddr = 0;
	u32 tls_fsize = 0;
	u32 tls_vsize = 0;

	// Process information
	u32 sdk_version = SYS_PROCESS_PARAM_SDK_VERSION_UNKNOWN;
	s32 primary_prio = 1001;
	u32 primary_stacksize = SYS_PROCESS_PARAM_STACK_SIZE_MAX;
	u32 malloc_pagesize = SYS_PROCESS_PARAM_MALLOC_PAGE_SIZE_1M;
	u32 ppc_seg = 0;

	// Limit for analysis
	u32 end = 0;

	// Executable hash
	sha1_context sha;
	sha1_starts(&sha);

	struct on_fatal_error
	{
		ppu_module& _main;
		bool errored = true;

		~on_fatal_error()
		{
			if (!errored)
			{
				return;
			}

			// Revert previous allocations on an error
			for (const auto& seg : _main.segs)
			{
				vm::dealloc(seg.addr);
			}
		}

	} error_handler{_main};

	// Allocate memory at fixed positions
	for (const auto& prog : elf.progs)
	{
		ppu_loader.notice("** Segment: p_type=0x%x, p_vaddr=0x%llx, p_filesz=0x%llx, p_memsz=0x%llx, flags=0x%x", prog.p_type, prog.p_vaddr, prog.p_filesz, prog.p_memsz, prog.p_flags);

		ppu_segment _seg;
		const u32 addr = _seg.addr = vm::cast(prog.p_vaddr);
		const u32 size = _seg.size = ::narrow<u32>(prog.p_memsz);
		const u32 type = _seg.type = prog.p_type;

		_seg.flags  = prog.p_flags;
		_seg.filesz = ::narrow<u32>(prog.p_filesz);

		// Hash big-endian values
		sha1_update(&sha, reinterpret_cast<const uchar*>(&prog.p_type), sizeof(prog.p_type));
		sha1_update(&sha, reinterpret_cast<const uchar*>(&prog.p_flags), sizeof(prog.p_flags));

		if (type == 0x1 /* LOAD */ && prog.p_memsz)
		{
			if (prog.bin.size() > size || prog.bin.size() != prog.p_filesz)
			{
				ppu_loader.fatal("ppu_load_exec(): Invalid binary size (0x%llx, memsz=0x%x)", prog.bin.size(), size);
				return false;
			}

			const bool already_loaded = ar && vm::check_addr(addr, vm::page_readable, size);

			if (already_loaded)
			{
			}
			else if (!vm::falloc(addr, size, vm::main))
			{
				ppu_loader.error("vm::falloc(vm::main) failed (addr=0x%x, memsz=0x%x)", addr, size); // TODO

				if (!vm::falloc(addr, size))
				{
					ppu_loader.fatal("ppu_load_exec(): vm::falloc() failed (addr=0x%x, memsz=0x%x)", addr, size);
					return false;
				}
			}

			// Copy segment data, hash it
			if (!already_loaded)
			{
				std::memcpy(vm::base(addr), prog.bin.data(), prog.bin.size());
			}
			else
			{
				// For backwards compatibility: already loaded memory will always be writable
				const u32 size0 = utils::align(size + addr % 0x10000, 0x10000);
				const u32 addr0 = addr & -0x10000;
				vm::page_protect(addr0, size0, 0, vm::page_writable | vm::page_readable, vm::page_executable);
			}

			sha1_update(&sha, reinterpret_cast<const uchar*>(&prog.p_vaddr), sizeof(prog.p_vaddr));
			sha1_update(&sha, reinterpret_cast<const uchar*>(&prog.p_memsz), sizeof(prog.p_memsz));
			sha1_update(&sha, prog.bin.data(), prog.bin.size());

			// Initialize executable code if necessary
			if (prog.p_flags & 0x1)
			{
				if (already_loaded && ar)
				{
					// Disable analysis optimization for savestates (it's not compatible with savestate with patches applied)
					end = std::max(end, utils::align<u32>(addr + size, 0x10000));
				}

				ppu_register_range(addr, size);
			}

			// Store only LOAD segments (TODO)
			_main.segs.emplace_back(_seg);
		}
	}

	// Load section list, used by the analyser
	for (const auto& s : elf.shdrs)
	{
		ppu_loader.notice("** Section: sh_type=0x%x, addr=0x%llx, size=0x%llx, flags=0x%x", std::bit_cast<u32>(s.sh_type), s.sh_addr, s.sh_size, s._sh_flags);

		if (s.sh_type != sec_type::sht_progbits) continue;

		ppu_segment _sec;
		const u32 addr = _sec.addr = vm::cast(s.sh_addr);
		const u32 size = _sec.size = vm::cast(s.sh_size);

		_sec.type = std::bit_cast<u32>(s.sh_type);
		_sec.flags = static_cast<u32>(s._sh_flags & 7);
		_sec.filesz = 0;

		if (addr && size)
		{
			_main.secs.emplace_back(_sec);

			if (_sec.flags & 0x4 && addr >= _main.segs[0].addr && addr + size <= _main.segs[0].addr + _main.segs[0].size)
			{
				end = std::max<u32>(end, addr + size);
			}
		}
	}

	sha1_finish(&sha, _main.sha1);

	// Format patch name
	std::string hash("PPU-0000000000000000000000000000000000000000");
	for (u32 i = 0; i < 20; i++)
	{
		constexpr auto pal = "0123456789abcdef";
		hash[4 + i * 2] = pal[_main.sha1[i] >> 4];
		hash[5 + i * 2] = pal[_main.sha1[i] & 15];
	}

	Emu.SetExecutableHash(hash);

	// Apply the patch
	auto applied = g_fxo->get<patch_engine>().apply(!ar ? hash : std::string{}, vm::g_base_addr);

	if (!ar && !Emu.GetTitleID().empty())
	{
		// Alternative patch
		applied += g_fxo->get<patch_engine>().apply(Emu.GetTitleID() + '-' + hash, vm::g_base_addr);
	}

	if (applied.empty())
	{
		ppu_loader.warning("PPU executable hash: %s", hash);
	}
	else
	{
		ppu_loader.success("PPU executable hash: %s (<- %u)", hash, applied.size());
	}

	// Initialize HLE modules
	ppu_initialize_modules(&link, ar);

	// Embedded SPU elf patching
	for (const auto& seg : _main.segs)
	{
		ppu_check_patch_spu_images(seg);
	}

	// Static HLE patching
	if (g_cfg.core.hook_functions)
	{
		auto shle = g_fxo->init<statichle_handler>(0);

		for (u32 i = _main.segs[0].addr; i < (_main.segs[0].addr + _main.segs[0].size); i += 4)
		{
			vm::cptr<u8> _ptr = vm::cast(i);
			shle->check_against_patterns(_ptr, (_main.segs[0].addr + _main.segs[0].size) - i, i);
		}
	}

	// Read control flags (0 if doesn't exist)
	g_ps3_process_info.ctrl_flags1 = 0;

	if (bool not_found = g_ps3_process_info.self_info.valid)
	{
		for (const auto& ctrl : g_ps3_process_info.self_info.ctrl_info)
		{
			if (ctrl.type == 1)
			{
				if (!std::exchange(not_found, false))
				{
					ppu_loader.error("More than one control flags header found! (flags1=0x%x)",
						ctrl.control_flags.ctrl_flag1);
					break;
				}

				g_ps3_process_info.ctrl_flags1 |= ctrl.control_flags.ctrl_flag1;
			}
		}

		ppu_loader.notice("SELF header information found: ctrl_flags1=0x%x, authid=0x%llx",
			g_ps3_process_info.ctrl_flags1, g_ps3_process_info.self_info.app_info.authid);
	}

	// Load other programs
	for (auto& prog : elf.progs)
	{
		switch (const u32 p_type = prog.p_type)
		{
		case 0x00000001: break; // LOAD (already loaded)

		case 0x00000007: // TLS
		{
			ppu_loader.notice("TLS info segment found: tls-image=*0x%x, image-size=0x%x, tls-size=0x%x", prog.p_vaddr, prog.p_filesz, prog.p_memsz);

			if ((prog.p_vaddr | prog.p_filesz | prog.p_memsz) > u32{umax})
			{
				ppu_loader.fatal("ppu_load_exec(): TLS segment is invalid!");
				return false;
			}

			tls_vaddr = vm::cast(prog.p_vaddr);
			tls_fsize = ::narrow<u32>(prog.p_filesz);
			tls_vsize = ::narrow<u32>(prog.p_memsz);
			break;
		}

		case 0x60000001: // LOOS+1
		{
			if (prog.p_filesz)
			{
				struct process_param_t
				{
					be_t<u32> size;
					be_t<u32> magic;
					be_t<u32> version;
					be_t<u32> sdk_version;
					be_t<s32> primary_prio;
					be_t<u32> primary_stacksize;
					be_t<u32> malloc_pagesize;
					be_t<u32> ppc_seg;
					//be_t<u32> crash_dump_param_addr;
				};

				const auto& info = vm::_ref<process_param_t>(vm::cast(prog.p_vaddr));

				if (info.size < sizeof(process_param_t))
				{
					ppu_loader.warning("Bad process_param size! [0x%x : 0x%x]", info.size, sizeof(process_param_t));
				}

				if (info.magic != SYS_PROCESS_PARAM_MAGIC)
				{
					ppu_loader.error("Bad process_param magic! [0x%x]", info.magic);
				}
				else
				{
					sdk_version = info.sdk_version;

					if (s32 prio = info.primary_prio; prio < 3072
						&& (prio >= (g_ps3_process_info.debug_or_root() ? 0 : -512)))
					{
						primary_prio = prio;
					}

					primary_stacksize = info.primary_stacksize;
					malloc_pagesize = info.malloc_pagesize;
					ppc_seg = info.ppc_seg;

					ppu_loader.notice("*** sdk version: 0x%x", info.sdk_version);
					ppu_loader.notice("*** primary prio: %d", info.primary_prio);
					ppu_loader.notice("*** primary stacksize: 0x%x", info.primary_stacksize);
					ppu_loader.notice("*** malloc pagesize: 0x%x", info.malloc_pagesize);
					ppu_loader.notice("*** ppc seg: 0x%x", info.ppc_seg);
					//ppu_loader.notice("*** crash dump param addr: 0x%x", info.crash_dump_param_addr);
				}
			}
			break;
		}

		case 0x60000002: // LOOS+2
		{
			if (prog.p_filesz)
			{
				struct ppu_proc_prx_param_t
				{
					be_t<u32> size;
					be_t<u32> magic;
					be_t<u32> version;
					be_t<u32> unk0;
					be_t<u32> libent_start;
					be_t<u32> libent_end;
					be_t<u32> libstub_start;
					be_t<u32> libstub_end;
					be_t<u16> ver;
					be_t<u16> unk1;
					be_t<u32> unk2;
				};

				const auto& proc_prx_param = vm::_ref<const ppu_proc_prx_param_t>(vm::cast(prog.p_vaddr));

				ppu_loader.notice("* libent_start = *0x%x", proc_prx_param.libent_start);
				ppu_loader.notice("* libstub_start = *0x%x", proc_prx_param.libstub_start);
				ppu_loader.notice("* unk0 = 0x%x", proc_prx_param.unk0);
				ppu_loader.notice("* unk2 = 0x%x", proc_prx_param.unk2);

				if (proc_prx_param.magic != 0x1b434cecu)
				{
					ppu_loader.fatal("ppu_load_exec(): Bad magic! (0x%x)", proc_prx_param.magic);
					return false;
				}

				ppu_load_exports(&link, proc_prx_param.libent_start, proc_prx_param.libent_end);
				ppu_load_imports(_main.relocs, &link, proc_prx_param.libstub_start, proc_prx_param.libstub_end);
				std::stable_sort(_main.relocs.begin(), _main.relocs.end());
			}
			break;
		}
		default:
		{
			ppu_loader.error("Unknown phdr type (0x%08x)", p_type);
		}
		}
	}

	// Initialize memory stats (according to sdk version)
	u32 mem_size;
	if (Emu.IsVsh())
	{
		// Because vsh.self comes before any generic application, more memory is available to it
		mem_size = 0xF000000;
	}
	else if (sdk_version > 0x0021FFFF)
	{
		mem_size = 0xD500000;
	}
	else if (sdk_version > 0x00192FFF)
	{
		mem_size = 0xD300000;
	}
	else if (sdk_version > 0x0018FFFF)
	{
		mem_size = 0xD100000;
	}
	else if (sdk_version > 0x0017FFFF)
	{
		mem_size = 0xD000000;
	}
	else if (sdk_version > 0x00154FFF)
	{
		mem_size = 0xCC00000;
	}
	else
	{
		mem_size = 0xC800000;
	}

	if (g_cfg.core.debug_console_mode)
	{
		// TODO: Check for all sdk versions
		mem_size += 0xC000000;
	}

	// Initialize process
	std::vector<std::shared_ptr<lv2_prx>> loaded_modules;

	// Module list to load at startup
	std::set<std::string> load_libs;

	if (g_cfg.core.libraries_control.get_set().count("liblv2.sprx:lle") || !g_cfg.core.libraries_control.get_set().count("liblv2.sprx:hle"))
	{
		// Will load libsysmodule.sprx internally
		load_libs.emplace("liblv2.sprx");
	}
	else if (g_cfg.core.libraries_control.get_set().count("libsysmodule.sprx:lle") || !g_cfg.core.libraries_control.get_set().count("libsysmodule.sprx:hle"))
	{
		// Load only libsysmodule.sprx
		load_libs.emplace("libsysmodule.sprx");
	}

	if (ar || Emu.IsVsh())
	{
		// Cannot be used with vsh.self or savestates (they self-manage itself)
		load_libs.clear();
	}

	const std::string lle_dir = vfs::get("/dev_flash/sys/external/");

	if (!fs::is_file(lle_dir + "liblv2.sprx"))
	{
		ppu_loader.error("PS3 firmware is not installed or the installed firmware is invalid."
			"\nYou should install the PS3 Firmware (Menu: File -> Install Firmware)."
			"\nVisit https://rpcs3.net/ for Quickstart Guide and more information.");
	}

	// Program entry
	u32 entry = 0;

	// Set path (TODO)
	_main.name.clear();
	_main.path = vfs::get(Emu.argv[0]);

	_main.elf_entry = static_cast<u32>(elf.header.e_entry);
	_main.seg0_code_end = end;
	_main.applied_pathes = applied;

	// Set SDK version
	g_ps3_process_info.sdk_ver = sdk_version;

	// Set ppc fixed allocations segment permission
	g_ps3_process_info.ppc_seg = ppc_seg;

	if (Emu.init_mem_containers)
	{
		// Refer to sys_process_exit2 for explanation
		Emu.init_mem_containers(mem_size);
	}
	else if (!ar)
	{
		g_fxo->init<id_manager::id_map<lv2_memory_container>>();
		g_fxo->init<lv2_memory_container>(mem_size);
	}

	void init_fxo_for_exec(utils::serial* ar, bool full);
	init_fxo_for_exec(ar, false);

	liblv2_begin = 0;
	liblv2_end = 0;

	if (!load_libs.empty())
	{
		for (const auto& name : load_libs)
		{
			const ppu_prx_object obj = decrypt_self(fs::file(lle_dir + name));

			if (obj == elf_error::ok)
			{
				ppu_loader.warning("Loading library: %s", name);

				auto prx = ppu_load_prx(obj, lle_dir + name, 0, nullptr);
				prx->state = PRX_STATE_STARTED;
				prx->load_exports();

				if (prx->funcs.empty())
				{
					ppu_loader.fatal("Module %s has no functions!", name);
				}
				else
				{
					// TODO: fix arguments
					prx->validate(prx->funcs[0].addr);
				}

				if (name == "liblv2.sprx")
				{
 					// Run liblv2.sprx entry point (TODO)
					entry = prx->start.addr();
				}
				else
				{
					loaded_modules.emplace_back(std::move(prx));
				}
			}
			else
			{
				ppu_loader.error("Failed to load /dev_flash/sys/external/%s: %s (forcing HLE implementation)", name, obj.get_error());
			}
		}
	}

	if (ar)
	{
		error_handler.errored = false;
		return true;
	}

	if (ppc_seg != 0x0)
	{
		if (ppc_seg != 0x1)
		{
			ppu_loader.todo("Unknown ppc_seg flag value = 0x%x", ppc_seg);
		}

		// Additional segment for fixed allocations
		if (!vm::map(0x30000000, 0x10000000, 0x200))
		{
			fmt::throw_exception("Failed to map ppc_seg's segment!");
		}
	}

	// Initialize process arguments
	auto args = vm::ptr<u64>::make(vm::alloc(u32{sizeof(u64)} * (::size32(Emu.argv) + ::size32(Emu.envp) + 2), vm::main));
	auto argv = args;

	for (const auto& arg : Emu.argv)
	{
		const u32 arg_size = ::size32(arg) + 1;
		const u32 arg_addr = vm::alloc(arg_size, vm::main);

		std::memcpy(vm::base(arg_addr), arg.data(), arg_size);

		*args++ = arg_addr;
	}

	*args++ = 0;
	auto envp = args;

	for (const auto& arg : Emu.envp)
	{
		const u32 arg_size = ::size32(arg) + 1;
		const u32 arg_addr = vm::alloc(arg_size, vm::main);

		std::memcpy(vm::base(arg_addr), arg.data(), arg_size);

		*args++ = arg_addr;
	}

	// Fix primary stack size
	switch (u32 sz = primary_stacksize)
	{
	case SYS_PROCESS_PRIMARY_STACK_SIZE_32K:  primary_stacksize =   32 * 1024; break;
	case SYS_PROCESS_PRIMARY_STACK_SIZE_64K:  primary_stacksize =   64 * 1024; break;
	case SYS_PROCESS_PRIMARY_STACK_SIZE_96K:  primary_stacksize =   96 * 1024; break;
	case SYS_PROCESS_PRIMARY_STACK_SIZE_128K: primary_stacksize =  128 * 1024; break;
	case SYS_PROCESS_PRIMARY_STACK_SIZE_256K: primary_stacksize =  256 * 1024; break;
	case SYS_PROCESS_PRIMARY_STACK_SIZE_512K: primary_stacksize =  512 * 1024; break;
	case SYS_PROCESS_PRIMARY_STACK_SIZE_1M:   primary_stacksize = 1024 * 1024; break;
	default:
	{
		// According to elad335, the min value seems to be 64KB instead of the expected 4KB (SYS_PROCESS_PARAM_STACK_SIZE_MIN)
		primary_stacksize = utils::align<u32>(std::clamp<u32>(sz, 0x10000, SYS_PROCESS_PARAM_STACK_SIZE_MAX), 4096);
		break;
	}
	}

	// Initialize main thread
	ppu_thread_params p{};
	p.stack_addr = vm::cast(vm::alloc(primary_stacksize, vm::stack, 4096));
	p.stack_size = primary_stacksize;

	auto ppu = idm::make_ptr<named_thread<ppu_thread>>(p, "main_thread", primary_prio, 1);

	// Write initial data (exitspawn)
	if (!Emu.data.empty())
	{
		std::memcpy(vm::base(ppu->stack_addr + ppu->stack_size - ::size32(Emu.data)), Emu.data.data(), Emu.data.size());
		ppu->gpr[1] -= Emu.data.size();
	}

	ensure(g_fxo->get<lv2_memory_container>().take(primary_stacksize));

	ppu->cmd_push({ppu_cmd::initialize, 0});

	if (!entry && !Emu.IsVsh())
	{
		// Set TLS args, call sys_initialize_tls
		ppu->cmd_list
		({
			{ ppu_cmd::set_args, 4 }, u64{ppu->id}, u64{tls_vaddr}, u64{tls_fsize}, u64{tls_vsize},
			{ ppu_cmd::hle_call, FIND_FUNC(sys_initialize_tls) },
		});
	}

	if (!entry)
	{
		entry = static_cast<u32>(elf.header.e_entry); // Run entry from elf
	}

	// Run start functions
	for (const auto& prx : loaded_modules)
	{
		if (!prx->start)
		{
			continue;
		}

		// Reset arguments, run module entry point function
		ppu->cmd_list
		({
			{ ppu_cmd::set_args, 2 }, u64{0}, u64{0},
			{ ppu_cmd::lle_call, prx->start.addr() },
		});
	}

	// Set command line arguments, run entry function
	ppu->cmd_list
	({
		{ ppu_cmd::set_args, 8 }, u64{Emu.argv.size()}, u64{argv.addr()}, u64{envp.addr()}, u64{0}, u64{ppu->id}, u64{tls_vaddr}, u64{tls_fsize}, u64{tls_vsize},
		{ ppu_cmd::set_gpr, 11 }, u64{elf.header.e_entry},
		{ ppu_cmd::set_gpr, 12 }, u64{malloc_pagesize},
		{ ppu_cmd::lle_call, entry },
	});

	// Set actual memory protection (experimental)
	for (const auto& prog : elf.progs)
	{
		const u32 addr = static_cast<u32>(prog.p_vaddr);
		const u32 size = static_cast<u32>(prog.p_memsz);

		if (prog.p_type == 0x1u /* LOAD */ && prog.p_memsz && (prog.p_flags & 0x022000002) == 0u /* W */)
		{
			// Set memory protection to read-only when necessary (only if PPU-W, SPU-W, RSX-W are all disabled)
			ensure(vm::page_protect(addr, utils::align(size, 0x1000), 0, 0, vm::page_writable));
		}
	}

	error_handler.errored = false;
	return true;
}

std::pair<std::shared_ptr<lv2_overlay>, CellError> ppu_load_overlay(const ppu_exec_object& elf, const std::string& path, s64 file_offset, utils::serial* ar)
{
	if (elf != elf_error::ok)
	{
		return {nullptr, CELL_ENOENT};
	}

	// Access linkage information object
	auto& link = g_fxo->get<ppu_linkage_info>();

	// Executable hash
	sha1_context sha;
	sha1_starts(&sha);

	// Check if it is an overlay executable first
	for (const auto& prog : elf.progs)
	{
		if (prog.p_type == 0x1u /* LOAD */ && prog.p_memsz)
		{
			using addr_range = utils::address_range;

			const addr_range r = addr_range::start_length(::narrow<u32>(prog.p_vaddr), ::narrow<u32>(prog.p_memsz));

			if (!r.valid() || !r.inside(addr_range::start_length(0x30000000, 0x10000000)))
			{
				// TODO: Check error and if there's a better way to error check
				return {nullptr, CELL_ENOEXEC};
			}
		}
	}

	const auto ovlm = std::make_shared<lv2_overlay>();

	// Set path (TODO)
	ovlm->name = path.substr(path.find_last_of('/') + 1);
	ovlm->path = path;
	ovlm->offset = file_offset;

	u32 end = 0;

	// Allocate memory at fixed positions
	for (const auto& prog : elf.progs)
	{
		ppu_loader.notice("** Segment: p_type=0x%x, p_vaddr=0x%llx, p_filesz=0x%llx, p_memsz=0x%llx, flags=0x%x", prog.p_type, prog.p_vaddr, prog.p_filesz, prog.p_memsz, prog.p_flags);

		ppu_segment _seg;
		const u32 addr = _seg.addr = vm::cast(prog.p_vaddr);
		const u32 size = _seg.size = ::narrow<u32>(prog.p_memsz);
		const u32 type = _seg.type = prog.p_type;

		_seg.flags = prog.p_flags;
		_seg.filesz = ::narrow<u32>(prog.p_filesz);

		// Hash big-endian values
		sha1_update(&sha, reinterpret_cast<const uchar*>(&prog.p_type), sizeof(prog.p_type));
		sha1_update(&sha, reinterpret_cast<const uchar*>(&prog.p_flags), sizeof(prog.p_flags));

		if (type == 0x1 /* LOAD */ && prog.p_memsz)
		{
			if (prog.bin.size() > size || prog.bin.size() != prog.p_filesz)
				fmt::throw_exception("Invalid binary size (0x%llx, memsz=0x%x)", prog.bin.size(), size);

			const bool already_loaded = !!ar; // Unimplemented optimization for savestates

			if (already_loaded)
			{
				if (!vm::check_addr(addr, vm::page_readable, size))
				{
					ppu_loader.fatal("ppu_load_overlay(): Archived PPU overlay memory has not been found! (addr=0x%x, memsz=0x%x)", addr, size);
					return {nullptr, CELL_EABORT};
				}
			}
			else if (!vm::get(vm::any, 0x30000000)->falloc(addr, size))
			{
				ppu_loader.error("ppu_load_overlay(): vm::falloc() failed (addr=0x%x, memsz=0x%x)", addr, size);

				// Revert previous allocations
				for (const auto& seg : ovlm->segs)
				{
					ensure(vm::dealloc(seg.addr));
				}

				// TODO: Check error code, maybe disallow more than one overlay instance completely
				return {nullptr, CELL_EBUSY};
			}

			// Copy segment data, hash it
			if (!already_loaded) std::memcpy(vm::base(addr), prog.bin.data(), prog.bin.size());
			sha1_update(&sha, reinterpret_cast<const uchar*>(&prog.p_vaddr), sizeof(prog.p_vaddr));
			sha1_update(&sha, reinterpret_cast<const uchar*>(&prog.p_memsz), sizeof(prog.p_memsz));
			sha1_update(&sha, prog.bin.data(), prog.bin.size());

			// Initialize executable code if necessary
			if (prog.p_flags & 0x1)
			{
				if (ar)
				{
					// Disable analysis optimization for savestates (it's not compatible with savestate with patches applied)
					end = std::max(end, utils::align<u32>(addr + size, 0x10000));
				}

				ppu_register_range(addr, size);
			}

			// Store only LOAD segments (TODO)
			ovlm->segs.emplace_back(_seg);
		}
	}

	// Load section list, used by the analyser
	for (const auto& s : elf.shdrs)
	{
		ppu_loader.notice("** Section: sh_type=0x%x, addr=0x%llx, size=0x%llx, flags=0x%x", std::bit_cast<u32>(s.sh_type), s.sh_addr, s.sh_size, s._sh_flags);

		if (s.sh_type != sec_type::sht_progbits) continue;

		ppu_segment _sec;
		const u32 addr = _sec.addr = vm::cast(s.sh_addr);
		const u32 size = _sec.size = vm::cast(s.sh_size);

		_sec.type = std::bit_cast<u32>(s.sh_type);
		_sec.flags = static_cast<u32>(s._sh_flags & 7);
		_sec.filesz = 0;

		if (addr && size)
		{
			ovlm->secs.emplace_back(_sec);

			if (_sec.flags & 0x4 && addr >= ovlm->segs[0].addr && addr + size <= ovlm->segs[0].addr + ovlm->segs[0].size)
			{
				end = std::max<u32>(end, addr + size);
			}
		}
	}

	sha1_finish(&sha, ovlm->sha1);

	// Format patch name
	std::string hash("OVL-0000000000000000000000000000000000000000");
	for (u32 i = 0; i < 20; i++)
	{
		constexpr auto pal = "0123456789abcdef";
		hash[4 + i * 2] = pal[ovlm->sha1[i] >> 4];
		hash[5 + i * 2] = pal[ovlm->sha1[i] & 15];
	}

	// Apply the patch
	auto applied = g_fxo->get<patch_engine>().apply(hash, vm::g_base_addr);

	if (!Emu.GetTitleID().empty())
	{
		// Alternative patch
		applied += g_fxo->get<patch_engine>().apply(Emu.GetTitleID() + '-' + hash, vm::g_base_addr);
	}

	// Embedded SPU elf patching
	for (const auto& seg : ovlm->segs)
	{
		ppu_check_patch_spu_images(seg);
	}

	if (applied.empty())
	{
		ppu_loader.warning("OVL hash of %s: %s", ovlm->name, hash);
	}
	else
	{
		ppu_loader.success("OVL hash of %s: %s (<- %u)", ovlm->name, hash, applied.size());
	}

	// Load other programs
	for (auto& prog : elf.progs)
	{
		switch (const u32 p_type = prog.p_type)
		{
		case 0x00000001: break; // LOAD (already loaded)

		case 0x60000001: // LOOS+1
		{
			if (prog.p_filesz)
			{
				struct process_param_t
				{
					be_t<u32> size;		//0x60
					be_t<u32> magic;	//string OVLM
					be_t<u32> version;	//0x17000
					be_t<u32> sdk_version;	//seems to be correct
											//string "stage_ovlm"
											//and a lot of zeros.
				};

				const auto& info = vm::_ref<process_param_t>(vm::cast(prog.p_vaddr));

				if (info.size < sizeof(process_param_t))
				{
					ppu_loader.warning("Bad process_param size! [0x%x : 0x%x]", info.size, u32{sizeof(process_param_t)});
				}

				if (info.magic != 0x4f564c4du) //string "OVLM"
				{
					ppu_loader.error("Bad process_param magic! [0x%x]", info.magic);
				}
				else
				{
					ppu_loader.notice("*** sdk version: 0x%x", info.sdk_version);
				}
			}
			break;
		}

		case 0x60000002: // LOOS+2 seems to be 0x0 in size for overlay elfs, at least in known cases
		{
			if (prog.p_filesz)
			{
				struct ppu_proc_prx_param_t
				{
					be_t<u32> size;
					be_t<u32> magic;
					be_t<u32> version;
					be_t<u32> unk0;
					be_t<u32> libent_start;
					be_t<u32> libent_end;
					be_t<u32> libstub_start;
					be_t<u32> libstub_end;
					be_t<u16> ver;
					be_t<u16> unk1;
					be_t<u32> unk2;
				};

				const auto& proc_prx_param = vm::_ref<const ppu_proc_prx_param_t>(vm::cast(prog.p_vaddr));

				ppu_loader.notice("* libent_start = *0x%x", proc_prx_param.libent_start);
				ppu_loader.notice("* libstub_start = *0x%x", proc_prx_param.libstub_start);
				ppu_loader.notice("* unk0 = 0x%x", proc_prx_param.unk0);
				ppu_loader.notice("* unk2 = 0x%x", proc_prx_param.unk2);

				if (proc_prx_param.magic != 0x1b434cecu)
				{
					fmt::throw_exception("Bad magic! (0x%x)", proc_prx_param.magic);
				}

				ppu_load_exports(&link, proc_prx_param.libent_start, proc_prx_param.libent_end);
				ppu_load_imports(ovlm->relocs, &link, proc_prx_param.libstub_start, proc_prx_param.libstub_end);
			}
			break;
		}
		default:
		{
			ppu_loader.error("Unknown phdr type (0x%08x)", p_type);
		}
		}
	}

	ovlm->entry = static_cast<u32>(elf.header.e_entry);

	// Analyse executable (TODO)
	ovlm->analyse(0, ovlm->entry, end, applied);

	// Validate analyser results (not required)
	ovlm->validate(0);

	if (!ar)
	{
		idm::import_existing<lv2_obj, lv2_overlay>(ovlm);
		try_spawn_ppu_if_exclusive_program(*ovlm);
	}

	return {std::move(ovlm), {}};
}

bool ppu_load_rel_exec(const ppu_rel_object& elf)
{
	ppu_module relm{};

	struct on_fatal_error
	{
		ppu_module& relm;
		bool errored = true;

		~on_fatal_error()
		{
			if (!errored)
			{
				return;
			}

			// Revert previous allocations on an error
			for (const auto& seg : relm.secs)
			{
				vm::dealloc(seg.addr);
			}
		}

	} error_handler{relm};

	u32 memsize = 0;

	for (const auto& s : elf.shdrs)
	{
		if (s.sh_type != sec_type::sht_progbits)
		{
			memsize = utils::align<u32>(memsize + vm::cast(s.sh_size), 128);
		}
	}

	u32 addr = vm::alloc(memsize, vm::main);

	if (!addr)
	{
		ppu_loader.fatal("ppu_load_rel_exec(): vm::alloc() failed (memsz=0x%x)", memsize);
		return false;
	}

	ppu_register_range(addr, memsize);

	// Copy references to sections for the purpose of sorting executable sections before non-executable ones
	std::vector<const elf_shdata<elf_be, u64>*> shdrs(elf.shdrs.size());

	for (auto& ref : shdrs)
	{
		ref = &elf.shdrs[&ref - shdrs.data()];
	}

	std::stable_sort(shdrs.begin(), shdrs.end(), [](auto& a, auto& b) -> bool
	{
		const bs_t<sh_flag> flags_a_has = a->sh_flags() - b->sh_flags();
		return flags_a_has.all_of(sh_flag::shf_execinstr);
	});

	// Load sections
	for (auto ptr : shdrs)
	{
		const auto& s = *ptr;

		ppu_loader.notice("** Section: sh_type=0x%x, addr=0x%llx, size=0x%llx, flags=0x%x", std::bit_cast<u32>(s.sh_type), s.sh_addr, s.sh_size, s._sh_flags);

		if (s.sh_type == sec_type::sht_progbits && s.sh_size && s.sh_flags().all_of(sh_flag::shf_alloc))
		{
			ppu_segment _sec;
			const u32 size = _sec.size = vm::cast(s.sh_size);

			_sec.type = std::bit_cast<u32>(s.sh_type);
			_sec.flags = static_cast<u32>(s._sh_flags & 7);
			_sec.filesz = size;

			_sec.addr = addr;
			relm.secs.emplace_back(_sec);

			std::memcpy(vm::base(addr), s.bin.data(), size);
			addr = utils::align<u32>(addr + size, 128);
		}
	}

	try_spawn_ppu_if_exclusive_program(relm);

	error_handler.errored = false;

	return true;
}
