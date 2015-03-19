/**
 * @file
 */

#ifndef SPEAD_SEND_STREAMBUF_H
#define SPEAD_SEND_STREAMBUF_H

#include <streambuf>
#include <functional>
#include <boost/asio.hpp>
#include "send_stream.h"

namespace spead
{
namespace send
{

class streambuf_stream : public stream<streambuf_stream>
{
private:
    friend class stream<streambuf_stream>;
    std::streambuf &streambuf;

    template<typename Handler>
    void async_send_packet(const packet &pkt, Handler &&handler)
    {
        std::size_t size = 0;
        for (const auto &buffer : pkt.buffers)
        {
            std::size_t buffer_size = boost::asio::buffer_size(buffer);
            // TODO: handle errors
            streambuf.sputn(boost::asio::buffer_cast<const char *>(buffer), buffer_size);
            size += buffer_size;
        }
        get_io_service().dispatch(std::bind(std::move(handler), boost::system::error_code(), size));
    }

public:
    streambuf_stream(
        boost::asio::io_service &io_service,
        std::streambuf &streambuf,
        const stream_config &config = stream_config());
};

} // namespace send
} // namespace spead

#endif // SPEAD_SEND_STREAMBUF_H
