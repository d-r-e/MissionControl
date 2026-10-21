// Offline tests for the Bluetooth LE pad identification rules.
// The advertisement records are the ones observed from a real Stadia Controller (18d1:9400) in
// tests/fixtures/stadia_advertisement_summary.txt; the report ids and sizes are from the captured HID report
// map and a macOS Bluetooth LE capture of the same pad. No hardware is touched.
#include "ble_advertisement.hpp"
#include "ble_pad.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>

using namespace ams::ble;

namespace {

    void TestStadiaAdvertisement() {
        adv::Summary summary;
        adv::Reset(&summary);

        const std::uint8_t flags[] = { 0x05 };
        const std::uint8_t appearance[] = { 0xC4, 0x03 };
        const char name[] = "StadiaPad-TestA"; // 15 data bytes as observed; the real name was not retained
        const std::uint8_t tx_power[] = { 0xF8 };
        const std::uint8_t services[] = { 0x12, 0x18 };

        adv::Feed(&summary, adv::AdTypeFlags, flags, sizeof(flags));
        adv::Feed(&summary, adv::AdTypeAppearance, appearance, sizeof(appearance));
        adv::Feed(&summary, adv::AdTypeCompleteLocalName, reinterpret_cast<const std::uint8_t *>(name), std::strlen(name));
        adv::Feed(&summary, adv::AdTypeTxPowerLevel, tx_power, sizeof(tx_power));
        adv::Feed(&summary, adv::AdTypeAppearance, appearance, sizeof(appearance));
        adv::Feed(&summary, adv::AdTypeServiceUuid16Incomplete, services, sizeof(services));

        assert(summary.has_flags);
        assert(summary.flags == 0x05);
        assert(summary.has_appearance);
        assert(summary.appearance == adv::AppearanceGamepad);
        assert(std::strcmp(summary.name, "StadiaPad-TestA") == 0);
        assert(summary.service_count == 1);
        assert(adv::HasService(summary, adv::UuidHumanInterfaceDevice));
        assert(pad::AdvertisesHidService(summary));
        assert(pad::IsWorthConnecting(summary));
        assert(!summary.has_manufacturer);

        // Feeding the same records again (a scan response repeating the primary packet) changes nothing.
        const adv::Summary first = summary;
        adv::Feed(&summary, adv::AdTypeFlags, flags, sizeof(flags));
        adv::Feed(&summary, adv::AdTypeAppearance, appearance, sizeof(appearance));
        adv::Feed(&summary, adv::AdTypeCompleteLocalName, reinterpret_cast<const std::uint8_t *>(name), std::strlen(name));
        adv::Feed(&summary, adv::AdTypeServiceUuid16Incomplete, services, sizeof(services));
        assert(std::memcmp(&first, &summary, sizeof(summary)) == 0);

        // A HOGP device that announces itself is never guessed at: refusing is what produces the log that
        // tells the next person which fingerprint to add.
        const pad::Policy legacy = pad::LegacyPolicy(summary);
        assert(legacy.kind == pad::Kind::None);
        assert(!legacy.identified);
    }

    void TestXboxAdvertisement() {
        adv::Summary summary;
        adv::Reset(&summary);

        const std::uint8_t flags[] = { 0x06 };
        const std::uint8_t appearance[] = { 0xC4, 0x03 };
        const char name[] = "Xbox Wireless Controller";

        adv::Feed(&summary, adv::AdTypeFlags, flags, sizeof(flags));
        adv::Feed(&summary, adv::AdTypeAppearance, appearance, sizeof(appearance));
        adv::Feed(&summary, adv::AdTypeCompleteLocalName, reinterpret_cast<const std::uint8_t *>(name), std::strlen(name));

        assert(pad::IsWorthConnecting(summary));
        assert(!pad::AdvertisesHidService(summary));

        // What shipped and was tested on hardware: an Xbox pad is assumed, loudly, when its endpoints say
        // nothing. The name is not needed for that conclusion.
        const pad::Policy legacy = pad::LegacyPolicy(summary);
        assert(legacy.kind == pad::Kind::Xbox);
        assert(!legacy.identified);
        assert(legacy.input_report_id == 0x01);
    }

    void TestUnknownGamepadIsNotGuessedAt() {
        adv::Summary summary;
        adv::Reset(&summary);

        const std::uint8_t appearance[] = { 0xC4, 0x03 };
        const std::uint8_t services[] = { 0x12, 0x18 };
        adv::Feed(&summary, adv::AdTypeAppearance, appearance, sizeof(appearance));
        adv::Feed(&summary, adv::AdTypeServiceUuid16Complete, services, sizeof(services));

        assert(pad::IsWorthConnecting(summary));
        assert(pad::LegacyPolicy(summary).kind == pad::Kind::None);
    }

    void TestMalformedRecords() {
        adv::Summary summary;
        adv::Reset(&summary);

        const std::uint8_t one_byte[] = { 0xC4 };
        adv::Feed(&summary, adv::AdTypeAppearance, one_byte, sizeof(one_byte));
        assert(!summary.has_appearance);

        // A service list with a trailing odd byte keeps the whole pairs and drops the remainder.
        const std::uint8_t odd[] = { 0x12, 0x18, 0x00 };
        adv::Feed(&summary, adv::AdTypeServiceUuid16Incomplete, odd, sizeof(odd));
        assert(summary.service_count == 1);

        const std::uint8_t manufacturer[] = { 0xD1, 0x18 };
        adv::Feed(&summary, adv::AdTypeManufacturerSpecificData, manufacturer, sizeof(manufacturer));
        assert(summary.has_manufacturer);
        assert(summary.company_id == 0x18D1);

        // Over-long names are truncated into the fixed buffer rather than written past it.
        std::uint8_t long_name[64];
        std::memset(long_name, 'a', sizeof(long_name));
        adv::Feed(&summary, adv::AdTypeCompleteLocalName, long_name, sizeof(long_name));
        assert(std::strlen(summary.name) == adv::MaxNameLength - 1);

        adv::Feed(&summary, adv::AdTypeAppearance, nullptr, 0);
        adv::Feed(nullptr, adv::AdTypeAppearance, one_byte, sizeof(one_byte));
    }

    void TestIdentification() {
        const pad::Fingerprint stadia{ 0x03, 10, 0x05, 4, true, true, false, true };
        assert(pad::Identify(stadia) == pad::Kind::Stadia);

        const pad::Fingerprint xbox_full{ 0x01, 16, 0x00, 0, false, false, true, false };
        const pad::Fingerprint xbox_short{ 0x01, 15, 0x00, 0, false, false, true, false };
        assert(pad::Identify(xbox_full) == pad::Kind::Xbox);
        assert(pad::Identify(xbox_short) == pad::Kind::Xbox);

        // Something else's input report is not reinterpreted as a known pad.
        const pad::Fingerprint unknown{ 0x02, 8, 0, 0, true, true, false, false };
        assert(pad::Identify(unknown) == pad::Kind::None);

        const pad::Fingerprint nothing{};
        assert(pad::Identify(nothing) == pad::Kind::None);

        assert(pad::PolicyFor(pad::Kind::Stadia).input_report_id == 0x03);
        assert(pad::PolicyFor(pad::Kind::Stadia).input_report_size == 10);
        assert(pad::PolicyFor(pad::Kind::Xbox).input_report_id == 0x01);
        assert(pad::PolicyFor(pad::Kind::None).kind == pad::Kind::None);
    }

    void TestNameMatching() {
        adv::Summary summary;
        adv::Reset(&summary);

        const char upper[] = "GOOGLE STADIA CONTROLLER";
        adv::Feed(&summary, adv::AdTypeCompleteLocalName, reinterpret_cast<const std::uint8_t *>(upper), std::strlen(upper));
        assert(adv::NameContains(summary, "stadia"));
        assert(!adv::NameContains(summary, "xbox"));
        assert(!adv::NameContains(summary, ""));

        adv::Reset(&summary);
        assert(!adv::NameContains(summary, "stadia"));
    }

}

int main() {
    TestStadiaAdvertisement();
    TestXboxAdvertisement();
    TestUnknownGamepadIsNotGuessedAt();
    TestMalformedRecords();
    TestIdentification();
    TestNameMatching();

    std::printf("BLE pad identification: captured Stadia/Xbox advertisements, malformed records, endpoint fingerprints and refusal-to-guess passed\n");
    return 0;
}
