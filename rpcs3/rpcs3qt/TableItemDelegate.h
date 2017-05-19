#ifndef TABLEITEMDELEGATE_H
#define TABLEITEMDELEGATE_H

#include <QItemDelegate>

/** This class is used to get rid of somewhat ugly item focus rectangles. You could change the rectangle instead of omiting it if you wanted */
class TableItemDelegate : public QItemDelegate
{
public:
	explicit TableItemDelegate(QObject *parent = 0) {}
	virtual void drawFocus(QPainter * /*painter*/, const QStyleOptionViewItem & /*option*/, const QRect & /*rect*/) const {}
};

#endif // !TABLEITEMDELEGATE_H
