#include "engine/layout.h"
#include "fountain/import.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

int main(int argc, char** argv) {
  if (argc != 2) { std::cerr << "usage: layout_probe FILE.fountain\n"; return 2; }
  std::ifstream input(argv[1]);
  if (!input) { std::cerr << "cannot open input\n"; return 2; }
  std::ostringstream source; source << input.rdbuf();
  try {
    const auto elements = apollo::import_fountain(source.str());
    const auto lines = apollo::layout(elements);
    for (const auto& line : lines) {
      if (!line.page) continue;
      const auto type = line.type == apollo::ElementType::heading ? "heading" :
                        line.type == apollo::ElementType::action ? "action" :
                        line.type == apollo::ElementType::cue ? "cue" :
                        line.type == apollo::ElementType::parenthetical ? (line.continuation ? "paren+" : "paren") :
                        line.type == apollo::ElementType::dialogue ? "dialogue" :
                        line.type == apollo::ElementType::more ? "more" :
                        line.type == apollo::ElementType::centered ? "centered" :
                        line.type == apollo::ElementType::lyric ? "lyric" : "transition";
      std::cout << line.page << '\t' << line.slot << '\t';
      if (line.column) std::cout << "dual-";
      std::cout << type;
      if (line.column) std::cout << '-' << line.column;
      std::cout << '\t' << line.text << '\n';
    }
    std::cerr << elements.size() << " elements, " << lines.size() << " lines\n";
  } catch (const std::exception& error) {
    std::cerr << "layout failed: " << error.what() << '\n'; return 1;
  }
}
