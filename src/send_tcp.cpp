/*
 * TCP sender for SPEAD protocol
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

#include <stdexcept>
#include <spead2/send_tcp.h>

namespace spead2
{
namespace send
{

namespace detail
{
using boost::asio::ip::tcp;

void prepare_socket(
    tcp::socket &socket,
    const tcp::endpoint &local_endpoint,
    std::size_t buffer_size)
{
    if (!local_endpoint.address().is_unspecified())
        socket.bind(local_endpoint);
    set_socket_send_buffer_size(socket, buffer_size);
}
}

constexpr std::size_t tcp_stream::default_buffer_size;

tcp_stream::tcp_stream(
    io_service_ref io_service,
    boost::asio::ip::tcp::socket &&socket,
    const stream_config &config)
    : stream_impl(io_service, config),
      socket(std::move(socket))
{
    if (&get_io_service() != &this->socket.get_io_service())
        throw std::invalid_argument("I/O service does not match the socket's I/O service");
}

} // namespace send
} // namespace spead2
