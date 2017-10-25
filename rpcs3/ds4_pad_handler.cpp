#include "stdafx.h"
#include "Emu/System.h"
#include "ds4_pad_handler.h"
#include "rpcs3qt/pad_settings_dialog.h"

#include <thread>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{
	const auto THREAD_SLEEP = 1ms; //ds4 has new data every ~4ms,
	const auto THREAD_SLEEP_INACTIVE = 100ms;

	const u32 DS4_ACC_RES_PER_G = 8192;
	const u32 DS4_GYRO_RES_PER_DEG_S = 16; // technically this could be 1024, but keeping it at 16 keeps us within 16 bits of precision
	const u32 DS4_FEATURE_REPORT_0x02_SIZE = 37;
	const u32 DS4_FEATURE_REPORT_0x05_SIZE = 41;
	const u32 DS4_FEATURE_REPORT_0x81_SIZE = 7;
	const u32 DS4_INPUT_REPORT_0x11_SIZE = 78;
	const u32 DS4_OUTPUT_REPORT_0x05_SIZE = 32;
	const u32 DS4_OUTPUT_REPORT_0x11_SIZE = 78;
	const u32 DS4_INPUT_REPORT_GYRO_X_OFFSET = 13;

	inline u16 Clamp0To255(f32 input)
	{
		if (input > 255.f)
			return 255;
		else if (input < 0.f)
			return 0;
		else return static_cast<u16>(input);
	}

	inline u16 Clamp0To1023(f32 input)
	{
		if (input > 1023.f)
			return 1023;
		else if (input < 0.f)
			return 0;
		else return static_cast<u16>(input);
	}

	// This tries to convert axis to give us the max even in the corners,
	// this actually might work 'too' well, we end up actually getting diagonals of actual max/min, we need the corners still a bit rounded to match ds3
	// im leaving it here for now, and future reference as it probably can be used later
	//taken from http://theinstructionlimit.com/squaring-the-thumbsticks
	/*std::tuple<u16, u16> ConvertToSquarePoint(u16 inX, u16 inY, u32 innerRoundness = 0) {
		// convert inX and Y to a (-1, 1) vector;
		const f32 x = (inX - 127) / 127.f;
		const f32 y = ((inY - 127) / 127.f) * -1;

		f32 outX, outY;
		const f32 piOver4 = M_PI / 4;
		const f32 angle = std::atan2(y, x) + M_PI;
		// x+ wall
		if (angle <= piOver4 || angle > 7 * piOver4) {
			outX = x * (f32)(1 / std::cos(angle));
			outY = y * (f32)(1 / std::cos(angle));
		}
		// y+ wall
		else if (angle > piOver4 && angle <= 3 * piOver4) {
			outX = x * (f32)(1 / std::sin(angle));
			outY = y * (f32)(1 / std::sin(angle));
		}
		// x- wall
		else if (angle > 3 * piOver4 && angle <= 5 * piOver4) {
			outX = x * (f32)(-1 / std::cos(angle));
			outY = y * (f32)(-1 / std::cos(angle));
		}
		// y- wall
		else if (angle > 5 * piOver4 && angle <= 7 * piOver4) {
			outX = x * (f32)(-1 / std::sin(angle));
			outY = y * (f32)(-1 / std::sin(angle));
		}
		else fmt::throw_exception("invalid angle in convertToSquarePoint");

		if (innerRoundness == 0)
			return std::tuple<u16, u16>(Clamp0To255((outX + 1) * 127.f), Clamp0To255(((outY * -1) + 1) * 127.f));

		const f32 len = std::sqrt(std::pow(x, 2) + std::pow(y, 2));
		const f32 factor = std::pow(len, innerRoundness);

		outX = (1 - factor) * x + factor * outX;
		outY = (1 - factor) * y + factor * outY;

		return std::tuple<u16, u16>(Clamp0To255((outX + 1) * 127.f), Clamp0To255(((outY * -1) + 1) * 127.f));
	}*/

	inline s16 GetS16LEData(const u8* buf)
	{
		return (s16)(((u16)buf[0] << 0) + ((u16)buf[1] << 8));
	}

	inline u32 GetU32LEData(const u8* buf)
	{
		return (u32)(((u32)buf[0] << 0) + ((u32)buf[1] << 8) + ((u32)buf[2] << 16) + ((u32)buf[3] << 24));
	}
}

ds4_pad_handler::ds4_pad_handler() : is_init(false)
{
	// Define border values
	THUMB_MIN = 0;
	THUMB_MAX = 255;
	TRIGGER_MIN = 0;
	TRIGGER_MAX = 255;
	VIBRATION_MIN = 0;
	VIBRATION_MAX = 255;

	// Set this handler's type and save location
	m_pad_config.cfg_type = "ds4";
	m_pad_config.cfg_name = fs::get_config_dir() + "/config_ds4.yml";

	// Set default button mapping
	m_pad_config.ls_left.def  = button_list.at(DS4KeyCodes::LSXNeg);
	m_pad_config.ls_down.def  = button_list.at(DS4KeyCodes::LSYNeg);
	m_pad_config.ls_right.def = button_list.at(DS4KeyCodes::LSXPos);
	m_pad_config.ls_up.def    = button_list.at(DS4KeyCodes::LSYPos);
	m_pad_config.rs_left.def  = button_list.at(DS4KeyCodes::RSXNeg);
	m_pad_config.rs_down.def  = button_list.at(DS4KeyCodes::RSYNeg);
	m_pad_config.rs_right.def = button_list.at(DS4KeyCodes::RSXPos);
	m_pad_config.rs_up.def    = button_list.at(DS4KeyCodes::RSYPos);
	m_pad_config.start.def    = button_list.at(DS4KeyCodes::Options);
	m_pad_config.select.def   = button_list.at(DS4KeyCodes::Share);
	m_pad_config.ps.def       = button_list.at(DS4KeyCodes::PSButton);
	m_pad_config.square.def   = button_list.at(DS4KeyCodes::Square);
	m_pad_config.cross.def    = button_list.at(DS4KeyCodes::Cross);
	m_pad_config.circle.def   = button_list.at(DS4KeyCodes::Circle);
	m_pad_config.triangle.def = button_list.at(DS4KeyCodes::Triangle);
	m_pad_config.left.def     = button_list.at(DS4KeyCodes::Left);
	m_pad_config.down.def     = button_list.at(DS4KeyCodes::Down);
	m_pad_config.right.def    = button_list.at(DS4KeyCodes::Right);
	m_pad_config.up.def       = button_list.at(DS4KeyCodes::Up);
	m_pad_config.r1.def       = button_list.at(DS4KeyCodes::R1);
	m_pad_config.r2.def       = button_list.at(DS4KeyCodes::R2);
	m_pad_config.r3.def       = button_list.at(DS4KeyCodes::R3);
	m_pad_config.l1.def       = button_list.at(DS4KeyCodes::L1);
	m_pad_config.l2.def       = button_list.at(DS4KeyCodes::L2);
	m_pad_config.l3.def       = button_list.at(DS4KeyCodes::L3);

	// Set default misc variables
	m_pad_config.lstickdeadzone.def = 40;   // between 0 and 255
	m_pad_config.rstickdeadzone.def = 40;   // between 0 and 255
	m_pad_config.ltriggerthreshold.def = 0; // between 0 and 255
	m_pad_config.rtriggerthreshold.def = 0; // between 0 and 255
	m_pad_config.padsquircling.def = 8000;

	// Set color value
	m_pad_config.colorR.def = 0;
	m_pad_config.colorG.def = 0;
	m_pad_config.colorB.def = 20;

	// apply defaults
	m_pad_config.from_default();

	// set capabilities
	b_has_config = true;
	b_has_rumble = true;
	b_has_deadzones = true;
}

void ds4_pad_handler::GetNextButtonPress(const std::string& padId, const std::vector<int>& deadzones, const std::function<void(std::string)>& callback)
{
	if (!Init())
	{
		return;
	}

	int ltriggerthreshold = deadzones[0];
	int rtriggerthreshold = deadzones[1];
	int lstickdeadzone = deadzones[2];
	int rstickdeadzone = deadzones[3];

	// Get the DS4 Device or return if none found
	size_t pos = padId.find("Ds4 Pad #");

	if (pos == std::string::npos) return;

	std::string pad_serial = padId.substr(pos + 9);

	std::shared_ptr<DS4Device> device = nullptr;

	for (auto& cur_control : controllers)
	{
		if (pad_serial == cur_control.first)
		{
			device = cur_control.second;
			break;
		}
	}

	if (device == nullptr || device->hidDevice == nullptr) return;

	// Now that we have found a device, get its status
	DS4DataStatus status = GetRawData(device);

	if (status == DS4DataStatus::ReadError)
	{
		// this also can mean disconnected, either way deal with it on next loop and reconnect
		hid_close(device->hidDevice);
		device->hidDevice = nullptr;
		return;
	}

	if (status != DS4DataStatus::NewData) return;

	// Get the current button values
	auto data = GetButtonValues(device);

	// Check for each button in our list if its corresponding (maybe remapped) button or axis was pressed.
	// Return the new value if the button was pressed (aka. its value was bigger than 0 or the defined threshold)
	// Use a pair to get all the legally pressed buttons and use the one with highest value (prioritize first)
	std::pair<u16, std::string> pressed_button = { 0, "" };
	for (const auto& button : button_list)
	{
		u32 keycode = button.first;
		u16 value = data[keycode];

		if (((keycode < DS4KeyCodes::L2) && (value > 0))
		 || ((keycode == DS4KeyCodes::L2) && (value > ltriggerthreshold))
		 || ((keycode == DS4KeyCodes::R2) && (value > rtriggerthreshold))
		 || ((keycode >= DS4KeyCodes::LSXNeg && keycode <= DS4KeyCodes::LSYPos) && (value > lstickdeadzone))
		 || ((keycode >= DS4KeyCodes::RSXNeg && keycode <= DS4KeyCodes::RSYPos) && (value > rstickdeadzone)))
		{
			if (value > pressed_button.first)
			{
				pressed_button = { value, button.second};
			}
		}
	}
	if (pressed_button.first > 0)
	{
		LOG_NOTICE(HLE, "GetNextButtonPress: %s button %s pressed with value %d", m_pad_config.cfg_type, pressed_button.second, pressed_button.first);
		return callback(pressed_button.second);
	}
}

void ds4_pad_handler::TestVibration(const std::string& padId, u32 largeMotor, u32 smallMotor)
{
	if (!Init())
	{
		return;
	}

	size_t pos = padId.find("Ds4 Pad #");

	if (pos == std::string::npos) return;

	std::string pad_serial = padId.substr(pos + 9);

	std::shared_ptr<DS4Device> device = nullptr;

	for (auto& cur_control : controllers)
	{
		if (pad_serial == cur_control.first)
		{
			device = cur_control.second;
			break;
		}
	}

	if (device == nullptr || device->hidDevice == nullptr) return;

	// Set the device's motor speeds to our requested values
	device->largeVibrate = largeMotor;
	device->smallVibrate = (smallMotor ? 255 : 0); // only on or off possible

	// Start/Stop the engines :)
	SendVibrateData(device);
}

void ds4_pad_handler::TranslateButtonPress(u32 keyCode, bool& pressed, u16& value, bool ignore_threshold)
{
	// Get the requested button value from a previously filled buffer
	const u16 val = button_values[keyCode];

	// Update the pad button values based on their type and thresholds.
	// With this you can use axis or triggers as buttons or vice versa
	switch (keyCode)
	{
	case DS4KeyCodes::L2:
		pressed = val > m_pad_config.ltriggerthreshold;
		value = pressed ? NormalizeTriggerInput(val, m_pad_config.ltriggerthreshold) : 0;
		break;
	case DS4KeyCodes::R2:
		pressed = val > m_pad_config.rtriggerthreshold;
		value = pressed ? NormalizeTriggerInput(val, m_pad_config.rtriggerthreshold) : 0;
		break;
	case DS4KeyCodes::LSXNeg:
	case DS4KeyCodes::LSXPos:
	case DS4KeyCodes::LSYNeg:
	case DS4KeyCodes::LSYPos:
		pressed = val > (ignore_threshold ? 0 : m_pad_config.lstickdeadzone);
		value = pressed ? NormalizeStickInput(val, m_pad_config.lstickdeadzone, ignore_threshold) : 0;
		break;
	case DS4KeyCodes::RSXNeg:
	case DS4KeyCodes::RSXPos:
	case DS4KeyCodes::RSYNeg:
	case DS4KeyCodes::RSYPos:
		pressed = val > (ignore_threshold ? 0 : m_pad_config.rstickdeadzone);
		value = pressed ? NormalizeStickInput(val, m_pad_config.rstickdeadzone, ignore_threshold) : 0;
		break;
	default: // normal button (should in theory also support sensitive buttons)
		pressed = val > 0;
		value = pressed ? val : 0;
		break;
	}
}

std::array<u16, ds4_pad_handler::DS4KeyCodes::KEYCODECOUNT> ds4_pad_handler::GetButtonValues(const std::shared_ptr<DS4Device>& device)
{
	std::array<u16, DS4KeyCodes::KEYCODECOUNT> keyBuffer;
	auto buf = device->padData;

	// Left Stick X Axis
	keyBuffer[DS4KeyCodes::LSXNeg] = Clamp0To255((127.5f - buf[1]) * 2.0f);
	keyBuffer[DS4KeyCodes::LSXPos] = Clamp0To255((buf[1] - 127.5f) * 2.0f);

	// Left Stick Y Axis (Up is the negative for some reason)
	keyBuffer[DS4KeyCodes::LSYNeg] = Clamp0To255((buf[2] - 127.5f) * 2.0f);
	keyBuffer[DS4KeyCodes::LSYPos] = Clamp0To255((127.5f - buf[2]) * 2.0f);

	// Right Stick X Axis
	keyBuffer[DS4KeyCodes::RSXNeg] = Clamp0To255((127.5f - buf[3]) * 2.0f);
	keyBuffer[DS4KeyCodes::RSXPos] = Clamp0To255((buf[3] - 127.5f) * 2.0f);

	// Right Stick Y Axis (Up is the negative for some reason)
	keyBuffer[DS4KeyCodes::RSYNeg] = Clamp0To255((buf[4] - 127.5f) * 2.0f);
	keyBuffer[DS4KeyCodes::RSYPos] = Clamp0To255((127.5f - buf[4]) * 2.0f);

	// bleh, dpad in buffer is stored in a different state
	u8 dpadState = buf[5] & 0xf;
	switch (dpadState)
	{
	case 0x08: // none pressed
		keyBuffer[DS4KeyCodes::Up] = 0;
		keyBuffer[DS4KeyCodes::Down] = 0;
		keyBuffer[DS4KeyCodes::Left] = 0;
		keyBuffer[DS4KeyCodes::Right] = 0;
		break;
	case 0x07: // NW...left and up
		keyBuffer[DS4KeyCodes::Up] = 255;
		keyBuffer[DS4KeyCodes::Down] = 0;
		keyBuffer[DS4KeyCodes::Left] = 255;
		keyBuffer[DS4KeyCodes::Right] = 0;
		break;
	case 0x06: // W..left
		keyBuffer[DS4KeyCodes::Up] = 0;
		keyBuffer[DS4KeyCodes::Down] = 0;
		keyBuffer[DS4KeyCodes::Left] = 255;
		keyBuffer[DS4KeyCodes::Right] = 0;
		break;
	case 0x05: // SW..left down
		keyBuffer[DS4KeyCodes::Up] = 0;
		keyBuffer[DS4KeyCodes::Down] = 255;
		keyBuffer[DS4KeyCodes::Left] = 255;
		keyBuffer[DS4KeyCodes::Right] = 0;
		break;
	case 0x04: // S..down
		keyBuffer[DS4KeyCodes::Up] = 0;
		keyBuffer[DS4KeyCodes::Down] = 255;
		keyBuffer[DS4KeyCodes::Left] = 0;
		keyBuffer[DS4KeyCodes::Right] = 0;
		break;
	case 0x03: // SE..down and right
		keyBuffer[DS4KeyCodes::Up] = 0;
		keyBuffer[DS4KeyCodes::Down] = 255;
		keyBuffer[DS4KeyCodes::Left] = 0;
		keyBuffer[DS4KeyCodes::Right] = 255;
		break;
	case 0x02: // E... right
		keyBuffer[DS4KeyCodes::Up] = 0;
		keyBuffer[DS4KeyCodes::Down] = 0;
		keyBuffer[DS4KeyCodes::Left] = 0;
		keyBuffer[DS4KeyCodes::Right] = 255;
		break;
	case 0x01: // NE.. up right
		keyBuffer[DS4KeyCodes::Up] = 255;
		keyBuffer[DS4KeyCodes::Down] = 0;
		keyBuffer[DS4KeyCodes::Left] = 0;
		keyBuffer[DS4KeyCodes::Right] = 255;
		break;
	case 0x00: // n.. up
		keyBuffer[DS4KeyCodes::Up] = 255;
		keyBuffer[DS4KeyCodes::Down] = 0;
		keyBuffer[DS4KeyCodes::Left] = 0;
		keyBuffer[DS4KeyCodes::Right] = 0;
		break;
	default:
		fmt::throw_exception("ds4 dpad state encountered unexpected input");
	}

	// square, cross, circle, triangle
	keyBuffer[DS4KeyCodes::Square] =   ((buf[5] & (1 << 4)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::Cross] =    ((buf[5] & (1 << 5)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::Circle] =   ((buf[5] & (1 << 6)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::Triangle] = ((buf[5] & (1 << 7)) != 0) ? 255 : 0;

	// L1, R1
	keyBuffer[DS4KeyCodes::L1] = ((buf[6] & (1 << 0)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::R1] = ((buf[6] & (1 << 1)) != 0) ? 255 : 0;

	// select, start, l3, r3
	keyBuffer[DS4KeyCodes::Share] =   ((buf[6] & (1 << 4)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::Options] = ((buf[6] & (1 << 5)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::L3] =      ((buf[6] & (1 << 6)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::R3] =      ((buf[6] & (1 << 7)) != 0) ? 255 : 0;

	// PS Button, Touch Button
	keyBuffer[DS4KeyCodes::PSButton] = ((buf[7] & (1 << 0)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::TouchPad] = ((buf[7] & (1 << 1)) != 0) ? 255 : 0;

	// L2, R2
	keyBuffer[DS4KeyCodes::L2] = buf[8];
	keyBuffer[DS4KeyCodes::R2] = buf[9];

	return keyBuffer;
}

void ds4_pad_handler::ProcessDataToPad(const std::shared_ptr<DS4Device>& device, const std::shared_ptr<Pad>& pad)
{
	auto buf = device->padData;

	button_values = GetButtonValues(device);

	// Translate any corresponding keycodes to our normal DS3 buttons and triggers
	for (auto & btn : pad->m_buttons)
	{
		TranslateButtonPress(btn.m_keyCode, btn.m_pressed, btn.m_value);
	}

#ifdef _WIN32
	for (int i = 6; i < 16; i++)
	{
		if (pad->m_buttons[i].m_pressed)
		{
			SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
			break;
		}
	}
#endif

	// used to get the absolute value of an axis
	float stick_val[4];

	// Translate any corresponding keycodes to our two sticks. (ignoring thresholds for now)
	for (int i = 0; i < static_cast<int>(pad->m_sticks.size()); i++)
	{
		bool pressed;
		u16 val_min, val_max;

		// m_keyCodeMin is the mapped key for left or down
		// m_keyCodeMax is the mapped key for right or up
		TranslateButtonPress(pad->m_sticks[i].m_keyCodeMin, pressed, val_min, true);
		TranslateButtonPress(pad->m_sticks[i].m_keyCodeMax, pressed, val_max, true);

		// cancel out opposing values and get the resulting difference
		stick_val[i] = val_max - val_min;
	}

	// Normalize our two stick's axis based on the thresholds
	NormalizeRawStickInput(stick_val[0], stick_val[1], m_pad_config.lstickdeadzone);
	NormalizeRawStickInput(stick_val[2], stick_val[3], m_pad_config.rstickdeadzone);

	// Convert the axis to use the 0-127-255 range
	stick_val[0] = ConvertAxis(stick_val[0]);
	stick_val[1] = 255 - ConvertAxis(stick_val[1]);
	stick_val[2] = ConvertAxis(stick_val[2]);
	stick_val[3] = 255 - ConvertAxis(stick_val[3]);

	// these are added with previous value and divided to 'smooth' out the readings
	// the ds4 seems to rapidly flicker sometimes between two values and this seems to stop that

	u16 lx, ly, rx, ry;

	if (m_pad_config.padsquircling != 0)
	{
		//std::tie(lx, ly) = ConvertToSquarePoint(stick_val[0], stick_val[1]);
		//std::tie(rx, ry) = ConvertToSquarePoint(stick_val[2], stick_val[3]);
		std::tie(lx, ly) = ConvertToSquirclePoint(stick_val[0], stick_val[1], m_pad_config.padsquircling);
		std::tie(rx, ry) = ConvertToSquirclePoint(stick_val[2], stick_val[3], m_pad_config.padsquircling);
	}
	
	pad->m_sticks[0].m_value = (lx + pad->m_sticks[0].m_value) / 2; // LX
	pad->m_sticks[1].m_value = (ly + pad->m_sticks[1].m_value) / 2; // LY
	pad->m_sticks[2].m_value = (rx + pad->m_sticks[2].m_value) / 2; // RX
	pad->m_sticks[3].m_value = (ry + pad->m_sticks[3].m_value) / 2; // RY

	// these values come already calibrated from our ds4Thread,
	// all we need to do is convert to ds3 range

	// accel
	f32 accelX = (((s16)((u16)(buf[20] << 8) | buf[19])) / static_cast<f32>(DS4_ACC_RES_PER_G)) * -1;
	f32 accelY = (((s16)((u16)(buf[22] << 8) | buf[21])) / static_cast<f32>(DS4_ACC_RES_PER_G)) * -1;
	f32 accelZ = (((s16)((u16)(buf[24] << 8) | buf[23])) / static_cast<f32>(DS4_ACC_RES_PER_G)) * -1;

	// now just use formula from ds3
	accelX = accelX * 113 + 512;
	accelY = accelY * 113 + 512;
	accelZ = accelZ * 113 + 512;

	pad->m_sensors[0].m_value = Clamp0To1023(accelX);
	pad->m_sensors[1].m_value = Clamp0To1023(accelY);
	pad->m_sensors[2].m_value = Clamp0To1023(accelZ);

	// gyroX is yaw, which is all that we need
	f32 gyroX = (((s16)((u16)(buf[16] << 8) | buf[15])) / static_cast<f32>(DS4_GYRO_RES_PER_DEG_S)) * -1;
	//const int gyroY = ((u16)(buf[14] << 8) | buf[13]) / 256;
	//const int gyroZ = ((u16)(buf[18] << 8) | buf[17]) / 256;

	// convert to ds3
	gyroX = gyroX * (123.f / 90.f) + 512;

	pad->m_sensors[3].m_value = Clamp0To1023(gyroX);
}

bool ds4_pad_handler::GetCalibrationData(const std::shared_ptr<DS4Device>& ds4Dev)
{
	std::array<u8, 64> buf;
	if (ds4Dev->btCon)
	{
		for (int tries = 0; tries < 3; ++tries)
		{
			buf[0] = 0x05;
			if (hid_get_feature_report(ds4Dev->hidDevice, buf.data(), DS4_FEATURE_REPORT_0x05_SIZE) <= 0)
				return false;

			const u8 btHdr = 0xA3;
			const u32 crcHdr = CRCPP::CRC::Calculate(&btHdr, 1, crcTable);
			const u32 crcCalc = CRCPP::CRC::Calculate(buf.data(), (DS4_FEATURE_REPORT_0x05_SIZE - 4), crcTable, crcHdr);
			const u32 crcReported = GetU32LEData(&buf[DS4_FEATURE_REPORT_0x05_SIZE - 4]);
			if (crcCalc != crcReported)
				LOG_WARNING(HLE, "[DS4] Calibration CRC check failed! Will retry up to 3 times. Received 0x%x, Expected 0x%x", crcReported, crcCalc);
			else break;
			if (tries == 2)
				return false;
		}
	}
	else
	{
		buf[0] = 0x02;
		if (hid_get_feature_report(ds4Dev->hidDevice, buf.data(), DS4_FEATURE_REPORT_0x02_SIZE) <= 0)
		{
			LOG_ERROR(HLE, "[DS4] Failed getting calibration data report!");
			return false;
		}
	}

	ds4Dev->calibData[DS4CalibIndex::PITCH].bias = GetS16LEData(&buf[1]);
	ds4Dev->calibData[DS4CalibIndex::YAW].bias = GetS16LEData(&buf[3]);
	ds4Dev->calibData[DS4CalibIndex::ROLL].bias = GetS16LEData(&buf[5]);

	s16 pitchPlus, pitchNeg, rollPlus, rollNeg, yawPlus, yawNeg;
	if (ds4Dev->btCon)
	{
		pitchPlus = GetS16LEData(&buf[7]);
		yawPlus   = GetS16LEData(&buf[9]);
		rollPlus  = GetS16LEData(&buf[11]);
		pitchNeg  = GetS16LEData(&buf[13]);
		yawNeg    = GetS16LEData(&buf[15]);
		rollNeg   = GetS16LEData(&buf[17]);
	}
	else
	{
		pitchPlus = GetS16LEData(&buf[7]);
		pitchNeg  = GetS16LEData(&buf[9]);
		yawPlus   = GetS16LEData(&buf[11]);
		yawNeg    = GetS16LEData(&buf[13]);
		rollPlus  = GetS16LEData(&buf[15]);
		rollNeg   = GetS16LEData(&buf[17]);
	}

	const s32 gyroSpeedScale = GetS16LEData(&buf[19]) + GetS16LEData(&buf[21]);

	ds4Dev->calibData[DS4CalibIndex::PITCH].sensNumer = gyroSpeedScale * DS4_GYRO_RES_PER_DEG_S;
	ds4Dev->calibData[DS4CalibIndex::PITCH].sensDenom = pitchPlus - pitchNeg;

	ds4Dev->calibData[DS4CalibIndex::YAW].sensNumer = gyroSpeedScale * DS4_GYRO_RES_PER_DEG_S;
	ds4Dev->calibData[DS4CalibIndex::YAW].sensDenom = yawPlus - yawNeg;

	ds4Dev->calibData[DS4CalibIndex::ROLL].sensNumer = gyroSpeedScale * DS4_GYRO_RES_PER_DEG_S;
	ds4Dev->calibData[DS4CalibIndex::ROLL].sensDenom = rollPlus - rollNeg;

	const s16 accelXPlus = GetS16LEData(&buf[23]);
	const s16 accelXNeg  = GetS16LEData(&buf[25]);
	const s16 accelYPlus = GetS16LEData(&buf[27]);
	const s16 accelYNeg  = GetS16LEData(&buf[29]);
	const s16 accelZPlus = GetS16LEData(&buf[31]);
	const s16 accelZNeg  = GetS16LEData(&buf[33]);

	const s32 accelXRange = accelXPlus - accelXNeg;
	ds4Dev->calibData[DS4CalibIndex::X].bias = accelXPlus - accelXRange / 2;
	ds4Dev->calibData[DS4CalibIndex::X].sensNumer = 2 * DS4_ACC_RES_PER_G;
	ds4Dev->calibData[DS4CalibIndex::X].sensDenom = accelXRange;

	const s32 accelYRange = accelYPlus - accelYNeg;
	ds4Dev->calibData[DS4CalibIndex::Y].bias = accelYPlus - accelYRange / 2;
	ds4Dev->calibData[DS4CalibIndex::Y].sensNumer = 2 * DS4_ACC_RES_PER_G;
	ds4Dev->calibData[DS4CalibIndex::Y].sensDenom = accelYRange;

	const s32 accelZRange = accelZPlus - accelZNeg;
	ds4Dev->calibData[DS4CalibIndex::Z].bias = accelZPlus - accelZRange / 2;
	ds4Dev->calibData[DS4CalibIndex::Z].sensNumer = 2 * DS4_ACC_RES_PER_G;
	ds4Dev->calibData[DS4CalibIndex::Z].sensDenom = accelZRange;

	// Make sure data 'looks' valid, dongle will report invalid calibration data with no controller connected

	for (const auto& data : ds4Dev->calibData)
	{
		if (data.sensDenom == 0)
			return false;
	}

	return true;
}

void ds4_pad_handler::CheckAddDevice(hid_device* hidDevice, hid_device_info* hidDevInfo)
{
	std::string serial = "";
	std::shared_ptr<DS4Device> ds4Dev = std::make_shared<DS4Device>();
	ds4Dev->hidDevice = hidDevice;
	// There isnt a nice 'portable' way with hidapi to detect bt vs wired as the pid/vid's are the same
	// Let's try getting 0x81 feature report, which should will return mac address on wired, and should error on bluetooth
	std::array<u8, 64> buf{};
	buf[0] = 0x81;
	if (hid_get_feature_report(hidDevice, buf.data(), DS4_FEATURE_REPORT_0x81_SIZE) > 0)
	{
		serial = fmt::format("%x%x%x%x%x%x", buf[6], buf[5], buf[4], buf[3], buf[2], buf[1]);
	}
	else
	{
		ds4Dev->btCon = true;
		std::wstring wSerial(hidDevInfo->serial_number);
		serial = std::string(wSerial.begin(), wSerial.end());
	}

	if (!GetCalibrationData(ds4Dev))
	{
		hid_close(hidDevice);
		return;
	}

	ds4Dev->hasCalibData = true;
	ds4Dev->path = hidDevInfo->path;

	hid_set_nonblocking(hidDevice, 1);
	controllers.emplace(serial, ds4Dev);
}

ds4_pad_handler::~ds4_pad_handler()
{
	for (auto& controller : controllers)
	{
		if (controller.second->hidDevice)
			hid_close(controller.second->hidDevice);
	}
	hid_exit();
}

void ds4_pad_handler::SendVibrateData(const std::shared_ptr<DS4Device>& device)
{
	std::array<u8, 78> outputBuf{0};
	// write rumble state
	if (device->btCon)
	{
		outputBuf[0] = 0x11;
		outputBuf[1] = 0xC4;
		outputBuf[3] = 0x07;
		outputBuf[6] = device->smallVibrate;
		outputBuf[7] = device->largeVibrate;
		outputBuf[8] = m_pad_config.colorR; // red
		outputBuf[9] = m_pad_config.colorG; // green
		outputBuf[10] = m_pad_config.colorB; // blue

		const u8 btHdr = 0xA2;
		const u32 crcHdr = CRCPP::CRC::Calculate(&btHdr, 1, crcTable);
		const u32 crcCalc = CRCPP::CRC::Calculate(outputBuf.data(), (DS4_OUTPUT_REPORT_0x11_SIZE - 4), crcTable, crcHdr);

		outputBuf[74] = (crcCalc >> 0) & 0xFF;
		outputBuf[75] = (crcCalc >> 8) & 0xFF;
		outputBuf[76] = (crcCalc >> 16) & 0xFF;
		outputBuf[77] = (crcCalc >> 24) & 0xFF;

		hid_write_control(device->hidDevice, outputBuf.data(), DS4_OUTPUT_REPORT_0x11_SIZE);
	}
	else
	{
		outputBuf[0] = 0x05;
		outputBuf[1] = 0x07;
		outputBuf[4] = device->smallVibrate;
		outputBuf[5] = device->largeVibrate;
		outputBuf[6] = m_pad_config.colorR; // red
		outputBuf[7] = m_pad_config.colorG; // green
		outputBuf[8] = m_pad_config.colorB; // blue

		hid_write(device->hidDevice, outputBuf.data(), DS4_OUTPUT_REPORT_0x05_SIZE);
	}
}

bool ds4_pad_handler::Init()
{
	if (is_init) return true;

	const int res = hid_init();
	if (res != 0)
		fmt::throw_exception("hidapi-init error.threadproc");

	// get all the possible controllers at start
	for (auto pid : ds4Pids)
	{
		hid_device_info* devInfo = hid_enumerate(DS4_VID, pid);
		hid_device_info* head = devInfo;
		while (devInfo)
		{
			if (controllers.size() >= MAX_GAMEPADS)	break;

			hid_device* dev = hid_open_path(devInfo->path);
			if (dev)
				CheckAddDevice(dev, devInfo);
			else
				LOG_ERROR(HLE, "[DS4] hid_open_path failed! Reason: %S", hid_error(dev));
			devInfo = devInfo->next;
		}
		hid_free_enumeration(head);
	}

	if (controllers.size() == 0)
		LOG_ERROR(HLE, "[DS4] No controllers found!");
	else
		LOG_SUCCESS(HLE, "[DS4] Controllers found: %d", controllers.size());

	m_pad_config.load();
	if (!m_pad_config.exist()) m_pad_config.save();

	is_init = true;
	return true;
}

std::vector<std::string> ds4_pad_handler::ListDevices()
{
	std::vector<std::string> ds4_pads_list;

	if (!Init()) return ds4_pads_list;

	for (auto& pad : controllers)
	{
		ds4_pads_list.emplace_back("Ds4 Pad #" + pad.first);
	}

	return ds4_pads_list;
}

bool ds4_pad_handler::bindPadToDevice(std::shared_ptr<Pad> pad, const std::string& device)
{
	size_t pos = device.find("Ds4 Pad #");

	if (pos == std::string::npos) return false;

	std::string pad_serial = device.substr(pos + 9);

	std::shared_ptr<DS4Device> device_id = nullptr;

	for (auto& cur_control : controllers)
	{
		if (pad_serial == cur_control.first)
		{
			device_id = cur_control.second;
			break;
		}
	}

	if (device_id == nullptr) return false;

	m_pad_config.load();

	pad->Init
	(
		CELL_PAD_STATUS_CONNECTED | CELL_PAD_STATUS_ASSIGN_CHANGES,
		CELL_PAD_SETTING_PRESS_OFF | CELL_PAD_SETTING_SENSOR_OFF,
		CELL_PAD_CAPABILITY_PS3_CONFORMITY | CELL_PAD_CAPABILITY_PRESS_MODE | CELL_PAD_CAPABILITY_HP_ANALOG_STICK | CELL_PAD_CAPABILITY_ACTUATOR | CELL_PAD_CAPABILITY_SENSOR_MODE,
		CELL_PAD_DEV_TYPE_STANDARD
	);

	// 'keycode' here is just 0 as we have to manually calculate this
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, m_pad_config.l2),       CELL_PAD_CTRL_L2);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, m_pad_config.r2),       CELL_PAD_CTRL_R2);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, m_pad_config.up),       CELL_PAD_CTRL_UP);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, m_pad_config.down),     CELL_PAD_CTRL_DOWN);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, m_pad_config.left),     CELL_PAD_CTRL_LEFT);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, m_pad_config.right),    CELL_PAD_CTRL_RIGHT);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, m_pad_config.square),   CELL_PAD_CTRL_SQUARE);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, m_pad_config.cross),    CELL_PAD_CTRL_CROSS);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, m_pad_config.circle),   CELL_PAD_CTRL_CIRCLE);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, m_pad_config.triangle), CELL_PAD_CTRL_TRIANGLE);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, m_pad_config.l1),       CELL_PAD_CTRL_L1);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, m_pad_config.r1),       CELL_PAD_CTRL_R1);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, m_pad_config.select),   CELL_PAD_CTRL_SELECT);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, m_pad_config.start),    CELL_PAD_CTRL_START);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, m_pad_config.l3),       CELL_PAD_CTRL_L3);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(button_list, m_pad_config.r3),       CELL_PAD_CTRL_R3);
	//pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(button_list, m_pad_config.ps),       CELL_PAD_CTRL_PS);
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, 0, 0x100/*CELL_PAD_CTRL_PS*/);// TODO: PS button support
	pad->m_buttons.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, 0, 0x0); // Reserved

	pad->m_sensors.emplace_back(CELL_PAD_BTN_OFFSET_SENSOR_X, 512);
	pad->m_sensors.emplace_back(CELL_PAD_BTN_OFFSET_SENSOR_Y, 399);
	pad->m_sensors.emplace_back(CELL_PAD_BTN_OFFSET_SENSOR_Z, 512);
	pad->m_sensors.emplace_back(CELL_PAD_BTN_OFFSET_SENSOR_G, 512);

	pad->m_sticks.emplace_back(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X,  FindKeyCode(button_list, m_pad_config.ls_left), FindKeyCode(button_list, m_pad_config.ls_right));
	pad->m_sticks.emplace_back(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y,  FindKeyCode(button_list, m_pad_config.ls_down), FindKeyCode(button_list, m_pad_config.ls_up));
	pad->m_sticks.emplace_back(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X, FindKeyCode(button_list, m_pad_config.rs_left), FindKeyCode(button_list, m_pad_config.rs_right));
	pad->m_sticks.emplace_back(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y, FindKeyCode(button_list, m_pad_config.rs_down), FindKeyCode(button_list, m_pad_config.rs_up));

	pad->m_vibrateMotors.emplace_back(true, 0);
	pad->m_vibrateMotors.emplace_back(false, 0);

	bindings.emplace_back(device_id, pad);

	return true;
}

void ds4_pad_handler::ThreadProc()
{
	for (auto &bind : bindings)
	{
		std::shared_ptr<DS4Device> device = bind.first;
		auto thepad = bind.second;

		if (device->hidDevice == nullptr)
		{
			// try to reconnect
			hid_device* dev = hid_open_path(device->path.c_str());
			if (dev)
			{
				hid_set_nonblocking(dev, 1);
				device->hidDevice = dev;
				thepad->m_port_status = CELL_PAD_STATUS_CONNECTED|CELL_PAD_STATUS_ASSIGN_CHANGES;
				if (!device->hasCalibData)
					device->hasCalibData = GetCalibrationData(device);
			}
			else
			{
				// nope, not there
				thepad->m_port_status = CELL_PAD_STATUS_DISCONNECTED|CELL_PAD_STATUS_ASSIGN_CHANGES;
				continue;
			}
		}

		DS4DataStatus status = GetRawData(device);

		if (status == DS4DataStatus::ReadError)
		{
			// this also can mean disconnected, either way deal with it on next loop and reconnect
			hid_close(device->hidDevice);
			device->hidDevice = nullptr;
			continue;
		}

		// Attempt to send rumble no matter what 
		device->newVibrateData = device->newVibrateData || device->largeVibrate != thepad->m_vibrateMotors[0].m_value || device->smallVibrate != (thepad->m_vibrateMotors[1].m_value ? 255 : 0);

		int idx_l = m_pad_config.switch_vibration_motors ? 1 : 0;
		int idx_s = m_pad_config.switch_vibration_motors ? 0 : 1;

		int speed_large = m_pad_config.enable_vibration_motor_large ? thepad->m_vibrateMotors[idx_l].m_value : VIBRATION_MIN;
		int speed_small = m_pad_config.enable_vibration_motor_small ? thepad->m_vibrateMotors[idx_s].m_value : VIBRATION_MIN;

		device->largeVibrate = speed_large;
		device->smallVibrate = (speed_small ? speed_small : 0);

		if (device->newVibrateData)
		{
			SendVibrateData(device);
			device->newVibrateData = false;
		}

		// no data? keep going
		if (status == DS4DataStatus::NoNewData)
			continue;

		else if (status == DS4DataStatus::NewData)
			ProcessDataToPad(device, thepad); // todo: change this to not loop
	}
}

ds4_pad_handler::DS4DataStatus ds4_pad_handler::GetRawData(const std::shared_ptr<DS4Device>& device)
{

	std::array<u8, 78> buf{};

	const int res = hid_read(device->hidDevice, buf.data(), device->btCon ? 78 : 64);
	if (res == -1)
	{
		// looks like controller disconnected or read error
		return DS4DataStatus::ReadError;
	}

	// no data? keep going
	if (res == 0)
		return DS4DataStatus::NoNewData;

	// bt controller sends this until 0x02 feature report is sent back (happens on controller init/restart)
	if (device->btCon && buf[0] == 0x1)
	{
		// tells controller to send 0x11 reports
		std::array<u8, 64> buf_error{};
		buf_error[0] = 0x2;
		hid_get_feature_report(device->hidDevice, buf_error.data(), buf_error.size());
		return DS4DataStatus::NoNewData;
	}

	int offset = 0;
	// check report and set offset
	if (device->btCon && buf[0] == 0x11 && res == 78)
	{
		offset = 2;

		const u8 btHdr = 0xA1;
		const u32 crcHdr = CRCPP::CRC::Calculate(&btHdr, 1, crcTable);
		const u32 crcCalc = CRCPP::CRC::Calculate(buf.data(), (DS4_INPUT_REPORT_0x11_SIZE - 4), crcTable, crcHdr);
		const u32 crcReported = GetU32LEData(&buf[DS4_INPUT_REPORT_0x11_SIZE - 4]);
		if (crcCalc != crcReported)
		{
			LOG_WARNING(HLE, "[DS4] Data packet CRC check failed, ignoring! Received 0x%x, Expected 0x%x", crcReported, crcCalc);
			return DS4DataStatus::NoNewData;
		}

	}
	else if (!device->btCon && buf[0] == 0x01 && res == 64)
	{
		// Ds4 Dongle uses this bit to actually report whether a controller is connected
		bool connected = (buf[31] & 0x04) ? false : true;
		if (connected && !device->hasCalibData)
			device->hasCalibData = GetCalibrationData(device);

		offset = 0;
	}
	else
		return DS4DataStatus::NoNewData;

	if (device->hasCalibData)
	{
		int calibOffset = offset + DS4_INPUT_REPORT_GYRO_X_OFFSET;
		for (int i = 0; i < DS4CalibIndex::COUNT; ++i)
		{
			const s16 rawValue = GetS16LEData(&buf[calibOffset]);
			const s16 calValue = ApplyCalibration(rawValue, device->calibData[i]);
			buf[calibOffset++] = ((u16)calValue >> 0) & 0xFF;
			buf[calibOffset++] = ((u16)calValue >> 8) & 0xFF;
		}
	}
	memcpy(device->padData.data(), &buf[offset], 64);

	return DS4DataStatus::NewData;
}
