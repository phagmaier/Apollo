#pragma once

#include "editor/fountain_export_job.h"
#include <QCoreApplication>
#include <QFile>
#include <QtTest>
#include <csignal>

// Exercise the actual serializer, then stop its wrapper before the parent can
// publish. SIGCONT exits cleanly; cancellation kills/reaps the stopped wrapper.
class HeldFountainWorker {
public:
  HeldFountainWorker(apollo::editor::FountainExportJob& job, const QString& path) : job_(job) {
    QFile script(path);
    if (!script.open(QIODevice::WriteOnly)) qFatal("cannot create held helper");
    const auto exporter = (QCoreApplication::applicationDirPath() + "/fountain_export").toUtf8().replace("'", "'\\''");
    script.write("#!/bin/sh\n'" + exporter + "' \"$@\" || exit 1\nkill -STOP $$\nexit 0\n");
    script.close();
    if (!script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) qFatal("cannot make helper executable");
    job_.set_worker_program(path);
  }
  bool stopped() const {
    QFile status(QStringLiteral("/proc/%1/status").arg(job_.worker_pid()));
    return job_.worker_pid() && status.open(QIODevice::ReadOnly) && status.readAll().contains("State:\tT");
  }
  bool resume() {
    const auto pid = job_.worker_pid();
    return pid > 0 && ::kill(static_cast<pid_t>(pid), SIGCONT) == 0;
  }
private:
  apollo::editor::FountainExportJob& job_;
};
