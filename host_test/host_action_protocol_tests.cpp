// Host Action v1 frozen-contract tests.
// Contract (flow/进展.md 04/12 record): config layer keeps the complete
// "host_action:<canonical-lowercase-uuid>" value; the runtime wire message
// reuses the AppCommand report id 0x11 with command kind 0x05, chunk index 0,
// total chunks 1, data length 36, and the data area carries the bare UUID
// ASCII without the "host_action:" prefix. Non-canonical UUIDs fail closed.

#include "keyboard/host_action_protocol.h"

#include "keyboard/config_payload.h"
#include "keyboard/config_status.h"
#include "keyboard/keymap.h"
#include "keyboard/status_hid_protocol.h"

#include <cassert>
#include <cstddef>
#include <string>

namespace {

constexpr const char* kSampleUuid = "3fa85f64-5717-4562-b3fc-2c963f66afa6";
constexpr const char* kFullConfigValue =
    "host_action:3fa85f64-5717-4562-b3fc-2c963f66afa6";

void wire_constants_match_frozen_contract() {
  assert(ai_keyboard::kHostActionAppCommandReportId == 0x11);
  assert(ai_keyboard::kHostActionAppCommandKind == 0x05);
  assert(ai_keyboard::kHostActionAppCommandChunkIndex == 0);
  assert(ai_keyboard::kHostActionAppCommandTotalChunks == 1);
  assert(ai_keyboard::kHostActionUuidAsciiLen == 36);
  // 0x04 belongs to the status response and must stay unoccupied.
  assert(ai_keyboard::kStatusResponseCommandKind == 0x04);
  assert(ai_keyboard::kHostActionAppCommandKind !=
         ai_keyboard::kStatusResponseCommandKind);
}

void wire_encoding_uses_contract_fields_without_prefix() {
  const auto payload = ai_keyboard::encode_host_action_v1(kSampleUuid);
  assert(payload.has_value());
  assert((*payload)[0] == ai_keyboard::kHostActionAppCommandKind);
  assert((*payload)[1] == ai_keyboard::kHostActionAppCommandChunkIndex);
  assert((*payload)[2] == ai_keyboard::kHostActionAppCommandTotalChunks);
  assert((*payload)[3] == ai_keyboard::kHostActionUuidAsciiLen);
  for (std::size_t i = 0; i < ai_keyboard::kHostActionUuidAsciiLen; ++i) {
    assert((*payload)[4 + i] == static_cast<unsigned char>(kSampleUuid[i]));
  }
  // Payload [40..62] is existing container slack and must stay zero-padded.
  for (std::size_t i = 4 + ai_keyboard::kHostActionUuidAsciiLen;
       i < ai_keyboard::kHostActionAppCommandPayloadLen; ++i) {
    assert((*payload)[i] == 0);
  }
  const std::string data(reinterpret_cast<const char*>(&(*payload)[4]),
                         ai_keyboard::kHostActionUuidAsciiLen);
  assert(data == kSampleUuid);
  assert(data.find("host_action:") == std::string::npos);
}

void canonical_lowercase_uuid_is_accepted() {
  assert(ai_keyboard::is_canonical_host_action_uuid(kSampleUuid));
  // Nil UUID and any version nibble stay allowed: no extra restrictions.
  assert(ai_keyboard::is_canonical_host_action_uuid(
      "00000000-0000-0000-0000-000000000000"));
  assert(ai_keyboard::is_canonical_host_action_uuid(
      "ffffffff-ffff-4fff-bfff-ffffffffffff"));
  assert(ai_keyboard::is_canonical_host_action_uuid(
      "12345678-90ab-cdef-0123-456789abcdef"));
}

void non_canonical_uuids_fail_closed() {
  // Uppercase is rejected and never auto-lowercased.
  assert(!ai_keyboard::is_canonical_host_action_uuid(
      "3FA85F64-5717-4562-B3FC-2C963F66AFA6"));
  assert(!ai_keyboard::is_canonical_host_action_uuid(
      "3fa85f64-5717-4562-b3fc-2c963f66afaA"));
  // Wrong lengths.
  assert(!ai_keyboard::is_canonical_host_action_uuid(
      "3fa85f64-5717-4562-b3fc-2c963f66afa"));
  assert(!ai_keyboard::is_canonical_host_action_uuid(
      "3fa85f64-5717-4562-b3fc-2c963f66afa60"));
  assert(!ai_keyboard::is_canonical_host_action_uuid(""));
  // Wrong hyphen positions.
  assert(!ai_keyboard::is_canonical_host_action_uuid(
      "3fa85f645-717-4562-b3fc-2c963f66afa6"));
  assert(!ai_keyboard::is_canonical_host_action_uuid(
      "3fa85f64-57174562-b3fc-2c963f66afa6"));
  // Illegal characters (including 'g'-'z', space, '+').
  assert(!ai_keyboard::is_canonical_host_action_uuid(
      "3ga85f64-5717-4562-b3fc-2c963f66afa6"));
  assert(!ai_keyboard::is_canonical_host_action_uuid(
      "3fa85f64-5717-4562-b3fc-2c963f66af 6"));
}

void config_layer_parses_full_prefixed_value() {
  const std::string json = std::string(R"({
    "schema":"ai_keyboard.v1",
    "profiles":[{"id":"default","keys":{
      "KEY1":{"press":")") + kFullConfigValue + R"("},
      "KEY2":{"press":"disabled"},
      "KEY3":{"press":"disabled"},
      "KEY4":{"press":"disabled"},
      "KEY5":{"press":"disabled"},
      "KEY6":{"press":"disabled"},
      "KEY7":{"press":"disabled"},
      "KEY8":{"press":"disabled"}
    },"encoder":{"left":"disabled","right":"disabled","press":"disabled"}}]
  })";
  const auto result = ai_keyboard::parse_config_payload(json);
  assert(result.status == ai_keyboard::ConfigParseStatus::Ok);
  const auto& action =
      result.config.keymap.action_for(ai_keyboard::InputId::Key1);
  assert(action.kind == ai_keyboard::ActionKind::HostAction);
  // The config layer keeps the complete prefixed value.
  assert(action.text == kFullConfigValue);
}

void config_layer_rejects_non_canonical_uuid_fail_closed() {
  const auto make_json = [](const char* value) {
    return std::string(R"({
      "schema":"ai_keyboard.v1",
      "profiles":[{"id":"default","keys":{
        "KEY1":{"press":")") + value + R"("},
        "KEY2":{"press":"disabled"},
        "KEY3":{"press":"disabled"},
        "KEY4":{"press":"disabled"},
        "KEY5":{"press":"disabled"},
        "KEY6":{"press":"disabled"},
        "KEY7":{"press":"disabled"},
        "KEY8":{"press":"disabled"}
      },"encoder":{"left":"disabled","right":"disabled","press":"disabled"}}]
    })";
  };
  const auto upper = ai_keyboard::parse_config_payload(
      make_json("host_action:3FA85F64-5717-4562-B3FC-2C963F66AFA6"));
  assert(upper.status == ai_keyboard::ConfigParseStatus::UnknownAction);
  const auto short_one = ai_keyboard::parse_config_payload(
      make_json("host_action:3fa85f64-5717-4562-b3fc-2c963f66afa"));
  assert(short_one.status == ai_keyboard::ConfigParseStatus::UnknownAction);
  const auto double_prefixed = ai_keyboard::parse_config_payload(make_json(
      "host_action:host_action:3fa85f64-5717-4562-b3fc-2c963f66afa6"));
  assert(double_prefixed.status == ai_keyboard::ConfigParseStatus::UnknownAction);
}

void press_emits_once_and_release_emits_none() {
  const ai_keyboard::Action action{
      ai_keyboard::ActionKind::HostAction, "", kFullConfigValue};
  const auto press = ai_keyboard::event_for_action(
      action, ai_keyboard::InputPhase::Pressed, "", "");
  assert(press.kind == ai_keyboard::FirmwareEventKind::HostAction);
  // The runtime event carries the bare UUID; the prefix stays config-only.
  assert(press.value == kSampleUuid);
  const auto release = ai_keyboard::event_for_action(
      action, ai_keyboard::InputPhase::Released, "", "");
  assert(release.kind == ai_keyboard::FirmwareEventKind::None);
  assert(release.value.empty());
}

}  // namespace

int main() {
  wire_constants_match_frozen_contract();
  wire_encoding_uses_contract_fields_without_prefix();
  canonical_lowercase_uuid_is_accepted();
  non_canonical_uuids_fail_closed();
  config_layer_parses_full_prefixed_value();
  config_layer_rejects_non_canonical_uuid_fail_closed();
  press_emits_once_and_release_emits_none();
  return 0;
}
