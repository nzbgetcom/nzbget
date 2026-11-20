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

#ifndef NETWORK_CONNECTION_H
#define NETWORK_CONNECTION_H

#include <atomic>

#include "Network.h"

namespace network
{

/**
 * Thread-confined: Connect, Close, Send and Read must run on the connection's executor.
 * Only Cancel() may be called from any thread.
 */
class Connection final : public std::enable_shared_from_this<Connection>
{
public:
	enum class State : uint8_t
	{
		Disconnected,
		Connecting,
		Handshaking,
		Connected,
		Closing
	};

	Connection(Executor executor, Options options, std::shared_ptr<ssl::context> sslCtx);
	Connection(const Connection&) = delete;
	Connection& operator=(const Connection&) = delete;
	Connection(Connection&&) = delete;
	Connection& operator=(Connection&&) = delete;
	~Connection();

	Awaitable<Result<void>>   Connect(tcp::endpoint endpoint);
	Awaitable<Result<void>>   Close();
	Awaitable<Result<void>>   Send(ConstBuffer buffer);
	Awaitable<Result<void>>   Send(std::span<const ConstBuffer> buffer);
	Awaitable<Result<size_t>> Read(MutableBuffer buffer);

	/** Thread-safe: closes the socket on the connection's executor. */
	void Cancel() noexcept;

	/** Local address of the socket; default-constructed when not connected. */
	tcp::endpoint LocalEndpoint() noexcept;

	State GetState() const noexcept { return m_state.load(std::memory_order_acquire); }
	bool IsConnected() const noexcept { return GetState() == State::Connected; }

private:
	Socket MakeSocket(Executor executor, std::shared_ptr<ssl::context> sslCtx);
	Awaitable<Result<void>> CloseConnection();
	/** A deadline guard: the timer plus the state that rejects a stale expiry. */
	struct TimerSlot
	{
		explicit TimerSlot(Executor executor) : timer{executor} {}

		SteadyTimer timer;
		uint64_t generation{0};
		bool timedOut{false};
	};

	void ArmTimer(TimerSlot& slot, std::chrono::milliseconds timeout);
	void DisarmTimer(TimerSlot& slot) noexcept;
	tcp::socket& GetRawSocket() noexcept;
	void ForceCloseSocket() noexcept;
	void ResetSocket() noexcept;
	void CloseNow() noexcept;

	template <typename Stream>
	Awaitable<Result<void>> WriteAll(Stream& stream, std::span<const ConstBuffer> buffers);
	template <typename Stream>
	Awaitable<Result<size_t>> ReadSome(Stream& stream, MutableBuffer buffer);

	template <typename Op>
	decltype(auto) Dispatch(Op&& op);

	Executor m_executor;
	Options m_options;
	std::shared_ptr<ssl::context> m_sslCtx;
	Socket m_socket;
	std::atomic<State> m_state{State::Disconnected};
	std::atomic<bool> m_canceled{false};
	TimerSlot m_connectTimer;
	TimerSlot m_readTimer;
	TimerSlot m_writeTimer;
	bool m_readPending{false};
	bool m_writePending{false};
};

}

#endif // NETWORK_CONNECTION_H
