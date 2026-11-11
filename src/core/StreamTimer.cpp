#include "core/StreamTimer.h"

namespace sm {

void StreamTimer::start(Clock::time_point at)
{
	start_ = at;
	running_ = true;
}

void StreamTimer::stop()
{
	running_ = false;
}

qint64 StreamTimer::elapsedMs(Clock::time_point now) const
{
	if (!running_)
		return 0;
	const auto d = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
	return d < 0 ? 0 : static_cast<qint64>(d);
}

} // namespace sm
