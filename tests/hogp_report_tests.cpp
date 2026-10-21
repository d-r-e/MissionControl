#include "hogp_report_router.hpp"
#include "hogp_session.hpp"
#include "stadia_report.hpp"
#include "stadia_gatt_input.hpp"
#include "stadia_virtual_input.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace ams::bluetooth::hogp;
using ams::controller::StadiaInputReport;

std::vector<std::uint8_t> Unhex(const std::string &hex) {
    assert(hex.size() % 2 == 0);
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i < hex.size(); i += 2)
        bytes.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    return bytes;
}

void TestLayouts() {
    ReportLayout layout;
    const auto generic = Unhex("05010905a10185077501950d8102950381018509750895048102c0");
    assert(layout.Parse(generic.data(), generic.size()) == LayoutResult::Ok);
    assert(layout.UsesReportIds());
    assert(layout.PayloadSize(7, ReportType::Input) == 2); // Includes constant padding.
    assert(layout.PayloadSize(9, ReportType::Input) == 4);
    assert(layout.PayloadSize(7, ReportType::Output) == 0);
    ReportRouter multi;
    assert(multi.Reset(1, 5));
    const Endpoint first{1, 5, 10, 11}, second{1, 5, 10, 12};
    const std::uint8_t ref7[] = {7, 1}, ref9[] = {9, 1};
    assert(multi.BindInput(first, ref7, 2, 0x10, layout) == RouteResult::Ok);
    assert(multi.BindInput(second, ref9, 2, 0x10, layout) == RouteResult::Ok);
    assert(multi.SetSubscribed(first, true) == RouteResult::Ok);
    assert(multi.SetSubscribed(second, true) == RouteResult::Ok);
    const std::uint8_t value7[] = {3, 0}, value9[] = {7, 0, 0, 0};
    InputReportView routed;
    assert(multi.Route(first, value7, sizeof(value7), &routed) == RouteResult::Ok);
    assert(routed.id == 7 && routed.data[0] == 3);
    assert(multi.Route(second, value9, sizeof(value9), &routed) == RouteResult::Ok);
    assert(routed.id == 9 && routed.data[0] == 7);
    const auto push_pop = Unhex("a101850175089501a4850295028102b48102c0");
    assert(layout.Parse(push_pop.data(), push_pop.size()) == LayoutResult::Ok);
    assert(layout.PayloadSize(1, ReportType::Input) == 1);
    assert(layout.PayloadSize(2, ReportType::Input) == 2);
    const auto implicit = Unhex("a1017508950281029102b102c0");
    assert(layout.Parse(implicit.data(), implicit.size()) == LayoutResult::Ok);
    assert(!layout.UsesReportIds());
    assert(layout.PayloadSize(0, ReportType::Input) == 2);
    assert(layout.PayloadSize(0, ReportType::Output) == 2);
    assert(layout.PayloadSize(0, ReportType::Feature) == 2);
    assert(layout.PayloadSize(0, static_cast<ReportType>(0)) == 0);
    for (const auto &hex : {
        "", "75", "fe", "b4", "c0", "a101", "a1018500c0", "a1018501750895018102",
        "a1017508950181028502c0", "a101a48501750895018102b4750895018102c0",
        "a101750095018102c0", "a101750895008102c0", "a1017508950180c0",
        "a101750895018102a4c0", "a101750895018102a4a4a4a4a4a4a4a4a4c0",
        "a10177ffffffff95018102c0", "a101750897ffffffff8102c0", "a10175089601028102c0",
        "a1017508960002810295018102c0", "a101c4c0", "a1010cc0"
    }) {
        const auto bad = Unhex(hex);
        assert(layout.Parse(bad.data(), bad.size()) != LayoutResult::Ok);
        assert(!layout.IsValid());
        assert(layout.PayloadSize(1, ReportType::Input) == 0);
    }
    assert(layout.Parse(nullptr, 1) == LayoutResult::Malformed);
    std::vector<std::uint8_t> huge(ReportLayout::MaxDescriptorSize + 1);
    assert(layout.Parse(huge.data(), huge.size()) == LayoutResult::LimitExceeded);
    const auto max_value = Unhex("a10175089600028102c0");
    assert(layout.Parse(max_value.data(), max_value.size()) == LayoutResult::Ok);
    assert(layout.PayloadSize(0, ReportType::Input) == 512);

    // Deterministic malformed-input coverage: no unbounded descriptor reads.
    std::uint32_t seed = 0x18122908;
    for (unsigned trial = 0; trial < 5000; ++trial) {
        std::vector<std::uint8_t> data(trial % 193);
        for (auto &byte : data) {
            seed = seed * 1664525u + 1013904223u;
            byte = static_cast<std::uint8_t>(seed >> 24);
        }
        layout.Parse(data.data(), data.size());
        for (unsigned id = 0; id < 256; ++id)
            assert(layout.PayloadSize(id, ReportType::Input) <= ReportLayout::MaxPayloadSize);
    }
}

void TestRouter() {
    ReportLayout layout;
    const auto descriptor = Unhex("a1018501750895018102c0");
    assert(layout.Parse(descriptor.data(), descriptor.size()) == LayoutResult::Ok);
    const std::uint8_t input[] = {1, 1}, output[] = {1, 2}, feature[] = {1, 3};
    const std::uint8_t invalid[] = {1, 0}, missing[] = {2, 1};
    const std::uint8_t data[] = {3}; // First value is not a report ID.
    ReportReference reference{};
    assert(!ReportReference::Parse(input, 1, &reference));
    assert(!ReportReference::Parse(input, 3, &reference));
    assert(!ReportReference::Parse(nullptr, 2, &reference));
    assert(!ReportReference::Parse(input, 2, nullptr));
    ReportRouter router;
    Endpoint endpoint{1, 8, 1, 2};
    InputReportView view;
    assert(router.BindInput(endpoint, input, 2, 0x10, layout) == RouteResult::WrongSession);
    assert(!router.Reset(0, 8));
    assert(!router.Reset(1, UINT32_MAX));
    assert(router.Reset(1, 8));
    assert(router.BindInput(endpoint, output, 2, 0x10, layout) == RouteResult::NotInput);
    assert(router.BindInput(endpoint, feature, 2, 0x10, layout) == RouteResult::NotInput);
    assert(router.BindInput(endpoint, invalid, 2, 0x10, layout) == RouteResult::InvalidReference);
    assert(router.BindInput(endpoint, missing, 2, 0x10, layout) == RouteResult::MissingLayoutReport);
    assert(router.BindInput(endpoint, input, 2, 0x02, layout) == RouteResult::UnsupportedProperties);
    assert(router.BindInput({1, 8, 0, 2}, input, 2, 0x10, layout) == RouteResult::InvalidEndpoint);
    assert(router.BindInput(endpoint, input, 2, 0x10, layout) == RouteResult::Ok);
    assert(router.BindInput(endpoint, input, 2, 0x10, layout) == RouteResult::DuplicateCharacteristic);
    assert(router.BindInput({1, 8, 1, 3}, input, 2, 0x10, layout) == RouteResult::DuplicateReport);
    assert(router.Route(endpoint, data, 1, &view) == RouteResult::NotSubscribed);
    assert(router.SetSubscribed(endpoint, true) == RouteResult::Ok);
    assert(router.Route(endpoint, data, 1, &view) == RouteResult::Ok);
    assert(view.id == 1 && view.data == data && view.size == 1);
    assert(router.Route(endpoint, data, 0, &view) == RouteResult::InvalidPayload);
    assert(view.data == nullptr && view.size == 0);
    assert(router.Route(endpoint, nullptr, 1, &view) == RouteResult::InvalidPayload);
    assert(router.Route(endpoint, data, 1, nullptr) == RouteResult::InvalidPayload);
    assert(router.Route({1, 9, 1, 2}, data, 1, &view) == RouteResult::WrongSession);
    assert(router.Route({1, 8, 1, 3}, data, 1, &view) == RouteResult::UnknownCharacteristic);
    assert(router.SetSubscribed({1, 8, 1, 3}, true) == RouteResult::UnknownCharacteristic);
    assert(router.SetSubscribed(endpoint, false) == RouteResult::Ok);
    assert(router.Route(endpoint, data, 1, &view) == RouteResult::NotSubscribed);
    assert(router.Reset(2, 8)); // Native connection number reused, old generation retired.
    assert(router.Route(endpoint, data, 1, &view) == RouteResult::WrongSession);
    endpoint.generation = 2;
    assert(router.Route(endpoint, data, 1, &view) == RouteResult::UnknownCharacteristic);
    assert(!router.Reset(2, 8)); // Cannot recycle the same lease.
    assert(router.BindInput(endpoint, input, 2, 0x10, layout) == RouteResult::WrongSession);
    assert(router.Reset(3, 8));
    for (unsigned i = 0; i < ReportRouter::MaxBindings; ++i) {
        const Endpoint ep{3, 8, static_cast<std::uint16_t>(1 + i * 3),
                                  static_cast<std::uint16_t>(2 + i * 3)};
        assert(router.BindInput(ep, input, 2, 0x10, layout) == RouteResult::Ok);
    }
    assert(router.BindInput({3, 8, 100, 101}, input, 2, 0x10, layout) == RouteResult::CapacityExceeded);
    router.Close();
    assert(router.SetSubscribed({3, 8, 1, 2}, true) == RouteResult::WrongSession);

    // ID zero is valid for maps WITHOUT Report ID items; 2908 still has two bytes.
    const auto implicit = Unhex("a101750895018102c0");
    assert(layout.Parse(implicit.data(), implicit.size()) == LayoutResult::Ok);
    const std::uint8_t zero[] = {0, 1};
    assert(router.Reset(4, 8));
    assert(router.BindInput({4, 8, 1, 2}, zero, 2, 0x10, layout) == RouteResult::Ok);
    assert(router.SetSubscribed({4, 8, 1, 2}, true) == RouteResult::Ok);
    assert(router.Route({4, 8, 1, 2}, data, 1, &view) == RouteResult::Ok);
    assert(view.id == 0 && view.data[0] == 3);
}

void TestStadia(const char *map_path, const char *reports_path) {
    std::ifstream map_file(map_path);
    std::string hex;
    assert(map_file >> hex);
    const auto descriptor = Unhex(hex);
    ReportLayout layout;
    assert(layout.Parse(descriptor.data(), descriptor.size()) == LayoutResult::Ok);
    for (std::size_t i = 0; i < descriptor.size(); ++i) {
        ReportLayout truncated;
        assert(truncated.Parse(descriptor.data(), i) != LayoutResult::Ok);
    }
    assert(layout.PayloadSize(3, ReportType::Input) == StadiaInputReport::PayloadSize);
    assert(layout.PayloadSize(5, ReportType::Output) == 4);
    assert(layout.PayloadSize(5, ReportType::Input) == 0);

    // The declared reports, enumerable rather than only queryable by a guessed id: this is what a pad is
    // recognised by once it is connected, so it has to be readable out of the device's own report map.
    std::uint8_t input_ids[8] = {}, output_ids[8] = {};
    assert(layout.ListReports(ReportType::Input, input_ids, 8) == 1);
    assert(input_ids[0] == StadiaInputReport::ReportId);
    assert(layout.ListReports(ReportType::Output, output_ids, 8) == 1);
    assert(output_ids[0] == 0x05);
    assert(layout.ListReports(ReportType::Feature, input_ids, 8) == 0);
    Session session;
    HidCatalog catalog{};
    catalog.service = 1; catalog.report_map = 2; catalog.count = 1;
    catalog.reports[0] = {3, 4, 5, 0x12};
    assert(session.Begin(1, 4, 0));
    assert(session.CompleteCatalog(session.TakeCommand(0), true, catalog, 1));
    assert(session.CompleteRead(session.TakeCommand(1), true, descriptor.data(), descriptor.size(), 2));
    const std::uint8_t model_reference[] = {3, 1}; // Modelled, not live evidence.
    assert(session.CompleteRead(session.TakeCommand(2), true, model_reference, 2, 3));
    assert(session.CompleteSubscription(session.TakeCommand(3), true, 4));
    assert(session.State() == SessionState::Ready);
    ReportRouter router;
    assert(router.Reset(1, 4));
    const Endpoint endpoint{1, 4, 1, 3};
    ams::controller::StadiaGattInput controller_input;
    assert(controller_input.Bind(endpoint));
    const std::uint8_t reference[] = {3, 1}; // Modelled 2908, NOT captured from Switch.
    assert(router.BindInput(endpoint, reference, 2, 0x12, layout) == RouteResult::Ok);
    assert(router.SetSubscribed(endpoint, true) == RouteResult::Ok);
    std::ifstream reports(reports_path);
    assert(reports.is_open());
    std::string line;
    unsigned count = 0;
    while (std::getline(reports, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream row(line);
        std::string name, value;
        assert(row >> name >> value);
        const auto prefixed = Unhex(value);
        InputReportView view;
        assert(router.Route(endpoint, prefixed.data(), prefixed.size(), &view) == RouteResult::InvalidPayload);
        assert(router.Route(endpoint, prefixed.data() + 1, prefixed.size() - 1, &view) == RouteResult::Ok);
        assert(session.Route(endpoint, prefixed.data() + 1, prefixed.size() - 1, &view) == RouteResult::Ok);
        StadiaInputReport expected{}, actual{};
        assert(StadiaInputReport::Parse(prefixed.data(), prefixed.size(), &expected));
        assert(controller_input.Consume(endpoint, view) == ams::controller::StadiaGattResult::Applied);
        actual = controller_input.State();
        assert(actual.dpad == expected.dpad && actual.buttons == expected.buttons);
        assert(actual.left_x == expected.left_x && actual.left_y == expected.left_y);
        assert(actual.right_x == expected.right_x && actual.right_y == expected.right_y);
        assert(actual.left_trigger == expected.left_trigger && actual.right_trigger == expected.right_trigger);
        assert(actual.consumer_buttons == expected.consumer_buttons);
        ++count;
    }
    assert(count >= 30);
    // Ambiguous first payload byte 03 MUST be retained as dpad, never stripped.
    const std::uint8_t diagonal[10] = {3, 0, 0, 128, 128, 128, 128, 0, 0, 0};
    InputReportView view;
    assert(router.Route(endpoint, diagonal, sizeof(diagonal), &view) == RouteResult::Ok);
    StadiaInputReport report{};
    assert(StadiaInputReport::ParseGatt(view.id, view.data, view.size, &report));
    assert(report.dpad == 3);
    assert(controller_input.Retire(endpoint));
    assert(controller_input.State().buttons == 0 && controller_input.State().dpad == 8);
    assert(controller_input.State().left_x == 128 && controller_input.State().right_y == 128);
    std::cout << "HOGP: generic layout/routing and " << count << " Stadia fixtures passed\n";
}

void TestVirtualPublication() {
    using ams::controller::StadiaVirtualOutputAllowed;
    using ams::controller::PublishStadiaVirtualInput;
    std::array<std::uint8_t, 512> packet{};
    assert(!StadiaVirtualOutputAllowed(nullptr, 49));
    packet[0] = 1; packet[10] = 3; packet[11] = 0x30;
    assert(StadiaVirtualOutputAllowed(packet.data(), 49));
    for (unsigned format = 0; format < 256; ++format) {
        packet[11] = format;
        assert(StadiaVirtualOutputAllowed(packet.data(), 49) == (format == 0x30));
    }
    packet[10] = 2; // GetDeviceInfo remains supported.
    assert(StadiaVirtualOutputAllowed(packet.data(), 49));
    for (unsigned size = 0; size < 49; ++size) assert(!StadiaVirtualOutputAllowed(packet.data(), size));
    packet[0] = 0x10;
    assert(StadiaVirtualOutputAllowed(packet.data(), 10));
    assert(!StadiaVirtualOutputAllowed(packet.data(), 9));
    assert(!StadiaVirtualOutputAllowed(packet.data(), 513));
    for (auto id : {0x03, 0x11, 0x12, 0x80, 0xff}) {
        packet[0] = id; assert(!StadiaVirtualOutputAllowed(packet.data(), 49));
    }
    // Both input and command replies use ONE remap-then-publish seam. These
    // callbacks are test doubles, not the SDK controller or HID shared memory.
    for (auto id : {0x30, 0x21}) {
        packet.fill(0); packet[0] = id;
        unsigned stage = 0;
        const auto remap = [&](std::uint8_t *copy, std::size_t size) {
            assert(stage++ == 0 && copy != packet.data() && size == 49);
            copy[3] = 0x10; // Synthetic remap marker, not a button-map claim.
        };
        const auto emit = [&](const std::uint8_t *copy, std::size_t size) {
            assert(stage++ == 1 && size == 49 && copy[3] == 0x10 && copy[0] == id);
        };
        assert(PublishStadiaVirtualInput(packet.data(), 49, remap, emit));
        assert(stage == 2 && packet[3] == 0);
        for (unsigned size = 0; size <= 512; ++size) {
            if (size == 49) continue;
            assert(!PublishStadiaVirtualInput(packet.data(), size, remap, emit));
        }
        packet[0] = 0x3f;
        assert(!PublishStadiaVirtualInput(packet.data(), 49, remap, emit));
        assert(!PublishStadiaVirtualInput(nullptr, 49, remap, emit));
        assert(stage == 2);
    }
}

void TestGattInputLifecycle() {
    using ams::controller::StadiaGattInput;
    using ams::controller::StadiaGattResult;
    StadiaGattInput input;
    const Endpoint ep{1, 5, 10, 12};
    const std::uint8_t held[] = {3, 0, 0x40, 1, 255, 128, 128, 255, 0, 7};
    const InputReportView view{3, held, sizeof(held)};
    assert(!input.Active() && !input.HasBinding());
    assert(input.Consume(ep, view) == StadiaGattResult::Ignored);
    assert(!input.Bind({0, 5, 10, 12}));
    assert(!input.Bind({1, UINT32_MAX, 10, 12}));
    assert(!input.Bind({1, 5, 0, 12}));
    assert(!input.Bind({1, 5, 10, 10}));
    assert(input.Bind(ep) && input.Active() && input.HasBinding());
    assert(input.Consume(ep, view) == StadiaGattResult::Applied);
    assert(input.State().IsPressed(StadiaInputReport::A));
    assert(input.State().dpad == 3 && input.State().left_trigger == 255);
    assert(!input.Bind({2, 5, 10, 12})); // Active lease cannot be replaced.
    for (unsigned field = 0; field < 4; ++field) {
        auto wrong = ep;
        if (field == 0) ++wrong.generation;
        if (field == 1) ++wrong.connection;
        if (field == 2) ++wrong.service;
        if (field == 3) ++wrong.characteristic;
        assert(input.Consume(wrong, view) == StadiaGattResult::Ignored);
        assert(!input.Retire(wrong));
        assert(input.State().IsPressed(StadiaInputReport::A));
    }
    assert(input.Retire(ep) && !input.Retire(ep));
    assert(!input.Active() && input.HasBinding());
    assert(input.State().dpad == 8 && input.State().buttons == 0 && input.State().left_trigger == 0);
    assert(input.State().consumer_buttons == 0 && input.State().left_x == 128);
    assert(!input.Bind(ep));
    const Endpoint newer{2, 5, 10, 12};
    assert(input.Bind(newer));
    assert(input.Consume(ep, view) == StadiaGattResult::Ignored);
    assert(input.Consume(newer, view) == StadiaGattResult::Applied);
    assert(input.Consume(newer, {3, held, 11}) == StadiaGattResult::Retired);
    assert(input.State().buttons == 0 && !input.Active());
    assert(!input.Retire(newer)); // Invalid owned payload already neutralized once.
    assert(input.Bind({3, 5, 10, 12}));
    assert(input.Consume({3, 5, 10, 12}, {4, held, 10}) == StadiaGattResult::Retired);
    assert(input.Bind({4, 5, 10, 12}));
    assert(input.Consume({4, 5, 10, 12}, {3, nullptr, 10}) == StadiaGattResult::Retired);
}

void TestSessions() {
    const auto map = Unhex("a10185017508950181028502750895019102c0");
    const std::uint8_t input[] = {1, 1}, output[] = {2, 2}, value[] = {3};
    HidCatalog catalog{};
    catalog.service = 1; catalog.report_map = 2; catalog.count = 2;
    catalog.reports[0] = {3, 4, 5, 0x12};
    catalog.reports[1] = {6, 7, 0, 0x08};
    Session session;
    InputReportView view;
    assert(!session.Begin(0, 1, 0));
    assert(!session.Begin(1, UINT32_MAX, 0));
    assert(!session.Begin(1, 1, UINT64_MAX));
    assert(session.Begin(1, 8, 0));
    auto discover = session.TakeCommand(0);
    assert(discover.operation == SetupOperation::DiscoverCatalog);
    assert(session.TakeCommand(0).operation == SetupOperation::None);
    auto wrong = discover; ++wrong.ticket;
    assert(!session.CompleteCatalog(wrong, true, catalog, 1));
    assert(session.CompleteCatalog(discover, true, catalog, 1));
    assert(!session.CompleteCatalog(discover, true, catalog, 1));
    auto read = session.TakeCommand(1);
    assert(read.operation == SetupOperation::ReadMap && read.attribute == 2);
    assert(session.CompleteRead(read, true, map.data(), map.size(), 2));
    auto reference = session.TakeCommand(2);
    assert(reference.operation == SetupOperation::ReadReference && reference.attribute == 4);
    assert(session.CompleteRead(reference, true, input, 2, 3));
    auto subscribe = session.TakeCommand(3);
    assert(subscribe.operation == SetupOperation::Subscribe && subscribe.attribute == 5);
    assert(session.Route(subscribe.endpoint, value, 1, &view) == RouteResult::NotSubscribed);
    wrong = subscribe; ++wrong.endpoint.characteristic;
    assert(!session.CompleteSubscription(wrong, true, 4));
    assert(session.CompleteSubscription(subscribe, true, 4));
    reference = session.TakeCommand(4);
    assert(reference.attribute == 7);
    assert(session.CompleteRead(reference, true, output, 2, 5));
    assert(session.State() == SessionState::Ready && session.InputCount() == 1);
    assert(session.TakeCommand(5).operation == SetupOperation::None);
    session.Tick(100000); // Setup timeout is not an idle-input timeout.
    assert(session.State() == SessionState::Ready);
    assert(session.Route(subscribe.endpoint, value, 1, &view) == RouteResult::Ok);
    assert(view.id == 1 && view.data[0] == 3);
    auto stale = subscribe.endpoint; ++stale.generation;
    assert(session.Route(stale, value, 1, &view) == RouteResult::WrongSession);
    assert(view.data == nullptr && session.State() == SessionState::Ready);
    assert(session.Route(subscribe.endpoint, value, 0, &view) == RouteResult::InvalidPayload);
    assert(view.data == nullptr && session.State() == SessionState::Retired);
    assert(!session.Begin(1, 8, 0));
    assert(session.Begin(2, 8, 0));
    assert(!session.CompleteSubscription(subscribe, true, 1));
    session.Tick(5000);
    assert(session.Failure() == SessionFailure::Timeout);
    assert(!session.CompleteCatalog(discover, true, catalog, 5001));

    auto start = [&](Session &s, const HidCatalog &c) {
        assert(s.Begin(1, 8, 0));
        const auto command = s.TakeCommand(0);
        assert(s.CompleteCatalog(command, true, c, 1));
    };
    for (unsigned i = 0; i < 7; ++i) {
        Session s;
        auto c = catalog;
        switch (i) {
            case 0: c.count = 0; break;
            case 1: c.count = c.reports.size() + 1; break;
            case 2: c.service = 0; break;
            case 3: c.report_map = 1; break;
            case 4: c.reports[1].reference = 4; break;
            case 5: c.reports[0].reference = 2; break;
            case 6: c.reports[0].cccd = 4; break;
        }
        start(s, c);
        assert(s.Failure() == SessionFailure::InvalidCatalog);
    }
    for (unsigned scenario = 0; scenario < 9; ++scenario) {
        Session s;
        auto c = catalog;
        if (scenario == 3) c.reports[0].cccd = 0;
        if (scenario == 4) c.reports[0].properties = 2;
        if (scenario == 8) c.count = 1;
        start(s, c);
        auto command = s.TakeCommand(1);
        if (scenario == 0) {
            assert(s.CompleteRead(command, false, nullptr, 0, 2));
            assert(s.Failure() == SessionFailure::OperationFailed); continue;
        }
        if (scenario == 1) {
            assert(s.CompleteRead(command, true, nullptr, 0, 2));
            assert(s.Failure() == SessionFailure::InvalidMap); continue;
        }
        assert(s.CompleteRead(command, true, map.data(), map.size(), 2));
        command = s.TakeCommand(2);
        if (scenario == 2) {
            assert(s.CompleteRead(command, true, input, 1, 3));
            assert(s.Failure() == SessionFailure::InvalidReference); continue;
        }
        assert(s.CompleteRead(command, true, scenario == 8 ? output : input, 2, 3));
        if (scenario == 3 || scenario == 4) {
            assert(s.Failure() == SessionFailure::BindingFailed); continue;
        }
        if (scenario == 8) { assert(s.Failure() == SessionFailure::NoInputs); continue; }
        command = s.TakeCommand(3);
        if (scenario == 5) {
            assert(s.CompleteSubscription(command, false, 4));
            assert(s.Failure() == SessionFailure::OperationFailed); continue;
        }
        if (scenario == 6) {
            assert(!s.CompleteSubscription(command, true, 5003));
            assert(s.Failure() == SessionFailure::Timeout); continue;
        }
        s.Retire();
        assert(!s.CompleteSubscription(command, true, 4));
        assert(s.Route(command.endpoint, value, 1, &view) == RouteResult::NotSubscribed);
        assert(view.data == nullptr && s.Failure() == SessionFailure::Closed);
    }
    // Each individual operation can complete within its step deadline.
    // Late/duplicate/unissued tickets do not advance the state machine.
    Session slow;
    start(slow, catalog);
    auto command = slow.TakeCommand(1);
    assert(slow.CompleteRead(command, true, map.data(), map.size(), 4999));
    command = slow.TakeCommand(4999);
    assert(slow.CompleteRead(command, true, input, 2, 9998));
    command = slow.TakeCommand(9998);
    assert(slow.CompleteSubscription(command, true, 14997));
    command = slow.TakeCommand(14997);
    assert(slow.CompleteRead(command, true, output, 2, 19996));
    assert(slow.State() == SessionState::Ready);
    Session bounded;
    auto many = catalog;
    many.count = 4;
    many.reports[2] = {8, 9, 0, 8}; many.reports[3] = {10, 11, 0, 8};
    assert(bounded.Begin(1, 8, 0));
    assert(bounded.CompleteCatalog(bounded.TakeCommand(0), true, many, 4999));
    assert(bounded.CompleteRead(bounded.TakeCommand(4999), true, map.data(), map.size(), 9998));
    assert(bounded.CompleteRead(bounded.TakeCommand(9998), true, input, 2, 14997));
    assert(bounded.CompleteSubscription(bounded.TakeCommand(14997), true, 19996));
    assert(bounded.CompleteRead(bounded.TakeCommand(19996), true, output, 2, 24995));
    assert(bounded.CompleteRead(bounded.TakeCommand(24995), true, output, 2, 29994));
    command = bounded.TakeCommand(29994);
    assert(!bounded.CompleteRead(command, true, output, 2, 30000));
    assert(bounded.Failure() == SessionFailure::Timeout);
    std::cout << "HOGP session: asynchronous tickets, metadata validation, subscription gating and retirement passed\n";
}

int main(int argc, char **argv) {
    assert(argc == 3);
    TestLayouts();
    TestRouter();
    TestSessions();
    TestGattInputLifecycle();
    TestVirtualPublication();
    TestStadia(argv[1], argv[2]);
}
