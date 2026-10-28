#include "engine/layout.h"
#include "export/pdf.h"
#include "export/snapshot.h"
#include "fountain/import.h"

#include <QGuiApplication>
#include <QFile>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <chrono>

int main(int argc, char** argv) {
  const auto now = [] { return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count(); };
  const auto started = now();
  const bool snapshot = argc == 4 && std::string(argv[1]) == "--snapshot-stdin";
  if (!snapshot && (argc < 3 || argc > 4 || (argc == 4 && std::string(argv[3]) != "--no-title-page"))) {
    std::cerr << "usage: pdf_export INPUT.fountain OUTPUT.pdf [--no-title-page]\n"; return 2;
  }
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && qEnvironmentVariableIsEmpty("DISPLAY") &&
      qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication application(argc, argv);
  try {
    if (snapshot) {
      QFile input;
      if (!input.open(stdin, QIODevice::ReadOnly)) throw std::runtime_error("cannot read snapshot");
      const auto captured = apollo::decode_export_snapshot(input.readAll());
      apollo::export_pdf(apollo::layout_document(captured.elements), QString::fromLocal8Bit(argv[2]),
                         QString::fromLocal8Bit(argv[3]), captured.include_title);
      std::cout << started << ' ' << now() << '\n';
      return 0;
    }
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
