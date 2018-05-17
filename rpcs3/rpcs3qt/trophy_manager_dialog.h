#ifndef TROPHY_MANAGER_DIALOG
#define TROPHY_MANAGER_DIALOG

#include "stdafx.h"
#include "rpcs3/Loader/TROPUSR.h"
#include "gui_settings.h"

#include "Utilities/rXml.h"

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QPixmap>
#include <QTableWidget>
#include <QSlider>
#include <QSplitter>

struct GameTrophiesData
{
	std::unique_ptr<TROPUSRLoader> trop_usr;
	rXmlDocument trop_config; // I'd like to use unique but the protocol inside of the function passes around shared pointers..
	std::vector<QPixmap> trophy_images;
	std::string game_name;
	std::string path;
};

enum TrophyColumns
{
	Icon = 0,
	Name = 1,
	Description = 2,
	Type = 3,
	IsUnlocked = 4,
	Id = 5,

	Count
};

enum GameColumns
{
	GameIcon = 0,
	GameName = 1,
	GameProgress = 2,

	GameColumnsCount
};

class trophy_manager_dialog : public QWidget
{
	const QString Bronze   = "Bronze";
	const QString Silver   = "Silver";
	const QString Gold     = "Gold";
	const QString Platinum = "Platinum";

public:
	explicit trophy_manager_dialog(std::shared_ptr<gui_settings> gui_settings);
private Q_SLOTS:
	void ResizeGameIcons(int val);
	void ResizeTrophyIcons(int val);
	void ApplyFilter();
	void ShowContextMenu(const QPoint& pos);
private:
	/** Loads a trophy folder. 
	Returns true if successful.  Does not attempt to install if failure occurs, like sceNpTrophy.
	*/
	bool LoadTrophyFolderToDB(const std::string& trop_name);

	/** Fills UI with information.
	Takes results from LoadTrophyFolderToDB and puts it into the UI.
	*/
	void PopulateUI();

	void ReadjustGameTable();
	void ReadjustTrophyTable();

	void closeEvent(QCloseEvent* event) override;

	std::shared_ptr<gui_settings> m_gui_settings;

	std::vector<std::unique_ptr<GameTrophiesData>> m_trophies_db; //! Holds all the trophy information.
	QComboBox* m_game_combo; //! Lets you choose a game
	QLabel* m_game_progress; //! Shows you the current game's progress
	QSplitter* m_splitter; //! Contains the game and trophy tables
	QTableWidget* m_trophy_table; //! UI element to display trophy stuff.
	QTableWidget* m_game_table; //! UI element to display games.

	bool m_show_hidden_trophies = false;
	bool m_show_unlocked_trophies = true;
	bool m_show_locked_trophies = true;
	bool m_show_bronze_trophies = true;
	bool m_show_silver_trophies = true;
	bool m_show_gold_trophies = true;
	bool m_show_platinum_trophies = true;

	int m_icon_height = 75;
	bool m_save_icon_height = false;
	QSlider* m_icon_slider = nullptr;

	int m_game_icon_size_index = 25;
	QSize m_game_icon_size = QSize(m_game_icon_size_index, m_game_icon_size_index);
	bool m_save_game_icon_size = false;
	QSlider* m_game_icon_slider = nullptr;
};

#endif
