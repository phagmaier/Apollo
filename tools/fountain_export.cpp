#include "export/snapshot.h"
#include "source/fountain_export.h"

#include <QCoreApplication>
#include <QFile>
#include <chrono>
#include <iostream>
#include <stdexcept>

// Private helper: the parent owns this staged destination and publishes only
// after a clean exit. No project path or live document crosses this boundary.
int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  if (argc != 2) return 2;
  const auto now = [] { return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count(); };
  const auto started = now();
  try {
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) throw std::runtime_error("cannot read snapshot");
    const auto snapshot = apollo::decode_export_snapshot(input.readAll());
    const auto result = apollo::source::export_fountain(snapshot.elements, snapshot.include_title);
    if (!result.blockers.isEmpty()) throw std::runtime_error(result.blockers.join('\n').toStdString());
    const auto verified = now();
    QFile output(QString::fromLocal8Bit(argv[1]));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate) || output.write(result.bytes) != result.bytes.size() || !output.flush())
      throw std::runtime_error(output.errorString().toStdString());
    output.close();
    std::cout << started << ' ' << verified << ' ' << now() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
