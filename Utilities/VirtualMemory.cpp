#include "stdafx.h"
#include "Utilities/Log.h"
#include "VirtualMemory.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#endif

namespace utils
{
	// Convert memory protection (internal)
	static auto operator +(protection prot)
	{
#ifdef _WIN32
		DWORD _prot = PAGE_NOACCESS;
		switch (prot)
		{
		case protection::rw: _prot = PAGE_READWRITE; break;
		case protection::ro: _prot = PAGE_READONLY; break;
		case protection::no: break;
		case protection::wx: _prot = PAGE_EXECUTE_READWRITE; break;
		case protection::rx: _prot = PAGE_EXECUTE_READ; break;
		}
#else
		int _prot = PROT_NONE;
		switch (prot)
		{
		case protection::rw: _prot = PROT_READ | PROT_WRITE; break;
		case protection::ro: _prot = PROT_READ; break;
		case protection::no: break;
		case protection::wx: _prot = PROT_READ | PROT_WRITE | PROT_EXEC; break;
		case protection::rx: _prot = PROT_READ | PROT_EXEC; break;
		}
#endif

		return _prot;
	}

	void* memory_reserve(std::size_t size, void* use_addr)
	{
#ifdef _WIN32
		return ::VirtualAlloc(use_addr, size, MEM_RESERVE, PAGE_NOACCESS);
#else
		auto ptr = ::mmap(use_addr, size, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);

		if (use_addr && ptr != use_addr)
		{
			::munmap(ptr, size);
			return nullptr;
		}

		return ptr;
#endif
	}

	void memory_commit(void* pointer, std::size_t size, protection prot)
	{
#ifdef _WIN32
		verify(HERE), ::VirtualAlloc(pointer, size, MEM_COMMIT, +prot);
#else
		verify(HERE), ::mprotect((void*)((u64)pointer & -4096), ::align(size, 4096), +prot) != -1;
#endif
	}

	void memory_decommit(void* pointer, std::size_t size)
	{
#ifdef _WIN32
		verify(HERE), ::VirtualFree(pointer, size, MEM_DECOMMIT);
#else
		verify(HERE), ::mmap(pointer, size, PROT_NONE, MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0);
#endif
	}

	void memory_release(void* pointer, std::size_t size)
	{
#ifdef _WIN32
		verify(HERE), ::VirtualFree(pointer, 0, MEM_RELEASE);
#else
		verify(HERE), ::munmap(pointer, size) != -1;
#endif
	}

	void memory_protect(void* pointer, std::size_t size, protection prot)
	{
#ifdef _WIN32
		for (u64 addr = (u64)pointer, end = addr + size; addr < end;)
		{
			// Query current region
			::MEMORY_BASIC_INFORMATION mem;
			verify(HERE), ::VirtualQuery((void*)addr, &mem, sizeof(mem));

			DWORD old;
			if (!::VirtualProtect(mem.BaseAddress, std::min<u64>(end - (u64)mem.BaseAddress, mem.RegionSize), +prot, &old))
			{
				fmt::throw_exception("VirtualProtect failed (%p, 0x%x, addr=0x%x, error=%#x)", pointer, size, addr, GetLastError());
			}

			// Next region
			addr = (u64)mem.BaseAddress + mem.RegionSize;
		}
#else
		verify(HERE), ::mprotect((void*)((u64)pointer & -4096), ::align(size, 4096), +prot) != -1;
#endif
	}

	shm::shm(u32 size)
		: m_size(::align(size, 0x10000))
		, m_ptr(nullptr)
	{
#ifdef _WIN32
		m_handle = ::CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READWRITE, 0, m_size, NULL);
//#elif __linux__
//		m_file = ::memfd_create("mem1", 0);
//		::ftruncate(m_file, m_size);
#else
		while ((m_file = ::shm_open("/rpcs3-mem1", O_RDWR | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR)) == -1)
		{
			if (errno != EEXIST)
				return;
		}

		::shm_unlink("/rpcs3-mem1");
		::ftruncate(m_file, m_size);
#endif

		m_ptr = this->map(nullptr);
	}

	shm::~shm()
	{
#ifdef _WIN32
		::UnmapViewOfFile(m_ptr);
		::CloseHandle(m_handle);
#else
		::munmap(m_ptr, m_size);
		::close(m_file);
#endif
	}

	u8* shm::map(void* ptr, protection prot) const
	{
#ifdef _WIN32
		DWORD access = 0;
		switch (prot)
		{
		case protection::rw: access = FILE_MAP_WRITE; break;
		case protection::ro: access = FILE_MAP_READ; break;
		case protection::no: break;
		case protection::wx: access = FILE_MAP_WRITE | FILE_MAP_EXECUTE; break;
		case protection::rx: access = FILE_MAP_READ | FILE_MAP_EXECUTE; break;
		}

		return static_cast<u8*>(::MapViewOfFileEx(m_handle, access, 0, 0, m_size, ptr));
#else
		return static_cast<u8*>(::mmap((void*)((u64)ptr & -0x10000), m_size, +prot, MAP_SHARED | (ptr ? MAP_FIXED : 0), m_file, 0));
#endif
	}

	u8* shm::map_critical(void* ptr, protection prot)
	{
		const auto target = (u8*)((u64)ptr & -0x10000);

#ifdef _WIN32
		::MEMORY_BASIC_INFORMATION mem;
		if (!::VirtualQuery(target, &mem, sizeof(mem)) || mem.State != MEM_RESERVE || !::VirtualFree(mem.AllocationBase, 0, MEM_RELEASE))
		{
			return nullptr;
		}

		const auto base = (u8*)mem.AllocationBase;
		const auto size = mem.RegionSize + (target - base);

		if (base < target && !::VirtualAlloc(base, target - base, MEM_RESERVE, PAGE_NOACCESS))
		{
			return nullptr;
		}

		if (target + m_size < base + size && !::VirtualAlloc(target + m_size, base + size - target - m_size, MEM_RESERVE, PAGE_NOACCESS))
		{
			return nullptr;
		}
#endif

		return this->map(target, prot);
	}

	void shm::unmap(void* ptr) const
	{
#ifdef _WIN32
		::UnmapViewOfFile(ptr);
#else
		::munmap(ptr, m_size);
#endif
	}

	void shm::unmap_critical(void* ptr)
	{
		const auto target = (u8*)((u64)ptr & -0x10000);

		this->unmap(target);

#ifdef _WIN32
		::MEMORY_BASIC_INFORMATION mem, mem2;
		if (!::VirtualQuery(target - 1, &mem, sizeof(mem)) || !::VirtualQuery(target + m_size, &mem2, sizeof(mem2)))
		{
			return;
		}

		if (mem.State == MEM_RESERVE && !::VirtualFree(mem.AllocationBase, 0, MEM_RELEASE))
		{
			return;
		}

		if (mem2.State == MEM_RESERVE && !::VirtualFree(mem2.AllocationBase, 0, MEM_RELEASE))
		{
			return;
		}

		const auto size1 = mem.State == MEM_RESERVE ? target - (u8*)mem.AllocationBase : 0;
		const auto size2 = mem2.State == MEM_RESERVE ? mem2.RegionSize : 0;

		if (!::VirtualAlloc(mem.State == MEM_RESERVE ? mem.AllocationBase : target, m_size + size1 + size2, MEM_RESERVE, PAGE_NOACCESS))
		{
			return;
		}
#endif
	}
}
