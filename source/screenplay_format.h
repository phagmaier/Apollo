#pragma once

#include "engine/layout.h"

#include <QByteArray>
#include <QString>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace apollo::source {
// The screenplay.json contract is in docs/source-format.md.
inline constexpr int screenplay_version = 1;
// Largest integer that version 1's JSON readers preserve exactly.
inline constexpr std::uint64_t max_revision = (std::uint64_t{1} << 53) - 1;

// One screenplay as saved. In memory, style offsets are UTF-8 bytes like the
// engine's; on disk they count Unicode code points. Title-page fields keep
// their relative order and come before the other elements.
struct Screenplay {
  std::uint64_t revision = 0;
  std::vector<Element> elements;
  bool operator==(const Screenplay&) const = default;
};

// newer_version: written by a later Apollo, so this one must not rewrite it.
// malformed: unreadable or invalid source. io: the filesystem refused.
// locked: another editable session. external_change: destination no longer
// matches the session's verified source, manifest, folder, or lock.
class SourceError : public std::runtime_error {
public:
  enum class Kind { malformed, newer_version, io, locked, external_change };
  SourceError(Kind kind, const QString& message) : std::runtime_error(message.toStdString()), kind_(kind) {}
  Kind kind() const { return kind_; }
private:
  Kind kind_;
};

bool is_title_field(ElementType type);
// A new random identity for an element.
std::string new_element_id();
// Sorted, with overlapping or touching spans of one style merged.
std::vector<StyleSpan> canonical_styles(std::vector<StyleSpan> styles);
// Title-page fields first, canonical styles, and a dual flag only on cues,
// where it pairs the speech with the preceding one. Elsewhere it has no effect.
Screenplay canonical(Screenplay screenplay);

// Deterministic: equal canonical screenplays give equal bytes. One element
// per line. Throws std::invalid_argument for a screenplay that cannot be
// represented, such as a missing or repeated id or a span inside a character.
QByteArray serialize_screenplay(const Screenplay& screenplay);
// Validates and returns the canonical screenplay. Throws SourceError.
Screenplay parse_screenplay(const QByteArray& bytes);
}
