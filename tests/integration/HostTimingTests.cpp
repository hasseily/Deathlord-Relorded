#include "HostTiming.h"

#include <cmath>
#include <cstdio>
#include <limits>

int main()
{
    int failures = 0;
    const auto check = [&](bool okay, const char* description) {
        if (!okay) { ++failures; std::fprintf(stderr, "FAIL: %s\n", description); }
    };

    constexpr double hz = 1020484.45;
    dlrl::EmulationClock cpu;
    for (double refresh : {30.0, 60.0, 144.0})
    {
        cpu.Reset(0);
        dlrl::RenderClock display;
        display.SetRefreshRate(refresh);
        unsigned frames = 0;
        unsigned cycles = 0;
        unsigned ticksBetweenFrames = 0;
        for (std::uint64_t tick = 1; tick <= 10000; ++tick)
        {
            const auto budget = cpu.Budget(tick * 1000000, hz, 300000);
            // Complete a synthetic instruction that overshoots by 3 cycles.
            const auto executed = budget ? budget + 3 : 0;
            cpu.Consume(executed);
            cycles += executed;
            if (display.Due(tick * 1000000)) ++frames;
            else if (executed) ++ticksBetweenFrames;
        }
        check(std::abs(static_cast<double>(cycles) - hz * 10) <= 4,
              "1 ms CPU ticks retain fractions and repay instruction overshoot at 1x");
        check(std::abs(static_cast<double>(frames) - refresh * 10) <= 1,
              "drawing follows the display rate without slowing the CPU");
        check(ticksBetweenFrames > 8000, "emulation continues between rendered frames");
    }

    cpu.Reset(1000000000);
    check(cpu.Budget(1000000000, hz, 300000) == 0, "a zero-duration callback executes no extra cycles");
    check(cpu.Budget(9000000000, hz, 300000) == 300000, "a long stall is bounded by the catch-up limit");
    cpu.Consume(300003);
    check(cpu.Budget(9000000000, hz, 300000) == 0, "overshoot never becomes an unsigned CPU budget");
    cpu.Reset(10000000000);
    check(cpu.Budget(10000000000, hz, 300000) == 0, "resuming discards paused wall time");
    check(cpu.Budget(10001000000, hz, 300000) == 1020, "resuming starts with one normal tick");
    cpu.Reset(10001000000);
    check(cpu.Budget(10002000000, hz * 6, 300000) == 6122, "loading acceleration uses its selected clock");

    dlrl::RenderClock display;
    display.SetRefreshRate(60);
    check(display.Due(1000000), "the first frame renders immediately");
    check(!display.Due(2000000), "an intervening callback does not draw");
    check(display.Due(1000000000), "a late frame renders once");
    check(!display.Due(1000000000), "missed display deadlines do not trigger a render burst");
    display.SetRefreshRate(std::numeric_limits<double>::quiet_NaN());
    check(display.Due(1000000000) && !display.Due(1001000000), "an unknown display rate falls back safely");

    if (!failures) std::puts("Host timing passed: 1 ms CPU budgets, render cadence, pause and catch-up limits");
    return failures ? 1 : 0;
}
