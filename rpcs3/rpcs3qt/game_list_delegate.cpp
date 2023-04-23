#include "game_list_delegate.h"
#include "movie_item.h"
#include "gui_settings.h"

game_list_delegate::game_list_delegate(QObject* parent)
	: table_item_delegate(parent, true)
{}

void game_list_delegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	table_item_delegate::paint(painter, option, index);

	// Find out if the icon or size items are visible
	if (index.column() == gui::game_list_columns::column_dir_size || (m_has_icons && index.column() == gui::game_list_columns::column_icon))
	{
		if (const QTableWidget* table = static_cast<const QTableWidget*>(parent()))
		{
			if (const QTableWidgetItem* current_item = table->item(index.row(), index.column());
				current_item && table->visibleRegion().intersects(table->visualItemRect(current_item)))
			{
				if (movie_item* item = static_cast<movie_item*>(table->item(index.row(), gui::game_list_columns::column_icon)))
				{
					if (index.column() == gui::game_list_columns::column_dir_size)
					{
						if (!item->size_on_disk_loading())
						{
							item->call_size_calc_func();
						}
					}
					else if (m_has_icons && index.column() == gui::game_list_columns::column_icon)
					{
						if (!item->icon_loading())
						{
							item->call_icon_load_func();
						}
					}
				}
			}
		}
	}
}
