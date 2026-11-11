// Headless test of the core logic: storage, categories, sessions, markers, export.
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>

#include "core/CategoryManager.h"
#include "core/ExportManager.h"
#include "core/SessionManager.h"
#include "core/StorageManager.h"

using namespace sm;
using Clock = StreamTimer::Clock;

static int failures = 0;
#define CHECK(cond)                                                                   \
	do {                                                                          \
		if (!(cond)) {                                                        \
			std::printf("FAIL line %d: %s\n", __LINE__, #cond);           \
			++failures;                                                   \
		}                                                                     \
	} while (0)

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	QTemporaryDir tmp;
	const QString dir = tmp.path() + "/cfg";

	{
		StorageManager st(dir);
		CategoryManager cats(st);
		SessionManager ses(st);

		// defaults + persistence
		CHECK(cats.categories().size() == 4);
		CHECK(cats.categories()[0].name == "Clutch / Kill");
		CHECK(QFile::exists(dir + "/categories.json"));

		// markers refused while not streaming (default)
		auto r = ses.record(cats.categories()[0]);
		CHECK(r.status == SessionManager::RecordStatus::NotStreaming);
		CHECK(ses.allMarkers().isEmpty());

		// stream started "125.4 s ago" -> elapsed must be 125
		ses.setTitle("My BGMI stream, \"test\"");
		ses.onStreamingStarted(Clock::now() - std::chrono::milliseconds(125400));
		CHECK(ses.streaming());
		r = ses.record(cats.categories()[0], "=SUM(1,2)");
		CHECK(r.status == SessionManager::RecordStatus::Ok);
		CHECK(r.marker.elapsedSeconds == 125);
		CHECK(r.marker.elapsedFormatted == "00:02:05");
		CHECK(r.marker.utcTimestamp.endsWith("Z"));
		CHECK(QFile::exists(dir + "/markers.json")); // saved immediately

		// duplicate start event must not create a second session
		const int n = ses.sessions().size();
		ses.onStreamingStarted();
		CHECK(ses.sessions().size() == n);

		// offset
		Settings s = ses.settings();
		s.offsetSeconds = -10;
		ses.setSettings(s);
		r = ses.record(cats.categories()[1]);
		CHECK(r.marker.elapsedSeconds == 115);
		s.offsetSeconds = 0;
		ses.setSettings(s);

		// category edit/move/delete
		CHECK(cats.move(0, 3));
		CHECK(cats.categories()[3].name == "Clutch / Kill");
		const QString added = cats.add("Rage Quit", "#abcdef", "x");
		CHECK(cats.categories().size() == 5);
		CHECK(cats.remove(added));
		CHECK(cats.categories().size() == 4);

		ses.onStreamingStopped();
		CHECK(!ses.sessionOpen());
		CHECK(!ses.sessions().last().isOpen());

		// second stream -> new session
		ses.onStreamingStarted();
		CHECK(ses.sessions().size() == 2);
		ses.record(cats.categories()[0]);
		// leave the second session OPEN to simulate a crash
	}

	{ // "restart OBS": everything reloads, open session is repaired
		StorageManager st(dir);
		CategoryManager cats(st);
		SessionManager ses(st);
		CHECK(cats.categories().size() == 4);
		CHECK(cats.categories()[3].name == "Clutch / Kill");
		CHECK(ses.sessions().size() == 2);
		CHECK(ses.allMarkers().size() == 3);
		CHECK(ses.sessions()[1].interrupted);
		CHECK(!ses.sessions()[1].isOpen());

		// CSV
		ExportManager ex(ses);
		const QString csv = QString::fromUtf8(ex.buildCsv({}));
		CHECK(csv.startsWith(QString::fromUtf8("\xEF\xBB\xBF" "Stream ID,Stream Title,Category,Stream Time,Elapsed Seconds,Date,Actual DateTime,UTC Timestamp,Notes\r\n")));
		CHECK(csv.contains("\"My BGMI stream, \"\"test\"\"\""));
		CHECK(csv.contains("'=SUM(1,2)")); // formula neutralised
		CHECK(csv.count("\r\n") == 4);
		CHECK(ex.exportJson(tmp.path() + "/out.json", {}).isEmpty());
		CHECK(ex.exportCsv(tmp.path() + "/out.csv", {}).isEmpty());

		// delete marker, clear, delete session
		CHECK(ses.deleteMarker(ses.allMarkers().first().id));
		CHECK(ses.allMarkers().size() == 2);
		CHECK(ses.clearMarkers(ses.sessions()[1].id) == 1);
		CHECK(ses.deleteSession(ses.sessions()[0].id));
		CHECK(ses.sessions().size() == 1);

		// offline markers when allowed
		Settings s = ses.settings();
		s.allowOfflineMarkers = true;
		ses.setSettings(s);
		auto r = ses.record(cats.categories()[0]);
		CHECK(r.status == SessionManager::RecordStatus::Ok);
		CHECK(ses.sessions().last().kind == "manual");
		ses.finalize();
	}

	{ // corrupt files are tolerated
		QFile f(dir + "/markers.json");
		f.open(QIODevice::WriteOnly | QIODevice::Truncate);
		f.write("{ this is not json");
		f.close();
		QFile c(dir + "/categories.json");
		c.open(QIODevice::WriteOnly | QIODevice::Truncate);
		c.write("garbage");
		c.close();

		StorageManager st(dir);
		CategoryManager cats(st);
		SessionManager ses(st);
		CHECK(cats.categories().size() >= 4); // recovered from the .bak copy (previous save)
		CHECK(QDir(dir).entryList({"*.corrupt-*"}).size() == 2);
	}

	std::printf(failures ? "\n%d FAILURE(S)\n" : "\nALL CHECKS PASSED\n", failures);
	return failures ? 1 : 0;
}
