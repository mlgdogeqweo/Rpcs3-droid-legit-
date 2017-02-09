#include "stdafx.h"
#include "stdafx_gui.h"
#ifdef _MSC_VER
#include "MMJoystickHandler.h"

namespace {
	const DWORD THREAD_SLEEP = 10;
	const DWORD THREAD_SLEEP_INACTIVE = 100;
	const DWORD THREAD_TIMEOUT = 1000;

	inline u16 ConvertAxis(DWORD value)
	{
		return static_cast<u16>((value) >> 8);
	}
}


MMJoystickHandler::MMJoystickHandler() : active(false), thread(nullptr)
{
}

MMJoystickHandler::~MMJoystickHandler()
{
	Close();
}

void MMJoystickHandler::Init(const u32 max_connect)
{
	supportedJoysticks = joyGetNumDevs();
	if (supportedJoysticks > 0)
	{
		LOG_ERROR(HLE, "Driver supports %u joysticks", supportedJoysticks);
	}
	else
	{
		LOG_ERROR(HLE, "Driver doesn't support Joysticks");
	}
	js_info.dwSize = sizeof(js_info);
	js_info.dwFlags = JOY_RETURNALL;
	joyGetDevCaps(JOYSTICKID1, &js_caps, sizeof(js_caps));
	bool JoyPresent = (joyGetPosEx(JOYSTICKID1, &js_info) == JOYERR_NOERROR);
	if (JoyPresent)
	{
		LOG_ERROR(HLE, "Found connected joystick with %u buttons", js_caps.wNumButtons);
		
		std::memset(&m_info, 0, sizeof m_info);
		m_info.max_connect = max_connect;

		for (u32 i = 0, max = std::min(max_connect, u32(1)); i != max; ++i)
		{
			m_pads.emplace_back(
				CELL_PAD_STATUS_ASSIGN_CHANGES,
				CELL_PAD_SETTING_PRESS_OFF | CELL_PAD_SETTING_SENSOR_OFF,
				CELL_PAD_CAPABILITY_PS3_CONFORMITY | CELL_PAD_CAPABILITY_PRESS_MODE,
				CELL_PAD_DEV_TYPE_STANDARD
			);
			auto & pad = m_pads.back();

			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, JOY_BUTTON1, CELL_PAD_CTRL_TRIANGLE);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, JOY_BUTTON2, CELL_PAD_CTRL_CIRCLE);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, JOY_BUTTON3, CELL_PAD_CTRL_CROSS);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, JOY_BUTTON4, CELL_PAD_CTRL_SQUARE);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, JOY_BUTTON5, CELL_PAD_CTRL_L2);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, JOY_BUTTON6, CELL_PAD_CTRL_R2);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, JOY_BUTTON7, CELL_PAD_CTRL_L1);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, JOY_BUTTON8, CELL_PAD_CTRL_R1);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, JOY_BUTTON9, CELL_PAD_CTRL_START);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, JOY_BUTTON10, CELL_PAD_CTRL_SELECT);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, JOY_BUTTON11, CELL_PAD_CTRL_L3);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, JOY_BUTTON12, CELL_PAD_CTRL_R3);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, 0, 0x100/*CELL_PAD_CTRL_PS*/);// TODO: PS button support

			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, JOY_POVFORWARD, CELL_PAD_CTRL_UP);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, JOY_POVBACKWARD, CELL_PAD_CTRL_DOWN);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, JOY_POVLEFT, CELL_PAD_CTRL_LEFT);
			pad.m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, JOY_POVRIGHT, CELL_PAD_CTRL_RIGHT);

			pad.m_sticks.emplace_back(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X, 0, 0);
			pad.m_sticks.emplace_back(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y, 0, 0);
			pad.m_sticks.emplace_back(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X, 0, 0);
			pad.m_sticks.emplace_back(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y, 0, 0);

			active = true;
			thread = CreateThread(NULL, 0, &MMJoystickHandler::ThreadProcProxy, this, 0, NULL);

		}
	}
	else
	{
		LOG_ERROR(HLE, "Joystick not found");
	}
}

void MMJoystickHandler::Close()
{
	if (active)
	{
		if (thread)
		{
			active = false;
			if (WaitForSingleObject(thread, THREAD_TIMEOUT) != WAIT_OBJECT_0)
				LOG_ERROR(HLE, "MMJoystick thread could not stop within %d milliseconds", (u32)THREAD_TIMEOUT);
			thread = nullptr;
		}
	}

	m_pads.clear();
}

DWORD MMJoystickHandler::ThreadProcedure()
{
	while (active)
	{
		MMRESULT status;
		DWORD online = 0;

		for (DWORD i = 0; i != m_pads.size(); ++i)
		{
			
			auto & pad = m_pads[i];
			status =joyGetPosEx(JOYSTICKID1, &js_info);

			switch (status)
			{
			case JOYERR_UNPLUGGED:
				pad.m_port_status &= ~CELL_PAD_STATUS_CONNECTED;
				break;

			case JOYERR_NOERROR:
				++online;
				pad.m_port_status |= CELL_PAD_STATUS_CONNECTED;
				for (DWORD j = 0; j <= 12; j++)
				{
					bool pressed = js_info.dwButtons & pad.m_buttons[j].m_keyCode;
					pad.m_buttons[j].m_pressed = pressed;
					pad.m_buttons[j].m_value = pressed ? 255 : 0;
				}
				for (DWORD j = 13; j <= 16; j++)//POV aka digital pad
				{
					bool pressed = js_info.dwPOV == pad.m_buttons[j].m_keyCode;
					pad.m_buttons[j].m_pressed = pressed;
					pad.m_buttons[j].m_value = pressed ? 255 : 0;
				}
				pad.m_sticks[0].m_value = ConvertAxis(js_info.dwXpos);
				pad.m_sticks[1].m_value = ConvertAxis(js_info.dwYpos);
				pad.m_sticks[2].m_value = ConvertAxis(js_info.dwZpos);
				pad.m_sticks[3].m_value = ConvertAxis(js_info.dwRpos);
				break;
			}
		}

		Sleep((online > 0) ? THREAD_SLEEP : THREAD_SLEEP_INACTIVE);
		m_info.now_connect = online;
	}

	return 0;
}

DWORD WINAPI MMJoystickHandler::ThreadProcProxy(LPVOID parameter)
{
	return reinterpret_cast<MMJoystickHandler *>(parameter)->ThreadProcedure();
}

#endif
