#pragma once
// Owns the list of marker categories and keeps categories.json in sync.

#include <QObject>
#include <QVector>
#include <optional>

#include "core/Models.h"
#include "core/StorageManager.h"

namespace sm {

class CategoryManager : public QObject {
	Q_OBJECT
public:
	explicit CategoryManager(StorageManager &storage, QObject *parent = nullptr);

	const QVector<Category> &categories() const { return cats_; }
	std::optional<Category> byId(const QString &id) const;
	int indexOf(const QString &id) const;

	QString add(const QString &name, const QString &color, const QString &icon); // returns new id
	bool update(const Category &c);
	bool remove(const QString &id);
	bool move(int from, int to);

	static QVector<Category> defaults();

signals:
	void changed();

private:
	void commit();
	void migrateOldDefaults();

	StorageManager &storage_;
	QVector<Category> cats_;
};

} // namespace sm
