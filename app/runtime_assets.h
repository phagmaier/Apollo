#pragma once

#include <QCoreApplication>
#include <QDir>

namespace apollo {
// Build and installed runtimes have the same private layout. Never consult
// the checkout or working directory: a missing asset is a broken runtime.
inline QString runtime_asset(const QString& relative) {
  return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("assets/") + relative);
}
}
