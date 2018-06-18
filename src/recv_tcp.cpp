/*
 * TCP receiver for SPEAD protocol
 *
 * ICRAR - International Centre for Radio Astronomy Research
 * (c) UWA - The University of Western Australia, 2018
 * Copyright by UWA (in the framework of the ICRAR)
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file
 */

#include <spead2/common_features.h>
#include <system_error>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <boost/asio.hpp>
#include <spead2/recv_reader.h>
#include <spead2/recv_tcp.h>
#include <spead2/common_endian.h>
#include <spead2/common_logging.h>
#include <spead2/common_socket.h>

namespace spead2
{
namespace recv
{

constexpr std::size_t tcp_reader::default_buffer_size;

tcp_reader::tcp_reader(
    stream &owner,
    boost::asio::ip::tcp::acceptor &&acceptor,
    std::size_t max_size,
    std::size_t buffer_size)
    : udp_reader_base(owner), acceptor(std::move(acceptor)),
    peer(this->acceptor.get_io_service()),
    max_size(max_size),
    buffer(new std::uint8_t[max_size * pkts_per_buffer]),
    buffer_size(buffer_size),
    head(buffer.get()),
    tail(buffer.get())
{
    assert(&this->acceptor.get_io_service() == &get_io_service());
    this->acceptor.async_accept(peer,
        get_stream().get_strand().wrap(
            std::bind(&tcp_reader::accept_handler, this, std::placeholders::_1)));
}

tcp_reader::tcp_reader(
    stream &owner,
    const boost::asio::ip::tcp::endpoint &endpoint,
    std::size_t max_size,
    std::size_t buffer_size)
    : tcp_reader(
          owner,
          boost::asio::ip::tcp::acceptor(owner.get_strand().get_io_service(), endpoint),
          max_size, buffer_size)
{
}

void tcp_reader::packet_handler(
    const boost::system::error_code &error,
    std::size_t bytes_transferred)
{
    bool read_more = false;
    if (!error)
    {
        if (get_stream_base().is_stopped())
            log_info("TCP reader: discarding packet received after stream stopped");
        else
            read_more = process_buffer(bytes_transferred);
    }
    else if (error != boost::asio::error::operation_aborted && error != boost::asio::error::eof)
        log_warning("Error in TCP receiver: %1%", error.message());

    if (read_more)
        enqueue_receive();
    else
    {
        peer.close();
        this->stopped();
    }
}

bool tcp_reader::parse_packet()
{
    assert(pkt_size > 0);
    assert(tail - head >= pkt_size);
    // Modify private fields first, in case process_one_packet throws
    auto head = this->head;
    auto pkt_size = this->pkt_size;
    this->head += pkt_size;
    this->pkt_size = 0;
    return process_one_packet(head, pkt_size, max_size);
}

bool tcp_reader::process_buffer(const std::size_t bytes_recv)
{
    tail += bytes_recv;
    while (tail > head)
    {
        if (parse_packet_size())
            return true;
        if (skip_bytes())
            return true;
        if (pkt_size == 0)
            continue;
        if (std::size_t(tail - head) < pkt_size)
            return true;
        if (parse_packet())
            return false;
    }
    return true;
}

bool tcp_reader::parse_packet_size()
{
    if (pkt_size > 0)
        return false;
    if (tail - head < 8)
        return true;
    pkt_size = load_be<std::uint64_t>(head);
    head += 8;
    if (pkt_size > max_size)
    {
        log_info("dropping packet due to truncation");
        to_skip = pkt_size;
    }
    return false;
}

bool tcp_reader::skip_bytes()
{
    if (to_skip == 0)
        return false;
    if (tail == head)
        return true;
    auto diff = std::min(std::size_t(tail - head), to_skip);
    head += diff;
    to_skip -= diff;
    if (to_skip == 0)
        pkt_size = 0;
    return to_skip > 0;
}

void tcp_reader::accept_handler(const boost::system::error_code &error)
{
    acceptor.close();
    if (!error)
    {
        set_socket_recv_buffer_size(peer, buffer_size);
        enqueue_receive();
    }
    else
    {
        if (error != boost::asio::error::operation_aborted)
            log_warning("Error in TCP accept: %1%", error.message());
        stopped();
    }
}

void tcp_reader::enqueue_receive()
{
    using namespace std::placeholders;

    auto buf = buffer.get();
    auto bufsize = max_size * pkts_per_buffer;
    assert(tail >= head);
    assert(tail >= buf);

    // Make room for the incoming data
    if ((tail - buf) > bufsize / 2)
    {
        auto len = tail - head;
        std::memmove(buf, head, std::size_t(len));
        head = buf;
        tail = head + len;
    }

    peer.async_receive(
        boost::asio::buffer(tail, bufsize - (tail - buf)),
        get_stream().get_strand().wrap(
            std::bind(&tcp_reader::packet_handler,
                this, _1, _2)));
}

void tcp_reader::stop()
{
    /* asio guarantees that closing a socket will cancel any pending
     * operations on it.
     * Don't put any logging here: it could be running in a shutdown
     * path where it is no longer safe to do so.
     */
    if (peer.is_open())
        peer.close();
    if (acceptor.is_open())
        acceptor.close();
}

bool tcp_reader::lossy() const
{
    return false;
}

} // namespace recv
} // namespace spead2
