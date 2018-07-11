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

#include <cstddef>
#include <utility>
#include <boost/asio.hpp>
#include <spead2/send_tcp.h>
#include <spead2/common_defines.h>
#include <spead2/common_socket.h>

namespace spead2
{
namespace send
{

constexpr std::size_t tcp_stream::default_buffer_size;

tcp_stream::tcp_stream(
    io_service_ref io_service,
    const boost::asio::ip::tcp::endpoint &endpoint,
    const boost::asio::ip::tcp::endpoint &local_endpoint,
    const stream_config &config,
    std::size_t buffer_size)
    : tcp_stream(std::move(io_service),
                 boost::asio::ip::tcp::socket(*io_service, endpoint.protocol()),
                 endpoint, local_endpoint, config, buffer_size)
{
}

tcp_stream::tcp_stream(
    boost::asio::ip::tcp::socket &&socket,
    const boost::asio::ip::tcp::endpoint &endpoint,
    const boost::asio::ip::tcp::endpoint &local_endpoint,
    const stream_config &config,
    std::size_t buffer_size)
    : tcp_stream(socket.get_io_service(), std::move(socket), endpoint, local_endpoint, config, buffer_size)
{
}

tcp_stream::tcp_stream(
    io_service_ref io_service,
    boost::asio::ip::tcp::socket &&socket,
    const boost::asio::ip::tcp::endpoint &endpoint,
    const boost::asio::ip::tcp::endpoint &local_endpoint,
    const stream_config &config,
    std::size_t buffer_size)
    : stream_impl<tcp_stream>(std::move(io_service), config),
    socket(std::move(socket)), endpoint(endpoint)
{
    if (&get_io_service() != &this->socket.get_io_service())
        throw std::invalid_argument("I/O service does not match the socket's I/O service");
    if (!socket.is_open())
    {
        if (!local_endpoint.address().is_unspecified())
            this->socket.bind(local_endpoint);
        this->socket.connect(endpoint);
    }
    set_socket_send_buffer_size(this->socket, buffer_size);
}

} // namespace send
} // namespace spead2
