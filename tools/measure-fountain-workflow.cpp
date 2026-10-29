// Disposable real-QML workflow measurement. Never log source, paths or diagnostics.
#include "editor/autosave.h"
#include "editor/editor_commands.h"
#include "editor/fountain_export_job.h"
#include "editor/pdf_export_job.h"
#include "editor/project_access_flow.h"
#include "editor/screenplay_layout.h"
#include "source/project.h"

#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickView>
#include <QScopeGuard>
#include <QSGRendererInterface>
#include <QTextBlock>
#include <QTextLayout>
#include <QTimer>
#include <QtTest/QTest>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {
using namespace apollo;
using namespace apollo::editor;
using namespace apollo::source;
using Clock = std::chrono::steady_clock;
qint64 now_ns() { return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count(); }
double ms(qint64 ns) { return static_cast<double>(ns) / 1e6; }
struct Failure { const char* code; };
void require(bool ok, const char* code) { if (!ok) throw Failure{code}; }
std::atomic<int> qt_messages{0};
qint64 deadline = 0;

void wait_until(const std::function<bool()>& ready, const char* failure, int timeout_ms = 30000) {
  const auto end = std::min(deadline, now_ns() + static_cast<qint64>(timeout_ms) * 1000000);
  while (!ready()) {
    require(now_ns() < end, failure);
    QEventLoop loop;
    QTimer::singleShot(2, &loop, &QEventLoop::quit);
    loop.exec();
  }
}

QByteArray bytes(const QString& path) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "artifact_read_failed");
  const auto data = file.readAll();
  require(file.error() == QFileDevice::NoError, "artifact_read_failed");
  return data;
}

struct Original {
  QString path;
  struct stat identity{};
  QByteArray digest;
  qint64 size = 0;
  explicit Original(const QString& input) : path(QFileInfo(input).canonicalFilePath()) {
    const auto name = QFile::encodeName(path);
    require(!path.isEmpty() && ::stat(name.constData(), &identity) == 0 && S_ISREG(identity.st_mode), "source_unavailable");
    size = identity.st_size;
    digest = hash();
  }
  QByteArray hash() const {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "source_unavailable");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    require(hash.addData(&file) && file.error() == QFileDevice::NoError, "source_read_failed");
    return hash.result();
  }
  bool unchanged() const {
    struct stat current{};
    const auto name = QFile::encodeName(path);
    return ::stat(name.constData(), &current) == 0 && current.st_dev == identity.st_dev && current.st_ino == identity.st_ino &&
        current.st_size == identity.st_size && current.st_mode == identity.st_mode &&
        current.st_mtim.tv_sec == identity.st_mtim.tv_sec && current.st_mtim.tv_nsec == identity.st_mtim.tv_nsec &&
        current.st_ctim.tv_sec == identity.st_ctim.tv_sec && current.st_ctim.tv_nsec == identity.st_ctim.tv_nsec && hash() == digest;
  }
};

QString private_directory(const QString& path, bool allow_existing = false) {
  const auto absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
  const auto name = QFile::encodeName(absolute);
  struct stat state{};
  if (::lstat(name.constData(), &state) != 0) {
    require(::mkdir(name.constData(), 0700) == 0, "output_creation_failed");
    require(::lstat(name.constData(), &state) == 0, "output_creation_failed");
  } else require(allow_existing, "output_already_exists");
  require(S_ISDIR(state.st_mode) && state.st_uid == ::geteuid() && (state.st_mode & 077) == 0, "output_not_private_directory");
  return QFileInfo(absolute).canonicalFilePath();
}

void isolate_state(const QString& root) {
  const auto xdg = private_directory(root + "/xdg");
  const std::pair<const char*, const char*> folders[] = {
      {"XDG_DATA_HOME", "data"}, {"XDG_CONFIG_HOME", "config"}, {"XDG_STATE_HOME", "state"}, {"XDG_CACHE_HOME", "cache"}};
  for (const auto& [variable, folder] : folders)
    qputenv(variable, QFile::encodeName(private_directory(xdg + '/' + QString::fromLatin1(folder))));
  // Keep the real runtime directory solely for Wayland/input-method sockets.
  qputenv("QML_DISK_CACHE_PATH", QFile::encodeName(private_directory(xdg + "/qml-cache")));
}

QJsonObject distribution(std::vector<double> values) {
  if (values.empty()) return {{"samples", 0}, {"p50_ms", QJsonValue::Null}, {"p95_ms", QJsonValue::Null}, {"max_ms", QJsonValue::Null}};
  std::sort(values.begin(), values.end());
  const auto percentile = [&](double p) { return values[static_cast<std::size_t>(p * static_cast<double>(values.size() - 1) + .5)]; };
  return {{"samples", static_cast<int>(values.size())}, {"p50_ms", percentile(.5)}, {"p95_ms", percentile(.95)}, {"max_ms", values.back()}};
}

double pss_mib() {
  QFile file(QStringLiteral("/proc/self/smaps_rollup"));
  if (!file.open(QIODevice::ReadOnly)) return -1;
  for (const auto& line : file.readAll().split('\n'))
    if (line.startsWith("Pss:")) return line.simplified().split(' ').at(1).toDouble() / 1024;
  return -1;
}

// A token is accepted only after a scene-graph synchronization following its
// mutation, then frameSwapped. An older in-flight frame cannot acknowledge it.
// frameSwapped queues presentation; it is not compositor/device input latency.
class Frames {
  QQuickView& view_;
  struct State {
    std::atomic<quint64> desired{0}, synchronized{0}, presented{0};
    std::atomic<qint64> presented_ns{0};
  };
  std::shared_ptr<State> state_ = std::make_shared<State>();
  QMetaObject::Connection sync_, swap_;
public:
  explicit Frames(QQuickView& view) : view_(view) {
    sync_ = QObject::connect(&view, &QQuickWindow::afterSynchronizing, &view, [state = state_] {
      state->synchronized.store(state->desired.load());
    }, Qt::DirectConnection);
    swap_ = QObject::connect(&view, &QQuickWindow::frameSwapped, &view, [state = state_] {
      const auto token = state->synchronized.load();
      if (token > state->presented.load()) {
        state->presented_ns.store(now_ns());
        state->presented.store(token);
      }
    }, Qt::DirectConnection);
  }
  ~Frames() { QObject::disconnect(sync_); QObject::disconnect(swap_); }
  qint64 next() {
    const auto token = state_->desired.fetch_add(1) + 1;
    view_.update();
    wait_until([&] { return state_->presented.load() >= token; }, "frame_not_observed", 5000);
    return state_->presented_ns.load();
  }
};

QQuickItem* find_item(QQuickItem* root, const QString& name) {
  if (!root) return nullptr;
  if (root->objectName() == name) return root;
  if (auto* item = root->findChild<QQuickItem*>(name)) return item;
  for (auto* child : root->childItems()) if (auto* found = find_item(child, name)) return found;
  return nullptr;
}

class Controls {
  QQuickView& view_;
  Frames& frames_;
  bool native_, focus_armed_ = false, focus_lost_ = false;
  QMetaObject::Connection focus_connection_;
public:
  Controls(QQuickView& view, Frames& frames, bool native) : view_(view), frames_(frames), native_(native) {
    focus_connection_ = QObject::connect(&view, &QWindow::activeChanged, &view, [this] {
      if (native_ && focus_armed_ && !view_.isActive()) focus_lost_ = true;
    });
  }
  ~Controls() { QObject::disconnect(focus_connection_); }
  void arm_focus() {
    if (native_) {
      view_.requestActivate();
      wait_until([&] { return view_.isActive(); }, "native_focus_not_acquired", 5000);
    }
    focus_armed_ = true;
  }
  void focus_ok() const { require(!native_ || (!focus_lost_ && view_.isActive()), "native_focus_lost"); }
  QQuickItem* item(const char* name) const {
    auto* found = find_item(view_.contentItem(), QString::fromLatin1(name));
    require(found != nullptr, "qml_control_missing");
    return found;
  }
  void text(const char* name, const QString& value) { require(item(name)->setProperty("text", value), "qml_field_unwritable"); }
  // Scroll and polish before timing the actual mouse dispatch, not within it.
  QPoint prepare_click(const char* name) {
    focus_ok();
    auto* target = item(name);
    require(target->isVisible() && target->isEnabled(), "qml_control_unavailable");
    auto* content = target->parentItem();
    while (content && !content->inherits("QQuickFlickable")) content = content->parentItem();
    if (content) {
      const auto y = target->mapToItem(content, QPointF()).y() + content->property("contentY").toReal();
      const auto maximum = qMax(0.0, content->property("contentHeight").toReal() - content->height());
      content->setProperty("contentY", qBound(0.0, y - 40, maximum));
    }
    frames_.next();
    const auto point = target->mapToScene(target->boundingRect().center()).toPoint();
    require(view_.contentItem()->contains(point), "qml_control_outside_window");
    return point;
  }
  void click_at(QPoint point) { focus_ok(); QTest::mouseClick(&view_, Qt::LeftButton, Qt::NoModifier, point, 0); }
  void click(const char* name) { click_at(prepare_click(name)); }
};

struct SaveEvent { qint64 mutation = 0, capture_start = 0, captured = 0, writing = 0, committed = 0, ack = 0; };
class Saves {
  struct State {
    std::mutex mutex;
    std::map<std::uint64_t, SaveEvent> events;
  };
  std::shared_ptr<State> state_ = std::make_shared<State>();
  QMetaObject::Connection mutation_;
public:
  Saves(QTextDocument& doc, Autosaver& saver) {
    mutation_ = QObject::connect(&doc, &QTextDocument::contentsChange, &saver, [state = state_, &saver](int, int, int) {
      std::lock_guard lock(state->mutex);
      state->events[saver.revision()].mutation = now_ns();
    });
    saver.set_hook([state = state_](Autosaver::Phase phase, std::uint64_t revision) {
      const auto time = now_ns();
      std::lock_guard lock(state->mutex);
      auto& event = state->events[revision];
      if (phase == Autosaver::Phase::capturing) event.capture_start = time;
      if (phase == Autosaver::Phase::captured) event.captured = time;
      if (phase == Autosaver::Phase::writing) event.writing = time;
      if (phase == Autosaver::Phase::committed) event.committed = time;
      if (phase == Autosaver::Phase::acknowledged)
        for (auto& [number, pending] : state->events)
          if (number <= revision && pending.mutation && !pending.ack) pending.ack = time;
    });
  }
  // The worker owns shared state even when a failure unwinds this observer.
  ~Saves() { QObject::disconnect(mutation_); }
  QJsonObject metrics() {
    std::lock_guard lock(state_->mutex);
    std::vector<double> acknowledgments, captures, writes;
    for (const auto& [revision, event] : state_->events) {
      (void)revision;
      if (event.mutation) {
        require(event.ack >= event.mutation, "autosave_acknowledgment_missing");
        acknowledgments.push_back(ms(event.ack - event.mutation));
      }
      if (event.capture_start && event.captured) captures.push_back(ms(event.captured - event.capture_start));
      if (event.writing && event.committed) writes.push_back(ms(event.committed - event.writing));
    }
    require(!acknowledgments.empty() && !captures.empty() && !writes.empty(), "autosave_measurements_missing");
    return {{"mutation_to_ack", distribution(acknowledgments)}, {"capture", distribution(captures)},
            {"worker_serialize_and_atomic_save", distribution(writes)}};
  }
};

void saved(ProjectAccessFlow& flow) {
  wait_until([&] {
    require(flow.autosaver() && flow.autosaver()->state() != QStringLiteral("failed"), "autosave_failed");
    return flow.autosaver()->state() == QStringLiteral("saved") && flow.autosaver()->saved_revision() == flow.autosaver()->revision();
  }, "autosave_not_verified");
}

void exact_model(const std::vector<Element>& expected, const QTextDocument& document) {
  require(document_elements(document) == expected, "in_memory_model_changed_unexpectedly");
  auto* layout = qobject_cast<ScreenplayLayout*>(document.documentLayout());
  require(layout && layout->error().isEmpty(), "layout_not_verified");
}

struct Site { int position = -1, block = -1; bool boundary = false; };
Site typing_site(const QTextDocument& document, bool prefer_boundary) {
  const auto* layout = static_cast<ScreenplayLayout*>(document.documentLayout());
  const double middle = layout->documentHeight() / 2;
  Site best;
  double score = 1e100;
  for (auto block = document.begin(); block.isValid(); block = block.next()) {
    const auto element = block_element(block);
    if ((element.type != ElementType::action && element.type != ElementType::dialogue) || !element.styles.empty() || block.text().isEmpty()) continue;
    const auto* text = block.layout();
    const auto top = layout->blockBoundingRect(block).top();
    for (int i = 0; i < text->lineCount(); ++i) {
      const auto line = text->lineAt(i);
      const auto y = top + line.y();
      const auto within_page = std::fmod(y, page_stride);
      const bool boundary = (i && line.y() - text->lineAt(i - 1).y() > page_gap) || within_page < 130 || within_page > page_height - 130;
      const auto candidate = std::abs(y - middle) + (prefer_boundary && !boundary ? layout->documentHeight() * 2 : 0);
      if (candidate < score) { score = candidate; best = {block.position() + line.textStart(), block.blockNumber(), boundary}; }
    }
  }
  require(best.position >= 0, "representative_unstyled_typing_site_missing");
  return best;
}

int correct_empty_speeches(EditorCommands& commands, QTextDocument& document, QQuickView& view, Controls& controls) {
  int count = 0;
  auto expected = document_elements(document);
  const int limit = static_cast<int>(expected.size());
  while (!commands.emptySpeeches().isEmpty()) {
    require(count < limit, "empty_speech_correction_not_converging");
    const auto finding = commands.emptySpeeches().front().toMap();
    require(finding.value("canTreatAsAction").toBool(), "empty_speech_requires_manual_resolution");
    const auto id = finding.value("id").toString();
    const auto before = expected;
    const auto found = std::find_if(expected.begin(), expected.end(), [&](const Element& e) { return e.id == id.toStdString(); });
    require(found != expected.end() && found->type == ElementType::cue, "empty_speech_identity_missing");
    found->type = ElementType::action;
    for (auto next = found + 1; next != expected.end() && next->type == ElementType::parenthetical; ++next) next->type = ElementType::action;
    controls.click("reviewEmptySpeeches");
    wait_until([&] { return find_item(view.contentItem(), "emptySpeechIssue0") != nullptr; }, "empty_speech_picker_missing");
    controls.click("emptySpeechIssue0");
    require(view.rootObject()->property("emptySpeechId").toString() == id, "empty_speech_selection_not_verified");
    controls.click("treatSpeechAsAction");
    exact_model(expected, document);
    QTest::keyClick(&view, Qt::Key_Z, Qt::ControlModifier);
    exact_model(before, document);
    QTest::keyClick(&view, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    exact_model(expected, document);
    ++count;
  }
  return count;
}

QJsonObject export_panel(QQuickView& view, Controls& controls, FountainExportJob& job, const QString& destination) {
  controls.click("exportButton");
  controls.click("exportFormatFountain");
  require(job.check(true).isEmpty(), "fountain_snapshot_ineligible");
  controls.text("fountainDestination", destination);
  const auto point = controls.prepare_click("exportConfirm");
  auto verified = std::make_shared<std::atomic<qint64>>(0);
  job.set_worker_hook([verified] { verified->store(now_ns()); });
  bool completed = false, success = false;
  qint64 busy_ns = 0, complete_ns = 0;
  const auto changed = QObject::connect(&job, &FountainExportJob::changed, &view, [&] {
    if (job.busy() && !busy_ns) busy_ns = now_ns();
  });
  const auto done = QObject::connect(&job, &FountainExportJob::completed, &view, [&](bool ok) {
    success = ok; completed = true; complete_ns = now_ns();
  });
  const auto disconnect = qScopeGuard([&] { QObject::disconnect(changed); QObject::disconnect(done); });
  const auto request_ns = now_ns();
  controls.click_at(point);
  const auto returned_ns = now_ns();
  require(job.busy() || completed, "fountain_panel_did_not_request_export");
  wait_until([&] { return completed; }, "fountain_export_timeout");
  require(success && !job.busy() && busy_ns && verified->load(), "fountain_export_not_verified");
  job.set_worker_hook({});
  require(QFileInfo(destination).isFile() && QFileInfo(destination).size() > 0, "fountain_export_missing");
  return {{"request_to_complete_ms", ms(complete_ns - request_ns)},
          {"mouse_handler_capture_dispatch_upper_bound_ms", ms(returned_ns - request_ns)},
          {"request_to_busy_signal_ms", ms(busy_ns - request_ns)},
          {"busy_signal_to_worker_verified_ms", ms(verified->load() - busy_ns)},
          {"worker_verified_to_gui_complete_ms", ms(complete_ns - verified->load())},
          {"worker_runtime_ms", QJsonValue::Null},
          {"timing_limits", "Busy signal precedes worker launch; verified hook follows serialization and verification, before atomic write. No worker-start/worker-end instrumentation exists."}};
}

QJsonObject run(const Original& original, const QString& root, int keys) {
  const auto platform = QGuiApplication::platformName();
  const bool native = platform == QStringLiteral("wayland");
  require(native || platform == QStringLiteral("offscreen"), "unsupported_platform_use_wayland_or_explicit_offscreen");
  QQuickView view;
  view.setTitle(QStringLiteral("Apollo disposable workflow measurement"));
  view.resize(1280, 1000);
  Frames frames(view);
  Controls controls(view, frames, native);
  ProjectAccessFlow flow(view, courier_prime(QStringLiteral(APOLLO_FONT_DIR)), QStringLiteral(APOLLO_FONT_DIR),
                         QCoreApplication::applicationDirPath() + "/pdf_export", root + "/session.json");
  flow.start();
  view.show();
  frames.next();
  controls.arm_focus();
  controls.click("showImport");
  controls.text("importSourcePath", original.path);
  const auto preview_point = controls.prepare_click("previewImport");
  const auto preview_start = now_ns();
  controls.click_at(preview_point);
  wait_until([&] { return !flow.busy(); }, "import_preview_timeout");
  require(flow.intake_preview() && !flow.document(), "import_preview_not_ready");
  const double preview_ms = ms(frames.next() - preview_start);
  auto* source = controls.item("intakeSource");
  auto reader_text = flow.intake_source();
  reader_text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  reader_text.replace('\r', '\n');
  require(source->property("readOnly").toBool() && source->property("text").toString() == reader_text, "actual_import_reader_not_verified");
  require(flow.metaObject()->indexOfProperty("intakeNormalizeSceneSpacing") >= 0 &&
          flow.metaObject()->indexOfProperty("intakeSceneSpacingCount") >= 0, "scene_spacing_option_unavailable");
  auto* normalize = controls.item("normalizeSceneSpacing");
  require(!normalize->property("checked").toBool() && !flow.property("intakeNormalizeSceneSpacing").toBool(), "scene_spacing_normalization_not_explicit");
  const int scene_gaps = flow.property("intakeSceneSpacingCount").toInt();
  require(scene_gaps >= 0, "invalid_scene_spacing_count");
  if (scene_gaps) {
    controls.click("normalizeSceneSpacing");
    require(normalize->property("checked").toBool() && flow.property("intakeNormalizeSceneSpacing").toBool(), "scene_spacing_option_not_applied");
  }
  require(flow.intake_edit_supported(), "editable_import_gate_refused");
  const auto projects = private_directory(root + "/projects");
  controls.text("intakeProjectName", QStringLiteral("Measured Copy"));
  controls.text("intakeProjectParent", projects);
  const auto publication_point = controls.prepare_click("editIntake");
  const auto publication_start = now_ns();
  controls.click_at(publication_point);
  wait_until([&] { return !flow.busy(); }, "editable_publication_timeout");
  require(flow.document() && flow.autosaver() && flow.editor() && flow.intake_working(), "editable_writer_not_published");
  frames.next();
  const double publication_ms = ms(now_ns() - publication_start);
  const double writer_ready_pss = pss_mib();
  const auto project = flow.autosaver()->location();
  require(project.startsWith(projects + '/'), "project_outside_disposable_output");
  auto initial = open_project(project);
  require(initial.project.import_working && initial.assets.original_fountain &&
          QCryptographicHash::hash(*initial.assets.original_fountain, QCryptographicHash::Sha256) == original.digest, "retained_original_not_exact");
  exact_model(initial.screenplay.elements, *flow.document());
  auto* commands = qobject_cast<EditorCommands*>(view.rootObject()->property("commands").value<QObject*>());
  auto* fountain = qobject_cast<FountainExportJob*>(view.rootObject()->property("fountainJob").value<QObject*>());
  auto* pdf = qobject_cast<PdfExportJob*>(view.rootObject()->property("exportJob").value<QObject*>());
  require(commands && fountain && pdf, "actual_writer_jobs_missing");
  require(commands->emptySpeeches().isEmpty() || !fountain->check(true).isEmpty(), "unfinished_empty_speech_export_not_blocked");
  // Keep the measurements through Home's asynchronous worker drain.
  auto saves = std::make_unique<Saves>(*flow.document(), *flow.autosaver());
  const int empty_corrections = correct_empty_speeches(*commands, *flow.document(), view, controls);
  require(fountain->check(true).isEmpty(), "corrected_snapshot_still_ineligible");
  saved(flow);
  std::vector<double> handled, key_frames;
  handled.reserve(static_cast<std::size_t>(keys));
  key_frames.reserve(static_cast<std::size_t>(keys));
  int boundary_sites = 0, middle_sites = 0;
  std::vector<QString> sample;
  sample.reserve(12);
  for (const auto ch : QStringLiteral("measurement ")) sample.emplace_back(ch);
  int typed = 0;
  for (int part = 0; part < (keys > 1 ? 2 : 1); ++part) {
    const int count = part == 0 && keys > 1 ? keys / 2 : keys - typed;
    const auto site = typing_site(*flow.document(), part == 0);
    boundary_sites += site.boundary;
    middle_sites += !site.boundary;
    auto expected = document_elements(*flow.document());
    const auto block = flow.document()->findBlockByNumber(site.block);
    const auto offset = element_text(block.text().left(site.position - block.position())).size();
    flow.editor()->setProperty("cursorPosition", site.position);
    flow.editor()->forceActiveFocus();
    frames.next();
    QString inserted;
    inserted.reserve(count);
    for (int i = 0; i < count; ++i) {
      controls.focus_ok();
      require(flow.editor()->hasActiveFocus(), "typing_editor_focus_lost");
      require(now_ns() < deadline, "overall_deadline_exceeded");
      const auto& text = sample[static_cast<std::size_t>(typed) % sample.size()];
      const auto key = text == QStringLiteral(" ") ? Qt::Key_Space : static_cast<Qt::Key>(text.front().toUpper().unicode());
      QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, text), release(QEvent::KeyRelease, key, Qt::NoModifier, text);
      const auto revision = flow.autosaver()->revision();
      const auto start = now_ns();
      QGuiApplication::sendEvent(&view, &press);
      QGuiApplication::sendEvent(&view, &release);
      handled.push_back(ms(now_ns() - start));
      require(flow.autosaver()->revision() > revision && flow.editor()->property("cursorPosition").toInt() == site.position + i + 1, "measurement_key_not_applied");
      key_frames.push_back(ms(frames.next() - start));
      controls.focus_ok();
      inserted += text;
      ++typed;
    }
    expected[static_cast<std::size_t>(site.block)].text.insert(offset, inserted.toStdString());
    exact_model(expected, *flow.document());
  }
  saved(flow);
  const auto expected = document_elements(*flow.document());
  const double typed_pss = pss_mib();
  const auto saved_project = open_project(project);
  require(saved_project.screenplay.elements == expected && saved_project.screenplay.revision == flow.autosaver()->saved_revision() &&
          saved_project.assets == initial.assets, "saved_model_or_import_assets_changed");
  const auto* layout = static_cast<ScreenplayLayout*>(flow.document()->documentLayout());
  const int pages = layout->pageCount();
  const auto api = view.rendererInterface()->graphicsApi();
  const char* renderer = api == QSGRendererInterface::Software ? "software" : api == QSGRendererInterface::OpenGL ? "OpenGL" :
      api == QSGRendererInterface::Vulkan ? "Vulkan" : "other";
  frames.next();
  require(view.grabWindow().save(root + "/writer.png"), "private_screenshot_failed");
  const auto sentinel_path = root + "/blocked-existing.pdf";
  const QByteArray sentinel("Disposable PDF destination must remain unchanged.\n");
  replace_file(sentinel_path, sentinel);
  require(!pdf->request(sentinel_path) && !pdf->busy() && bytes(sentinel_path) == sentinel, "imported_pdf_gate_or_destination_changed");
  const auto first_export = root + "/edited.fountain";
  const auto export_metrics = export_panel(view, controls, *fountain, first_export);
  const auto exported = bytes(first_export);
  exact_model(expected, *flow.document());
  saved(flow);
  controls.click("goHome");
  wait_until([&] { return !flow.document() && !flow.busy() && !flow.leaving(); }, "home_did_not_drain_writer");
  require(!flow.leave_failed(), "home_save_drain_failed");
  const auto save_metrics = saves->metrics();
  saves.reset();
  const auto home_project = open_project(project);
  require(home_project.screenplay == saved_project.screenplay && home_project.assets == initial.assets, "home_changed_saved_model");
  controls.text("openLocation", project);
  const auto reopen_point = controls.prepare_click("openProject");
  const auto reopen_start = now_ns();
  controls.click_at(reopen_point);
  wait_until([&] { return !flow.busy(); }, "project_reopen_timeout");
  require(flow.document() && flow.autosaver() && flow.intake_working(), "reopened_writer_missing");
  frames.next();
  const double reopen_ms = ms(now_ns() - reopen_start);
  exact_model(expected, *flow.document());
  saved(flow);
  const auto reopened_project = open_project(project);
  require(reopened_project.screenplay.elements == expected && reopened_project.assets == initial.assets, "reopened_saved_model_changed");
  fountain = qobject_cast<FountainExportJob*>(view.rootObject()->property("fountainJob").value<QObject*>());
  require(fountain, "reopened_fountain_job_missing");
  const auto reopened_export = root + "/reopened.fountain";
  const auto reexport_metrics = export_panel(view, controls, *fountain, reopened_export);
  require(bytes(reopened_export) == exported, "reopened_fountain_bytes_changed");
  exact_model(expected, *flow.document());
  controls.click("goHome");
  wait_until([&] { return !flow.document() && !flow.busy() && !flow.leaving(); }, "final_writer_drain_failed");
  require(original.unchanged(), "original_input_changed");
  controls.focus_ok();
  return {{"platform", platform}, {"renderer", renderer},
          {"rendering_limits", native ? "Synthetic QWindow key dispatch to synchronized frameSwapped; not device/compositor input latency." :
              "Offscreen Qt Quick runtime; frames are not native display/compositor evidence or native rendering performance."},
          {"preview_ready_ms", preview_ms}, {"editable_publication_writer_ready_ms", publication_ms}, {"reopen_ready_ms", reopen_ms},
          {"key_handling", distribution(handled)}, {"key_to_frame", distribution(key_frames)},
          {"key_to_frame_stalls_over_100_ms", static_cast<int>(std::count_if(key_frames.begin(), key_frames.end(), [](double n) { return n > 100; }))},
          {"unobserved_frames", 0}, {"keys", keys}, {"boundary_typing_sites", boundary_sites}, {"middle_typing_sites", middle_sites},
          {"autosave", save_metrics}, {"fountain_export", export_metrics}, {"reopened_fountain_export", reexport_metrics},
          {"pages", pages}, {"elements", static_cast<int>(expected.size())}, {"source_bytes", original.size}, {"export_bytes", exported.size()},
          {"normalized_scene_gaps", scene_gaps}, {"normalization_selected", scene_gaps > 0}, {"empty_speech_corrections", empty_corrections},
          {"correction_scope", "Disposable measurement-only cue/parenthetical-to-action corrections; not author acceptance."},
          {"pss_mib_writer_ready", writer_ready_pss}, {"pss_mib_after_typing", typed_pss}, {"pss_mib_after_workflow", pss_mib()},
          {"pdf_gate_verified", true}, {"original_unchanged", true},
          {"saved_and_reopened_model_exact_except_revision", true}, {"reexport_bytes_identical", true}, {"private_screenshot_saved", true}};
}
} // namespace

int main(int argc, char** argv) {
  ::umask(0077);
  const struct rlimit no_core{0, 0};
  ::setrlimit(RLIMIT_CORE, &no_core);
  qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString&) { ++qt_messages; });
  QJsonObject result{{"success", false}, {"harness", "fountain-workflow"}};
  QString aggregate_path;
  std::unique_ptr<Original> original;
  try {
    QString source, output;
    int keys = 120;
    for (int i = 1; i < argc; ++i) {
      const auto option = QString::fromLocal8Bit(argv[i]);
      require(i + 1 < argc, "invalid_arguments");
      const auto value = QString::fromLocal8Bit(argv[++i]);
      if (option == QStringLiteral("--source")) { require(source.isEmpty(), "invalid_arguments"); source = value; }
      else if (option == QStringLiteral("--output")) { require(output.isEmpty(), "invalid_arguments"); output = value; }
      else if (option == QStringLiteral("--keys")) { bool ok = false; keys = value.toInt(&ok); require(ok && keys > 0 && keys <= 2000, "invalid_key_count"); }
      else throw Failure{"invalid_arguments"};
    }
    require(!source.isEmpty() && !output.isEmpty(), "invalid_arguments");
    const auto root = private_directory(output, true);
    original = std::make_unique<Original>(source);
    require(!original->path.startsWith(root + '/'), "input_must_be_outside_disposable_output");
    const auto run_root = private_directory(root + "/run");
    aggregate_path = root + "/results.json";
    isolate_state(run_root);
    deadline = now_ns() + 240000000000LL;
    // The harness consumes its own options, not Qt's command-line parser.
    int qt_argc = 1;
    QGuiApplication application(qt_argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("apollo-workflow-measurement"));
    result = run(*original, run_root, keys);
    result.insert("success", true);
    result.insert("harness", "fountain-workflow");
  } catch (const Failure& failure) {
    result.insert("failure", QString::fromLatin1(failure.code));
  } catch (...) {
    // Exception messages can contain the private path or source diagnostics.
    result.insert("failure", "application_exception_redacted");
  }
  if (original) {
    try {
      const bool unchanged = original->unchanged();
      result.insert("original_unchanged", unchanged);
      if (!unchanged) { result.insert("success", false); result.insert("failure", "original_input_changed"); }
    } catch (...) { result.insert("success", false); result.insert("failure", "original_integrity_not_verified"); }
  }
  result.insert("qt_messages_redacted", qt_messages.load());
  const auto json = QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n';
  if (!aggregate_path.isEmpty()) {
    QFile file(aggregate_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(json) != json.size()) {
      result.insert("success", false);
      result.insert("failure", "aggregate_output_write_failed");
    }
  }
  const auto stdout_json = QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n';
  std::fwrite(stdout_json.constData(), 1, static_cast<std::size_t>(stdout_json.size()), stdout);
  return result.value("success").toBool() ? 0 : 1;
}
