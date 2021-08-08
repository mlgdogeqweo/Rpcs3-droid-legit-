#include "stdafx.h"
#include "pad_thread.h"
#include "product_info.h"
#include "ds3_pad_handler.h"
#include "ds4_pad_handler.h"
#include "dualsense_pad_handler.h"
#ifdef _WIN32
#include "xinput_pad_handler.h"
#include "mm_joystick_handler.h"
#elif HAVE_LIBEVDEV
#include "evdev_joystick_handler.h"
#endif
#include "keyboard_pad_handler.h"
#include "Emu/Io/Null/NullPadHandler.h"
#include "Emu/Io/PadHandler.h"
#include "Emu/Io/pad_config.h"

LOG_CHANNEL(input_log, "Input");

namespace pad
{
	atomic_t<pad_thread*> g_current = nullptr;
	shared_mutex g_pad_mutex;
	std::string g_title_id;
	atomic_t<bool> g_reset{false};
	atomic_t<bool> g_enabled{true};
	atomic_t<bool> g_active{false};
}

struct pad_setting
{
	u32 port_status = 0;
	u32 device_capability = 0;
	u32 device_type = 0;
	s32 ldd_handle = -1;
};

pad_thread::pad_thread(void *_curthread, void *_curwindow, std::string_view title_id) : curthread(_curthread), curwindow(_curwindow)
{
	pad::g_title_id = title_id;
	Init();

	thread = std::make_shared<std::thread>(&pad_thread::ThreadFunc, this);
	pad::g_current = this;
}

pad_thread::~pad_thread()
{
	pad::g_current = nullptr;
	pad::g_active = false;
	thread->join();

	handlers.clear();
}

void pad_thread::Init()
{
	std::lock_guard lock(pad::g_pad_mutex);

	// Cache old settings if possible
	std::array<pad_setting, CELL_PAD_MAX_PORT_NUM> pad_settings;
	for (u32 i = 0; i < CELL_PAD_MAX_PORT_NUM; i++) // max 7 pads
	{
		if (m_pads[i])
		{
			pad_settings[i] =
			{
				m_pads[i]->m_port_status,
				m_pads[i]->m_device_capability,
				m_pads[i]->m_device_type,
				m_pads[i]->ldd ? static_cast<s32>(i) : -1
			};
		}
		else
		{
			pad_settings[i] =
			{
				CELL_PAD_STATUS_DISCONNECTED,
				CELL_PAD_CAPABILITY_PS3_CONFORMITY | CELL_PAD_CAPABILITY_PRESS_MODE | CELL_PAD_CAPABILITY_ACTUATOR,
				CELL_PAD_DEV_TYPE_STANDARD,
				-1
			};
		}
	}

	num_ldd_pad = 0;

	m_info.now_connect = 0;

	handlers.clear();

	g_cfg_input.load(pad::g_title_id);

	std::shared_ptr<keyboard_pad_handler> keyptr;

	// Always have a Null Pad Handler
	std::shared_ptr<NullPadHandler> nullpad = std::make_shared<NullPadHandler>();
	handlers.emplace(pad_handler::null, nullpad);

	for (u32 i = 0; i < CELL_PAD_MAX_PORT_NUM; i++) // max 7 pads
	{
		std::shared_ptr<PadHandlerBase> cur_pad_handler;

		const bool is_ldd_pad = pad_settings[i].ldd_handle == static_cast<s32>(i);
		const auto handler_type = is_ldd_pad ? pad_handler::null : g_cfg_input.player[i]->handler.get();

		if (handlers.contains(handler_type))
		{
			cur_pad_handler = handlers[handler_type];
		}
		else
		{
			switch (handler_type)
			{
			case pad_handler::keyboard:
				keyptr = std::make_shared<keyboard_pad_handler>();
				keyptr->moveToThread(static_cast<QThread*>(curthread));
				keyptr->SetTargetWindow(static_cast<QWindow*>(curwindow));
				cur_pad_handler = keyptr;
				break;
			case pad_handler::ds3:
				cur_pad_handler = std::make_shared<ds3_pad_handler>();
				break;
			case pad_handler::ds4:
				cur_pad_handler = std::make_shared<ds4_pad_handler>();
				break;
			case pad_handler::dualsense:
				cur_pad_handler = std::make_shared<dualsense_pad_handler>();
				break;
#ifdef _WIN32
			case pad_handler::xinput:
				cur_pad_handler = std::make_shared<xinput_pad_handler>();
				break;
			case pad_handler::mm:
				cur_pad_handler = std::make_shared<mm_joystick_handler>();
				break;
#endif
#ifdef HAVE_LIBEVDEV
			case pad_handler::evdev:
				cur_pad_handler = std::make_shared<evdev_joystick_handler>();
				break;
#endif
			case pad_handler::null:
				break;
			}
			handlers.emplace(handler_type, cur_pad_handler);
		}
		cur_pad_handler->set_player(i);
		cur_pad_handler->Init();

		m_pads[i] = std::make_shared<Pad>(CELL_PAD_STATUS_DISCONNECTED, pad_settings[i].device_capability, pad_settings[i].device_type);

		if (is_ldd_pad)
		{
			InitLddPad(pad_settings[i].ldd_handle);
		}
		else if (cur_pad_handler->bindPadToDevice(m_pads[i], g_cfg_input.player[i]->device.to_string()) == false)
		{
			// Failed to bind the device to cur_pad_handler so binds to NullPadHandler
			input_log.error("Failed to bind device %s to handler %s", g_cfg_input.player[i]->device.to_string(), handler_type);
			nullpad->bindPadToDevice(m_pads[i], g_cfg_input.player[i]->device.to_string());
		}

		m_pads_interface[i] = std::make_shared<Pad>(CELL_PAD_STATUS_DISCONNECTED, pad_settings[i].device_capability, pad_settings[i].device_type);
		*m_pads_interface[i] = *m_pads[i];
	}
}

void pad_thread::SetRumble(const u32 pad, u8 largeMotor, bool smallMotor)
{
	if (pad > m_pads_interface.size())
		return;

	if (m_pads_interface[pad]->m_vibrateMotors.size() >= 2)
	{
		m_pads_interface[pad]->m_vibrateMotors[0].m_value = largeMotor;
		m_pads_interface[pad]->m_vibrateMotors[1].m_value = smallMotor ? 255 : 0;
	}
}

void pad_thread::SetIntercepted(bool intercepted)
{
	if (intercepted)
	{
		m_info.system_info |= CELL_PAD_INFO_INTERCEPTED;
		m_info.ignore_input = true;
	}
	else
	{
		m_info.system_info &= ~CELL_PAD_INFO_INTERCEPTED;
	}
}

void pad_thread::ThreadFunc()
{
	pad::g_active = true;
	while (pad::g_active)
	{
		if (!pad::g_enabled)
		{
			std::this_thread::sleep_for(1ms);
			continue;
		}

		if (pad::g_reset && pad::g_reset.exchange(false))
		{
			Init();
		}

		u32 connected_devices = 0;

		// Copy public pad data - which might have been changed - to internal pads
		{
			std::lock_guard lock(pad::g_pad_mutex);

			for (usz i = 0; i < m_pads.size(); i++)
			{
				*m_pads[i] = *m_pads_interface[i];
			}
		}

		for (auto& cur_pad_handler : handlers)
		{
			cur_pad_handler.second->ThreadProc();
			connected_devices += cur_pad_handler.second->connected_devices;
		}

		// Copy new internal pad data back to public pads
		{
			std::lock_guard lock(pad::g_pad_mutex);

			m_info.now_connect = connected_devices + num_ldd_pad;

			// The input_ignored section is only reached when a dialog was closed and the pads are still intercepted.
			// As long as any of the listed buttons is pressed, cellPadGetData will ignore all input (needed for Hotline Miami).
			// ignore_input was added because if we keep the pads intercepted, then some games will enter the menu due to unexpected system interception (tested with Ninja Gaiden Sigma).
			const bool input_ignored = m_info.ignore_input && !(m_info.system_info & CELL_PAD_INFO_INTERCEPTED);
			bool any_button_pressed = false;

			for (usz i = 0; i < m_pads.size(); i++)
			{
				const auto& pad = m_pads[i];

				// I guess this is the best place to add pressure sensitivity without too much code duplication.
				if (pad->m_port_status & CELL_PAD_STATUS_CONNECTED)
				{
					const bool adjust_pressure = pad->m_pressure_intensity_button_index >= 0 && pad->m_buttons[pad->m_pressure_intensity_button_index].m_pressed;

					for (auto& button : pad->m_buttons)
					{
						if (button.m_pressed)
						{
							if (button.m_outKeyCode == CELL_PAD_CTRL_CROSS ||
								button.m_outKeyCode == CELL_PAD_CTRL_CIRCLE ||
								button.m_outKeyCode == CELL_PAD_CTRL_TRIANGLE ||
								button.m_outKeyCode == CELL_PAD_CTRL_SQUARE ||
								button.m_outKeyCode == CELL_PAD_CTRL_START ||
								button.m_outKeyCode == CELL_PAD_CTRL_SELECT)
							{
								any_button_pressed = true;
							}

							if (adjust_pressure)
							{
								button.m_value = pad->m_pressure_intensity;
							}
						}
					}
				}

				*m_pads_interface[i] = *pad;
			}

			if (input_ignored && !any_button_pressed)
			{
				m_info.ignore_input = false;
			}
		}

		std::this_thread::sleep_for(1ms);
	}
}

void pad_thread::InitLddPad(u32 handle)
{
	if (handle >= m_pads.size())
	{
		return;
	}

	static const auto product = input::get_product_info(input::product_type::playstation_3_controller);

	m_pads[handle]->ldd = true;
	m_pads[handle]->Init
	(
		CELL_PAD_STATUS_CONNECTED | CELL_PAD_STATUS_ASSIGN_CHANGES | CELL_PAD_STATUS_CUSTOM_CONTROLLER,
		CELL_PAD_CAPABILITY_PS3_CONFORMITY,
		CELL_PAD_DEV_TYPE_LDD,
		0, // CELL_PAD_PCLASS_TYPE_STANDARD
		product.pclass_profile,
		product.vendor_id,
		product.product_id,
		50
	);

	*m_pads_interface[handle] = *m_pads[handle];

	num_ldd_pad++;
}

s32 pad_thread::AddLddPad()
{
	// Look for first null pad
	for (u32 i = 0; i < CELL_PAD_MAX_PORT_NUM; i++)
	{
		if (g_cfg_input.player[i]->handler == pad_handler::null && !m_pads[i]->ldd)
		{
			InitLddPad(i);
			return i;
		}
	}

	return -1;
}

void pad_thread::UnregisterLddPad(u32 handle)
{
	ensure(handle < m_pads.size());

	m_pads[handle]->ldd = false;
	m_pads_interface[handle]->ldd = false;

	num_ldd_pad--;
}
