#include "Timer.h"

#include "java/Arithmetic.h"
#include "java/System.h"

Timer::Timer(float f) :
	ticksPerSecond(f),
	elapsedTicks(0),
	renderPartialTicks(0.0f),
	timerSpeed(1.0f),
	elapsedPartialTicks(0.0f),
	lastHRTime(0.0),
	lastSyncSysClock(System::currentTimeMillis()),
	lastSyncHRClock(System::nanoTime() / 0xf4240LL),
	accumulatedSysClock(0),
	timeSyncAdjustment(1.0)
{
}

void Timer::updateTimer()
{
	long_t l = System::currentTimeMillis();
	long_t l1 = JavaArithmetic::longSub(l, lastSyncSysClock);
	long_t l2 = System::nanoTime() / 0xf4240LL;
	double d = (double)l2 / 1000.0;
	if (l1 > 1000LL)
	{
		lastHRTime = d;
	}
	else if (l1 < 0LL)
	{
		lastHRTime = d;
	}
	else
	{
		accumulatedSysClock = JavaArithmetic::longAdd(accumulatedSysClock, l1);
		if (accumulatedSysClock > 1000LL)
		{
			long_t l3 = JavaArithmetic::longSub(l2, lastSyncHRClock);
			double d2 = (double)accumulatedSysClock / (double)l3;
			timeSyncAdjustment += (d2 - timeSyncAdjustment) * 0.20000000298023224;
			lastSyncHRClock = l2;
			accumulatedSysClock = 0LL;
		}
		if (accumulatedSysClock < 0LL)
		{
			lastSyncHRClock = l2;
		}
	}
	lastSyncSysClock = l;
	double d1 = (d - lastHRTime) * timeSyncAdjustment;
	lastHRTime = d;
	if (d1 < 0.0)
	{
		d1 = 0.0;
	}
	if (d1 > 1.0)
	{
		d1 = 1.0;
	}
	elapsedPartialTicks += d1 * (double)timerSpeed * (double)ticksPerSecond;
#if defined(PS2_PLATFORM)
	// Keep a small amount of catch-up work when rendering falls below 20 FPS,
	// but discard larger backlogs so a slow frame cannot start a tick spiral.
	if (elapsedPartialTicks > 2.0)
		elapsedPartialTicks = 2.0;
#endif
	elapsedTicks = JavaArithmetic::floatToInt(elapsedPartialTicks);
	elapsedPartialTicks -= elapsedTicks;
#if defined(PS2_PLATFORM)
	// Never let a single rendered frame run more than two simulation ticks.
	if (elapsedTicks > 2)
	{
		elapsedTicks = 2;
	}
#elif defined(WII_PLATFORM)
	// Bound catch-up work so one slow chunk or mesh frame cannot queue enough
	// simulation work to cause a self-sustaining sequence of long frames.
	if (elapsedTicks > 2)
	{
		elapsedTicks = 2;
	}
#else
	if (elapsedTicks > 10)
	{
		elapsedTicks = 10;
	}
#endif
	renderPartialTicks = elapsedPartialTicks;
}
