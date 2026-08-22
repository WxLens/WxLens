#pragma once

#include <scwx/util/logger.hpp>

namespace nimbus
{
namespace log
{

/**
 * Thin wrapper over wxdata's util::Logger (docs/ROADMAP.md §3.4). Call Initialize() once at
 * startup before creating any per-subsystem logger. Subsystem names should match the directory
 * map in CLAUDE.md (e.g. "data", "render", "panes", "theme", "provider.satellite") so log lines
 * are greppable by subsystem.
 */
void Initialize();

std::shared_ptr<spdlog::logger> Create(const std::string& subsystemName);

} // namespace log
} // namespace nimbus
