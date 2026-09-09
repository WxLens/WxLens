#pragma once

class QQuickWindow;

namespace wxlens
{
namespace log
{
// Opt-in diagnostic capture, enabled by WXLENS_FRAME_TIMINGS=<new CSV path>.
// Measures Qt render-thread wall time and swap intervals, not GPU execution or
// display presentation. Idle swap gaps must not be interpreted as slow frames.
void AttachFrameTiming(QQuickWindow* window);
} // namespace log
} // namespace wxlens
