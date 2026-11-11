#include "core/CategoryManager.h"

namespace sm {

CategoryManager::CategoryManager(StorageManager &storage, QObject *parent) : QObject(parent), storage_(storage)
{
	bool existed = false;
	cats_ = storage_.loadCategories(&existed);
	if (!existed) { // first run, or file missing/corrupt and no usable backup
		cats_ = defaults();
		storage_.saveCategories(cats_);
	} else {
		migrateOldDefaults();
	}
}

QVector<Category> CategoryManager::defaults()
{
	// Colours: dataviz reference palette, dark-mode categorical steps (aqua, yellow, blue, magenta).
	return {
		{"clutch-kill", "Clutch / Kill", "#199e70", QString::fromUtf8("\U0001F3AF")},   // target
		{"funny-moment", "Funny Moment", "#c98500", QString::fromUtf8("\U0001F602")},   // laughing face
		{"winner-winner", "Winner Winner", "#3987e5", QString::fromUtf8("\U0001F3C6")}, // trophy
		{"epic-moment", "Epic Moment", "#d55181", QString::fromUtf8("\U0001F525")},     // fire
	};
}

// v1.0 shipped pastel defaults. Upgrade untouched default categories to the new palette;
// anything the user edited is left alone.
void CategoryManager::migrateOldDefaults()
{
	struct Old {
		const char *id;
		const char *color;
		QString icon;
	};
	const Old old[] = {{"clutch-kill", "#B9F0C0", QString::fromUtf8("\U0001F3AF")},
			   {"funny-moment", "#FFF1A8", QString::fromUtf8("\U0001F602")},
			   {"winner-winner", "#B8DDFB", QString::fromUtf8("\U0001F357")},
			   {"epic-moment", "#FFC4DC", QString::fromUtf8("⭐")}};
	const QVector<Category> fresh = defaults();
	bool changed = false;
	for (Category &c : cats_)
		for (const Old &o : old) {
			if (c.id != QLatin1String(o.id) || c.color.compare(QLatin1String(o.color), Qt::CaseInsensitive) != 0)
				continue;
			for (const Category &f : fresh)
				if (f.id == c.id) {
					c.color = f.color;
					if (c.icon == o.icon)
						c.icon = f.icon;
					changed = true;
				}
		}
	if (changed)
		storage_.saveCategories(cats_);
}

std::optional<Category> CategoryManager::byId(const QString &id) const
{
	const int i = indexOf(id);
	if (i < 0)
		return std::nullopt;
	return cats_[i];
}

int CategoryManager::indexOf(const QString &id) const
{
	for (int i = 0; i < cats_.size(); ++i)
		if (cats_[i].id == id)
			return i;
	return -1;
}

QString CategoryManager::add(const QString &name, const QString &color, const QString &icon)
{
	Category c{newId(), name.trimmed(), color, icon};
	cats_.push_back(c);
	commit();
	return c.id;
}

bool CategoryManager::update(const Category &c)
{
	const int i = indexOf(c.id);
	if (i < 0)
		return false;
	cats_[i] = c;
	commit();
	return true;
}

bool CategoryManager::remove(const QString &id)
{
	const int i = indexOf(id);
	if (i < 0)
		return false;
	cats_.removeAt(i);
	commit();
	return true;
}

bool CategoryManager::move(int from, int to)
{
	if (from < 0 || from >= cats_.size() || to < 0 || to >= cats_.size() || from == to)
		return false;
	cats_.move(from, to);
	commit();
	return true;
}

void CategoryManager::commit()
{
	storage_.saveCategories(cats_);
	emit changed();
}

} // namespace sm
