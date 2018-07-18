#include <boost/test/unit_test.hpp>
#include <spead2/send_heap.h>

namespace spead2
{
namespace unittest
{

BOOST_AUTO_TEST_SUITE(send)
BOOST_AUTO_TEST_SUITE(heap)

BOOST_AUTO_TEST_CASE(get_item)
{
    using spead2::send::heap;
    using spead2::send::item;

    heap h;
    int value1 = 0xabc, value2 = 0xdef;
    auto handle1 = h.add_item(0x1234, value1);
    auto handle2 = h.add_item(0x2345, &value2, sizeof(value2), false);
    item &item1 = h.get_item(handle1);
    const item &item2 = const_cast<const heap &>(h).get_item(handle2);

    BOOST_CHECK_EQUAL(item1.id, 0x1234);
    BOOST_CHECK(item1.is_inline);
    BOOST_CHECK_EQUAL(item1.data.immediate, 0xabc);
    item1.data.immediate = 0xabcd;
    // Check that updates are reflected
    BOOST_CHECK_EQUAL(h.get_item(handle1).data.immediate, 0xabcd);

    BOOST_CHECK_EQUAL(item2.id, 0x2345);
    BOOST_CHECK(!item2.is_inline);
    BOOST_CHECK_EQUAL(item2.data.buffer.ptr, (const unsigned char *) &value2);
}

BOOST_AUTO_TEST_SUITE_END()  // heap
BOOST_AUTO_TEST_SUITE_END()  // send

}} // namespace spead2::unittest