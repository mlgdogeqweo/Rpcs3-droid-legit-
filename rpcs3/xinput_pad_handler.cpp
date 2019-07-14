﻿
#ifdef _WIN32
#include "xinput_pad_handler.h"

#define NOMINMAX
#include <Windows.h>
#include <Xinput.h>

// ScpToolkit defined structure for pressure sensitive button query
typedef struct
{
	float SCP_UP;
	float SCP_RIGHT;
	float SCP_DOWN;
	float SCP_LEFT;

	float SCP_LX;
	float SCP_LY;

	float SCP_L1;
	float SCP_L2;
	float SCP_L3;

	float SCP_RX;
	float SCP_RY;

	float SCP_R1;
	float SCP_R2;
	float SCP_R3;

	float SCP_T;
	float SCP_C;
	float SCP_X;
	float SCP_S;

	float SCP_SELECT;
	float SCP_START;

	float SCP_PS;

} SCP_EXTN;

class xinput_pad_processor_base
{
protected:
	typedef DWORD(WINAPI* PFN_XINPUTSETSTATE)(DWORD, XINPUT_VIBRATION*);
	typedef DWORD(WINAPI* PFN_XINPUTGETBATTERYINFORMATION)(DWORD, BYTE, XINPUT_BATTERY_INFORMATION*);

protected:
	bool is_init{false};
	HMODULE library{nullptr};
	PFN_XINPUTSETSTATE xinputSetState{nullptr};
	PFN_XINPUTGETBATTERYINFORMATION xinputGetBatteryInformation{nullptr};

public:
	// This union should hold state structures of all implemented children of xinput_pad_processor_base
	union XInputStateUnion
	{
		XINPUT_STATE xiState;
		SCP_EXTN scpState;
	};

public:
	virtual ~xinput_pad_processor_base()
	{
		if (library)
		{
			FreeLibrary(library);
		}
	}

	virtual pad_handler GetHandlerType() const = 0;
	virtual const char* GetNameString() const = 0;
	virtual const std::unordered_map<u32, std::string>& GetButtonList() const = 0;
	virtual bool Init() = 0;
	virtual DWORD GetState(DWORD userIndex, XInputStateUnion* state) = 0;
	virtual std::array<u16, xinput_pad_handler::XInputKeyCodes::KeyCodeCount> GetButtonValues(const XInputStateUnion& state) = 0;

	DWORD SetState(DWORD userIndex, u16 leftMotor, u16 rightMotor)
	{
		XINPUT_VIBRATION state { leftMotor, rightMotor };
		return xinputSetState(userIndex, &state);
	}

	DWORD GetBatteryInformation(DWORD userIndex, BYTE devType, XINPUT_BATTERY_INFORMATION* batteryInformation)
	{
		return xinputGetBatteryInformation(userIndex, devType, batteryInformation);
	}
};

class xinput_pad_processor_xi final : public xinput_pad_processor_base
{
	static constexpr DWORD GUIDE_BUTTON          = 0x0400;
	static constexpr LPCWSTR LIBRARY_FILENAMES[] = {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"};

	typedef DWORD(WINAPI* PFN_XINPUTGETSTATE)(DWORD, XINPUT_STATE*);

private:
	PFN_XINPUTGETSTATE xinputGetState{nullptr};

public:
	virtual pad_handler GetHandlerType() const override
	{
		return pad_handler::xinput;
	}

	virtual const char* GetNameString() const override
	{
		return "XInput Pad #";
	}

	virtual const std::unordered_map<u32, std::string>& GetButtonList() const override
	{
		static const std::unordered_map<u32, std::string> button_list =
		{
			{ xinput_pad_handler::XInputKeyCodes::A,      "A" },
			{ xinput_pad_handler::XInputKeyCodes::B,      "B" },
			{ xinput_pad_handler::XInputKeyCodes::X,      "X" },
			{ xinput_pad_handler::XInputKeyCodes::Y,      "Y" },
			{ xinput_pad_handler::XInputKeyCodes::Left,   "Left" },
			{ xinput_pad_handler::XInputKeyCodes::Right,  "Right" },
			{ xinput_pad_handler::XInputKeyCodes::Up,     "Up" },
			{ xinput_pad_handler::XInputKeyCodes::Down,   "Down" },
			{ xinput_pad_handler::XInputKeyCodes::LB,     "LB" },
			{ xinput_pad_handler::XInputKeyCodes::RB,     "RB" },
			{ xinput_pad_handler::XInputKeyCodes::Back,   "Back" },
			{ xinput_pad_handler::XInputKeyCodes::Start,  "Start" },
			{ xinput_pad_handler::XInputKeyCodes::LS,     "LS" },
			{ xinput_pad_handler::XInputKeyCodes::RS,     "RS" },
			{ xinput_pad_handler::XInputKeyCodes::Guide,  "Guide" },
			{ xinput_pad_handler::XInputKeyCodes::LT,     "LT" },
			{ xinput_pad_handler::XInputKeyCodes::RT,     "RT" },
			{ xinput_pad_handler::XInputKeyCodes::LSXNeg, "LS X-" },
			{ xinput_pad_handler::XInputKeyCodes::LSXPos, "LS X+" },
			{ xinput_pad_handler::XInputKeyCodes::LSYPos, "LS Y+" },
			{ xinput_pad_handler::XInputKeyCodes::LSYNeg, "LS Y-" },
			{ xinput_pad_handler::XInputKeyCodes::RSXNeg, "RS X-" },
			{ xinput_pad_handler::XInputKeyCodes::RSXPos, "RS X+" },
			{ xinput_pad_handler::XInputKeyCodes::RSYPos, "RS Y+" },
			{ xinput_pad_handler::XInputKeyCodes::RSYNeg, "RS Y-" }
		};
		return button_list;
	}

	bool Init() override
	{
		if (is_init)
			return true;

		for (auto it : LIBRARY_FILENAMES)
		{
			library = LoadLibrary(it);
			if (library)
			{
				xinputGetState = reinterpret_cast<PFN_XINPUTGETSTATE>(GetProcAddress(library, reinterpret_cast<LPCSTR>(100)));
				if (!xinputGetState)
					xinputGetState = reinterpret_cast<PFN_XINPUTGETSTATE>(GetProcAddress(library, "XInputGetState"));

				xinputSetState              = reinterpret_cast<PFN_XINPUTSETSTATE>(GetProcAddress(library, "XInputSetState"));
				xinputGetBatteryInformation = reinterpret_cast<PFN_XINPUTGETBATTERYINFORMATION>(GetProcAddress(library, "XInputGetBatteryInformation"));

				if (xinputGetState && xinputSetState && xinputGetBatteryInformation)
				{
					is_init = true;
					break;
				}

				FreeLibrary(library);
				library                     = nullptr;
				xinputGetState              = nullptr;
				xinputGetBatteryInformation = nullptr;
			}
		}

		return is_init;
	}

	DWORD GetState(DWORD userIndex, XInputStateUnion* state) override
	{
		return xinputGetState(userIndex, &state->xiState);
	}

	std::array<u16, xinput_pad_handler::XInputKeyCodes::KeyCodeCount> GetButtonValues(const XInputStateUnion& s) override
	{
		const XINPUT_STATE& state = s.xiState;
		std::array<u16, xinput_pad_handler::XInputKeyCodes::KeyCodeCount> values;

		// Triggers
		values[xinput_pad_handler::XInputKeyCodes::LT] = state.Gamepad.bLeftTrigger;
		values[xinput_pad_handler::XInputKeyCodes::RT] = state.Gamepad.bRightTrigger;

		// Sticks
		int lx = state.Gamepad.sThumbLX;
		int ly = state.Gamepad.sThumbLY;
		int rx = state.Gamepad.sThumbRX;
		int ry = state.Gamepad.sThumbRY;

		// Left Stick X Axis
		values[xinput_pad_handler::XInputKeyCodes::LSXNeg] = lx < 0 ? -lx : 0;
		values[xinput_pad_handler::XInputKeyCodes::LSXPos] = lx > 0 ? lx : 0;

		// Left Stick Y Axis
		values[xinput_pad_handler::XInputKeyCodes::LSYNeg] = ly < 0 ? -ly : 0;
		values[xinput_pad_handler::XInputKeyCodes::LSYPos] = ly > 0 ? ly : 0;

		// Right Stick X Axis
		values[xinput_pad_handler::XInputKeyCodes::RSXNeg] = rx < 0 ? -rx : 0;
		values[xinput_pad_handler::XInputKeyCodes::RSXPos] = rx > 0 ? rx : 0;

		// Right Stick Y Axis
		values[xinput_pad_handler::XInputKeyCodes::RSYNeg] = ry < 0 ? -ry : 0;
		values[xinput_pad_handler::XInputKeyCodes::RSYPos] = ry > 0 ? ry : 0;

		// Buttons
		const WORD buttons = state.Gamepad.wButtons;

		// A, B, X, Y
		values[xinput_pad_handler::XInputKeyCodes::A] = buttons & XINPUT_GAMEPAD_A ? 255 : 0;
		values[xinput_pad_handler::XInputKeyCodes::B] = buttons & XINPUT_GAMEPAD_B ? 255 : 0;
		values[xinput_pad_handler::XInputKeyCodes::X] = buttons & XINPUT_GAMEPAD_X ? 255 : 0;
		values[xinput_pad_handler::XInputKeyCodes::Y] = buttons & XINPUT_GAMEPAD_Y ? 255 : 0;

		// D-Pad
		values[xinput_pad_handler::XInputKeyCodes::Left]  = buttons & XINPUT_GAMEPAD_DPAD_LEFT ? 255 : 0;
		values[xinput_pad_handler::XInputKeyCodes::Right] = buttons & XINPUT_GAMEPAD_DPAD_RIGHT ? 255 : 0;
		values[xinput_pad_handler::XInputKeyCodes::Up]    = buttons & XINPUT_GAMEPAD_DPAD_UP ? 255 : 0;
		values[xinput_pad_handler::XInputKeyCodes::Down]  = buttons & XINPUT_GAMEPAD_DPAD_DOWN ? 255 : 0;

		// LB, RB, LS, RS
		values[xinput_pad_handler::XInputKeyCodes::LB] = buttons & XINPUT_GAMEPAD_LEFT_SHOULDER ? 255 : 0;
		values[xinput_pad_handler::XInputKeyCodes::RB] = buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER ? 255 : 0;
		values[xinput_pad_handler::XInputKeyCodes::LS] = buttons & XINPUT_GAMEPAD_LEFT_THUMB ? 255 : 0;
		values[xinput_pad_handler::XInputKeyCodes::RS] = buttons & XINPUT_GAMEPAD_RIGHT_THUMB ? 255 : 0;

		// Start, Back, Guide
		values[xinput_pad_handler::XInputKeyCodes::Start] = buttons & XINPUT_GAMEPAD_START ? 255 : 0;
		values[xinput_pad_handler::XInputKeyCodes::Back]  = buttons & XINPUT_GAMEPAD_BACK ? 255 : 0;
		values[xinput_pad_handler::XInputKeyCodes::Guide] = buttons & GUIDE_BUTTON ? 255 : 0;

		return values;
	}
};

class xinput_pad_processor_scp final : public xinput_pad_processor_base
{
	typedef DWORD(WINAPI* PFN_XINPUTGETEXTENDED)(DWORD, SCP_EXTN*);

private:
	PFN_XINPUTGETEXTENDED xinputGetExtended{nullptr};

public:
	virtual pad_handler GetHandlerType() const override
	{
		return pad_handler::xinput_scp;
	}

	virtual const char* GetNameString() const override
	{
		return "DS3 Pad #";
	}

	virtual const std::unordered_map<u32, std::string>& GetButtonList() const override
	{
		static const std::unordered_map<u32, std::string> button_list =
		{
			{ xinput_pad_handler::XInputKeyCodes::A,      "Triangle" },
			{ xinput_pad_handler::XInputKeyCodes::B,      "Circle" },
			{ xinput_pad_handler::XInputKeyCodes::X,      "Cross" },
			{ xinput_pad_handler::XInputKeyCodes::Y,      "Square" },
			{ xinput_pad_handler::XInputKeyCodes::Left,   "Left" },
			{ xinput_pad_handler::XInputKeyCodes::Right,  "Right" },
			{ xinput_pad_handler::XInputKeyCodes::Up,     "Up" },
			{ xinput_pad_handler::XInputKeyCodes::Down,   "Down" },
			{ xinput_pad_handler::XInputKeyCodes::LB,     "R1" },
			{ xinput_pad_handler::XInputKeyCodes::RB,     "R2" },
			{ xinput_pad_handler::XInputKeyCodes::Back,   "R3" },
			{ xinput_pad_handler::XInputKeyCodes::Start,  "Start" },
			{ xinput_pad_handler::XInputKeyCodes::LS,     "Select" },
			{ xinput_pad_handler::XInputKeyCodes::RS,     "PS Button" },
			{ xinput_pad_handler::XInputKeyCodes::Guide,  "L1" },
			{ xinput_pad_handler::XInputKeyCodes::LT,     "L2" },
			{ xinput_pad_handler::XInputKeyCodes::RT,     "L3" },
			{ xinput_pad_handler::XInputKeyCodes::LSXNeg, "LS X-" },
			{ xinput_pad_handler::XInputKeyCodes::LSXPos, "LS X+" },
			{ xinput_pad_handler::XInputKeyCodes::LSYPos, "LS Y+" },
			{ xinput_pad_handler::XInputKeyCodes::LSYNeg, "LS Y-" },
			{ xinput_pad_handler::XInputKeyCodes::RSXNeg, "RS X-" },
			{ xinput_pad_handler::XInputKeyCodes::RSXPos, "RS X+" },
			{ xinput_pad_handler::XInputKeyCodes::RSYPos, "RS Y+" },
			{ xinput_pad_handler::XInputKeyCodes::RSYNeg, "RS Y-" }
		};
		return button_list;
	}

	bool Init() override
	{
		if (is_init)
			return true;

		library = LoadLibrary(L"XInput1_3.dll");
		if (library)
		{
			xinputGetExtended           = reinterpret_cast<PFN_XINPUTGETEXTENDED>(GetProcAddress(library, "XInputGetExtended"));
			xinputSetState              = reinterpret_cast<PFN_XINPUTSETSTATE>(GetProcAddress(library, "XInputSetState"));
			xinputGetBatteryInformation = reinterpret_cast<PFN_XINPUTGETBATTERYINFORMATION>(GetProcAddress(library, "XInputGetBatteryInformation"));

			if (xinputGetExtended && xinputSetState && xinputGetBatteryInformation)
			{
				is_init = true;
				return true;
			}

			FreeLibrary(library);
			library                     = nullptr;
			xinputGetExtended           = nullptr;
			xinputGetBatteryInformation = nullptr;
		}

		return false;
	}

	DWORD GetState(DWORD userIndex, XInputStateUnion* state) override
	{
		return xinputGetExtended(userIndex, &state->scpState);
	}

	std::array<u16, xinput_pad_handler::XInputKeyCodes::KeyCodeCount> GetButtonValues(const XInputStateUnion& s) override
	{
		const SCP_EXTN& state = s.scpState;
		std::array<u16, xinput_pad_handler::XInputKeyCodes::KeyCodeCount> values;

		// Triggers
		values[xinput_pad_handler::XInputKeyCodes::LT] = static_cast<u16>(state.SCP_L2 * 255.0f);
		values[xinput_pad_handler::XInputKeyCodes::RT] = static_cast<u16>(state.SCP_R2 * 255.0f);

		// Sticks
		float lx = state.SCP_LX;
		float ly = state.SCP_LY;
		float rx = state.SCP_RX;
		float ry = state.SCP_RY;

		// Left Stick X Axis
		values[xinput_pad_handler::XInputKeyCodes::LSXNeg] = lx < 0.0f ? static_cast<u16>(lx * -32768.0f) : 0;
		values[xinput_pad_handler::XInputKeyCodes::LSXPos] = lx > 0.0f ? static_cast<u16>(lx * 32767.0f) : 0;

		// Left Stick Y Axis
		values[xinput_pad_handler::XInputKeyCodes::LSYNeg] = ly < 0.0f ? static_cast<u16>(ly * -32768.0f) : 0;
		values[xinput_pad_handler::XInputKeyCodes::LSYPos] = ly > 0.0f ? static_cast<u16>(ly * 32767.0f) : 0;

		// Right Stick X Axis
		values[xinput_pad_handler::XInputKeyCodes::RSXNeg] = rx < 0.0f ? static_cast<u16>(rx * -32768.0f) : 0;
		values[xinput_pad_handler::XInputKeyCodes::RSXPos] = rx > 0.0f ? static_cast<u16>(rx * 32767.0f) : 0;

		// Right Stick Y Axis
		values[xinput_pad_handler::XInputKeyCodes::RSYNeg] = ry < 0.0f ? static_cast<u16>(ry * -32768.0f) : 0;
		values[xinput_pad_handler::XInputKeyCodes::RSYPos] = ry > 0.0f ? static_cast<u16>(ry * 32767.0f) : 0;

		// A, B, X, Y
		values[xinput_pad_handler::XInputKeyCodes::A] = static_cast<u16>(state.SCP_X * 255.0f);
		values[xinput_pad_handler::XInputKeyCodes::B] = static_cast<u16>(state.SCP_C * 255.0f);
		values[xinput_pad_handler::XInputKeyCodes::X] = static_cast<u16>(state.SCP_S * 255.0f);
		values[xinput_pad_handler::XInputKeyCodes::Y] = static_cast<u16>(state.SCP_T * 255.0f);

		// D-Pad
		values[xinput_pad_handler::XInputKeyCodes::Left]  = static_cast<u16>(state.SCP_LEFT * 255.0f);
		values[xinput_pad_handler::XInputKeyCodes::Right] = static_cast<u16>(state.SCP_RIGHT * 255.0f);
		values[xinput_pad_handler::XInputKeyCodes::Up]    = static_cast<u16>(state.SCP_UP * 255.0f);
		values[xinput_pad_handler::XInputKeyCodes::Down]  = static_cast<u16>(state.SCP_DOWN * 255.0f);

		// LB, RB, LS, RS
		values[xinput_pad_handler::XInputKeyCodes::LB] = static_cast<u16>(state.SCP_L1 * 255.0f);
		values[xinput_pad_handler::XInputKeyCodes::RB] = static_cast<u16>(state.SCP_R1 * 255.0f);
		values[xinput_pad_handler::XInputKeyCodes::LS] = static_cast<u16>(state.SCP_L3 * 255.0f);
		values[xinput_pad_handler::XInputKeyCodes::RS] = static_cast<u16>(state.SCP_R3 * 255.0f);

		// Start, Back, Guide
		values[xinput_pad_handler::XInputKeyCodes::Start] = static_cast<u16>(state.SCP_START * 255.0f);
		values[xinput_pad_handler::XInputKeyCodes::Back]  = static_cast<u16>(state.SCP_SELECT * 255.0f);
		values[xinput_pad_handler::XInputKeyCodes::Guide] = static_cast<u16>(state.SCP_PS * 255.0f);

		return values;
	}
};

xinput_pad_handler::xinput_pad_handler(pad_handler handler)
    : PadHandlerBase(handler)
    , m_processor(makeProcessorFromType(handler))
    , button_list(m_processor->GetButtonList())
{
	init_configs();

	// Define border values
	thumb_min = -32768;
	thumb_max = 32767;
	trigger_min = 0;
	trigger_max = 255;
	vibration_min = 0;
	vibration_max = 65535;

	// set capabilities
	b_has_config = true;
	b_has_rumble = true;
	b_has_deadzones = true;

	m_name_string = m_processor->GetNameString();
	m_max_devices = XUSER_MAX_COUNT;

	m_trigger_threshold = trigger_max / 2;
	m_thumb_threshold = thumb_max / 2;
}

xinput_pad_handler::~xinput_pad_handler()
{
}

void xinput_pad_handler::init_config(pad_config* cfg, const std::string& name)
{
	// Set this profile's save location
	cfg->cfg_name = name;

	// Set default button mapping
	cfg->ls_left.def  = button_list.at(XInputKeyCodes::LSXNeg);
	cfg->ls_down.def  = button_list.at(XInputKeyCodes::LSYNeg);
	cfg->ls_right.def = button_list.at(XInputKeyCodes::LSXPos);
	cfg->ls_up.def    = button_list.at(XInputKeyCodes::LSYPos);
	cfg->rs_left.def  = button_list.at(XInputKeyCodes::RSXNeg);
	cfg->rs_down.def  = button_list.at(XInputKeyCodes::RSYNeg);
	cfg->rs_right.def = button_list.at(XInputKeyCodes::RSXPos);
	cfg->rs_up.def    = button_list.at(XInputKeyCodes::RSYPos);
	cfg->start.def    = button_list.at(XInputKeyCodes::Start);
	cfg->select.def   = button_list.at(XInputKeyCodes::Back);
	cfg->ps.def       = button_list.at(XInputKeyCodes::Guide);
	cfg->square.def   = button_list.at(XInputKeyCodes::X);
	cfg->cross.def    = button_list.at(XInputKeyCodes::A);
	cfg->circle.def   = button_list.at(XInputKeyCodes::B);
	cfg->triangle.def = button_list.at(XInputKeyCodes::Y);
	cfg->left.def     = button_list.at(XInputKeyCodes::Left);
	cfg->down.def     = button_list.at(XInputKeyCodes::Down);
	cfg->right.def    = button_list.at(XInputKeyCodes::Right);
	cfg->up.def       = button_list.at(XInputKeyCodes::Up);
	cfg->r1.def       = button_list.at(XInputKeyCodes::RB);
	cfg->r2.def       = button_list.at(XInputKeyCodes::RT);
	cfg->r3.def       = button_list.at(XInputKeyCodes::RS);
	cfg->l1.def       = button_list.at(XInputKeyCodes::LB);
	cfg->l2.def       = button_list.at(XInputKeyCodes::LT);
	cfg->l3.def       = button_list.at(XInputKeyCodes::LS);

	// Set default misc variables
	cfg->lstickdeadzone.def    = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;  // between 0 and 32767
	cfg->rstickdeadzone.def    = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE; // between 0 and 32767
	cfg->ltriggerthreshold.def = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;    // between 0 and 255
	cfg->rtriggerthreshold.def = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;    // between 0 and 255
	cfg->padsquircling.def     = 8000;

	// apply defaults
	cfg->from_default();
}

void xinput_pad_handler::GetNextButtonPress(const std::string& padId, const std::function<void(u16, std::string, std::string, int[])>& callback, const std::function<void(std::string)>& fail_callback, bool get_blacklist, const std::vector<std::string>& /*buttons*/)
{
	if (get_blacklist)
		blacklist.clear();

	int device_number = GetDeviceNumber(padId);
	if (device_number < 0)
		return fail_callback(padId);

	xinput_pad_processor_base::XInputStateUnion state;

	// Simply get the state of the controller from XInput.
	DWORD dwResult = m_processor->GetState(static_cast<u32>(device_number), &state);
	if (dwResult != ERROR_SUCCESS)
		return fail_callback(padId);

	// Check for each button in our list if its corresponding (maybe remapped) button or axis was pressed.
	// Return the new value if the button was pressed (aka. its value was bigger than 0 or the defined threshold)
	// Use a pair to get all the legally pressed buttons and use the one with highest value (prioritize first)
	std::pair<u16, std::string> pressed_button = { 0, "" };
	auto data = m_processor->GetButtonValues(state);
	for (const auto& button : button_list)
	{
		u32 keycode = button.first;
		u16 value = data[keycode];

		if (!get_blacklist && std::find(blacklist.begin(), blacklist.end(), keycode) != blacklist.end())
			continue;

		if (((keycode < XInputKeyCodes::LT) && (value > 0))
		 || ((keycode == XInputKeyCodes::LT) && (value > m_trigger_threshold))
		 || ((keycode == XInputKeyCodes::RT) && (value > m_trigger_threshold))
		 || ((keycode >= XInputKeyCodes::LSXNeg && keycode <= XInputKeyCodes::LSYPos) && (value > m_thumb_threshold))
		 || ((keycode >= XInputKeyCodes::RSXNeg && keycode <= XInputKeyCodes::RSYPos) && (value > m_thumb_threshold)))
		{
			if (get_blacklist)
			{
				blacklist.emplace_back(keycode);
				LOG_ERROR(HLE, "XInput Calibration: Added key [ %d = %s ] to blacklist. Value = %d", keycode, button.second, value);
			}
			else if (value > pressed_button.first)
				pressed_button = { value, button.second };
		}
	}

	if (get_blacklist)
	{
		if (blacklist.empty())
			LOG_SUCCESS(HLE, "XInput Calibration: Blacklist is clear. No input spam detected");
		return;
	}

	int preview_values[6] = { data[LT], data[RT], data[LSXPos] - data[LSXNeg], data[LSYPos] - data[LSYNeg], data[RSXPos] - data[RSXNeg], data[RSYPos] - data[RSYNeg] };

	if (pressed_button.first > 0)
		return callback(pressed_button.first, pressed_button.second, padId, preview_values);
	else
		return callback(0, "", padId, preview_values);
}

void xinput_pad_handler::SetPadData(const std::string& padId, u32 largeMotor, u32 smallMotor, s32/* r*/, s32/* g*/, s32/* b*/)
{
	int device_number = GetDeviceNumber(padId);
	if (device_number < 0)
		return;

	// The left motor is the low-frequency rumble motor. The right motor is the high-frequency rumble motor.
	// The two motors are not the same, and they create different vibration effects.
	m_processor->SetState(static_cast<u32>(device_number), static_cast<u16>(largeMotor), static_cast<u16>(smallMotor));
}

void xinput_pad_handler::TranslateButtonPress(u64 keyCode, bool& pressed, u16& val, bool ignore_threshold)
{
	// Update the pad button values based on their type and thresholds.
	// With this you can use axis or triggers as buttons or vice versa
	auto p_profile = m_dev->config;
	switch (keyCode)
	{
	case XInputKeyCodes::LT:
		pressed = val > p_profile->ltriggerthreshold;
		val = pressed ? NormalizeTriggerInput(val, p_profile->ltriggerthreshold) : 0;
		break;
	case XInputKeyCodes::RT:
		pressed = val > p_profile->rtriggerthreshold;
		val = pressed ? NormalizeTriggerInput(val, p_profile->rtriggerthreshold) : 0;
		break;
	case XInputKeyCodes::LSXNeg:
	case XInputKeyCodes::LSXPos:
	case XInputKeyCodes::LSYPos:
	case XInputKeyCodes::LSYNeg:
		pressed = val > (ignore_threshold ? 0 : p_profile->lstickdeadzone);
		val = pressed ? NormalizeStickInput(val, p_profile->lstickdeadzone, p_profile->lstickmultiplier, ignore_threshold) : 0;
		break;
	case XInputKeyCodes::RSXNeg:
	case XInputKeyCodes::RSXPos:
	case XInputKeyCodes::RSYPos:
	case XInputKeyCodes::RSYNeg:
		pressed = val > (ignore_threshold ? 0 : p_profile->rstickdeadzone);
		val = pressed ? NormalizeStickInput(val, p_profile->rstickdeadzone, p_profile->rstickmultiplier, ignore_threshold) : 0;
		break;
	default: // normal button (should in theory also support sensitive buttons)
		pressed = val > 0;
		val = pressed ? val : 0;
		break;
	}
}

int xinput_pad_handler::GetDeviceNumber(const std::string& padId)
{
	if (!Init())
		return -1;

	size_t pos = padId.find(m_name_string);
	if (pos == std::string::npos)
		return -1;

	int device_number = std::stoul(padId.substr(pos + m_name_string.length())) - 1; // Controllers 1-n in GUI
	if (device_number >= XUSER_MAX_COUNT)
		return -1;

	return device_number;
}

bool xinput_pad_handler::Init()
{
	return m_processor->Init();
}

void xinput_pad_handler::ThreadProc()
{
	for (int i = 0; i < static_cast<int>(bindings.size()); ++i)
	{
		auto& bind = bindings[i];
		m_dev = bind.first;
		auto padnum = m_dev->deviceNumber;
		auto profile = m_dev->config;
		auto pad = bind.second;

		xinput_pad_processor_base::XInputStateUnion state;
		DWORD result = m_processor->GetState(padnum, &state);

		switch (result)
		{
		case ERROR_DEVICE_NOT_CONNECTED:
		{
			if (last_connection_status[i] == true)
			{
				LOG_ERROR(HLE, "XInput device %d disconnected", padnum);
				pad->m_port_status &= ~CELL_PAD_STATUS_CONNECTED;
				pad->m_port_status |= CELL_PAD_STATUS_ASSIGN_CHANGES;
				last_connection_status[i] = false;
				connected--;
			}
			continue;
		}
		case ERROR_SUCCESS:
		{
			if (last_connection_status[i] == false)
			{
				LOG_SUCCESS(HLE, "XInput device %d reconnected", padnum);
				pad->m_port_status |= CELL_PAD_STATUS_CONNECTED;
				pad->m_port_status |= CELL_PAD_STATUS_ASSIGN_CHANGES;
				last_connection_status[i] = true;
				connected++;
			}

			std::array<u16, XInputKeyCodes::KeyCodeCount> button_values = m_processor->GetButtonValues(state);

			// Translate any corresponding keycodes to our normal DS3 buttons and triggers
			for (auto& btn : pad->m_buttons)
			{
				btn.m_value = button_values[btn.m_keyCode];
				TranslateButtonPress(btn.m_keyCode, btn.m_pressed, btn.m_value);
			}

			for (const auto& btn : pad->m_buttons)
			{
				if (btn.m_pressed)
				{
					SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
					break;
				}
			}

			// used to get the absolute value of an axis
			s32 stick_val[4]{0};

			// Translate any corresponding keycodes to our two sticks. (ignoring thresholds for now)
			for (int i = 0; i < static_cast<int>(pad->m_sticks.size()); i++)
			{
				bool pressed;

				// m_keyCodeMin is the mapped key for left or down
				u32 key_min = pad->m_sticks[i].m_keyCodeMin;
				u16 val_min = button_values[key_min];
				TranslateButtonPress(key_min, pressed, val_min, true);

				// m_keyCodeMax is the mapped key for right or up
				u32 key_max = pad->m_sticks[i].m_keyCodeMax;
				u16 val_max = button_values[key_max];
				TranslateButtonPress(key_max, pressed, val_max, true);

				// cancel out opposing values and get the resulting difference
				stick_val[i] = val_max - val_min;
			}

			u16 lx, ly, rx, ry;

			// Normalize our two stick's axis based on the thresholds
			std::tie(lx, ly) = NormalizeStickDeadzone(stick_val[0], stick_val[1], profile->lstickdeadzone);
			std::tie(rx, ry) = NormalizeStickDeadzone(stick_val[2], stick_val[3], profile->rstickdeadzone);

			if (profile->padsquircling != 0)
			{
				std::tie(lx, ly) = ConvertToSquirclePoint(lx, ly, profile->padsquircling);
				std::tie(rx, ry) = ConvertToSquirclePoint(rx, ry, profile->padsquircling);
			}

			pad->m_sticks[0].m_value = lx;
			pad->m_sticks[1].m_value = 255 - ly;
			pad->m_sticks[2].m_value = rx;
			pad->m_sticks[3].m_value = 255 - ry;

			// Receive Battery Info. If device is not on cable, get battery level, else assume full
			XINPUT_BATTERY_INFORMATION battery_info;
			if (m_processor->GetBatteryInformation(padnum, BATTERY_DEVTYPE_GAMEPAD, &battery_info) == ERROR_SUCCESS)
			{
				pad->m_cable_state = battery_info.BatteryType == BATTERY_TYPE_WIRED ? 1 : 0;
				pad->m_battery_level = pad->m_cable_state ? BATTERY_LEVEL_FULL : battery_info.BatteryLevel;
			}

			// The left motor is the low-frequency rumble motor. The right motor is the high-frequency rumble motor.
			// The two motors are not the same, and they create different vibration effects. Values range between 0 to 65535.
			size_t idx_l = profile->switch_vibration_motors ? 1 : 0;
			size_t idx_s = profile->switch_vibration_motors ? 0 : 1;

			u16 speed_large = profile->enable_vibration_motor_large ? pad->m_vibrateMotors[idx_l].m_value : static_cast<u16>(vibration_min);
			u16 speed_small = profile->enable_vibration_motor_small ? pad->m_vibrateMotors[idx_s].m_value : static_cast<u16>(vibration_min);

			m_dev->newVibrateData |= m_dev->largeVibrate != speed_large || m_dev->smallVibrate != speed_small;

			m_dev->largeVibrate = speed_large;
			m_dev->smallVibrate = speed_small;

			// XBox One Controller can't handle faster vibration updates than ~10ms. Elite is even worse. So I'll use 20ms to be on the safe side. No lag was noticable.
			if (m_dev->newVibrateData && (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_dev->last_vibration) > 20ms))
			{
				if (m_processor->SetState(padnum, speed_large * 257, speed_small * 257) == ERROR_SUCCESS)
				{
					m_dev->newVibrateData = false;
					m_dev->last_vibration = std::chrono::high_resolution_clock::now();
				}
			}
			break;
		}
		default:
			break;
		}
	}
}

std::vector<std::string> xinput_pad_handler::ListDevices()
{
	std::vector<std::string> xinput_pads_list;

	if (!Init())
		return xinput_pads_list;

	for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
	{
		xinput_pad_processor_base::XInputStateUnion state;
		if (m_processor->GetState(i, &state) == ERROR_SUCCESS)
			xinput_pads_list.push_back(m_name_string + std::to_string(i + 1)); // Controllers 1-n in GUI
	}
	return xinput_pads_list;
}

bool xinput_pad_handler::bindPadToDevice(std::shared_ptr<Pad> pad, const std::string& device)
{
	//Convert device string to u32 representing xinput device number
	int device_number = GetDeviceNumber(device);
	if (device_number < 0)
		return false;

	std::shared_ptr<XInputDevice> x_device = std::make_shared<XInputDevice>();
	x_device->deviceNumber = static_cast<u32>(device_number);

	int index = static_cast<int>(bindings.size());
	m_pad_configs[index].load();
	x_device->config = &m_pad_configs[index];
	pad_config* p_profile = x_device->config;
	if (p_profile == nullptr)
		return false;

	pad->Init
	(
		CELL_PAD_STATUS_DISCONNECTED,
		CELL_PAD_CAPABILITY_PS3_CONFORMITY | CELL_PAD_CAPABILITY_PRESS_MODE | CELL_PAD_CAPABILITY_HP_ANALOG_STICK | CELL_PAD_CAPABILITY_ACTUATOR | CELL_PAD_CAPABILITY_SENSOR_MODE,
		CELL_PAD_DEV_TYPE_STANDARD,
		p_profile->device_class_type
	);

	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, p_profile->up),       CELL_PAD_CTRL_UP);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, p_profile->down),     CELL_PAD_CTRL_DOWN);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, p_profile->left),     CELL_PAD_CTRL_LEFT);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, p_profile->right),    CELL_PAD_CTRL_RIGHT);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, p_profile->start),    CELL_PAD_CTRL_START);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, p_profile->select),   CELL_PAD_CTRL_SELECT);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, p_profile->l3),       CELL_PAD_CTRL_L3);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, p_profile->r3),       CELL_PAD_CTRL_R3);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, p_profile->l1),       CELL_PAD_CTRL_L1);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, p_profile->r1),       CELL_PAD_CTRL_R1);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, p_profile->ps),       0x100/*CELL_PAD_CTRL_PS*/);// TODO: PS button support
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, p_profile->cross),    CELL_PAD_CTRL_CROSS);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, p_profile->circle),   CELL_PAD_CTRL_CIRCLE);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, p_profile->square),   CELL_PAD_CTRL_SQUARE);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, p_profile->triangle), CELL_PAD_CTRL_TRIANGLE);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, p_profile->l2),       CELL_PAD_CTRL_L2);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, p_profile->r2),       CELL_PAD_CTRL_R2);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, 0, 0x0); // Reserved

	pad->m_sticks.emplace_back(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X,  FindKeyCode(button_list, p_profile->ls_left), FindKeyCode(button_list, p_profile->ls_right));
	pad->m_sticks.emplace_back(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y,  FindKeyCode(button_list, p_profile->ls_down), FindKeyCode(button_list, p_profile->ls_up));
	pad->m_sticks.emplace_back(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X, FindKeyCode(button_list, p_profile->rs_left), FindKeyCode(button_list, p_profile->rs_right));
	pad->m_sticks.emplace_back(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y, FindKeyCode(button_list, p_profile->rs_down), FindKeyCode(button_list, p_profile->rs_up));

	pad->m_sensors.emplace_back(CELL_PAD_BTN_OFFSET_SENSOR_X, 512);
	pad->m_sensors.emplace_back(CELL_PAD_BTN_OFFSET_SENSOR_Y, 399);
	pad->m_sensors.emplace_back(CELL_PAD_BTN_OFFSET_SENSOR_Z, 512);
	pad->m_sensors.emplace_back(CELL_PAD_BTN_OFFSET_SENSOR_G, 512);

	pad->m_vibrateMotors.emplace_back(true, 0);
	pad->m_vibrateMotors.emplace_back(false, 0);

	bindings.emplace_back(x_device, pad);

	return true;
}

std::unique_ptr<xinput_pad_processor_base> xinput_pad_handler::makeProcessorFromType(pad_handler type)
{
	std::unique_ptr<xinput_pad_processor_base> result;

	switch (type)
	{
	case pad_handler::xinput:
		result = std::make_unique<xinput_pad_processor_xi>();
		break;
	case pad_handler::xinput_scp:
		result = std::make_unique<xinput_pad_processor_scp>();
		break;
	}

	return result;
}

#endif
