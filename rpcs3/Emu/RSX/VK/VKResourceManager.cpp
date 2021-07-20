#include "stdafx.h"
#include "VKResourceManager.h"
#include "VKGSRender.h"
#include "VKCommandStream.h"

namespace vk
{
	struct vmm_memory_stats
	{
		std::unordered_map<uptr, vmm_allocation_t> allocations;
		std::unordered_map<uptr, atomic_t<u64>> memory_usage;
		std::unordered_map<vmm_allocation_pool, atomic_t<u64>> pool_usage;

		void clear()
		{
			allocations.clear();
			memory_usage.clear();
			pool_usage.clear();
		}
	}
	g_vmm_stats;

	resource_manager g_resource_manager;
	atomic_t<u64> g_event_ctr;
	atomic_t<u64> g_last_completed_event;

	constexpr u64 s_vmm_warn_threshold_size = 2000 * 0x100000; // Warn if allocation on a single heap exceeds this value

	resource_manager* get_resource_manager()
	{
		return &g_resource_manager;
	}

	u64 get_event_id()
	{
		return g_event_ctr++;
	}

	u64 current_event_id()
	{
		return g_event_ctr.load();
	}

	u64 last_completed_event_id()
	{
		return g_last_completed_event.load();
	}

	void on_event_completed(u64 event_id, bool flush)
	{
		if (!flush && g_cfg.video.multithreaded_rsx)
		{
			auto& offloader_thread = g_fxo->get<rsx::dma_manager>();
			ensure(!offloader_thread.is_current_thread());

			offloader_thread.backend_ctrl(rctrl_run_gc, reinterpret_cast<void*>(event_id));
			return;
		}

		g_resource_manager.eid_completed(event_id);
		g_last_completed_event = std::max(event_id, g_last_completed_event.load());
	}

	static constexpr f32 size_in_GiB(u64 size)
	{
		return size / (1024.f * 1024.f * 1024.f);
	}

	void vmm_notify_memory_allocated(void* handle, u32 memory_type, u64 memory_size, vmm_allocation_pool pool)
	{
		auto key = reinterpret_cast<uptr>(handle);
		const vmm_allocation_t info = { memory_size, memory_type, pool };

		if (const auto ins = g_vmm_stats.allocations.insert_or_assign(key, info);
			!ins.second)
		{
			rsx_log.error("Duplicate vmm entry with memory handle 0x%llx", key);
		}

		g_vmm_stats.pool_usage[pool] += memory_size;

		auto& vmm_size = g_vmm_stats.memory_usage[memory_type];
		vmm_size += memory_size;

		if (vmm_size > s_vmm_warn_threshold_size && (vmm_size - memory_size) <= s_vmm_warn_threshold_size)
		{
			rsx_log.warning("Memory type 0x%x has allocated more than %04.2fG. Currently allocated %04.2fG",
				memory_type, size_in_GiB(s_vmm_warn_threshold_size), size_in_GiB(vmm_size));
		}
	}

	void vmm_notify_memory_freed(void* handle)
	{
		auto key = reinterpret_cast<uptr>(handle);
		if (auto found = g_vmm_stats.allocations.find(key);
			found != g_vmm_stats.allocations.end())
		{
			const auto& info = found->second;
			g_vmm_stats.memory_usage[info.type_index] -= info.size;
			g_vmm_stats.pool_usage[info.pool] -= info.size;
			g_vmm_stats.allocations.erase(found);
		}
	}

	void vmm_reset()
	{
		g_vmm_stats.clear();
		g_event_ctr = 0;
		g_last_completed_event = 0;
	}

	u64 vmm_get_application_memory_usage(u32 memory_type)
	{
		return g_vmm_stats.memory_usage[memory_type];
	}

	u64 vmm_get_application_pool_usage(vmm_allocation_pool pool)
	{
		return g_vmm_stats.pool_usage[pool];
	}

	rsx::problem_severity vmm_determine_memory_load_severity()
	{
		const auto vmm_load = get_current_mem_allocator()->get_memory_usage();
		rsx::problem_severity load_severity = rsx::problem_severity::low;

		// Fragmentation tuning
		if (vmm_load < 50.f)
		{
			get_current_mem_allocator()->set_fastest_allocation_flags();
		}
		else if (vmm_load > 75.f)
		{
			// Avoid fragmentation if we can
			get_current_mem_allocator()->set_safest_allocation_flags();

			if (vmm_load > 95.f)
			{
				// Drivers will often crash long before returning OUT_OF_DEVICE_MEMORY errors.
				load_severity = rsx::problem_severity::fatal;
			}
			else if (vmm_load > 90.f)
			{
				load_severity = rsx::problem_severity::severe;
			}
			else
			{
				load_severity = rsx::problem_severity::moderate;
			}

			// Query actual usage for comparison. Maybe we just have really fragmented memory...
			const auto mem_info = get_current_renderer()->get_memory_mapping();
			const auto local_memory_usage = vmm_get_application_memory_usage(mem_info.device_local);

			constexpr u64 _1GB = (1024 * 0x100000);
			if (local_memory_usage < (mem_info.device_local_total_bytes / 2) ||    // Less than 50% VRAM usage OR
				(mem_info.device_local_total_bytes - local_memory_usage) > _1GB)   // More than 1GiB of unallocated memory
			{
				// Lower severity to avoid slowing performance too much
				load_severity = rsx::problem_severity::low;
			}
			else if ((mem_info.device_local_total_bytes - local_memory_usage) > 512 * 0x100000)
			{
				// At least 512MB left, do not overreact
				load_severity = rsx::problem_severity::moderate;
			}

			if (load_severity >= rsx::problem_severity::moderate)
			{
				// NOTE: For some reason fmt::format with a sized float followed by percentage sign causes random crashing.
				// This is a bug unrelated to this, but explains why we're going with integral percentages here.
				const auto application_memory_load = (local_memory_usage * 100) / mem_info.device_local_total_bytes;
				rsx_log.warning("Actual device memory used by internal allocations is %lluM (%llu%%)", local_memory_usage / 0x100000, application_memory_load);
				rsx_log.warning("Video memory usage is at %d%%. Will attempt to reclaim some resources.", static_cast<int>(vmm_load));
			}
		}

		return load_severity;
	}

	bool vmm_handle_memory_pressure(rsx::problem_severity severity)
	{
		if (auto vkthr = dynamic_cast<VKGSRender*>(rsx::get_current_renderer()))
		{
			return vkthr->on_vram_exhausted(severity);
		}

		return false;
	}

	void vmm_check_memory_usage()
	{
		if (const auto load_severity = vmm_determine_memory_load_severity();
			load_severity >= rsx::problem_severity::moderate)
		{
			vmm_handle_memory_pressure(load_severity);
		}
	}
}
