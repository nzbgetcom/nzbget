/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef DAEMON_ASYNC_COMPONENT_H
#define DAEMON_ASYNC_COMPONENT_H

#include <chrono>
#include <concepts>
#include <cstddef>
#include <string_view>

namespace async
{

using Clock = std::chrono::steady_clock;

/// One slot per component; Runtime stops them in reverse order.
enum class ComponentId : std::size_t
{
	Network,
	Count
};

/// Slot and log name of a component type; specialise next to each component.
template<class T>
struct ComponentTraits;

/// Subsystem owned by Runtime: the constructor acquires resources (may throw), Stop() releases them.
class Component
{
public:
	Component() = default;
	virtual ~Component() = default;

	Component(const Component&) = delete;
	Component& operator=(const Component&) = delete;
	Component(Component&&) = delete;
	Component& operator=(Component&&) = delete;

	/// Releases resources at once.
	virtual void Stop() noexcept = 0;

	/// Graceful stop that must finish by @p deadline; defaults to Stop().
	virtual void Stop(Clock::time_point /* deadline */) noexcept { Stop(); }
};

/// A Component subclass with ComponentTraits<T>::Id and ::Name.
template<class T>
concept ComponentType = std::derived_from<T, Component> && requires {
	{ ComponentTraits<T>::Id } -> std::convertible_to<ComponentId>;
	{ ComponentTraits<T>::Name } -> std::convertible_to<std::string_view>;
};

} // namespace async

#endif // DAEMON_ASYNC_COMPONENT_H
