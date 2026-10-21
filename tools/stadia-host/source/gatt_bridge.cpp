/* SPDX-License-Identifier: GPL-2.0-only */
#include "gatt_bridge.h"
#include "stadia_report.hpp"
#include "hogp_report_router.hpp"

uint32_t mc_stadia_max_map_bytes(void) {
    return ams::bluetooth::hogp::ReportLayout::MaxDescriptorSize;
}
int mc_stadia_validate_map(const uint8_t *data, size_t size) {
    namespace hogp = ams::bluetooth::hogp;
    using Report = ams::controller::StadiaInputReport;
    hogp::ReportLayout layout;
    return layout.Parse(data, size) == hogp::LayoutResult::Ok && layout.UsesReportIds() &&
        layout.PayloadSize(Report::ReportId, hogp::ReportType::Input) == Report::PayloadSize;
}
int mc_hogp_parse_reference(const uint8_t *data, size_t size) {
    ams::bluetooth::hogp::ReportReference reference{};
    if (!ams::bluetooth::hogp::ReportReference::Parse(data, size, &reference)) return -1;
    return reference.id | (static_cast<unsigned>(reference.type) << 8);
}
int mc_hogp_reference_size(const uint8_t *map, size_t map_size,
                           const uint8_t *reference, size_t reference_size) {
    namespace hogp = ams::bluetooth::hogp;
    hogp::ReportLayout layout;
    hogp::ReportReference parsed{};
    if (layout.Parse(map, map_size) != hogp::LayoutResult::Ok ||
        !hogp::ReportReference::Parse(reference, reference_size, &parsed)) return -1;
    const auto size = layout.PayloadSize(parsed.id, parsed.type);
    return size ? static_cast<int>(size) : -1;
}
