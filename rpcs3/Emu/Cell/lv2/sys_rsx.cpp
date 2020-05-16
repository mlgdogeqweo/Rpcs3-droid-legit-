﻿#include "stdafx.h"
#include "sys_rsx.h"

#include "Emu/Cell/PPUModule.h"
#include "Emu/RSX/GSRender.h"
#include "Emu/Cell/ErrorCodes.h"
#include "sys_event.h"


LOG_CHANNEL(sys_rsx);

extern u64 get_timebased_time();

static shared_mutex s_rsxmem_mtx;

// Unknown error code returned by sys_rsx_context_attribute
enum sys_rsx_error : s32
{
	SYS_RSX_CONTEXT_ATTRIBUTE_ERROR = -17
};

template<>
void fmt_class_string<sys_rsx_error>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto error)
	{
		switch (error)
		{
		STR_CASE(SYS_RSX_CONTEXT_ATTRIBUTE_ERROR);
		}

		return unknown;
	});
}

u64 rsxTimeStamp()
{
	return get_timebased_time();
}

void lv2_rsx_config::send_event(u64 data1, u64 event_flags, u64 data3) const
{
	// Filter event bits, send them only if they are masked by gcm
	// Except the upper 32-bits, they are reserved for unmapped io events and execute unconditionally
	event_flags &= vm::_ref<RsxDriverInfo>(driver_info).handlers | 0xffff'ffffull << 32;

	if (!event_flags)
	{
		// Nothing to do
		return;
	}

	auto error = sys_event_port_send(rsx_event_port, data1, event_flags, data3);

	while (error + 0u == CELL_EBUSY)
	{
		auto cpu = get_current_cpu_thread();

		if (cpu && cpu->id_type() != 1)
		{
			cpu = nullptr;
		}

		if (cpu)
		{
			// Deschedule
			lv2_obj::sleep(*cpu, 100);
		}
		else if (const auto rsx = rsx::get_current_renderer(); rsx->is_current_thread())
		{
			rsx->on_semaphore_acquire_wait();
		}
		
		// Wait a bit before resending event
		thread_ctrl::wait_for(100);

		if (Emu.IsStopped() || (cpu && cpu->check_state()))
		{
			error = 0;
			break;
		}

		error = sys_event_port_send(rsx_event_port, data1, event_flags, data3);
	}

	if (error && error + 0u != CELL_ENOTCONN)
	{
		fmt::throw_exception("lv2_rsx_config::send_event() Failed to send event! (error=%x)" HERE, +error);
	}
}

error_code sys_rsx_device_open()
{
	sys_rsx.todo("sys_rsx_device_open()");

	return CELL_OK;
}

error_code sys_rsx_device_close()
{
	sys_rsx.todo("sys_rsx_device_close()");

	return CELL_OK;
}

/*
 * lv2 SysCall 668 (0x29C): sys_rsx_memory_allocate
 * @param mem_handle (OUT): Context / ID, which is used by sys_rsx_memory_free to free allocated memory.
 * @param mem_addr (OUT): Returns the local memory base address, usually 0xC0000000.
 * @param size (IN): Local memory size. E.g. 0x0F900000 (249 MB). (changes with sdk version)
 * @param flags (IN): E.g. Immediate value passed in cellGcmSys is 8.
 * @param a5 (IN): E.g. Immediate value passed in cellGcmSys is 0x00300000 (3 MB?).
 * @param a6 (IN): E.g. Immediate value passed in cellGcmSys is 16.
 * @param a7 (IN): E.g. Immediate value passed in cellGcmSys is 8.
 */
error_code sys_rsx_memory_allocate(vm::ptr<u32> mem_handle, vm::ptr<u64> mem_addr, u32 size, u64 flags, u64 a5, u64 a6, u64 a7)
{
	sys_rsx.warning("sys_rsx_memory_allocate(mem_handle=*0x%x, mem_addr=*0x%x, size=0x%x, flags=0x%llx, a5=0x%llx, a6=0x%llx, a7=0x%llx)", mem_handle, mem_addr, size, flags, a5, a6, a7);

	if (u32 addr = vm::falloc(rsx::constants::local_mem_base, size, vm::video))
	{
		g_fxo->get<lv2_rsx_config>()->memory_size = size;
		*mem_addr = addr;
		*mem_handle = 0x5a5a5a5b;
		return CELL_OK;
	}

	return CELL_ENOMEM;
}

/*
 * lv2 SysCall 669 (0x29D): sys_rsx_memory_free
 * @param mem_handle (OUT): Context / ID, for allocated local memory generated by sys_rsx_memory_allocate
 */
error_code sys_rsx_memory_free(u32 mem_handle)
{
	sys_rsx.warning("sys_rsx_memory_free(mem_handle=0x%x)", mem_handle);

	if (!vm::check_addr(rsx::constants::local_mem_base))
	{
		return CELL_ENOMEM;
	}

	if (g_fxo->get<lv2_rsx_config>()->context_base)
	{
		fmt::throw_exception("Attempting to dealloc rsx memory when the context is still being used" HERE);
	}

	if (!vm::dealloc(rsx::constants::local_mem_base))
	{
		return CELL_ENOMEM;
	}

	return CELL_OK;
}

/*
 * lv2 SysCall 670 (0x29E): sys_rsx_context_allocate
 * @param context_id (OUT): RSX context, E.g. 0x55555555 (in vsh.self)
 * @param lpar_dma_control (OUT): Control register area. E.g. 0x60100000 (in vsh.self)
 * @param lpar_driver_info (OUT): RSX data like frequencies, sizes, version... E.g. 0x60200000 (in vsh.self)
 * @param lpar_reports (OUT): Report data area. E.g. 0x60300000 (in vsh.self)
 * @param mem_ctx (IN): mem_ctx given by sys_rsx_memory_allocate
 * @param system_mode (IN):
 */
error_code sys_rsx_context_allocate(vm::ptr<u32> context_id, vm::ptr<u64> lpar_dma_control, vm::ptr<u64> lpar_driver_info, vm::ptr<u64> lpar_reports, u64 mem_ctx, u64 system_mode)
{
	sys_rsx.warning("sys_rsx_context_allocate(context_id=*0x%x, lpar_dma_control=*0x%x, lpar_driver_info=*0x%x, lpar_reports=*0x%x, mem_ctx=0x%llx, system_mode=0x%llx)",
		context_id, lpar_dma_control, lpar_driver_info, lpar_reports, mem_ctx, system_mode);

	if (!vm::check_addr(rsx::constants::local_mem_base))
	{
		return CELL_EINVAL;
	}

	auto rsx_cfg = g_fxo->get<lv2_rsx_config>();

	std::lock_guard lock(s_rsxmem_mtx);

	if (rsx_cfg->context_base)
	{
		// We currently do not support multiple contexts
		fmt::throw_exception("sys_rsx_context_allocate was called twice" HERE);
	}

	const auto area = vm::reserve_map(vm::rsx_context, 0, 0x10000000, 0x403);
	const u32 context_base = area ? area->alloc(0x300000) : 0;

	if (!context_base)
	{
		return CELL_ENOMEM;
	}

	*lpar_dma_control = context_base;
	*lpar_driver_info = context_base + 0x100000;
	*lpar_reports = context_base + 0x200000;

	auto &reports = vm::_ref<RsxReports>(vm::cast(*lpar_reports, HERE));
	std::memset(&reports, 0, sizeof(RsxReports));

	for (int i = 0; i < 64; ++i)
		reports.notify[i].timestamp = -1;

	for (int i = 0; i < 256; ++i)
	{
		reports.semaphore[i].val = 0x1337C0D3;
		reports.semaphore[i].zero = 0x1337BABE;
		reports.semaphore[i].zero2 = 0x1337BEEF1337F001;
	}

	for (int i = 0; i < 2048; ++i)
	{
		reports.report[i].val = 0;
		reports.report[i].timestamp = -1;
		reports.report[i].pad = -1;
	}

	auto &driverInfo = vm::_ref<RsxDriverInfo>(vm::cast(*lpar_driver_info, HERE));

	std::memset(&driverInfo, 0, sizeof(RsxDriverInfo));

	driverInfo.version_driver = 0x211;
	driverInfo.version_gpu = 0x5c;
	driverInfo.memory_size = rsx_cfg->memory_size;
	driverInfo.nvcore_frequency = 500000000; // 0x1DCD6500
	driverInfo.memory_frequency = 650000000; // 0x26BE3680
	driverInfo.reportsNotifyOffset = 0x1000;
	driverInfo.reportsOffset = 0;
	driverInfo.reportsReportOffset = 0x1400;
	driverInfo.systemModeFlags = static_cast<u32>(system_mode);
	driverInfo.hardware_channel = 1; // * i think* this 1 for games, 0 for vsh

	rsx_cfg->driver_info = vm::cast(*lpar_driver_info, HERE);

	auto &dmaControl = vm::_ref<RsxDmaControl>(vm::cast(*lpar_dma_control, HERE));
	dmaControl.get = 0;
	dmaControl.put = 0;
	dmaControl.ref = 0; // Set later to -1 by cellGcmSys

	if ((false/*system_mode & something*/ || g_cfg.video.decr_memory_layout)
		&& g_cfg.core.debug_console_mode)
		rsx::get_current_renderer()->main_mem_size = 0x20000000; //512MB
	else
		rsx::get_current_renderer()->main_mem_size = 0x10000000; //256MB

	vm::var<sys_event_queue_attribute_t, vm::page_allocator<>> attr;
	attr->protocol = SYS_SYNC_PRIORITY;
	attr->type = SYS_PPU_QUEUE;
	attr->name_u64 = 0;

	sys_event_port_create(vm::get_addr(&driverInfo.handler_queue), SYS_EVENT_PORT_LOCAL, 0);
	rsx_cfg->rsx_event_port = driverInfo.handler_queue;
	sys_event_queue_create(vm::get_addr(&driverInfo.handler_queue), attr, 0, 0x20);
	sys_event_port_connect_local(rsx_cfg->rsx_event_port, driverInfo.handler_queue);

	rsx_cfg->dma_address = vm::cast(*lpar_dma_control, HERE);

	const auto render = rsx::get_current_renderer();
	render->display_buffers_count = 0;
	render->current_display_buffer = 0;
	render->label_addr = vm::cast(*lpar_reports, HERE);
	render->device_addr = rsx_cfg->device_addr;
	render->dma_address = rsx_cfg->dma_address;
	render->local_mem_size = rsx_cfg->memory_size;
	render->init(vm::cast(*lpar_dma_control, HERE));

	rsx_cfg->context_base = context_base;
	*context_id = 0x55555555;

	return CELL_OK;
}

/*
 * lv2 SysCall 671 (0x29F): sys_rsx_context_free
 * @param context_id (IN): RSX context generated by sys_rsx_context_allocate to free the context.
 */
error_code sys_rsx_context_free(u32 context_id)
{
	sys_rsx.todo("sys_rsx_context_free(context_id=0x%x)", context_id);

	std::scoped_lock lock(s_rsxmem_mtx);

	auto rsx_cfg = g_fxo->get<lv2_rsx_config>();

	if (context_id != 0x55555555 || !rsx_cfg->context_base)
	{
		return CELL_EINVAL;
	}

	return CELL_OK;
}

/*
 * lv2 SysCall 672 (0x2A0): sys_rsx_context_iomap
 * @param context_id (IN): RSX context, E.g. 0x55555555 (in vsh.self)
 * @param io (IN): IO offset mapping area. E.g. 0x00600000
 * @param ea (IN): Start address of mapping area. E.g. 0x20400000
 * @param size (IN): Size of mapping area in bytes. E.g. 0x00200000
 * @param flags (IN):
 */
error_code sys_rsx_context_iomap(u32 context_id, u32 io, u32 ea, u32 size, u64 flags)
{
	sys_rsx.warning("sys_rsx_context_iomap(context_id=0x%x, io=0x%x, ea=0x%x, size=0x%x, flags=0x%llx)", context_id, io, ea, size, flags);

	const auto render = rsx::get_current_renderer();

	if (!size || io & 0xFFFFF || ea + u64{size} > rsx::constants::local_mem_base || ea & 0xFFFFF || size & 0xFFFFF ||
		context_id != 0x55555555 || render->main_mem_size < io + u64{size})
	{
		return CELL_EINVAL;
	}

	if (!render->is_fifo_idle())
	{
		sys_rsx.warning("sys_rsx_context_iomap(): RSX is not idle while mapping io");
	}

	vm::reader_lock rlock;

	for (u32 addr = ea, end = ea + size; addr < end; addr += 0x100000)
	{
		if (!vm::check_addr(addr, 1, vm::page_readable | (addr < 0x20000000 ? 0 : vm::page_1m_size)))
		{
			return CELL_EINVAL;
		}
	}

	io >>= 20, ea >>= 20, size >>= 20;

	std::scoped_lock lock(s_rsxmem_mtx);

	for (u32 i = 0; i < size; i++)
	{
		auto& table = render->iomap_table;

		// TODO: Investigate relaxed memory ordering
		const u32 prev_ea = table.ea[io + i];
		table.ea[io + i].release((ea + i) << 20);
		if (prev_ea + 1) table.io[prev_ea >> 20].release(-1); // Clear previous mapping if exists
		table.io[ea + i].release((io + i) << 20);
	}

	return CELL_OK;
}

/*
 * lv2 SysCall 673 (0x2A1): sys_rsx_context_iounmap
 * @param context_id (IN): RSX context, E.g. 0x55555555 (in vsh.self)
 * @param io (IN): IO address. E.g. 0x00600000 (Start page 6)
 * @param size (IN): Size to unmap in byte. E.g. 0x00200000
 */
error_code sys_rsx_context_iounmap(u32 context_id, u32 io, u32 size)
{
	sys_rsx.warning("sys_rsx_context_iounmap(context_id=0x%x, io=0x%x, size=0x%x)", context_id, io, size);

	const auto render = rsx::get_current_renderer();

	if (!size || size & 0xFFFFF || io & 0xFFFFF || context_id != 0x55555555 ||
			render->main_mem_size < io + u64{size})
	{
		return CELL_EINVAL;
	}

	if (!render->is_fifo_idle())
	{
		sys_rsx.warning("sys_rsx_context_iounmap(): RSX is not idle while unmapping io");
	}

	vm::reader_lock rlock;

	std::scoped_lock lock(s_rsxmem_mtx);

	for (const u32 end = (io >>= 20) + (size >>= 20); io < end;)
	{
		auto& table = render->iomap_table;

		const u32 ea_entry = table.ea[io];
		table.ea[io++].release(-1);
		if (ea_entry + 1) table.io[ea_entry >> 20].release(-1);
	}

	return CELL_OK;
}

/*
 * lv2 SysCall 674 (0x2A2): sys_rsx_context_attribute
 * @param context_id (IN): RSX context, e.g. 0x55555555
 * @param package_id (IN):
 * @param a3 (IN):
 * @param a4 (IN):
 * @param a5 (IN):
 * @param a6 (IN):
 */
error_code sys_rsx_context_attribute(u32 context_id, u32 package_id, u64 a3, u64 a4, u64 a5, u64 a6)
{
	// Flip/queue/reset flip/flip event/user command/vblank as trace to help with log spam
	if (package_id == 0x102 || package_id == 0x103 || package_id == 0x10a || package_id == 0xFEC || package_id == 0xFED || package_id == 0xFEF)
		sys_rsx.trace("sys_rsx_context_attribute(context_id=0x%x, package_id=0x%x, a3=0x%llx, a4=0x%llx, a5=0x%llx, a6=0x%llx)", context_id, package_id, a3, a4, a5, a6);
	else
		sys_rsx.warning("sys_rsx_context_attribute(context_id=0x%x, package_id=0x%x, a3=0x%llx, a4=0x%llx, a5=0x%llx, a6=0x%llx)", context_id, package_id, a3, a4, a5, a6);

	// todo: these event ports probly 'shouldnt' be here as i think its supposed to be interrupts that are sent from rsx somewhere in lv1

	const auto render = rsx::get_current_renderer();

	auto rsx_cfg = g_fxo->get<lv2_rsx_config>();

	if (!rsx_cfg->context_base || context_id != 0x55555555)
	{
		sys_rsx.error("sys_rsx_context_attribute(): invalid context failure (context_id=0x%x)", context_id);
		return CELL_OK; // Actually returns CELL_OK, cellGCmSys seem to be relying on this as well
	}

	auto &driverInfo = vm::_ref<RsxDriverInfo>(rsx_cfg->driver_info);
	switch (package_id)
	{
	case 0x001: // FIFO
	{
		render->pause();
		const u64 get = static_cast<u32>(a3);
		const u64 put = static_cast<u32>(a4);
		vm::_ref<atomic_be_t<u64>>(rsx_cfg->dma_address + ::offset32(&RsxDmaControl::put)).release(put << 32 | get);
		render->sync_point_request.release(true);
		render->unpause();
		break;
	}

	case 0x100: // Display mode set
		break;
	case 0x101: // Display sync set, cellGcmSetFlipMode
		// a4 == 2 is vsync, a4 == 1 is hsync
		render->requested_vsync.store(a4 == 2);
		break;

	case 0x102: // Display flip
	{
		u32 flip_idx = ~0u;

		// high bit signifys grabbing a queued buffer
		// otherwise it contains a display buffer offset
		if ((a4 & 0x80000000) != 0)
		{
			// NOTE: There currently seem to only be 2 active heads on PS3
			verify(HERE), a3 < 2;

			// last half byte gives buffer, 0xf seems to trigger just last queued
			u8 idx_check = a4 & 0xf;
			if (idx_check > 7)
				flip_idx = driverInfo.head[a3].lastQueuedBufferId;
			else
				flip_idx = idx_check;

			// fyi -- u32 hardware_channel = (a4 >> 8) & 0xFF;

			// sanity check, the head should have a 'queued' buffer on it, and it should have been previously 'queued'
			const u32 sanity_check = 0x40000000 & (1 << flip_idx);
			if ((driverInfo.head[a3].flipFlags & sanity_check) != sanity_check)
				rsx_log.error("Display Flip Queued: Flipping non previously queued buffer 0x%llx", a4);
		}
		else
		{
			for (u32 i = 0; i < render->display_buffers_count; ++i)
			{
				if (render->display_buffers[i].offset == a4)
				{
					flip_idx = i;
					break;
				}
			}
			if (flip_idx == ~0u)
			{
				rsx_log.error("Display Flip: Couldn't find display buffer offset, flipping 0. Offset: 0x%x", a4);
				flip_idx = 0;
			}
		}

		render->request_emu_flip(flip_idx);
	}
	break;

	case 0x103: // Display Queue
	{
		// NOTE: There currently seem to only be 2 active heads on PS3
		verify(HERE), a3 < 2;

		driverInfo.head[a3].lastQueuedBufferId = static_cast<u32>(a4);
		driverInfo.head[a3].flipFlags |= 0x40000000 | (1 << a4);

		rsx_cfg->send_event(0, SYS_RSX_EVENT_QUEUE_BASE << a3, 0);

		render->on_frame_end(static_cast<u32>(a4));
	}
	break;

	case 0x104: // Display buffer
	{
		const u8 id = a3 & 0xFF;
		if (id > 7)
		{
			return SYS_RSX_CONTEXT_ATTRIBUTE_ERROR;
		}

		std::lock_guard lock(s_rsxmem_mtx);

		// Note: no error checking is being done

		const u32 width = (a4 >> 32) & 0xFFFFFFFF;
		const u32 height = a4 & 0xFFFFFFFF;
		const u32 pitch = (a5 >> 32) & 0xFFFFFFFF;
		const u32 offset = a5 & 0xFFFFFFFF;

		render->display_buffers[id].width = width;
		render->display_buffers[id].height = height;
		render->display_buffers[id].pitch = pitch;
		render->display_buffers[id].offset = offset;

		render->display_buffers_count = std::max<u32>(id + 1, render->display_buffers_count);
	}
	break;

	case 0x105: // destroy buffer?
		break;

	case 0x106: // ? (Used by cellGcmInitPerfMon)
		break;

	case 0x108: // cellGcmSetSecondVFrequency
		// a4 == 3, CELL_GCM_DISPLAY_FREQUENCY_59_94HZ
		// a4 == 2, CELL_GCM_DISPLAY_FREQUENCY_SCANOUT
		// a4 == 4, CELL_GCM_DISPLAY_FREQUENCY_DISABLE
		// Note: Scanout/59_94 is ignored currently as we report refresh rate of 59_94hz as it is, so the difference doesnt matter
		render->enable_second_vhandler.store(a4 != 4);
		break;

	case 0x10a: // ? Involved in managing flip status through cellGcmResetFlipStatus
	{
		if (a3 > 7)
		{
			return SYS_RSX_CONTEXT_ATTRIBUTE_ERROR;
		}

		// NOTE: There currently seem to only be 2 active heads on PS3
		verify(HERE), a3 < 2;

		driverInfo.head[a3].flipFlags.atomic_op([&](be_t<u32>& flipStatus)
		{
			flipStatus = (flipStatus & static_cast<u32>(a4)) | static_cast<u32>(a5);
		});
	}
	break;

	case 0x10D: // Called by cellGcmInitCursor
		break;

	case 0x300: // Tiles
	{
		//a4 high bits = ret.tile = (location + 1) | (bank << 4) | ((offset / 0x10000) << 16) | (location << 31);
		//a4 low bits = ret.limit = ((offset + size - 1) / 0x10000) << 16 | (location << 31);
		//a5 high bits = ret.pitch = (pitch / 0x100) << 8;
		//a5 low bits = ret.format = base | ((base + ((size - 1) / 0x10000)) << 13) | (comp << 26) | (1 << 30);

		verify(HERE), a3 < std::size(render->tiles);

		if (!render->is_fifo_idle())
		{
			sys_rsx.warning("sys_rsx_context_attribute(): RSX is not idle while setting tile");
		}

		auto& tile = render->tiles[a3];

		const u32 location = ((a4 >> 32) & 0x3) - 1;
		const u32 offset = ((((a4 >> 32) & 0x7FFFFFFF) >> 16) * 0x10000);
		const u32 size = ((((a4 & 0x7FFFFFFF) >> 16) + 1) * 0x10000) - offset;
		const u32 pitch = (((a5 >> 32) & 0xFFFFFFFF) >> 8) * 0x100;
		const u32 comp = ((a5 & 0xFFFFFFFF) >> 26) & 0xF;
		const u32 base = (a5 & 0xFFFFFFFF) & 0x7FF;
		const u32 bank = (((a4 >> 32) & 0xFFFFFFFF) >> 4) & 0xF;
		const bool bound = ((a4 >> 32) & 0x3) != 0;

		const auto range = utils::address_range::start_length(offset, size);

		if (bound)
		{
			if (!size || !pitch)
			{
				return CELL_EINVAL;
			}

			u32 limit = UINT32_MAX;

			switch (location)
			{
			case CELL_GCM_LOCATION_MAIN: limit = render->main_mem_size; break;
			case CELL_GCM_LOCATION_LOCAL: limit = render->local_mem_size; break;
			default: fmt::throw_exception("sys_rsx_context_attribute(): Unexpected location value (location=0x%x)" HERE, location);
			}

			if (!range.valid() || range.end >= limit)
			{
				return CELL_EINVAL;
			}

			// Hardcoded value in gcm
			verify(HERE), !!(a5 & (1 << 30));
		}

		std::lock_guard lock(s_rsxmem_mtx);

		// When tile is going to be unbound, we can use it as a hint that the address will no longer be used as a surface and can be removed/invalidated
		// Todo: There may be more checks such as format/size/width can could be done
		if (tile.bound && !bound)
			render->notify_tile_unbound(static_cast<u32>(a3));

		if (location == CELL_GCM_LOCATION_MAIN && bound)
		{
			vm::reader_lock rlock;

			for (u32 io = (offset >> 20), end = (range.end >> 20); io <= end; io++)
			{
				if (render->iomap_table.ea[io] == umax)
				{
					return CELL_EINVAL;
				}
			}
		}

		tile.location = location;
		tile.offset = offset;
		tile.size = size;
		tile.pitch = pitch;
		tile.comp = comp;
		tile.base = base;
		tile.bank = base;
		tile.bound = bound;
	}
	break;

	case 0x301: // Depth-buffer (Z-cull)
	{
		//a4 high = region = (1 << 0) | (zFormat << 4) | (aaFormat << 8);
		//a4 low = size = ((width >> 6) << 22) | ((height >> 6) << 6);
		//a5 high = start = cullStart&(~0xFFF);
		//a5 low = offset = offset;
		//a6 high = status0 = (zcullDir << 1) | (zcullFormat << 2) | ((sFunc & 0xF) << 12) | (sRef << 16) | (sMask << 24);
		//a6 low = status1 = (0x2000 << 0) | (0x20 << 16);

		verify(HERE), a3 < std::size(render->zculls);

		if (!render->is_fifo_idle())
		{
			sys_rsx.warning("sys_rsx_context_attribute(): RSX is not idle while setting zcull");
		}

		const u32 width = ((a4 & 0xFFFFFFFF) >> 22) << 6;
		const u32 height = ((a4 & 0x0000FFFF) >> 6) << 6;
		const u32 cullStart = (a5 >> 32) & ~0xFFF;
		const u32 offset = (a5 & 0x0FFFFFFF);
		const bool bound = (a6 & 0xFFFFFFFF) != 0;

		if (bound)
		{
			const auto cull_range = utils::address_range::start_length(cullStart, width * height);

			// cullStart is an offset inside ZCULL RAM which is 3MB long, check bounds
			// width and height are not allowed to be zero (checked by range.valid())
			if (!cull_range.valid() || cull_range.end >= 3u << 20 || offset >= render->local_mem_size)
			{
				return CELL_EINVAL;
			}

			if (a5 & 0xF0000000)
			{
				sys_rsx.warning("sys_rsx_context_attribute(): ZCULL offset greater than 256MB (offset=0x%x)", offset);
			}

			// Hardcoded values in gcm
			verify(HERE), !!(a4 & (1ull << 32)), (a6 & 0xFFFFFFFF) == 0u + ((0x2000 << 0) | (0x20 << 16));
		}

		std::lock_guard lock(s_rsxmem_mtx);

		auto &zcull = render->zculls[a3];

		zcull.zFormat = ((a4 >> 32) >> 4) & 0xF;
		zcull.aaFormat = ((a4 >> 32) >> 8) & 0xF;
		zcull.width = width;
		zcull.height = height;
		zcull.cullStart = cullStart;
		zcull.offset = offset;
		zcull.zcullDir = ((a6 >> 32) >> 1) & 0x1;
		zcull.zcullFormat = ((a6 >> 32) >> 2) & 0x3FF;
		zcull.sFunc = ((a6 >> 32) >> 12) & 0xF;
		zcull.sRef = ((a6 >> 32) >> 16) & 0xFF;
		zcull.sMask = ((a6 >> 32) >> 24) & 0xFF;
		zcull.bound = bound;
	}
	break;

	case 0x302: // something with zcull
		break;

	case 0x600: // Framebuffer setup
		break;

	case 0x601: // Framebuffer blit
		break;

	case 0x602: // Framebuffer blit sync
		break;

	case 0x603: // Framebuffer close
		break;

	case 0xFEC: // hack: flip event notification

		// we only ever use head 1 for now
		driverInfo.head[1].flipFlags |= 0x80000000;
		driverInfo.head[1].lastFlipTime = rsxTimeStamp(); // should rsxthread set this?
		driverInfo.head[1].flipBufferId = static_cast<u32>(a3);

		// seems gcmSysWaitLabel uses this offset, so lets set it to 0 every flip
		vm::_ref<u32>(render->label_addr + 0x10) = 0;

		rsx_cfg->send_event(0, SYS_RSX_EVENT_FLIP_BASE << 1, 0);
		break;

	case 0xFED: // hack: vblank command
	{
		// NOTE: There currently seem to only be 2 active heads on PS3
		verify(HERE), a3 < 2;

		// todo: this is wrong and should be 'second' vblank handler and freq, but since currently everything is reported as being 59.94, this should be fine
		vm::_ref<u32>(render->device_addr + 0x30) = 1;

		const u64 current_time = rsxTimeStamp();

		driverInfo.head[a3].lastSecondVTime = current_time;

		// Note: not atomic
		driverInfo.head[a3].lastVTimeLow = static_cast<u32>(current_time);
		driverInfo.head[a3].lastVTimeHigh = static_cast<u32>(current_time >> 32);

		driverInfo.head[a3].vBlankCount++;

		u64 event_flags = SYS_RSX_EVENT_VBLANK;

		if (render->enable_second_vhandler)
			event_flags |= SYS_RSX_EVENT_SECOND_VBLANK_BASE << a3; // second vhandler

		rsx_cfg->send_event(0, event_flags, 0);
		break;
	}

	case 0xFEF: // hack: user command
		// 'custom' invalid package id for now
		// as i think we need custom lv1 interrupts to handle this accurately
		// this also should probly be set by rsxthread
		driverInfo.userCmdParam = static_cast<u32>(a4);
		rsx_cfg->send_event(0, SYS_RSX_EVENT_USER_CMD, 0);
		break;

	default:
		return CELL_EINVAL;
	}

	return CELL_OK;
}

/*
 * lv2 SysCall 675 (0x2A3): sys_rsx_device_map
 * @param a1 (OUT): rsx device map address : 0x40000000, 0x50000000.. 0xB0000000
 * @param a2 (OUT): Unused
 * @param dev_id (IN): An immediate value and always 8. (cellGcmInitPerfMon uses 11, 10, 9, 7, 12 successively).
 */
error_code sys_rsx_device_map(vm::ptr<u64> dev_addr, vm::ptr<u64> a2, u32 dev_id)
{
	sys_rsx.warning("sys_rsx_device_map(dev_addr=*0x%x, a2=*0x%x, dev_id=0x%x)", dev_addr, a2, dev_id);

	if (dev_id != 8) {
		// TODO: lv1 related
		fmt::throw_exception("sys_rsx_device_map: Invalid dev_id %d", dev_id);
	}

	auto rsx_cfg = g_fxo->get<lv2_rsx_config>();

	static shared_mutex device_map_mtx;
	std::scoped_lock lock(device_map_mtx);

	if (!rsx_cfg->device_addr)
	{
		const auto area = vm::reserve_map(vm::rsx_context, 0, 0x10000000, 0x403);
		const u32 addr = area ? area->alloc(0x100000) : 0;

		if (!addr)
		{
			return CELL_ENOMEM;
		}

		*dev_addr = addr;
		rsx_cfg->device_addr = addr;
		return CELL_OK;
	}

	*dev_addr = rsx_cfg->device_addr;
	return CELL_OK;
}

/*
 * lv2 SysCall 676 (0x2A4): sys_rsx_device_unmap
 * @param dev_id (IN): An immediate value and always 8.
 */
error_code sys_rsx_device_unmap(u32 dev_id)
{
	sys_rsx.todo("sys_rsx_device_unmap(dev_id=0x%x)", dev_id);

	return CELL_OK;
}

/*
 * lv2 SysCall 677 (0x2A5): sys_rsx_attribute
 */
error_code sys_rsx_attribute(u32 packageId, u32 a2, u32 a3, u32 a4, u32 a5)
{
	sys_rsx.warning("sys_rsx_attribute(packageId=0x%x, a2=0x%x, a3=0x%x, a4=0x%x, a5=0x%x)", packageId, a2, a3, a4, a5);

	return CELL_OK;
}
