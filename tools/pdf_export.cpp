#include "engine/layout.h"
#include "export/pdf.h"
#include "fountain/import.h"

#include <QGuiApplication>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

int main(int argc, char** argv) {
  if (argc < 3 || argc > 4 || (argc == 4 && std::string(argv[3]) != "--no-title-page")) {
    std::cerr << "usage: pdf_export INPUT.fountain OUTPUT.pdf [--no-title-page]\n"; return 2;
  }
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && qEnvironmentVariableIsEmpty("DISPLAY") &&
      qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication application(argc, argv);
  try {
    std::ifstream input(argv[1]);
    if (!input) throw std::runtime_error("cannot open input");
    std::error_code path_error;
    if (std::filesystem::equivalent(argv[1], argv[2], path_error))
      throw std::runtime_error("input and PDF destination must differ");
    std::ostringstream source; source << input.rdbuf();
    const auto document = apollo::layout_document(apollo::import_fountain(source.str()));
    apollo::export_pdf(document, QString::fromLocal8Bit(argv[2]), QStringLiteral(APOLLO_FONT_DIR), argc != 4);
  } catch (const std::exception& error) {
    std::cerr << "export failed: " << error.what() << '\n'; return 1;
  }
}
