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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CORE_COMPONENT_H
#define CORE_COMPONENT_H

#include <memory>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/detached.hpp>
#include "Types.h"

namespace Core
{

class Component : public std::enable_shared_from_this<Component>
{
public:
	Component() = default;
	Component(const Component&) = delete;
	Component& operator=(const Component&) = delete;
	Component(Component&&) = delete;
	Component& operator=(Component&&) = delete;

	virtual ~Component() = default;
	virtual const char* Name() const noexcept = 0;
	virtual void Start() = 0;
	virtual void Stop() noexcept = 0;
};

}

#endif
