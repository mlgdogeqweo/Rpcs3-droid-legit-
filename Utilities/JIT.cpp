﻿#include "types.h"
#include "JIT.h"
#include "StrFmt.h"
#include "File.h"
#include "util/logs.hpp"
#include "mutex.h"
#include "sysinfo.h"
#include "VirtualMemory.h"
#include <immintrin.h>
#include <zlib.h>

#ifdef __linux__
#include <sys/mman.h>
#define CAN_OVERCOMMIT
#endif

LOG_CHANNEL(jit_log, "JIT");

static u8* get_jit_memory()
{
	// Reserve 2G memory (magic static)
	static void* const s_memory2 = []() -> void*
	{
		void* ptr = utils::memory_reserve(0x80000000);

#ifdef CAN_OVERCOMMIT
		utils::memory_commit(ptr, 0x80000000);
		utils::memory_protect(ptr, 0x40000000, utils::protection::wx);
#endif
		return ptr;
	}();

	return static_cast<u8*>(s_memory2);
}

// Allocation counters (1G code, 1G data subranges)
static atomic_t<u64> s_code_pos{0}, s_data_pos{0};

// Snapshot of code generated before main()
static std::vector<u8> s_code_init, s_data_init;

template <atomic_t<u64>& Ctr, uint Off, utils::protection Prot>
static u8* add_jit_memory(std::size_t size, uint align)
{
	// Select subrange
	u8* pointer = get_jit_memory() + Off;

	if (!size && !align) [[unlikely]]
	{
		// Return subrange info
		return pointer;
	}

	u64 olda, newa;

	// Simple allocation by incrementing pointer to the next free data
	const u64 pos = Ctr.atomic_op([&](u64& ctr) -> u64
	{
		const u64 _pos = ::align(ctr & 0xffff'ffff, align);
		const u64 _new = ::align(_pos + size, align);

		if (_new > 0x40000000) [[unlikely]]
		{
			// Sorry, we failed, and further attempts should fail too.
			ctr |= 0x40000000;
			return -1;
		}

		// Last allocation is stored in highest bits
		olda = ctr >> 32;
		newa = olda;

		// Check the necessity to commit more memory
		if (_new > olda) [[unlikely]]
		{
			newa = ::align(_new, 0x100000);
		}

		ctr += _new - (ctr & 0xffff'ffff);
		return _pos;
	});

	if (pos == umax) [[unlikely]]
	{
		jit_log.error("JIT: Out of memory (size=0x%x, align=0x%x, off=0x%x)", size, align, Off);
		return nullptr;
	}

	if (olda != newa) [[unlikely]]
	{
#ifdef CAN_OVERCOMMIT
		madvise(pointer + olda, newa - olda, MADV_WILLNEED);
#else
		// Commit more memory
		utils::memory_commit(pointer + olda, newa - olda, Prot);
#endif
		// Acknowledge committed memory
		Ctr.atomic_op([&](u64& ctr)
		{
			if ((ctr >> 32) < newa)
			{
				ctr += (newa - (ctr >> 32)) << 32;
			}
		});
	}

	return pointer + pos;
}

jit_runtime::jit_runtime()
	: HostRuntime()
{
}

jit_runtime::~jit_runtime()
{
}

asmjit::Error jit_runtime::_add(void** dst, asmjit::CodeHolder* code) noexcept
{
	std::size_t codeSize = code->getCodeSize();
	if (!codeSize) [[unlikely]]
	{
		*dst = nullptr;
		return asmjit::kErrorNoCodeGenerated;
	}

	void* p = jit_runtime::alloc(codeSize, 16);
	if (!p) [[unlikely]]
	{
		*dst = nullptr;
		return asmjit::kErrorNoVirtualMemory;
	}

	std::size_t relocSize = code->relocate(p);
	if (!relocSize) [[unlikely]]
	{
		*dst = nullptr;
		return asmjit::kErrorInvalidState;
	}

	flush(p, relocSize);
	*dst = p;

	return asmjit::kErrorOk;
}

asmjit::Error jit_runtime::_release(void* ptr) noexcept
{
	return asmjit::kErrorOk;
}

u8* jit_runtime::alloc(std::size_t size, uint align, bool exec) noexcept
{
	if (exec)
	{
		return add_jit_memory<s_code_pos, 0x0, utils::protection::wx>(size, align);
	}
	else
	{
		return add_jit_memory<s_data_pos, 0x40000000, utils::protection::rw>(size, align);
	}
}

void jit_runtime::initialize()
{
	if (!s_code_init.empty() || !s_data_init.empty())
	{
		return;
	}

	// Create code/data snapshot
	s_code_init.resize(s_code_pos & 0xffff'ffff);
	std::memcpy(s_code_init.data(), alloc(0, 0, true), s_code_init.size());
	s_data_init.resize(s_data_pos & 0xffff'ffff);
	std::memcpy(s_data_init.data(), alloc(0, 0, false), s_data_init.size());
}

void jit_runtime::finalize() noexcept
{
	// Reset JIT memory
#ifdef CAN_OVERCOMMIT
	utils::memory_reset(get_jit_memory(), 0x80000000);
	utils::memory_protect(get_jit_memory(), 0x40000000, utils::protection::wx);
#else
	utils::memory_decommit(get_jit_memory(), 0x80000000);
#endif

	s_code_pos = 0;
	s_data_pos = 0;

	// Restore code/data snapshot
	std::memcpy(alloc(s_code_init.size(), 1, true), s_code_init.data(), s_code_init.size());
	std::memcpy(alloc(s_data_init.size(), 1, false), s_data_init.data(), s_data_init.size());
}

asmjit::JitRuntime& asmjit::get_global_runtime()
{
	// Magic static
	static asmjit::JitRuntime g_rt;
	return g_rt;
}

void asmjit::build_transaction_enter(asmjit::X86Assembler& c, asmjit::Label fallback, const asmjit::X86Gp& ctr, uint less_than)
{
	Label fall = c.newLabel();
	Label begin = c.newLabel();
	c.jmp(begin);
	c.bind(fall);

	if (less_than < 65)
	{
		c.add(ctr, 1);
		c.test(x86::eax, _XABORT_RETRY);
		c.jz(fallback);
	}
	else
	{
		// Don't repeat on explicit XABORT instruction (workaround)
		c.test(x86::eax, _XABORT_EXPLICIT);
		c.jnz(fallback);

		// Count an attempt without RETRY flag as 65 normal attempts and continue
		c.push(x86::rax);
		c.not_(x86::eax);
		c.and_(x86::eax, _XABORT_RETRY);
		c.shl(x86::eax, 5);
		c.add(x86::eax, 1); // eax = RETRY ? 1 : 65
		c.add(ctr, x86::rax);
		c.pop(x86::rax);
	}

	c.cmp(ctr, less_than);
	c.jae(fallback);
	c.align(kAlignCode, 16);
	c.bind(begin);
	c.xbegin(fall);
}

void asmjit::build_transaction_abort(asmjit::X86Assembler& c, unsigned char code)
{
	c.db(0xc6);
	c.db(0xf8);
	c.db(code);
}

#ifdef LLVM_AVAILABLE

#include <unordered_map>
#include <map>
#include <unordered_set>
#include <set>
#include <array>
#include <deque>

#ifdef _MSC_VER
#pragma warning(push, 0)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/ExecutionEngine/ObjectCache.h"
#ifdef _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

const bool jit_initialize = []() -> bool
{
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
	LLVMLinkInMCJIT();
	return true;
}();

// Simple memory manager
struct MemoryManager1 : llvm::RTDyldMemoryManager
{
	// 256 MiB for code or data
	static constexpr u64 c_max_size = 0x20000000 / 2;

	// Allocation unit
	static constexpr u64 c_page_size = 4096;

	// Reserve 512 MiB
	u8* const ptr = static_cast<u8*>(utils::memory_reserve(c_max_size * 2));

	u64 code_ptr = 0;
	u64 data_ptr = c_max_size;

	MemoryManager1() = default;

	~MemoryManager1() override
	{
		utils::memory_release(ptr, c_max_size * 2);
	}

	[[noreturn]] static void null()
	{
		fmt::throw_exception("Null function" HERE);
	}

	llvm::JITSymbol findSymbol(const std::string& name) override
	{
		u64 addr = RTDyldMemoryManager::getSymbolAddress(name);

		if (!addr)
		{
			addr = reinterpret_cast<uptr>(&null);
		}

		return {addr, llvm::JITSymbolFlags::Exported};
	}

	u8* allocate(u64& oldp, std::uintptr_t size, uint align, utils::protection prot)
	{
		if (align > c_page_size)
		{
			jit_log.fatal("JIT: Unsupported alignment (size=0x%x, align=0x%x)", size, align);
			return nullptr;
		}

		const u64 olda = ::align(oldp, align);
		const u64 newp = ::align(olda + size, align);

		if ((newp - 1) / c_max_size != oldp / c_max_size)
		{
			jit_log.fatal("JIT: Out of memory (size=0x%x, align=0x%x)", size, align);
			return nullptr;
		}

		if ((oldp - 1) / c_page_size != (newp - 1) / c_page_size)
		{
			// Allocate pages on demand
			const u64 pagea = ::align(oldp, c_page_size);
			const u64 psize = ::align(newp - pagea, c_page_size);
			utils::memory_commit(this->ptr + pagea, psize, prot);
		}

		// Update allocation counter
		oldp = newp;

		return this->ptr + olda;
	}

	u8* allocateCodeSection(std::uintptr_t size, uint align, uint sec_id, llvm::StringRef sec_name) override
	{
		return allocate(code_ptr, size, align, utils::protection::wx);
	}

	u8* allocateDataSection(std::uintptr_t size, uint align, uint sec_id, llvm::StringRef sec_name, bool is_ro) override
	{
		return allocate(data_ptr, size, align, utils::protection::rw);
	}

	bool finalizeMemory(std::string* = nullptr) override
	{
		return false;
	}

	void registerEHFrames(u8* addr, u64 load_addr, std::size_t size) override
	{
	}

	void deregisterEHFrames() override
	{
	}
};

// Simple memory manager
struct MemoryManager2 : llvm::RTDyldMemoryManager
{
	MemoryManager2() = default;

	~MemoryManager2() override
	{
	}

	u8* allocateCodeSection(std::uintptr_t size, uint align, uint sec_id, llvm::StringRef sec_name) override
	{
		return jit_runtime::alloc(size, align, true);
	}

	u8* allocateDataSection(std::uintptr_t size, uint align, uint sec_id, llvm::StringRef sec_name, bool is_ro) override
	{
		return jit_runtime::alloc(size, align, false);
	}

	bool finalizeMemory(std::string* = nullptr) override
	{
		return false;
	}

	void registerEHFrames(u8* addr, u64 load_addr, std::size_t size) override
	{
	}

	void deregisterEHFrames() override
	{
	}
};

// Helper class
class ObjectCache final : public llvm::ObjectCache
{
	const std::string& m_path;

public:
	ObjectCache(const std::string& path)
		: m_path(path)
	{
	}

	~ObjectCache() override = default;

	void notifyObjectCompiled(const llvm::Module* _module, llvm::MemoryBufferRef obj) override
	{
		std::string name = m_path;
		name.append(_module->getName().data());
		//fs::file(name, fs::rewrite).write(obj.getBufferStart(), obj.getBufferSize());
		name.append(".gz");

		z_stream zs{};
		uLong zsz = compressBound(::narrow<u32>(obj.getBufferSize(), HERE)) + 256;
		auto zbuf = std::make_unique<uchar[]>(zsz);
#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
		deflateInit2(&zs, 9, Z_DEFLATED, 16 + 15, 9, Z_DEFAULT_STRATEGY);
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif
		zs.avail_in  = static_cast<uInt>(obj.getBufferSize());
		zs.next_in   = reinterpret_cast<uchar*>(const_cast<char*>(obj.getBufferStart()));
		zs.avail_out = static_cast<uInt>(zsz);
		zs.next_out  = zbuf.get();

		switch (deflate(&zs, Z_FINISH))
		{
		case Z_OK:
		case Z_STREAM_END:
		{
			deflateEnd(&zs);
			break;
		}
		default:
		{
			jit_log.error("LLVM: Failed to compress module: %s", _module->getName().data());
			deflateEnd(&zs);
			return;
		}
		}

		fs::file(name, fs::rewrite).write(zbuf.get(), zsz - zs.avail_out);
		jit_log.notice("LLVM: Created module: %s", _module->getName().data());
	}

	static std::unique_ptr<llvm::MemoryBuffer> load(const std::string& path)
	{
		if (fs::file cached{path + ".gz", fs::read})
		{
			std::vector<uchar> gz = cached.to_vector<uchar>();
			std::vector<uchar> out;
			z_stream zs{};

			if (gz.empty()) [[unlikely]]
			{
				return nullptr;
			}
#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
			inflateInit2(&zs, 16 + 15);
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif
			zs.avail_in = static_cast<uInt>(gz.size());
			zs.next_in  = gz.data();
			out.resize(gz.size() * 6);
			zs.avail_out = static_cast<uInt>(out.size());
			zs.next_out  = out.data();

			while (zs.avail_in)
			{
				switch (inflate(&zs, Z_FINISH))
				{
				case Z_OK: break;
				case Z_STREAM_END: break;
				case Z_BUF_ERROR:
				{
					if (zs.avail_in)
						break;
					[[fallthrough]];
				}
				default:
					inflateEnd(&zs);
					return nullptr;
				}

				if (zs.avail_in)
				{
					auto cur_size = zs.next_out - out.data();
					out.resize(out.size() + 65536);
					zs.avail_out = static_cast<uInt>(out.size() - cur_size);
					zs.next_out = out.data() + cur_size;
				}
			}

			out.resize(zs.next_out - out.data());
			inflateEnd(&zs);

			auto buf = llvm::WritableMemoryBuffer::getNewUninitMemBuffer(out.size());
			std::memcpy(buf->getBufferStart(), out.data(), out.size());
			return buf;
		}

		if (fs::file cached{path, fs::read})
		{
			if (cached.size() == 0) [[unlikely]]
			{
				return nullptr;
			}

			auto buf = llvm::WritableMemoryBuffer::getNewUninitMemBuffer(cached.size());
			cached.read(buf->getBufferStart(), buf->getBufferSize());
			return buf;
		}

		return nullptr;
	}

	std::unique_ptr<llvm::MemoryBuffer> getObject(const llvm::Module* _module) override
	{
		std::string path = m_path;
		path.append(_module->getName().data());

		if (auto buf = load(path))
		{
			jit_log.notice("LLVM: Loaded module: %s", _module->getName().data());
			return buf;
		}

		return nullptr;
	}
};

std::string jit_compiler::cpu(const std::string& _cpu)
{
	std::string m_cpu = _cpu;

	if (m_cpu.empty())
	{
		m_cpu = llvm::sys::getHostCPUName().operator std::string();

		if (m_cpu == "sandybridge" ||
			m_cpu == "ivybridge" ||
			m_cpu == "haswell" ||
			m_cpu == "broadwell" ||
			m_cpu == "skylake" ||
			m_cpu == "skylake-avx512" ||
			m_cpu == "cascadelake" ||
			m_cpu == "cooperlake" ||
			m_cpu == "cannonlake" ||
			m_cpu == "icelake" ||
			m_cpu == "icelake-client" ||
			m_cpu == "icelake-server" ||
			m_cpu == "tigerlake")
		{
			// Downgrade if AVX is not supported by some chips
			if (!utils::has_avx())
			{
				m_cpu = "nehalem";
			}
		}

		if (m_cpu == "skylake-avx512" ||
			m_cpu == "cascadelake" ||
			m_cpu == "cooperlake" ||
			m_cpu == "cannonlake" ||
			m_cpu == "icelake" ||
			m_cpu == "icelake-client" ||
			m_cpu == "icelake-server" ||
			m_cpu == "tigerlake")
		{
			// Downgrade if AVX-512 is disabled or not supported
			if (!utils::has_avx512())
			{
				m_cpu = "skylake";
			}
		}

		if (m_cpu == "znver1" && utils::has_clwb())
		{
			// Upgrade
			m_cpu = "znver2";
		}
	}

	return m_cpu;
}

jit_compiler::jit_compiler(const std::unordered_map<std::string, u64>& _link, const std::string& _cpu, u32 flags)
	: m_cpu(cpu(_cpu))
{
	std::string result;

	auto null_mod = std::make_unique<llvm::Module> ("null_", m_context);

	if (_link.empty())
	{
		std::unique_ptr<llvm::RTDyldMemoryManager> mem;

		if (flags & 0x1)
		{
			mem = std::make_unique<MemoryManager1>();
		}
		else
		{
			mem = std::make_unique<MemoryManager2>();
			null_mod->setTargetTriple(llvm::Triple::normalize("x86_64-unknown-linux-gnu"));
		}

		// Auxiliary JIT (does not use custom memory manager, only writes the objects)
		m_engine.reset(llvm::EngineBuilder(std::move(null_mod))
			.setErrorStr(&result)
			.setEngineKind(llvm::EngineKind::JIT)
			.setMCJITMemoryManager(std::move(mem))
			.setOptLevel(llvm::CodeGenOpt::Aggressive)
			.setCodeModel(flags & 0x2 ? llvm::CodeModel::Large : llvm::CodeModel::Small)
			.setMCPU(m_cpu)
			.create());
	}
	else
	{
		// Primary JIT
		m_engine.reset(llvm::EngineBuilder(std::move(null_mod))
			.setErrorStr(&result)
			.setEngineKind(llvm::EngineKind::JIT)
			.setMCJITMemoryManager(std::make_unique<MemoryManager1>())
			.setOptLevel(llvm::CodeGenOpt::Aggressive)
			.setCodeModel(flags & 0x2 ? llvm::CodeModel::Large : llvm::CodeModel::Small)
			.setMCPU(m_cpu)
			.create());

		for (auto&& [name, addr] : _link)
		{
			m_engine->addGlobalMapping(name, addr);
		}
	}

	if (!m_engine)
	{
		fmt::throw_exception("LLVM: Failed to create ExecutionEngine: %s", result);
	}
}

jit_compiler::~jit_compiler()
{
}

void jit_compiler::add(std::unique_ptr<llvm::Module> _module, const std::string& path)
{
	ObjectCache cache{path};
	m_engine->setObjectCache(&cache);

	const auto ptr = _module.get();
	m_engine->addModule(std::move(_module));
	m_engine->generateCodeForModule(ptr);
	m_engine->setObjectCache(nullptr);

	for (auto& func : ptr->functions())
	{
		// Delete IR to lower memory consumption
		func.deleteBody();
	}
}

void jit_compiler::add(std::unique_ptr<llvm::Module> _module)
{
	const auto ptr = _module.get();
	m_engine->addModule(std::move(_module));
	m_engine->generateCodeForModule(ptr);

	for (auto& func : ptr->functions())
	{
		// Delete IR to lower memory consumption
		func.deleteBody();
	}
}

void jit_compiler::add(const std::string& path)
{
	auto cache = ObjectCache::load(path);

	if (auto object_file = llvm::object::ObjectFile::createObjectFile(*cache))
	{
		m_engine->addObjectFile( std::move(*object_file) );
	}
	else
	{
		jit_log.error("ObjectCache: Adding failed: %s", path);
	}
}

bool jit_compiler::check(const std::string& path)
{
	if (auto cache = ObjectCache::load(path))
	{
		if (auto object_file = llvm::object::ObjectFile::createObjectFile(*cache))
		{
			return true;
		}

		if (fs::remove_file(path))
		{
			jit_log.error("ObjectCache: Removed damaged file: %s", path);
		}
	}

	return false;
}

void jit_compiler::fin()
{
	m_engine->finalizeObject();
}

u64 jit_compiler::get(const std::string& name)
{
	return m_engine->getGlobalValueAddress(name);
}

#endif
