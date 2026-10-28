#include "fountain/import.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

namespace apollo {
namespace {
std::string trim(std::string text) {
  auto first = text.find_first_not_of(" \t\r");
  if (first == std::string::npos) return {};
  return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}
bool cue(const std::string& line) {
  bool letter = false;
  for (unsigned char c : line) {
    if (std::isalpha(c)) { letter = true; if (std::islower(c)) return false; }
  }
  return letter;
}
bool heading(const std::string& line) {
  return line.starts_with("INT.") || line.starts_with("EXT.") || line.starts_with("INT/EXT.") ||
         line.starts_with("INT./EXT.") || line.starts_with("I/E.") || line.starts_with('.');
}
void unsupported(const std::string& line) {
  if (line.starts_with('#') || line.starts_with('=') || line.find("[[") != std::string::npos || line.find("/*") != std::string::npos)
    throw std::runtime_error("unsupported Fountain construct: " + line);
}
Element styled(ElementType type, std::string_view input, bool dual = false) {
  Element out{type, "", dual};
  struct Open { std::string marker; std::size_t start; };
  std::vector<Open> open;
  for (std::size_t i = 0; i < input.size();) {
    if (input[i] == '\\' && i + 1 < input.size()) { out.text += input[i + 1]; i += 2; continue; }
    if (input[i] != '*' && input[i] != '_') { out.text += input[i++]; continue; }
    std::size_t n = 1;
    if (input[i] == '*') while (n < 3 && i + n < input.size() && input[i + n] == '*') ++n;
    std::string marker(input.substr(i, n));
    if (!open.empty() && open.back().marker == marker) {
      auto start = open.back().start; open.pop_back();
      if (marker == "*" || marker == "***") out.styles.push_back({start, out.text.size(), Style::italic});
      if (marker == "**" || marker == "***") out.styles.push_back({start, out.text.size(), Style::bold});
      if (marker == "_") out.styles.push_back({start, out.text.size(), Style::underline});
    } else open.push_back({marker, out.text.size()});
    i += n;
  }
  if (!open.empty()) throw std::runtime_error("unclosed Fountain emphasis");
  std::stable_sort(out.styles.begin(), out.styles.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
  return out;
}
void append(Element& target, const std::string& text) {
  auto more = styled(target.type, text);
  target.text += ' ';
  auto offset = target.text.size();
  target.text += more.text;
  for (auto span : more.styles) { span.start += offset; span.end += offset; target.styles.push_back(span); }
}
}

std::vector<Element> import_fountain(std::string_view source) {
  std::istringstream in{std::string(source)};
  std::vector<std::string> lines;
  for (std::string line; std::getline(in, line);) lines.push_back(line);
  std::vector<Element> result;
  std::size_t i = 0;
  ElementType field = ElementType::title;
  bool has_header = !lines.empty() && trim(lines[0]).starts_with("Title:");
  if (has_header) {
    while (i < lines.size() && !trim(lines[i]).empty()) {
      const bool indent = !lines[i].empty() && (lines[i][0] == ' ' || lines[i][0] == '\t');
      auto line = trim(lines[i++]);
      if (!indent) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) throw std::runtime_error("invalid title field: " + line);
        auto name = line.substr(0, colon);
        if (name == "Title") field = ElementType::title;
        else if (name == "Credit") field = ElementType::credit;
        else if (name == "Author" || name == "Authors") field = ElementType::author;
        else if (name == "Source") field = ElementType::source;
        else if (name == "Contact") field = ElementType::contact;
        else if (name == "Draft date") field = ElementType::draft_date;
        else throw std::runtime_error("unsupported title field: " + name);
        line = trim(line.substr(colon + 1));
      }
      if (!line.empty()) result.push_back(styled(field, line));
    }
  }
  for (; i < lines.size();) {
    auto line = trim(lines[i]);
    if (line.empty()) { ++i; continue; }
    if (line == "===") { result.push_back({ElementType::page_break, ""}); ++i; continue; }
    const bool dual_right = line.ends_with(" ^");
    if (dual_right) line.erase(line.size() - 2);
    if (line.find('^') != std::string::npos) throw std::runtime_error("unsupported dual-dialogue marker: " + line);
    unsupported(line);
    if (heading(line)) {
      if (line.starts_with('.')) line.erase(0, 1);
      result.push_back(styled(ElementType::heading, line)); ++i; continue;
    }
    if (line.starts_with('!')) {
      line.erase(0, 1);
      result.push_back(styled(ElementType::action, line)); ++i; continue;
    }
    if (line.starts_with('~')) {
      auto lyric = styled(ElementType::lyric, trim(line.substr(1)));
      lyric.styles.push_back({0, lyric.text.size(), Style::italic});
      result.push_back(std::move(lyric)); ++i; continue;
    }
    if (line.starts_with('>')) {
      bool centered = line.ends_with('<');
      line = trim(line.substr(1, line.size() - (centered ? 2 : 1)));
      result.push_back(styled(centered ? ElementType::centered : ElementType::transition, line)); ++i; continue;
    }
    if (line.ends_with("TO:") && cue(line)) { result.push_back(styled(ElementType::transition, line)); ++i; continue; }
    if (cue(line) && i + 1 < lines.size() && !trim(lines[i + 1]).empty()) {
      result.push_back(styled(ElementType::cue, line, dual_right)); ++i;
      while (i < lines.size() && !trim(lines[i]).empty()) {
        line = trim(lines[i++]); unsupported(line);
        if (line.starts_with('(')) result.push_back(styled(ElementType::parenthetical, line));
        else if (!result.empty() && result.back().type == ElementType::dialogue) append(result.back(), line);
        else result.push_back(styled(ElementType::dialogue, line));
      }
      continue;
    }
    if (dual_right) throw std::runtime_error("dual marker requires a character cue");
    auto paragraph = styled(ElementType::action, line);
    ++i;
    while (i < lines.size() && !trim(lines[i]).empty()) {
      line = trim(lines[i++]); unsupported(line); append(paragraph, line);
    }
    result.push_back(std::move(paragraph));
  }
  return result;
}
}
