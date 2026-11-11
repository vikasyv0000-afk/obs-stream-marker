#pragma once
// Monotonic stream clock. Uses std::chrono::steady_clock so that system clock
// changes (NTP, DST, manual edits) can never shift recorded stream times.

#include <chrono>
#include <QtGlobal>

namespace sm {

class StreamTimer {
public:
	using Clock = std::chrono::steady_clock;

	void start(Clock::time_point at = Clock::now());
	void stop();
	bool running() const { return running_; }

	// Milliseconds since start() (0 when not running).
	qint64 elapsedMs(Clock::time_point now = Clock::now()) const;
	qint64 elapsedSeconds(Clock::time_point now = Clock::now()) const { return elapsedMs(now) / 1000; }

private:
	Clock::time_point start_{};
	bool running_ = false;
};

} // namespace sm
