#pragma once

#include "util/types.hpp"

#include <QDir>
#include <QComboBox>
#include <QFont>
#include <QIcon>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QTreeWidgetItem>
#include <QPainter>

#include <string>
#include <map>

namespace gui
{
	namespace utils
	{
		class circle_pixmap : public QPixmap
		{
		public:
			circle_pixmap(const QColor& color, qreal pixel_ratio)
			    : QPixmap(pixel_ratio * 16, pixel_ratio * 16)
			{
				fill(Qt::transparent);

				QPainter painter(this);
				setDevicePixelRatio(pixel_ratio);
				painter.setRenderHint(QPainter::Antialiasing);
				painter.setPen(Qt::NoPen);
				painter.setBrush(color);
				painter.drawEllipse(0, 0, width(), height());
			}
		};

		template<typename T>
		static QSet<T> list_to_set(const QList<T>& list)
		{
			return QSet<T>(list.begin(), list.end());
		}

		// Creates a frame geometry rectangle with target width and height that's centered inside the base,
		// while still considering screen boundaries.
		QRect create_centered_window_geometry(const QScreen* screen, const QRect& base, s32 target_width, s32 target_height);

		// Creates a square pixmap while keeping the original aspect ratio of the image.
		bool create_square_pixmap(QPixmap& pixmap, int target_size);

		// Returns a custom colored QPixmap based on another QPixmap.
		// use colorize_all to repaint every opaque pixel with the chosen color
		// use_special_masks is only used for pixmaps with multiple predefined colors
		QPixmap get_colorized_pixmap(const QPixmap& old_pixmap, const QColor& old_color, const QColor& new_color, bool use_special_masks = false, bool colorize_all = false);

		// Returns a custom colored QIcon based on another QIcon.
		// use colorize_all to repaint every opaque pixel with the chosen color
		// use_special_masks is only used for icons with multiple predefined colors
		QIcon get_colorized_icon(const QIcon& old_icon, const QColor& old_color, const QColor& new_color, bool use_special_masks = false, bool colorize_all = false);
		QIcon get_colorized_icon(const QIcon& old_icon, const QColor& old_color, const std::map<QIcon::Mode, QColor>& new_colors, bool use_special_masks = false, bool colorize_all = false);

		// Returns a list of all base names of files in dir whose complete file names contain one of the given name_filters
		QStringList get_dir_entries(const QDir& dir, const QStringList& name_filters);

		// Returns the color specified by its color_role for the QLabels with object_name
		QColor get_label_color(const QString& object_name, QPalette::ColorRole color_role = QPalette::WindowText);

		// Returns the font of the QLabels with object_name
		QFont get_label_font(const QString& object_name);

		// Returns the width of the text
		int get_label_width(const QString& text, const QFont* font = nullptr);

		template <typename T>
		void set_font_size(T& qobj, int size)
		{
			QFont font = qobj.font();
			font.setPointSize(size);
			qobj.setFont(font);
		}

		// Returns a scaled, centered QImage
		QImage get_centered_image(const QString& path, const QSize& icon_size, int offset_x, int offset_y, qreal device_pixel_ratio);

		// Returns a scaled, centered QPixmap
		QPixmap get_centered_pixmap(const QString& path, const QSize& icon_size, int offset_x, int offset_y, qreal device_pixel_ratio);

		// Returns the part of the image loaded from path that is inside the bounding box of its opaque areas
		QImage get_opaque_image_area(const QString& path);

		// Workaround: resize the dropdown combobox items
		void resize_combo_box_view(QComboBox* combo);

		// Recalculates a table's item count based on the available visible space and fills it with empty items
		void update_table_item_count(QTableWidget* table);

		// Opens an image in a new window with original size
		void show_windowed_image(const QImage& img, const QString& title = "");

		// Loads the app icon from path and embeds it centered into an empty square icon
		QIcon get_app_icon_from_path(const std::string& path, const std::string& title_id);

		// Open a path in the explorer and mark the file
		void open_dir(const std::string& spath);

		// Open a path in the explorer and mark the file
		void open_dir(const QString& path);

		// Finds a child of a QTreeWidgetItem with given text
		QTreeWidgetItem* find_child(QTreeWidgetItem* parent, const QString& text);

		// Finds all children of a QTreeWidgetItem that match the given criteria
		QList<QTreeWidgetItem*> find_children_by_data(QTreeWidgetItem* parent, const QList<QPair<int /*role*/, QVariant /*data*/>>& criteria, bool recursive);

		// Constructs and adds a child to a QTreeWidgetItem
		QTreeWidgetItem* add_child(QTreeWidgetItem* parent, const QString& text, int column = 0);

		// Removes all children of a QTreeWidgetItem
		void remove_children(QTreeWidgetItem* parent);

		// Removes all children of a QTreeWidgetItem that don't match the given criteria
		void remove_children(QTreeWidgetItem* parent, const QList<QPair<int /*role*/, QVariant /*data*/>>& criteria, bool recursive);

		// Sort a QTreeWidget (currently only column 0)
		void sort_tree(QTreeWidget* tree, Qt::SortOrder sort_order, bool recursive);
	} // utils
} // gui
