#pragma once

#include "Utilities/Config.h"
#include "Emu/Io/PadHandler.h"

#include <QKeyEvent>
#include <QObject>

struct KeyboardPadConfig final : cfg::node
{
	const std::string cfg_name = fs::get_config_dir() + "/config_kbpad.yml";

	cfg::int32_entry left_stick_left{ *this, "Left Analog Stick Left", static_cast<int>('A') };
	cfg::int32_entry left_stick_down{ *this, "Left Analog Stick Down", static_cast<int>('S') };
	cfg::int32_entry left_stick_right{ *this, "Left Analog Stick Right", static_cast<int>('D') };
	cfg::int32_entry left_stick_up{ *this, "Left Analog Stick Up", static_cast<int>('W') };
	cfg::int32_entry right_stick_left{ *this, "Right Analog Stick Left", 313 };
	cfg::int32_entry right_stick_down{ *this, "Right Analog Stick Down", 367 };
	cfg::int32_entry right_stick_right{ *this, "Right Analog Stick Right", 312 };
	cfg::int32_entry right_stick_up{ *this, "Right Analog Stick Up", 366 };
	cfg::int32_entry start{ *this, "Start", 13 };
	cfg::int32_entry select{ *this, "Select", 32 };
	cfg::int32_entry square{ *this, "Square", static_cast<int>('Z') };
	cfg::int32_entry cross{ *this, "Cross", static_cast<int>('X') };
	cfg::int32_entry circle{ *this, "Circle", static_cast<int>('C') };
	cfg::int32_entry triangle{ *this, "Triangle", static_cast<int>('V') };
	cfg::int32_entry left{ *this, "Left", 314 };
	cfg::int32_entry down{ *this, "Down", 317 };
	cfg::int32_entry right{ *this, "Right", 316 };
	cfg::int32_entry up{ *this, "Up", 315 };
	cfg::int32_entry r1{ *this, "R1", static_cast<int>('E') };
	cfg::int32_entry r2{ *this, "R2", static_cast<int>('T') };
	cfg::int32_entry r3{ *this, "R3", static_cast<int>('G') };
	cfg::int32_entry l1{ *this, "L1", static_cast<int>('Q') };
	cfg::int32_entry l2{ *this, "L2", static_cast<int>('R') };
	cfg::int32_entry l3{ *this, "L3", static_cast<int>('F') };

	bool load()
	{
		if (fs::file cfg_file{ cfg_name, fs::read })
		{
			return from_string(cfg_file.to_string());
		}

		return false;
	}

	void save()
	{
		fs::file(cfg_name, fs::rewrite).write(to_string());
	}
};

class KeyboardPadHandler final : public QObject, public PadHandlerBase
{
public:
	virtual void Init(const u32 max_connect) override;

	KeyboardPadHandler(QObject* target, QObject* parent);

	void keyPressEvent(QKeyEvent* event);
	void keyReleaseEvent(QKeyEvent* event);
	void LoadSettings();

	bool eventFilter(QObject* obj, QEvent* ev);
private:
	QObject* m_target;
};
