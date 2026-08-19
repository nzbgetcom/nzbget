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

#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <cstddef>
#include <boost/asio.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/error_code.hpp>

namespace Core
{

namespace asio = boost::asio;
using ErrorCode = boost::system::error_code;
using Executor = asio::any_io_executor;
using ExecutorGuard = asio::executor_work_guard<Executor>;
using ConstBuffer = asio::const_buffer;
using MutableBuffer = asio::mutable_buffer;
using SteadyTimer = asio::steady_timer;
template <typename T, typename Exec = Executor>
using Awaitable = asio::awaitable<T, Exec>;

}

#endif
