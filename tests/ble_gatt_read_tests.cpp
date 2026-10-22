#include "ble_gatt_read.hpp"
#include <cassert>
#include <cstdio>

using ams::ble::gatt::ReadState;
using ams::ble::gatt::ReadDeadline;

int main() {
    constexpr std::int64_t Started = 123'456;
    constexpr ReadDeadline deadline(Started);
    static_assert(!deadline.Expired(Started + 10'000));
    static_assert(!deadline.Expired(Started + 30'000));
    static_assert(!deadline.Expired(Started + 34'999));
    static_assert(deadline.Expired(Started + 35'000));
    static_assert(deadline.Expired(Started + 35'001));

    // Actual portable deadline/state used by Read: a value at31s can finish the SAME pending request.
    ReadState delayed;
    assert(delayed.Begin(6, 0x2a4b));
    assert(!deadline.Expired(Started + 31'000));
    assert(!delayed.Begin(6, 0x2a4b));
    assert(delayed.Accept(6, 0x2a4b, 0, 118, 512));
    assert(delayed.answered && !delayed.failed && delayed.size == 118);
    delayed.Finish();

    ReadState read;
    assert(!read.Accept(6, 0x2a4b, 0, 118, 512));
    assert(read.Begin(6, 0x2a4b));
    // Do not overwrite an outstanding read, even with the next characteristic on the same link.
    assert(!read.Begin(6, 0x2a4a));
    assert(!read.Accept(7, 0x2a4b, 0, 118, 512));
    assert(!read.Accept(6, 0x2a4a, 0, 4, 512));
    assert(!read.answered);
    assert(read.Accept(6, 0x2a4b, 0, 118, 512));
    assert(read.answered && !read.failed && read.size == 118);
    // The first result wins if a stack delivers the same completion on both streams.
    assert(!read.Accept(6, 0x2a4b, 55, 0, 512));
    assert(!read.failed && read.size == 118);
    assert(!read.Begin(6, 0x2a4a));
    read.Finish();

    assert(read.Begin(6, 0x2a4a));
    assert(!read.answered && !read.failed && read.size == 0);
    assert(read.Accept(6, 0x2a4a, 5, 4, 512));
    assert(read.failed && read.size == 0);
    read.Finish();

    assert(read.Begin(6, 0x2a4b));
    assert(read.Accept(6, 0x2a4b, 0, 513, 512));
    assert(read.failed && read.size == 0); // never copy an oversized value
    read.Finish();

    assert(read.Begin(6, 0x2a4b));
    assert(read.Accept(6, 0x2a4b, 0, 0, 512));
    assert(read.failed && read.size == 0); // an empty map is not authentication success
    read.Finish();

    assert(read.Begin(6, 0x2a4b));
    read.Finish(); // timeout/disconnect: a late completion may not resurrect this request
    assert(!read.Accept(6, 0x2a4b, 0, 118, 512));
    assert(!read.pending && !read.answered);

    std::puts("BLE GATT read tests passed (35s deadline, late single-request completion, correlation, errors, bounds, timeout)");
}
