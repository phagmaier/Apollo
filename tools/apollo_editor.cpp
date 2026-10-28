#include "editor/autosave.h"
#include "editor/editor_view.h"
#include "editor/screenplay_layout.h"
#include "editor/pdf_export_job.h"
#include "editor/project_access_flow.h"
#include "fountain/import.h"
#include "source/project.h"

#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQuickWindow>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
#include <QTextStream>
#include <QTimer>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>

namespace {
using apollo::editor::Autosaver;
using apollo::editor::ScreenplayLayout;

QString exporter() { return QCoreApplication::applicationDirPath() + QStringLiteral("/pdf_export"); }

std::string read_fountain(const char* path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open input");
  std::ostringstream source; source << input.rdbuf();
  return source.str();
}

// Explicit spike/instrumentation path: Fountain parses unsaved; editable
// projects use autosave. Ordinary product opening uses ProjectAccessFlow.
struct Opened {
  std::unique_ptr<QTextDocument> document;
  std::unique_ptr<Autosaver> saver;
  QString export_destination;
};

Opened open_document(const char* path) {
  const auto font = apollo::editor::courier_prime(QStringLiteral(APOLLO_FONT_DIR));
  const QFileInfo info(QString::fromLocal8Bit(path));
  Opened opened;
  opened.export_destination = info.absolutePath() + "/" + info.completeBaseName() + ".pdf";
  if (!info.isDir() && info.isFile() && info.suffix().compare("fountain", Qt::CaseInsensitive) == 0) {
    opened.document = apollo::editor::screenplay_document(apollo::import_fountain(read_fountain(path)), font);
    return opened;
  }
  auto session = std::make_unique<apollo::source::ProjectSession>(info.absoluteFilePath());
  const auto& project = session->opened();
  if (project.project.import_review) throw std::runtime_error("Fountain intake is review-only; open the project normally to read retained material. Editing/layout/export conversion is pending.");
  opened.export_destination = info.absoluteFilePath() + ".pdf";
  opened.document = apollo::editor::screenplay_document(project.screenplay.elements, font);
  opened.saver = std::make_unique<Autosaver>(opened.document.get(), std::move(session));
  return opened;
}

// Process events until the window swaps a frame, or give up after a second.
bool wait_for_frame(QQuickWindow& window) {
  QEventLoop loop;
  bool swapped = false;
  const auto connection = QObject::connect(&window, &QQuickWindow::frameSwapped, &loop, [&] { swapped = true; loop.quit(); });
  QTimer::singleShot(1000, &loop, &QEventLoop::quit);
  loop.exec();
  QObject::disconnect(connection);
  return swapped;
}

void idle(int milliseconds) {
  QEventLoop loop;
  QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
  loop.exec();
}

double pss_mib(qint64 pid = 0) {
  QFile file(pid ? QStringLiteral("/proc/%1/smaps_rollup").arg(pid) : QStringLiteral("/proc/self/smaps_rollup"));
  if (!file.open(QIODevice::ReadOnly)) return -1;
  for (const auto& line : file.readAll().split('\n'))
    if (line.startsWith("Pss:")) return line.simplified().split(' ').at(1).toDouble() / 1024;
  return -1;
}

qint64 monotonic_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

double percentile(std::vector<double> values, double p) {
  std::sort(values.begin(), values.end());
  return values[static_cast<std::size_t>(p * static_cast<double>(values.size() - 1) + 0.5)];
}

// The first character on the later page of a block that crosses a page
// break, starting from the middle page of the document.
int page_boundary_position(const QTextDocument& document) {
  const auto* layout = static_cast<const ScreenplayLayout*>(document.documentLayout());
  const double middle = layout->documentHeight() / 2;
  for (auto block = document.begin(); block != document.end(); block = block.next()) {
    const auto* text = block.layout();
    const double top = layout->blockBoundingRect(block).top();
    for (int i = 1; i < text->lineCount(); ++i) {
      const double y = top + text->lineAt(i).y();
      if (y > middle && text->lineAt(i).y() - text->lineAt(i - 1).y() > apollo::editor::page_gap)
        return block.position() + text->lineAt(i).textStart();
    }
  }
  throw std::runtime_error("no block crosses a page break after the middle of the document");
}

// Synthetic key presses sent to the window, timed from dispatch to the next
// frameSwapped. This measures Apollo and Qt Quick, not compositor or device
// input latency.
int measure(const char* path, int keys, const QString& export_directory = {}) {
  if (keys <= 0) throw std::runtime_error("KEYS must be positive");
  QElapsedTimer clock; clock.start();
  auto opened = open_document(path);
  auto& document = opened.document;
  apollo::editor::PdfExportJob job(document.get(), QStringLiteral(APOLLO_FONT_DIR), exporter(), QString::fromLocal8Bit(path));
  const bool exporting = !export_directory.isEmpty();
  if (exporting && !QDir().mkpath(export_directory)) throw std::runtime_error("cannot create export directory");
  const auto destination = QDir(export_directory).filePath(QStringLiteral("while-typing.pdf"));
  QQuickView view;
  auto* editor = apollo::editor::show_editor(view, *document, exporting ? &job : nullptr, destination, opened.saver.get());
  view.resize(880, 1000);
  view.show();
  if (!wait_for_frame(view)) throw std::runtime_error("the window never rendered");
  const double open_ms = static_cast<double>(clock.nsecsElapsed()) / 1e6;
  const auto* layout = static_cast<const ScreenplayLayout*>(document->documentLayout());
  const int position = page_boundary_position(*document);
  editor->setProperty("cursorPosition", position);
  editor->forceActiveFocus();
  idle(200);
  const QString typed = QStringLiteral("the lamp turns and ");
  const auto original_text = document->toPlainText();
  QString inserted;
  std::vector<double> frame_ms, handled_ms, relayout_ms;
  std::vector<double> export_ms, capture_ms, overlap_ms;
  std::vector<std::pair<qint64, qint64>> worker_intervals, key_frames;
  bool export_failed = false;
  QObject::connect(&job, &apollo::editor::PdfExportJob::completed, &view, [&](bool success) {
    export_failed |= !success;
    export_ms.push_back(job.total_ms());
    capture_ms.push_back(job.capture_ms());
    worker_intervals.emplace_back(job.worker_start_ns(), job.worker_end_ns());
  });
  int overlap_keys = 0, overlap_frames = 0;
  double peak_sampled_pss = pss_mib(), peak_sampled_combined_pss = peak_sampled_pss;
  int worker_memory_samples = 0;
  QTimer memory;
  if (exporting) {
    QObject::connect(&memory, &QTimer::timeout, &view, [&] {
      const double editor_pss = pss_mib();
      const double worker_pss = job.worker_pid() ? pss_mib(job.worker_pid()) : 0;
      worker_memory_samples += worker_pss > 0;
      peak_sampled_pss = std::max(peak_sampled_pss, editor_pss);
      peak_sampled_combined_pss = std::max(peak_sampled_combined_pss, editor_pss + std::max(0.0, worker_pss));
    });
    memory.start(20);
  }
  int missed = 0;
  for (int i = 0; i < keys; ++i) {
    if (QGuiApplication::platformName() == QStringLiteral("wayland") && !view.isActive())
      throw std::runtime_error("timing window lost focus; rerun and keep it focused until it closes");
    if (export_failed) throw std::runtime_error(job.status().toStdString());
    const QString text = typed.mid(i % typed.size(), 1);
    const auto key = text == QStringLiteral(" ") ? Qt::Key_Space : static_cast<Qt::Key>(Qt::Key_A + (text[0].unicode() - 'a'));
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, text), release(QEvent::KeyRelease, key, Qt::NoModifier, text);
    const auto before = layout->relayout_nanoseconds();
    clock.restart();
    // Start the same path as the visible Export PDF control. Include snapshot
    // capture/dispatch in this key's latency, rather than hiding that GUI work
    // between samples. Repeated genuine exports keep the short jobs overlapping.
    if (exporting && !job.busy() && !job.request(destination)) throw std::runtime_error(job.status().toStdString());
    const auto key_ns = monotonic_ns();
    QGuiApplication::sendEvent(&view, &press);
    QGuiApplication::sendEvent(&view, &release);
    inserted += text;
    if (editor->property("cursorPosition").toInt() != position + i + 1) throw std::runtime_error("typing caret did not advance");
    handled_ms.push_back(static_cast<double>(clock.nsecsElapsed()) / 1e6);
    relayout_ms.push_back(static_cast<double>(layout->relayout_nanoseconds() - before) / 1e6);
    if (wait_for_frame(view)) {
      const double elapsed = static_cast<double>(clock.nsecsElapsed()) / 1e6;
      frame_ms.push_back(elapsed);
      key_frames.emplace_back(key_ns, monotonic_ns());
    }
    else ++missed;
    idle(60);
  }
  if (frame_ms.empty()) throw std::runtime_error("no key produced a frame");
  if (document->toPlainText() != original_text.left(position) + inserted + original_text.mid(position))
    throw std::runtime_error("measurement keys did not produce the expected document");
  QElapsedTimer drain; drain.start();
  while (job.busy() && drain.elapsed() < 15000) idle(5);
  if (job.busy() || export_failed) throw std::runtime_error("export did not finish successfully: " + job.status().toStdString());
  if (exporting) {
    QFile events(QDir(export_directory).filePath(QStringLiteral("events.tsv")));
    if (!events.open(QIODevice::WriteOnly | QIODevice::Truncate)) throw std::runtime_error("cannot save timing events");
    QTextStream saved(&events);
    saved << "# Linux steady_clock nanoseconds; key latency includes capture/dispatch before key timestamp\n";
    for (std::size_t i = 0; i < worker_intervals.size(); ++i)
      saved << "worker\t" << worker_intervals[i].first << '\t' << worker_intervals[i].second << '\t' << export_ms[i] << '\t' << capture_ms[i] << '\n';
    for (std::size_t i = 0; i < key_frames.size(); ++i)
      saved << "key-frame\t" << key_frames[i].first << '\t' << key_frames[i].second << '\t' << frame_ms[i] << '\n';
    for (std::size_t i = 0; i < key_frames.size(); ++i) {
      const auto [key, frame] = key_frames[i];
      auto in_worker = [&](qint64 instant) {
        return std::any_of(worker_intervals.begin(), worker_intervals.end(), [&](const auto& interval) {
          return interval.first <= instant && instant <= interval.second;
        });
      };
      if (in_worker(key)) { ++overlap_keys; overlap_ms.push_back(frame_ms[i]); }
      overlap_frames += in_worker(frame);
    }
    if (overlap_ms.empty()) throw std::runtime_error("no key overlapped actual worker execution");
  }
  const auto stalls = std::count_if(frame_ms.begin(), frame_ms.end(), [](double ms) { return ms > 100; });
  const auto api = view.rendererInterface()->graphicsApi();
  std::printf("platform %s, renderer %s, %d blocks, %d pages, caret at %d\n", qPrintable(QGuiApplication::platformName()),
              api == QSGRendererInterface::Software ? "software" : api == QSGRendererInterface::OpenGL ? "OpenGL"
              : api == QSGRendererInterface::Vulkan ? "Vulkan" : "other", document->blockCount(), layout->pageCount(), position);
  std::printf("open to first frame: %.1f ms\n", open_ms);
  std::printf("key to frameSwapped over %zu keys: p50 %.1f ms, p95 %.1f ms, max %.1f ms, >100 ms: %td, no frame: %d\n",
              frame_ms.size(), percentile(frame_ms, 0.5), percentile(frame_ms, 0.95),
              *std::max_element(frame_ms.begin(), frame_ms.end()), stalls, missed);
  std::printf("key handling (synchronous): p50 %.1f ms, p95 %.1f ms; of which relayout p50 %.1f ms, p95 %.1f ms\n",
              percentile(handled_ms, 0.5), percentile(handled_ms, 0.95), percentile(relayout_ms, 0.5), percentile(relayout_ms, 0.95));
  std::printf("PSS after typing: %.1f MiB\n", pss_mib());
  if (exporting) {
    std::printf("real exports: %zu; request through completion p50 %.1f ms, p95 %.1f ms, max %.1f ms\n",
                export_ms.size(), percentile(export_ms, 0.5), percentile(export_ms, 0.95), *std::max_element(export_ms.begin(), export_ms.end()));
    std::printf("snapshot capture and dispatch: p50 %.1f ms, p95 %.1f ms, max %.1f ms\n",
                percentile(capture_ms, 0.5), percentile(capture_ms, 0.95), *std::max_element(capture_ms.begin(), capture_ms.end()));
    std::printf("in-flight overlap: %d keys, %d frames; overlapping key to frame p95 %.1f ms\n", overlap_keys, overlap_frames, percentile(overlap_ms, 0.95));
    if (worker_memory_samples == 0) throw std::runtime_error("no worker PSS samples were available");
    std::printf("sampled PSS maxima (20 ms timer, %d helper samples): editor %.1f MiB, editor plus helper %.1f MiB\n",
                worker_memory_samples, peak_sampled_pss, peak_sampled_combined_pss);
  }
  return 0;
}

void send_key(QQuickWindow& window, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier, const QString& text = {}) {
  QKeyEvent press(QEvent::KeyPress, key, modifiers, text), release(QEvent::KeyRelease, key, modifiers, text);
  QGuiApplication::sendEvent(&window, &press);
  QGuiApplication::sendEvent(&window, &release);
}

// Elements with a line in the last four slots of a page or the first two
// slots of a later page, by block number.
std::vector<int> boundary_blocks(const QTextDocument& document) {
  std::vector<int> blocks;
  for (const auto& line : apollo::layout(apollo::editor::document_elements(document)))
    if (line.source != apollo::no_source && line.page > 0 && (line.slot >= 53 || (line.page > 1 && line.slot <= 2)) &&
        (blocks.empty() || blocks.back() != static_cast<int>(line.source)))
      blocks.push_back(static_cast<int>(line.source));
  return blocks;
}

// The page of each element's first body line, counted from the document end
// so that elements after an edit keep their index.
std::vector<int> pages_from_end(const QTextDocument& document) {
  const auto elements = apollo::editor::document_elements(document);
  std::vector<int> pages(elements.size(), 0);
  for (const auto& line : apollo::layout(elements))
    if (line.source != apollo::no_source && !pages[elements.size() - 1 - line.source]) pages[elements.size() - 1 - line.source] = line.page;
  return pages;
}

// Deterministic edits through the editor's key handling, each at an element
// near a page boundary. Ten kinds of edit, five times each for 50 edits.
int scripted_edits(QQuickWindow& window, QQuickItem& editor, QTextDocument& document, int count) {
  std::mt19937 random(1);
  int moved = 0;
  const QStringList sentences{QStringLiteral(" The wind keeps rising and every window in the lamp room rattles in its frame."),
                              QStringLiteral(" Nobody moves."),
                              QStringLiteral(" She counts the seconds between the flashes, then counts them again.")};
  for (int edit = 0; edit < count; ++edit) {
    const auto candidates = boundary_blocks(document);
    if (candidates.empty()) throw std::runtime_error("no element is near a page boundary");
    auto block = document.findBlockByNumber(candidates[random() % candidates.size()]);
    if (apollo::editor::element_type(block) == apollo::ElementType::page_break) block = block.next().isValid() ? block.next() : block.previous();
    const auto before = pages_from_end(document);
    const auto sentence = sentences[static_cast<qsizetype>(random() % static_cast<unsigned>(sentences.size()))];
    auto place = [&](int position) { editor.setProperty("cursorPosition", position); };
    const int end = block.position() + block.length() - 1;
    switch (edit % 10) {
      case 0: place(end); send_key(window, Qt::Key_unknown, Qt::NoModifier, sentence); break;
      case 1: place(end); send_key(window, Qt::Key_Return); send_key(window, Qt::Key_unknown, Qt::NoModifier, sentence.trimmed()); break;
      case 2: {
        const auto space = block.text().indexOf(QLatin1Char(' '), static_cast<qsizetype>(block.text().size() / 2));
        place(space > 0 ? block.position() + static_cast<int>(space) + 1 : end);
        send_key(window, Qt::Key_Return);
        break;
      }
      case 3: place(block.position()); send_key(window, Qt::Key_Backspace); break;
      case 4: place(block.position()); send_key(window, Qt::Key_Tab); break;
      case 5:
        place(block.position());
        send_key(window, Qt::Key_Down, Qt::ShiftModifier);
        send_key(window, Qt::Key_Down, Qt::ShiftModifier);
        send_key(window, Qt::Key_Delete);
        break;
      case 6:
        QGuiApplication::clipboard()->setText(sentence.trimmed() + QStringLiteral("\n\nA second pasted paragraph."));
        place(end);
        send_key(window, Qt::Key_V, Qt::ControlModifier);
        break;
      case 7: place(block.position()); send_key(window, Qt::Key_Up, Qt::AltModifier | Qt::ShiftModifier); break;
      case 8: send_key(window, Qt::Key_Z, Qt::ControlModifier); break;
      case 9: place(block.position()); send_key(window, Qt::Key_unknown, Qt::NoModifier, sentence.trimmed() + QLatin1Char(' ')); break;
    }
    const auto* layout = static_cast<const ScreenplayLayout*>(document.documentLayout());
    if (!layout->error().isEmpty())
      throw std::runtime_error("edit " + std::to_string(edit) + ": " + layout->error().toStdString());
    const auto after = pages_from_end(document);
    for (std::size_t i = 0; i < std::min(before.size(), after.size()); ++i) moved += before[i] && after[i] && before[i] != after[i];
  }
  return moved;
}

// Writes the editor's line dump and exports the PDF of the same document.
int agreement(const char* path, const QString& directory, int edits, bool snapshot = false, bool include_title = true) {
  auto opened = open_document(path);
  auto& document = opened.document;
  apollo::editor::PdfExportJob job(document.get(), QStringLiteral(APOLLO_FONT_DIR), exporter(), QString::fromLocal8Bit(path));
  QQuickView view;
  const auto destination = QDir(directory).filePath(QStringLiteral("editor.pdf"));
  auto* editor = apollo::editor::show_editor(view, *document, &job, destination, opened.saver.get());
  view.resize(880, 1000);
  view.show();
  if (!wait_for_frame(view)) throw std::runtime_error("the window never rendered");
  editor->forceActiveFocus();
  const int moved = edits ? scripted_edits(view, *editor, *document, edits) : 0;
  const auto* layout = static_cast<const ScreenplayLayout*>(document->documentLayout());
  if (!layout->error().isEmpty()) throw std::runtime_error(layout->error().toStdString());
  QDir().mkpath(directory);
  QFile dump(QDir(directory).filePath(QStringLiteral("editor.tsv")));
  if (!dump.open(QIODevice::WriteOnly | QIODevice::Truncate)) throw std::runtime_error("cannot write the editor dump");
  QTextStream out(&dump);
  out << "pages\t" << layout->pageCount() << '\t' << (layout->has_title_page() ? 1 : 0) << '\n';
  for (const auto& line : apollo::editor::shown_lines(*document))
    out << line.page << '\t' << line.slot << '\t' << line.kind << '\t' << QString::number(line.x, 'f', 4) << '\t' << line.text << '\n';
  out.flush();
  const auto original = document->toPlainText();
  const int original_pages = layout->pageCount();
  bool success = false;
  QObject::connect(&job, &apollo::editor::PdfExportJob::completed, &view, [&](bool ok) { success = ok; });
  if (!job.request(destination, include_title)) throw std::runtime_error(job.status().toStdString());
  if (snapshot) {
    // Real editor typing, caret movement, undo and redo after the request.
    // The resulting live text cannot be exported (unsupported glyph) and
    // repaginates the multi-page fixtures. The accepted snapshot must survive.
    send_key(view, Qt::Key_A, Qt::ControlModifier);
    send_key(view, Qt::Key_unknown, Qt::NoModifier, QString::fromUtf8("Changed after request 🚀."));
    send_key(view, Qt::Key_Left);
    send_key(view, Qt::Key_Z, Qt::ControlModifier);
    if (document->toPlainText() != original) throw std::runtime_error("undo during export did not restore the document");
    send_key(view, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    if (document->toPlainText() == original) throw std::runtime_error("redo during export did not change the document");
    if (original_pages > 1 && layout->pageCount() >= original_pages) throw std::runtime_error("snapshot edit did not repaginate");
  }
  QElapsedTimer deadline; deadline.start();
  while (job.busy() && deadline.elapsed() < 15000) idle(5);
  if (job.busy() || !success) throw std::runtime_error(job.status().toStdString());
  std::cout << "snapshot capture/dispatch " << job.capture_ms() << " ms, total " << job.total_ms() << " ms\n";
  std::cout << document->blockCount() << " elements, " << original_pages << " pages, " << edits << " edits, "
            << moved << " later elements changed page\n";
  return 0;
}

// Appends whole lines with one write each, so a line is complete in the
// file even if the process is killed straight after.
class EventLog {
public:
  explicit EventLog(const QString& path) : fd_(::open(QFile::encodeName(path).constData(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644)) {
    if (fd_ < 0) throw std::runtime_error("cannot open the event log");
  }
  ~EventLog() { ::close(fd_); }
  void line(const QString& text) const {
    const auto bytes = (text + QLatin1Char('\n')).toUtf8();
    if (::write(fd_, bytes.constData(), static_cast<std::size_t>(bytes.size())) != bytes.size()) std::perror("event log");
  }
private:
  int fd_;
};

const char* phase_name(Autosaver::Phase phase) {
  switch (phase) {
    case Autosaver::Phase::capturing: return "capturing";
    case Autosaver::Phase::captured: return "captured";
    case Autosaver::Phase::writing: return "writing";
    case Autosaver::Phase::written: return "written";
    case Autosaver::Phase::committed: return "committed";
    case Autosaver::Phase::acknowledged: return "acknowledged";
    case Autosaver::Phase::copying: return "copying";
    case Autosaver::Phase::copied: return "copied";
    case Autosaver::Phase::recovering: return "recovering";
    case Autosaver::Phase::recovered: return "recovered";
    case Autosaver::Phase::reloading: return "reloading";
    case Autosaver::Phase::snapshotting: return "snapshotting";
    case Autosaver::Phase::snapshotted: return "snapshotted";
  }
  return "unknown";
}

// Types into a project at a steady rate with autosave, logging every key
// and save phase with its monotonic time for tests/autosave_crash_tests.py.
// KILL_AT (PHASE:N) kills the process at the Nth occurrence of that phase.
int autosave_typing(const char* path, const QString& log_path, int keys, int interval_ms, const QString& kill_at) {
  if (keys <= 0 || interval_ms <= 0) throw std::runtime_error("KEYS and INTERVAL must be positive");
  // The hook uses these, so they outlive the saver.
  const EventLog log(log_path);
  const auto kill_phase = kill_at.section(QLatin1Char(':'), 0, 0);
  const int kill_count = kill_at.isEmpty() ? 0 : kill_at.section(QLatin1Char(':'), 1).toInt();
  if (!kill_at.isEmpty() && kill_count <= 0) throw std::runtime_error("--kill-at needs PHASE:N");
  std::atomic<int> seen{0};
  auto opened = open_document(path);
  if (!opened.saver) throw std::runtime_error("--autosave-typing needs a project folder");
  opened.saver->set_hook([&](Autosaver::Phase phase, std::uint64_t revision) {
    const auto name = QString::fromLatin1(phase_name(phase));
    log.line(QStringLiteral("phase\t%1\t%2\t%3").arg(name).arg(revision).arg(monotonic_ns()));
    if (name == kill_phase && ++seen == kill_count) {
      log.line(QStringLiteral("kill\t%1\t%2\t%3").arg(name).arg(revision).arg(monotonic_ns()));
      std::raise(SIGKILL);
    }
  });
  QQuickView view;
  auto* editor = apollo::editor::show_editor(view, *opened.document, nullptr, {}, opened.saver.get());
  view.resize(880, 1000);
  view.show();
  if (!wait_for_frame(view)) throw std::runtime_error("the window never rendered");
  int position;
  try {
    position = page_boundary_position(*opened.document);
  } catch (const std::runtime_error&) {
    position = opened.document->lastBlock().position() + opened.document->lastBlock().length() - 1;
  }
  const auto block = opened.document->findBlock(position);
  const QString typed = QStringLiteral("the lamp turns and ");
  // Offsets count code points, as in the source format.
  log.line(QStringLiteral("target\t%1\t%2\t%3\t%4").arg(block.blockFormat().stringProperty(apollo::editor::element_id_property))
               .arg(block.text().left(position - block.position()).toUcs4().size()).arg(typed).arg(interval_ms));
  editor->setProperty("cursorPosition", position);
  editor->forceActiveFocus();
  for (int i = 0; i < keys; ++i) {
    const QString text = typed.mid(i % typed.size(), 1);
    send_key(view, text == QStringLiteral(" ") ? Qt::Key_Space : static_cast<Qt::Key>(Qt::Key_A + (text[0].unicode() - 'a')),
             Qt::NoModifier, text);
    if (editor->property("cursorPosition").toInt() != position + i + 1) throw std::runtime_error("typing caret did not advance");
    log.line(QStringLiteral("key\t%1\t%2\t%3").arg(i + 1).arg(monotonic_ns()).arg(opened.saver->revision()));
    idle(interval_ms);
  }
  opened.saver->flush();
  QElapsedTimer deadline; deadline.start();
  while (opened.saver->state() == QStringLiteral("saving") && deadline.elapsed() < 10000) idle(5);
  log.line(QStringLiteral("done\t%1\t%2\t%3").arg(opened.saver->state()).arg(opened.saver->saved_revision()).arg(monotonic_ns()));
  if (opened.saver->state() != QStringLiteral("saved")) throw std::runtime_error(opened.saver->status().toStdString());
  return 0;
}
}

// Ordinary Fountain input previews lossless intake; projects open through safe
// access as a writer or a review-only reader. --retain-fountain captures a
// review-only project. The explicit spike DIR --create [FILE.fountain] uses
// the limited parser and exits. --autosave-typing types into an editable project
// for the crash checks. --check only lays the document out and
// reports whether every block matched the engine's line breaks. --measure
// times synthetic typing at a page boundary; see docs/spike-progress.md for
// what that does and does not establish. --agreement writes the editor's
// line dump and an asynchronous PDF snapshot after optional scripted edits.
// --snapshot changes/undoes/redoes the live document after requesting export;
// --measure-export repeats real exports while measuring typing and overlap.
int main(int argc, char** argv) {
  // Read options before QGuiApplication removes Qt's own options from argv.
  const std::string mode = argc >= 3 ? argv[2] : "";
  const bool check = mode == "--check", measuring = mode == "--measure", comparing = mode == "--agreement",
             snapshot = mode == "--snapshot", export_measuring = mode == "--measure-export", creating = mode == "--create",
             typing = mode == "--autosave-typing", retaining = mode == "--retain-fountain";
  if ((check || comparing || typing || creating || retaining) && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication application(argc, argv);
  const bool usage = retaining ? argc != 4 : check ? argc != 3 : measuring ? argc != 3 && argc != 4 : creating ? argc != 3 && argc != 4
      : typing ? argc < 4 || argc > 7
      : (comparing || snapshot || export_measuring) ? argc != 4 && argc != 5 : argc != 1 && argc != 2;
  if (usage) {
    std::cerr << "usage: apollo_editor [FILE.fountain|PROJECT] [--check | --measure [KEYS] | --agreement OUTDIR [EDITS] | --snapshot OUTDIR [--no-title-page] | --measure-export OUTDIR [KEYS]]\n"
                 "       apollo_editor PROJECT --create [FILE.fountain]\n"
                 "       apollo_editor PROJECT --retain-fountain FILE\n"
                 "       apollo_editor PROJECT --autosave-typing LOG [KEYS [INTERVAL_MS [PHASE:N]]]\n";
    return 2;
  }
  try {
    if (retaining) {
      const auto intake = apollo::source::read_fountain_intake(QString::fromLocal8Bit(argv[3]));
      const auto project = apollo::source::create_project(QString::fromLocal8Bit(argv[1]), {0, {{apollo::ElementType::action, ""}}}, intake.assets);
      std::cout << "retained for read-only review at " << project.directory.toStdString() << '\n';
      return 0;
    }
    if (creating) {
      std::vector<apollo::Element> elements{{apollo::ElementType::action, ""}};
      if (argc == 4) elements = apollo::import_fountain(read_fountain(argv[3]));
      const auto project = apollo::source::create_project(QString::fromLocal8Bit(argv[1]), {1, elements});
      std::cout << "created " << project.directory.toStdString() << '\n';
      return 0;
    }
    if (typing)
      return autosave_typing(argv[1], QString::fromLocal8Bit(argv[3]), argc >= 5 ? std::stoi(argv[4]) : 100,
                             argc >= 6 ? std::stoi(argv[5]) : 30, argc == 7 ? QString::fromLocal8Bit(argv[6]) : QString());
    if (measuring) return measure(argv[1], argc == 4 ? std::stoi(argv[3]) : 200);
    if (export_measuring) return measure(argv[1], argc == 5 ? std::stoi(argv[4]) : 200, QString::fromLocal8Bit(argv[3]));
    if (snapshot) {
      if (argc == 5 && std::string(argv[4]) != "--no-title-page") throw std::runtime_error("unknown snapshot option");
      return agreement(argv[1], QString::fromLocal8Bit(argv[3]), 0, true, argc != 5);
    }
    if (comparing) return agreement(argv[1], QString::fromLocal8Bit(argv[3]), argc == 5 ? std::stoi(argv[4]) : 0);
    // Instrumentation modes retain their explicit input and never update the
    // owner's recents/cursor. Ordinary projects use the safe access flow.
    const QString input = argc == 2 ? QString::fromLocal8Bit(argv[1]) : QString();
    const QFileInfo input_info(input);
    const bool fountain = !input.isEmpty() && input_info.isFile() &&
                         input_info.suffix().compare("fountain", Qt::CaseInsensitive) == 0;
    if (!check) {
      QQuickView view;
      apollo::editor::ProjectAccessFlow flow(view, apollo::editor::courier_prime(QStringLiteral(APOLLO_FONT_DIR)),
                                            QStringLiteral(APOLLO_FONT_DIR), exporter());
      flow.start(fountain ? QString() : input, fountain ? input : QString());
      view.setTitle(QStringLiteral("Apollo"));
      view.setMinimumSize(QSize(560, 400));
      view.resize(1280, 1000);
      view.show();
      return application.exec();
    }
    auto opened = open_document(argv[1]);
    auto& document = opened.document;
    const auto* layout = static_cast<ScreenplayLayout*>(document->documentLayout());
    if (check) {
      std::cout << document->blockCount() << " blocks, " << layout->pageCount() << " pages\n";
      if (!layout->error().isEmpty()) throw std::runtime_error(layout->error().toStdString());
      return 0;
    }
    apollo::editor::PdfExportJob job(document.get(), QStringLiteral(APOLLO_FONT_DIR), exporter(), QString::fromLocal8Bit(argv[1]));
    QQuickView view;
    apollo::editor::show_editor(view, *document, &job, opened.export_destination, opened.saver.get());
    if (opened.saver) opened.saver->guard_close(view);
    view.setTitle(QStringLiteral("Apollo editor"));
    view.setMinimumSize(QSize(560, 400));
    view.resize(1280, 1000);
    view.show();
    return application.exec();
  } catch (const std::exception& error) {
    std::cerr << "apollo_editor failed: " << error.what() << '\n'; return 1;
  }
}
