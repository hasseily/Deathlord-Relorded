#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dlrl
{

// Wall-clock CPU budget, independent of how often the host draws. Keep both
// fractional cycles and whole-instruction overshoot across the 1 ms ticks.
class EmulationClock
{
public:
    void Reset(std::uint64_t now)
    {
        last_ = now;
        balance_ = 0;
    }

    std::uint32_t Budget(std::uint64_t now, double hz, std::uint32_t maximum)
    {
        const auto elapsed = now >= last_ ? now - last_ : 0;
        last_ = now;
        balance_ = std::min(balance_ + elapsed * hz * 1e-9,
                            static_cast<double>(maximum));
        return balance_ >= 1 ? static_cast<std::uint32_t>(balance_) : 0;
    }

    void Consume(std::uint32_t cycles) { balance_ -= cycles; }

private:
    std::uint64_t last_ = 0;
    double balance_ = 0;
};

// Render deadlines never sleep or hold up input/emulation. Skip missed
// deadlines instead of rendering a burst after a slow frame or window move.
class RenderClock
{
public:
    void SetRefreshRate(double hz)
    {
        if (!std::isfinite(hz) || hz <= 0) hz = 60;
        interval_ = static_cast<std::uint64_t>(1e9 / std::clamp(hz, 1.0, 1000.0));
        next_ = 0;
    }

    bool Due(std::uint64_t now)
    {
        if (now < next_) return false;
        if (next_ == 0) next_ = now;
        next_ += ((now - next_) / interval_ + 1) * interval_;
        return true;
    }

private:
    std::uint64_t interval_ = 1000000000 / 60;
    std::uint64_t next_ = 0;
};

} // namespace dlrl
