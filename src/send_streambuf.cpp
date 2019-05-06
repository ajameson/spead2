/* Copyright 2015 SKA South Africa
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

#include <streambuf>
#include <spead2/send_streambuf.h>

namespace spead2
{
namespace send
{

void streambuf_stream::async_send_packets()
{
    if (next_packet(current_item))
    {
        for (const auto &buffer : current_item.pkt.buffers)
        {
            std::size_t buffer_size = boost::asio::buffer_size(buffer);
            // TODO: handle errors
            streambuf.sputn(boost::asio::buffer_cast<const char *>(buffer), buffer_size);
        }
        current_item.result = boost::system::error_code();
        get_io_service().post([this] { packets_handler(&current_item, 1); });
    }
    else
        get_io_service().post([this] { packets_handler(nullptr, 0); });
}

streambuf_stream::streambuf_stream(
    io_service_ref io_service,
    std::streambuf &streambuf,
    const stream_config &config)
    : stream_impl<streambuf_stream>(std::move(io_service), config), streambuf(streambuf)
{
}

streambuf_stream::~streambuf_stream()
{
    flush();
}

} // namespace send
} // namespace spead2
