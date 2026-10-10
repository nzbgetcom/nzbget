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

#ifndef CORE_SIGNAL_HANDLER_H
#define CORE_SIGNAL_HANDLER_H

#include <functional>
#include "Component.h"
#include "Types.h"

namespace Core
{

class SignalHandler final : public Component
{
public:
	using Callback = std::function<void()>;

	SignalHandler(Executor executor, Callback onShutdown, Callback onReload);
	~SignalHandler() override;

	const char* Name() const noexcept override { return "SignalHandler"; }

	void Start() override;
	void Stop() noexcept override;

private:
	Awaitable<void> SignalLoop(std::shared_ptr<Component> self);

	asio::signal_set m_signals;
	Callback m_onShutdown;
	Callback m_onReload;
};

}

#endif
