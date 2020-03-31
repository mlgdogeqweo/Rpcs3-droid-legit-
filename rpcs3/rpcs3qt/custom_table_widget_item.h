#pragma once

#include "movie_item.h"

class custom_table_widget_item : public movie_item
{
private:
	int m_sort_role = Qt::DisplayRole;

public:
	custom_table_widget_item(){}
	custom_table_widget_item(const std::string& text, int sort_role = Qt::DisplayRole, const QVariant& sort_value = 0)
	: movie_item(QString::fromStdString(text).simplified()) // simplified() forces single line text
	{
		if (sort_role != Qt::DisplayRole)
		{
			setData(sort_role, sort_value, true);
		}
	}
	custom_table_widget_item(const QString& text, int sort_role = Qt::DisplayRole, const QVariant& sort_value = 0)
	: movie_item(text.simplified()) // simplified() forces single line text
	{
		if (sort_role != Qt::DisplayRole)
		{
			setData(sort_role, sort_value, true);
		}
	}

	bool operator <(const QTableWidgetItem &other) const
	{
		return data(m_sort_role) < other.data(m_sort_role);
	}

	void setData(int role, const QVariant &value, bool assign_sort_role = false)
	{
		if (assign_sort_role)
		{
			m_sort_role = role;
		}
		QTableWidgetItem::setData(role, value);
	}
};
