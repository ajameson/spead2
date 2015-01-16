/**
 * @file
 */

#include <endian.h>
#include <algorithm>
#include <cassert>
#include <utility>
#include <cstring>
#include <string>
#include "recv_heap.h"
#include "recv_frozen_heap.h"
#include "recv_stream.h"
#include "recv_utils.h"
#include "common_logging.h"

namespace spead
{
namespace recv
{

/**
 * Read @a len bytes from @a ptr, and interpret them as a big-endian number.
 * It is not necessary for @a ptr to be aligned.
 *
 * @pre @a 0 &lt;= len &lt;= 8
 */
static inline std::uint64_t load_bytes_be(const std::uint8_t *ptr, int len)
{
    assert(0 <= len && len <= 8);
    std::uint64_t out = 0;
    std::memcpy(reinterpret_cast<char *>(&out) + 8 - len, ptr, len);
    return be64toh(out);
}

frozen_heap::frozen_heap(heap &&h)
{
    assert(h.is_contiguous());
    log_debug("freezing heap with ID %d, %d item pointers, %d bytes payload",
              h.cnt(), h.pointers.size(), h.min_length);
    /* The length of addressed items is measured from the item to the
     * address of the next item, or the end of the heap. We may receive
     * packets (and hence pointers) out-of-order, so we have to sort.
     * The mask preserves both the address and the immediate-mode flag,
     * so that all addressed items sort together.
     *
     * The sort needs to be stable, because there can be zero-length fields.
     * TODO: these can still break if they cross packet boundaries.
     */
    std::uint64_t sort_mask =
        (std::uint64_t(1) << 63)
        | ((std::uint64_t(1) << h.heap_address_bits) - 1);
    auto compare = [sort_mask](std::uint64_t a, std::uint64_t b) {
        return (a & sort_mask) < (b & sort_mask);
    };
    std::stable_sort(h.pointers.begin(), h.pointers.end(), compare);

    pointer_decoder decoder(h.heap_address_bits);
    // Determine how much memory is needed to store immediates
    std::size_t n_immediates = 0;
    for (std::size_t i = 0; i < h.pointers.size(); i++)
    {
        std::uint64_t pointer = h.pointers[i];
        if (decoder.is_immediate(pointer))
            n_immediates++;
    }
    // Allocate memory
    const std::size_t immediate_size = h.heap_address_bits / 8;
    if (n_immediates > 0)
        immediate_payload.reset(new uint8_t[immediate_size * n_immediates]);
    uint8_t *next_immediate = immediate_payload.get();
    items.reserve(h.pointers.size());

    for (std::size_t i = 0; i < h.pointers.size(); i++)
    {
        item new_item;
        std::uint64_t pointer = h.pointers[i];
        new_item.id = decoder.get_id(pointer);
        if (new_item.id == 0)
            continue; // just padding
        new_item.is_immediate = decoder.is_immediate(pointer);
        if (new_item.is_immediate)
        {
            new_item.ptr = next_immediate;
            new_item.length = immediate_size;
            std::uint64_t pointer_be = htobe64(pointer);
            std::memcpy(
                next_immediate,
                reinterpret_cast<const std::uint8_t *>(&pointer_be) + 8 - immediate_size,
                immediate_size);
            log_debug("Found new immediate item ID %d, value %d",
                      new_item.id, decoder.get_immediate(pointer));
            next_immediate += immediate_size;
        }
        else
        {
            std::int64_t start = decoder.get_address(pointer);
            std::int64_t end;
            if (i + 1 < h.pointers.size()
                && !decoder.is_immediate(h.pointers[i + 1]))
                end = decoder.get_address(h.pointers[i + 1]);
            else
                end = h.min_length;
            if (start == end)
            {
                log_debug("skipping empty item %d", new_item.id);
                continue;
            }
            new_item.ptr = h.payload.get() + start;
            new_item.length = end - start;
            log_debug("found new addressed item ID %d, offset %d, length %d",
                      new_item.id, start, end - start);
        }
        items.push_back(new_item);
    }
    heap_cnt = h.heap_cnt;
    heap_address_bits = h.heap_address_bits;
    bug_compat = h.bug_compat;
    payload = std::move(h.payload);
    // Reset h so that it still satisfies its invariants
    h = heap(0, h.bug_compat);
}

descriptor frozen_heap::to_descriptor() const
{
    descriptor out;
    for (const item &item : items)
    {
        switch (item.id)
        {
            case DESCRIPTOR_ID_ID:
                if (item.is_immediate)
                    out.id = load_bytes_be(item.ptr, item.length);
                else
                    log_info("Ignoring descriptor ID that is not an immediate");
                break;
            case DESCRIPTOR_NAME_ID:
                out.name = std::string(reinterpret_cast<const char *>(item.ptr), item.length);
                break;
            case DESCRIPTOR_DESCRIPTION_ID:
                out.description = std::string(reinterpret_cast<const char *>(item.ptr), item.length);
                break;
            case DESCRIPTOR_FORMAT_ID:
                {
                    int field_size = (bug_compat & BUG_COMPAT_DESCRIPTOR_WIDTHS) ? 4 : 9 - heap_address_bits / 8;
                    for (std::size_t i = 0; i + field_size <= item.length; i += field_size)
                    {
                        char type = item.ptr[i];
                        std::int64_t bits = load_bytes_be(item.ptr + i + 1, field_size - 1);
                        out.format.emplace_back(type, bits);
                    }
                    break;
                }
            case DESCRIPTOR_SHAPE_ID:
                {
                    int field_size = (bug_compat & BUG_COMPAT_DESCRIPTOR_WIDTHS) ? 8 : 1 + heap_address_bits / 8;
                    for (std::size_t i = 0; i + field_size <= item.length; i += field_size)
                    {
                        int mask = (bug_compat & BUG_COMPAT_SHAPE_BIT_1) ? 2 : 1;
                        bool variable = (item.ptr[i] & mask);
                        std::int64_t size = variable ? -1 : load_bytes_be(item.ptr + i + 1, field_size - 1);
                        out.shape.push_back(size);
                    }
                    break;
                }
            case DESCRIPTOR_DTYPE_ID:
                out.numpy_header = std::string(reinterpret_cast<const char *>(item.ptr), item.length);
                break;
            default:
                log_info("Unrecognised descriptor item ID %x", item.id);
                break;
        }
    }
    // DTYPE overrides format and type
    if (!out.numpy_header.empty())
    {
        out.shape.clear();
        out.format.clear();
    }
    return out;
}

namespace
{

class descriptor_stream : public stream
{
private:
    virtual void heap_ready(heap &&h) override;
public:
    using stream::stream;
    std::vector<descriptor> descriptors;
};

void descriptor_stream::heap_ready(heap &&h)
{
    if (h.is_contiguous())
    {
        frozen_heap frozen(std::move(h));
        descriptor d = frozen.to_descriptor();
        if (d.id != 0) // check that we got an ID field
            descriptors.push_back(std::move(d));
        else
            log_info("incomplete descriptor (no ID)");
    }
}

} // anonymous namespace

std::vector<descriptor> frozen_heap::get_descriptors() const
{
    descriptor_stream s(bug_compat, 1);
    for (const item &item : items)
    {
        if (item.id == DESCRIPTOR_ID)
        {
            mem_to_stream(s, item.ptr, item.length);
            s.flush();
        }
    }
    s.stop();
    return s.descriptors;
}

} // namespace recv
} // namespace spead
