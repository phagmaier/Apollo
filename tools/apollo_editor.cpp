#include "editor/editor_view.h"
#include "editor/screenplay_layout.h"
#include "export/pdf.h"
#include "fountain/import.h"

#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQuickWindow>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextStream>
#include <QTimer>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using apollo::editor::ScreenplayLayout;

std::unique_ptr<QTextDocument> open_document(const char* path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open input");
  std::ostringstream source; source << input.rdbuf();
  return apollo::editor::screenplay_document(apollo::import_fountain(source.str()),
                                             apollo::editor::courier_prime(QStringLiteral(APOLLO_FONT_DIR)));
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

double pss_mib() {
  QFile file(QStringLiteral("/proc/self/smaps_rollup"));
  if (!file.open(QIODevice::ReadOnly)) return -1;
  for (const auto& line : file.readAll().split('\n'))
    if (line.startsWith("Pss:")) return line.simplified().split(' ').at(1).toDouble() / 1024;
  return -1;
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
int measure(const char* path, int keys) {
  QElapsedTimer clock; clock.start();
  auto document = open_document(path);
  QQuickView view;
  auto* editor = apollo::editor::show_editor(view, *document);
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
  std::vector<double> frame_ms, handled_ms, relayout_ms;
  int missed = 0;
  for (int i = 0; i < keys; ++i) {
    const QString text = typed.mid(i % typed.size(), 1);
    const auto key = text == QStringLiteral(" ") ? Qt::Key_Space : static_cast<Qt::Key>(Qt::Key_A + (text[0].unicode() - 'a'));
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, text), release(QEvent::KeyRelease, key, Qt::NoModifier, text);
    const auto before = layout->relayout_nanoseconds();
    clock.restart();
    QGuiApplication::sendEvent(&view, &press);
    QGuiApplication::sendEvent(&view, &release);
    handled_ms.push_back(static_cast<double>(clock.nsecsElapsed()) / 1e6);
    relayout_ms.push_back(static_cast<double>(layout->relayout_nanoseconds() - before) / 1e6);
    if (wait_for_frame(view)) frame_ms.push_back(static_cast<double>(clock.nsecsElapsed()) / 1e6);
    else ++missed;
    idle(60);
  }
  if (frame_ms.empty()) throw std::runtime_error("no key produced a frame");
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
int agreement(const char* path, const QString& directory, int edits) {
  auto document = open_document(path);
  QQuickView view;
  auto* editor = apollo::editor::show_editor(view, *document);
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
  apollo::export_pdf(apollo::layout_document(apollo::editor::document_elements(*document)),
                     QDir(directory).filePath(QStringLiteral("editor.pdf")), QStringLiteral(APOLLO_FONT_DIR));
  std::cout << document->blockCount() << " elements, " << layout->pageCount() << " pages, " << edits << " edits, "
            << moved << " later elements changed page\n";
  return 0;
}
}

// Opens a Fountain file in the editor. --check only lays the document out and
// reports whether every block matched the engine's line breaks. --measure
// times synthetic typing at a page boundary; see docs/spike-progress.md for
// what that does and does not establish. --agreement writes the editor's
// line dump and the PDF of the same document, after optional scripted edits.
int main(int argc, char** argv) {
  // Read options before QGuiApplication removes Qt's own options from argv.
  const std::string mode = argc >= 3 ? argv[2] : "";
  const bool check = mode == "--check", measuring = mode == "--measure", comparing = mode == "--agreement";
  if ((check || comparing) && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication application(argc, argv);
  const bool usage = check ? argc != 3 : measuring ? argc != 3 && argc != 4 : comparing ? argc != 4 && argc != 5 : argc != 2;
  if (usage) {
    std::cerr << "usage: apollo_editor FILE.fountain [--check | --measure [KEYS] | --agreement OUTDIR [EDITS]]\n";
    return 2;
  }
  try {
    if (measuring) return measure(argv[1], argc == 4 ? std::stoi(argv[3]) : 200);
    if (comparing) return agreement(argv[1], QString::fromLocal8Bit(argv[3]), argc == 5 ? std::stoi(argv[4]) : 0);
    auto document = open_document(argv[1]);
    const auto* layout = static_cast<ScreenplayLayout*>(document->documentLayout());
    if (check) {
      std::cout << document->blockCount() << " blocks, " << layout->pageCount() << " pages\n";
      if (!layout->error().isEmpty()) throw std::runtime_error(layout->error().toStdString());
      return 0;
    }
    QQuickView view;
    apollo::editor::show_editor(view, *document);
    view.setTitle(QStringLiteral("Apollo editor"));
    view.resize(880, 1000);
    view.show();
    return application.exec();
  } catch (const std::exception& error) {
    std::cerr << "apollo_editor failed: " << error.what() << '\n'; return 1;
  }
}
