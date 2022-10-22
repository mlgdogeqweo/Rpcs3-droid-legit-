#pragma once

#include <QButtonGroup>
#include <QDialog>
#include <QLabel>
#include <QTimer>
#include <QComboBox>

#include <mutex>

#include "Emu/Io/PadHandler.h"
#include "Emu/Io/pad_config.h"
#include "Emu/GameInfo.h"
#include "Utilities/Thread.h"
#include "pad_device_info.h"

class gui_settings;
class PadHandlerBase;

namespace Ui
{
	class pad_settings_dialog;
}

class pad_settings_dialog : public QDialog
{
	Q_OBJECT

	const int MAX_PLAYERS = 7;

	enum button_ids
	{
		id_pad_begin, // begin

		id_pad_lstick_left,
		id_pad_lstick_down,
		id_pad_lstick_right,
		id_pad_lstick_up,

		id_pad_left,
		id_pad_down,
		id_pad_right,
		id_pad_up,

		id_pad_l1,
		id_pad_l2,
		id_pad_l3,

		id_pad_start,
		id_pad_select,
		id_pad_ps,

		id_pad_r1,
		id_pad_r2,
		id_pad_r3,

		id_pad_square,
		id_pad_cross,
		id_pad_circle,
		id_pad_triangle,

		id_pad_rstick_left,
		id_pad_rstick_down,
		id_pad_rstick_right,
		id_pad_rstick_up,

		id_pressure_intensity, // Special button for pressure intensity

		id_pad_end, // end

		id_led,
		id_reset_parameters,
		id_blacklist,
		id_refresh,
		id_add_profile,
		id_ok,
		id_cancel
	};

	struct pad_button
	{
		cfg::string* cfg_text = nullptr;
		std::string key;
		QString text;
	};

	const QString Disconnected_suffix = tr(" (disconnected)");

public:
	explicit pad_settings_dialog(std::shared_ptr<gui_settings> gui_settings, QWidget *parent = nullptr, const GameInfo *game = nullptr);
	~pad_settings_dialog();

private Q_SLOTS:
	void OnPadButtonClicked(int id);
	void OnTabChanged(int index);
	void RefreshHandlers();
	void ChangeHandler();
	void ChangeProfile(const QString& profile);
	void ChangeDevice(int index);
	void HandleDeviceClassChange(int index) const;
	void AddProfile();
	/** Update the current player config with the GUI values. */
	void ApplyCurrentPlayerConfig(int new_player_id);
	void RefreshPads();

private:
	std::unique_ptr<Ui::pad_settings_dialog> ui;
	std::string m_title_id;
	std::shared_ptr<gui_settings> m_gui_settings;

	// Tooltips
	QString m_description;
	QHash<QObject*, QString> m_descriptions;

	// Capabilities
	bool m_enable_buttons{ false };
	bool m_enable_rumble{ false };
	bool m_enable_deadzones{ false };
	bool m_enable_led{ false };
	bool m_enable_battery{ false };
	bool m_enable_motion{ false };
	bool m_enable_pressure_intensity_button{ true };

	// Button Mapping
	QButtonGroup* m_pad_buttons = nullptr;
	u32 m_button_id = id_pad_begin;
	std::map<int /*id*/, pad_button /*info*/> m_cfg_entries;
	std::map<int /*id*/, std::string> m_duplicate_buttons;

	// Real time stick values
	int m_lx = 0;
	int m_ly = 0;
	int m_rx = 0;
	int m_ry = 0;

	// Rumble
	s32 m_min_force = 0;
	s32 m_max_force = 0;

	// Backup for standard button palette
	QPalette m_palette;

	// Pad Handlers
	std::shared_ptr<PadHandlerBase> m_handler;
	std::mutex m_handler_mutex;
	std::string m_device_name;
	std::string m_buddy_device_name;
	std::string m_profile;
	QTimer m_timer_pad_refresh;
	int m_last_player_id = 0;

	// Remap Timer
	const int MAX_SECONDS = 5;
	int m_seconds = MAX_SECONDS;
	QTimer m_timer;

	// Mouse Move
	QPoint m_last_pos;

	// Input timer. Updates the GUI with input values
	QTimer m_timer_input;
	std::mutex m_input_mutex;
	struct input_callback_data
	{
		PadHandlerBase::connection status = PadHandlerBase::disconnected;
		bool has_new_data = false;
		u16 val = 0;
		std::string name;
		std::string pad_name;
		u32 battery_level = 0;
		std::array<int, 6> preview_values{};
	} m_input_callback_data;

	// Input thread. Its Callback handles the input
	std::unique_ptr<named_thread<std::function<void()>>> m_input_thread;
	enum class input_thread_state { paused, pausing, active };
	atomic_t<input_thread_state> m_input_thread_state{input_thread_state::paused};

	void start_input_thread();
	void pause_input_thread();

	void SaveExit();
	void CancelExit();

	// Set vibrate data while keeping the current color
	void SetPadData(u32 large_motor, u32 small_motor, bool led_battery_indicator = false);

	/** Update all the Button Labels with current button mapping */
	void UpdateLabels(bool is_reset = false);

	pad_device_info get_pad_info(QComboBox* combo, int index);
	void switch_pad_info(int index, pad_device_info info, bool is_connected);
	void SwitchPadInfo(const std::string& name, bool is_connected);

	/** Enable/Disable Buttons while trying to remap an other */
	void SwitchButtons(bool is_enabled);

	/** Resets the view to default. Resets the Remap Timer */
	void ReactivateButtons();

	void InitButtons();
	void ReloadButtons();

	/** Repaints a stick deadzone preview label */
	void RepaintPreviewLabel(QLabel* l, int deadzone, int desired_width, int x, int y, int squircle, double multiplier) const;

	QString GetLocalizedPadHandler(const QString& original, pad_handler handler);

	/** Checks if the port at the given index is already reserved by the application as custom controller (ldd pad) */
	bool GetIsLddPad(u32 index) const;

	/** Returns the current player index */
	u32 GetPlayerIndex() const;

	/** Returns the current player config */
	cfg_pad& GetPlayerConfig() const;

	/** Resizes the dialog. We need to do this because the main scroll area can't determine the size on its own. */
	void ResizeDialog();

	/** Register a widget for tooltips */
	void SubscribeTooltip(QObject* object, const QString& tooltip);

	/** Used to keep all tooltip subscriptions in one place. */
	void SubscribeTooltips();

protected:
	void showEvent(QShowEvent* event) override;

	/** Handle keyboard handler input */
	void keyPressEvent(QKeyEvent *keyEvent) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	bool eventFilter(QObject* object, QEvent* event) override;
};
