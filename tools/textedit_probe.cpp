#include "editor/probe_view.h"
#include "editor/screenplay_layout.h"
#include "fountain/import.h"

#include <QGuiApplication>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

// Opens a Fountain file in the TextEdit probe for manual checks. --check
// only lays the document out and reports whether every block matched the
// engine's line breaks; it is not a timing or interaction measurement.
int main(int argc, char** argv) {
  // Read --check before QGuiApplication removes Qt's own options from argv.
  const bool check = argc >= 3 && std::string(argv[argc - 1]) == "--check";
  if (check && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication application(argc, argv);
  if (argc != (check ? 3 : 2)) { std::cerr << "usage: textedit_probe FILE.fountain [--check]\n"; return 2; }
  try {
    std::ifstream input(argv[1]);
    if (!input) throw std::runtime_error("cannot open input");
    std::ostringstream source; source << input.rdbuf();
    auto document = apollo::editor::screenplay_document(apollo::import_fountain(source.str()),
                                                        apollo::editor::courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
    const auto* layout = static_cast<apollo::editor::ScreenplayLayout*>(document->documentLayout());
    if (check) {
      std::cout << document->blockCount() << " blocks, " << layout->pageCount() << " body pages\n";
      if (!layout->error().isEmpty()) throw std::runtime_error(layout->error().toStdString());
      return 0;
    }
    QQuickView view;
    apollo::editor::show_probe(view, *document);
    view.setTitle(QStringLiteral("Apollo TextEdit probe"));
    view.resize(880, 1000);
    view.show();
    return application.exec();
  } catch (const std::exception& error) {
    std::cerr << "probe failed: " << error.what() << '\n'; return 1;
  }
}
