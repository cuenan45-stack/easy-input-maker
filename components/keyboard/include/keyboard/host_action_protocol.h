#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ai_keyboard {

// Host Action v1 frozen wire contract (flow/进展.md 04/12 record):
// - reuses the existing AppCommand report id 0x11 (fixed_text_protocol.h);
// - command kind 0x05; 0x04 stays reserved for the status response;
// - exactly one chunk: chunk index 0, total chunks 1, data length 36;
// - the data area carries the canonical lowercase UUID ASCII without the
//   "host_action:" prefix; payload [40..62] is existing container slack.
inline constexpr std::uint8_t kHostActionAppCommandReportId = 0x11;
inline constexpr std::uint8_t kHostActionAppCommandKind = 0x05;
inline constexpr std::size_t kHostActionAppCommandHeaderLen = 4;
inline constexpr std::size_t kHostActionAppCommandPayloadLen = 63;
inline constexpr std::uint8_t kHostActionAppCommandChunkIndex = 0;
inline constexpr std::uint8_t kHostActionAppCommandTotalChunks = 1;
inline constexpr std::size_t kHostActionUuidAsciiLen = 36;
inline constexpr std::size_t kHostActionConfigPrefixLen = 12;  // "host_action:"

static_assert(kHostActionAppCommandHeaderLen + kHostActionUuidAsciiLen <=
                  kHostActionAppCommandPayloadLen,
              "Host Action UUID must fit the AppCommand container");
static_assert(kHostActionAppCommandKind != 0x04,
              "0x04 belongs to the status response");

// Strict canonical form: 36 characters, hyphens at 8/13/18/23, lowercase hex
// everywhere else. Uppercase, wrong length, wrong hyphen positions or illegal
// characters all fail closed; no case folding and no version or nil-UUID
// restrictions are added beyond the frozen contract.
inline bool is_canonical_host_action_uuid(std::string_view value) {
  if (value.size() != kHostActionUuidAsciiLen) {
    return false;
  }
  for (std::size_t i = 0; i < value.size(); ++i) {
    const char c = value[i];
    const bool hyphen_position = i == 8 || i == 13 || i == 18 || i == 23;
    if (hyphen_position) {
      if (c != '-') {
        return false;
      }
    } else {
      const bool digit = c >= '0' && c <= '9';
      const bool lowercase_hex = c >= 'a' && c <= 'f';
      if (!digit && !lowercase_hex) {
        return false;
      }
    }
  }
  return true;
}

// Splits a full config-layer value "host_action:<uuid>" into the bare UUID.
// Returns nullopt when the prefix is missing or the remainder is not a
// canonical lowercase UUID (fail closed, no case folding).
inline std::optional<std::string_view> host_action_uuid_from_config_value(
    std::string_view value) {
  if (value.size() <= kHostActionConfigPrefixLen) {
    return std::nullopt;
  }
  if (value.substr(0, kHostActionConfigPrefixLen) != "host_action:") {
    return std::nullopt;
  }
  const auto uuid = value.substr(kHostActionConfigPrefixLen);
  if (!is_canonical_host_action_uuid(uuid)) {
    return std::nullopt;
  }
  return uuid;
}

// Encodes one Host Action into the zero-padded 63-byte AppCommand payload:
// [0]=kind 0x05, [1]=chunk index 0, [2]=total chunks 1, [3]=data length 36,
// [4..39]=UUID ASCII, [40..62]=0. Returns nullopt for non-canonical UUIDs.
inline std::optional<std::array<std::uint8_t, kHostActionAppCommandPayloadLen>>
encode_host_action_v1(std::string_view uuid) {
  if (!is_canonical_host_action_uuid(uuid)) {
    return std::nullopt;
  }
  std::array<std::uint8_t, kHostActionAppCommandPayloadLen> payload{};
  payload[0] = kHostActionAppCommandKind;
  payload[1] = kHostActionAppCommandChunkIndex;
  payload[2] = kHostActionAppCommandTotalChunks;
  payload[3] = static_cast<std::uint8_t>(kHostActionUuidAsciiLen);
  for (std::size_t i = 0; i < kHostActionUuidAsciiLen; ++i) {
    payload[kHostActionAppCommandHeaderLen + i] =
        static_cast<std::uint8_t>(uuid[i]);
  }
  return payload;
}

}  // namespace ai_keyboard
