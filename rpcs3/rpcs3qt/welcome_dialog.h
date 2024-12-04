#pragma once

#include <QDialog>

namespace Ui
{
	class welcome_dialog;
}

class gui_settings;

class welcome_dialog : public QDialog
{
	Q_OBJECT

public:
	explicit welcome_dialog(std::shared_ptr<gui_settings> gui_settings, bool is_manual_show, QWidget* parent = nullptr);
	~welcome_dialog();

	bool use_dark_theme() const
	{
		return m_use_dark_theme;
	}
private:
	std::unique_ptr<Ui::welcome_dialog> ui;
	std::shared_ptr<gui_settings> m_gui_settings;
	bool m_use_dark_theme = false;
};
