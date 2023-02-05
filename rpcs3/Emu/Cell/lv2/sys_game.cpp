#include "stdafx.h"
#include "util/sysinfo.hpp"
#include "util/v128.hpp"
#include "Emu/Memory/vm_ptr.h"
#include "Emu/Cell/ErrorCodes.h"
#include "Emu/System.h"
#include "Emu/system_utils.hpp"
#include "Emu/IdManager.h"
#include "Utilities/Thread.h"

#include "sys_game.h"

LOG_CHANNEL(sys_game);

struct board_storage
{
public:
	bool read(u8* buffer)
	{
		if (!buffer)
			return false;

		const auto data = storage.load();
		memcpy(buffer, &data, size);

		return true;
	}

	bool write(u8* buffer)
	{
		if (!buffer)
			return false;

		storage.store(read_from_ptr<be_t<v128>>(buffer));
		written = true;

		return true;
	}

	board_storage()
	{
		if (fs::file file; file.open(file_path, fs::read))
			file.read(&storage.raw(), size);
		else
			memset(&storage.raw(), 0, size);
	}

	board_storage(const board_storage&) = delete;

	board_storage& operator =(const board_storage&) = delete;

	~board_storage()
	{
		if (written)
		{
			if (fs::file file; file.open(file_path, fs::create + fs::write + fs::lock))
			{
				file.write(&storage.raw(), size);
				file.trunc(size);
			}
		}
	}

private:
	atomic_be_t<v128> storage;
	bool written = false;
	const std::string file_path = rpcs3::utils::get_hdd1_dir() + "/caches/board_storage.bin";
	static constexpr size_t size = sizeof(v128);
};

struct watchdog_t
{
	struct alignas(8) control_t
	{
		bool needs_restart = false;
		bool active = false;
		char pad[sizeof(u32) - sizeof(bool) * 2]{};
		u32 timeout = 0;
	};

	atomic_t<control_t> control;

	void operator()()
	{
		u64 start_time = get_system_time();
		u64 old_time = start_time;
		u64 current_time = old_time;

		constexpr u64 sleep_time = 50'000;

		while (thread_ctrl::state() != thread_state::aborting)
		{
			if (Emu.GetStatus(false) == system_state::paused)
			{
				start_time += current_time - old_time;
				old_time = current_time;
				thread_ctrl::wait_for(sleep_time);
				current_time = get_system_time();
				continue;
			}

			old_time = std::exchange(current_time, get_system_time());

			const auto old = control.fetch_op([&](control_t& data)
			{
				if (data.needs_restart)
				{
					data.needs_restart = false;
					return true;
				}

				return false;
			}).first;

			if (old.active && old.needs_restart)
			{
				start_time = current_time;
				old_time = current_time;
				continue;
			}

			if (old.active && current_time - start_time >= old.timeout)
			{
				sys_game.success("Watchdog timeout! Restarting the game...");

				Emu.CallFromMainThread([]()
				{
					Emu.Restart(false);
				});

				return;
			}

			thread_ctrl::wait_for(sleep_time);
		}
	}

	static constexpr auto thread_name = "LV2 Watchdog Thread"sv;
};

void abort_lv2_watchdog()
{
	if (auto thr = g_fxo->try_get<named_thread<watchdog_t>>())
	{
		sys_game.notice("Aborting %s...", thr->thread_name);
		*thr = thread_state::aborting;
	}
}

error_code _sys_game_watchdog_start(u32 timeout)
{
	sys_game.trace("sys_game_watchdog_start(timeout=%d)", timeout);

	// According to disassembly
	timeout *= 1'000'000;
	timeout &= -64;

	if (!g_fxo->get<named_thread<watchdog_t>>().control.fetch_op([&](watchdog_t::control_t& data)
	{
		if (data.active)
		{
			return false;
		}

		data.needs_restart = true;
		data.active = true;
		data.timeout = timeout;
		return true;
	}).second)
	{
		return CELL_EABORT;
	}

	return CELL_OK;
}

error_code _sys_game_watchdog_stop()
{
	sys_game.trace("sys_game_watchdog_stop()");

	g_fxo->get<named_thread<watchdog_t>>().control.fetch_op([](watchdog_t::control_t& data)
	{
		if (!data.active)
		{
			return false;
		}

		data.active = false;
		return true;
	});

	return CELL_OK;
}

error_code _sys_game_watchdog_clear()
{
	sys_game.trace("sys_game_watchdog_clear()");

	g_fxo->get<named_thread<watchdog_t>>().control.fetch_op([](watchdog_t::control_t& data)
	{
		if (!data.active || data.needs_restart)
		{
			return false;
		}

		data.needs_restart = true;
		return true;
	});

	return CELL_OK;
}

u64 _sys_game_get_system_sw_version()
{
	return stof(utils::get_firmware_version()) * 10000;
}

error_code _sys_game_board_storage_read(vm::ptr<u8> buffer1, vm::ptr<u8> buffer2)
{
	sys_game.trace("sys_game_board_storage_read(buffer1=*0x%x, buffer2=*0x%x)", buffer1, buffer2);

	if (!buffer1 || !buffer2)
	{
		return CELL_EFAULT;
	}

	*buffer2 = g_fxo->get<board_storage>().read(buffer1.get_ptr()) ? 0x00 : 0xFF;

	return CELL_OK;
}

error_code _sys_game_board_storage_write(vm::ptr<u8> buffer1, vm::ptr<u8> buffer2)
{
	sys_game.trace("sys_game_board_storage_write(buffer1=*0x%x, buffer2=*0x%x)", buffer1, buffer2);

	if (!buffer1 || !buffer2)
	{
		return CELL_EFAULT;
	}

	*buffer2 = g_fxo->get<board_storage>().write(buffer1.get_ptr()) ? 0x00 : 0xFF;

	return CELL_OK;
}

error_code _sys_game_get_rtc_status(vm::ptr<s32> status)
{
	sys_game.trace("sys_game_get_rtc_status(status=*0x%x)", status);

	if (!status)
	{
		return CELL_EFAULT;
	}

	*status = 0;

	return CELL_OK;
}
