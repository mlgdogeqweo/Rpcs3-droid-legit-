#include "stdafx.h"
#include "Emu/System.h"
#include "CPUThread.h"

DECLARE(cpu_thread::g_threads_created){0};
DECLARE(cpu_thread::g_threads_deleted){0};

template<>
void fmt_class_string<cpu_flag>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](cpu_flag f)
	{
		switch (f)
		{
		case cpu_flag::stop: return "STOP";
		case cpu_flag::exit: return "EXIT";
		case cpu_flag::suspend: return "s";
		case cpu_flag::ret: return "ret";
		case cpu_flag::signal: return "sig";
		case cpu_flag::dbg_global_pause: return "G.PAUSE";
		case cpu_flag::dbg_global_stop: return "G.EXIT";
		case cpu_flag::dbg_pause: return "PAUSE";
		case cpu_flag::dbg_step: return "STEP";
		case cpu_flag::__bitset_enum_max: break;
		}

		return unknown;
	});
}

template<>
void fmt_class_string<bs_t<cpu_flag>>::format(std::string& out, u64 arg)
{
	format_bitset(out, arg, "[", "|", "]", &fmt_class_string<cpu_flag>::format);
}

thread_local cpu_thread* g_tls_current_cpu_thread = nullptr;

void cpu_thread::on_task()
{
	state -= cpu_flag::exit;

	g_tls_current_cpu_thread = this;

	// Check thread status
	while (!test(state, cpu_flag::exit + cpu_flag::dbg_global_stop))
	{
		// Check stop status
		if (!test(state & cpu_flag::stop))
		{
			try
			{
				cpu_task();
			}
			catch (cpu_flag _s)
			{
				state += _s;
			}
			catch (const std::exception&)
			{
				LOG_NOTICE(GENERAL, "\n%s", dump());
				throw;
			}

			state -= cpu_flag::ret;
			continue;
		}

		thread_ctrl::wait();
	}
}

void cpu_thread::on_stop()
{
	state += cpu_flag::exit;
	notify();
}

cpu_thread::~cpu_thread()
{
	g_threads_deleted++;
}

cpu_thread::cpu_thread(u32 id)
	: id(id)
{
	g_threads_created++;
}

bool cpu_thread::check_state()
{
	bool cpu_sleep_called = false;

	while (true)
	{
		if (test(state & cpu_flag::exit))
		{
			return true;
		}

		if (test(state & cpu_flag::signal) && state.test_and_reset(cpu_flag::signal))
		{
			cpu_sleep_called = false;
		}

		if (!test(state & (cpu_state_pause + cpu_flag::dbg_global_stop)))
		{
			break;
		}

		if (test(state & cpu_flag::suspend) && !cpu_sleep_called)
		{
			cpu_sleep();
			cpu_sleep_called = true;
			continue;
		}

		thread_ctrl::wait();
	}

	const auto state_ = state.load();

	if (test(state_, cpu_flag::ret + cpu_flag::stop))
	{
		return true;
	}

	if (test(state_, cpu_flag::dbg_step))
	{
		state += cpu_flag::dbg_pause;
		state -= cpu_flag::dbg_step;
	}

	return false;
}

void cpu_thread::run()
{
	state -= cpu_flag::stop;
	notify();
}

std::string cpu_thread::dump() const
{
	return fmt::format("Type: %s\n" "State: %s\n", typeid(*this).name(), state.load());
}
