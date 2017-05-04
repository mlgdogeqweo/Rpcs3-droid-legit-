#ifndef GAMELISTFRAME_H
#define GAMELISTFRAME_H

#include "GuiSettings.h"

#include "Utilities\types.h"
#include "Emu\GameInfo.h"

#include <memory>

#include <QDockWidget>
#include <QTableWidget>
#include <QListView>
#include <QList>

struct Column
{
	u32 pos;
	u32 width;
	bool shown;
	std::vector<std::string> data;

	const std::string name;
	const u32 def_pos;
	const u32 def_width;

	Column(const u32 _def_pos, const u32 _def_width, const std::string& _name)
		: def_pos(_def_pos)
		, def_width(_def_width)
		, pos(_def_pos)
		, width(_def_width)
		, shown(true)
		, name(_name)
	{
		data.clear();
	}

};

struct ColumnsArr
{
	std::vector<Column> m_columns;

	ColumnsArr();

	std::vector<Column*> GetSortedColumnsByPos();

	Column* GetColumnByPos(u32 pos);

public:
	Column* m_col_icon;
	Column* m_col_name;
	Column* m_col_serial;
	Column* m_col_fw;
	Column* m_col_app_ver;
	Column* m_col_category;
	Column* m_col_path;

	QList<QImage*>* m_img_list;
	std::vector<int> m_icon_indexes;
	
	void Init();
	
	void Update(const std::vector<GameInfo>& game_data);
	
	void ShowData(QTableWidget* list);
};

class GameListFrame : public QDockWidget {
	Q_OBJECT

	int m_sortColumn;
	bool m_sortAscending;
	std::vector<std::string> m_games;
	std::vector<GameInfo> m_game_data;
	ColumnsArr m_columns;

public:
	explicit GameListFrame(std::shared_ptr<GuiSettings> settings, QWidget *parent = nullptr);
	~GameListFrame();
	void Refresh();

	/** Loads from settings. Public so that main frame can easily reset these settings if needed. */
	void LoadSettings();

	/** Saves settings. Public so that main frame can save this when a caching of column widths is needed for settings backup */
	void SaveSettings();
	
private slots:
	void Boot(int row);
	void Configure(int row);
	void RemoveGame(int row);
	void RemoveCustomConfiguration(int row);
	void OpenGameFolder(int row);
	void OpenConfigFolder(int row);
	
	void OnColClicked(int col);
signals:
	void GameListFrameClosed();
protected:
	/** Override inherited method from Qt to allow signalling when close happened.*/
	void closeEvent(QCloseEvent* event);
private:
	QTableWidget *gameList;

	void CreateActions();

	void ShowContextMenu(const QPoint &pos);
	void ShowHeaderContextMenu(const QPoint& pos);
	void doubleClickedSlot(const QModelIndex& index);

	void LoadGames();
	void LoadPSF();
	void ShowData();

	// Actions regarding showing/hiding columns
	QAction* showIconColAct;
	QAction* showNameColAct;
	QAction* showSerialColAct;
	QAction* showFWColAct;
	QAction* showAppVersionColAct;
	QAction* showCategoryColAct;
	QAction* showPathColAct;

	QList<QAction*> columnActs;
	std::shared_ptr<GuiSettings> xGuiSettings;
};

#endif // GAMELISTFRAME_H
