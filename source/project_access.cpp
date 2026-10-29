#include "source/project_access.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <map>
#include <set>
#include <cerrno>
#include <sys/stat.h>

namespace apollo::source {
namespace {
[[noreturn]] void changed(const QString& path) {
  throw SourceError(SourceError::Kind::external_change,
                    QStringLiteral("The inspected project or recovery copy changed or is no longer available at %1. "
                                   "Open the project again to inspect the retained evidence.").arg(path));
}
bool same_project(const OpenedProject& a, const OpenedProject& b) {
  return a.project.id == b.project.id && a.project.screenplay_file == b.project.screenplay_file &&
         a.project.import_review == b.project.import_review &&
         a.project.import_working == b.project.import_working &&
         a.screenplay == b.screenplay && a.assets == b.assets;
}
bool valid_id(const QString& id) {
  return !QUuid::fromString(id).isNull() && QUuid::fromString(id).toString(QUuid::WithoutBraces) == id;
}
QString normalized(const QString& path) {
  const QFileInfo info(path);
  const auto canonical = info.canonicalFilePath();
  return canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : canonical;
}
}

struct ProjectAccess::State {
  // Unlike an editable-session baseline, damaged evidence can be absent,
  // unreadable, or non-regular. Remember those states rather than treating a
  // later readable replacement as permission to recover an older alternative.
  struct EntryMetadata {
    int error = 0;
    quint64 device = 0, inode = 0, mode = 0, owner = 0, group = 0, links = 0;
    qint64 size = 0, modified_seconds = 0, modified_nanos = 0, changed_seconds = 0, changed_nanos = 0;
    bool operator==(const EntryMetadata&) const = default;
  };
  struct EntryState {
    EntryMetadata metadata;
    QByteArray bytes;
    QFileDevice::FileError read_error = QFileDevice::NoError;
    bool readable = false;
    bool operator==(const EntryState&) const = default;
  };
  struct PackageState {
    ProjectSession::Identity directory;
    ProjectSession::FileState manifest, screenplay, metadata;
    bool operator==(const PackageState&) const = default;
  };
  struct InspectedPackage {
    RecoveryPackage package;
    PackageState state;
  };
  QString directory, error, project_id;
  SourceError::Kind error_kind = SourceError::Kind::io;
  std::uint64_t revision = 0;
  bool ready = false, consumed = false;
  std::unique_ptr<ProjectSession> original;
  std::vector<AccessCandidate> candidates;
  std::map<QString, PackageState> packages;
  std::optional<EntryState> non_directory, followed_target;
  std::map<QString, EntryState> original_entries;

  static EntryMetadata metadata(const QString& path, bool follow = false) {
    struct stat info {};
    const auto name = QFile::encodeName(path);
    if ((follow ? ::stat(name.constData(), &info) : ::lstat(name.constData(), &info)) != 0) {
      EntryMetadata result;
      result.error = errno;
      return result;
    }
    EntryMetadata result{0, static_cast<quint64>(info.st_dev), static_cast<quint64>(info.st_ino),
                         static_cast<quint64>(info.st_mode), static_cast<quint64>(info.st_uid),
                         static_cast<quint64>(info.st_gid), static_cast<quint64>(info.st_nlink)};
    // Directory timestamps/size change for unrelated children and runtime
    // locks. Identity and permissions still detect replacement/access changes.
    if (!S_ISDIR(info.st_mode)) {
      result.size = info.st_size;
      result.modified_seconds = info.st_mtim.tv_sec;
      result.modified_nanos = info.st_mtim.tv_nsec;
      result.changed_seconds = info.st_ctim.tv_sec;
      result.changed_nanos = info.st_ctim.tv_nsec;
    }
    return result;
  }
  static EntryState entry(const QString& path, bool follow = false) {
    EntryState result;
    result.metadata = metadata(path, follow);
    if (!result.metadata.error && S_ISREG(result.metadata.mode)) {
      QFile file(path);
      if (file.open(QIODevice::ReadOnly)) {
        result.bytes = file.readAll();
        result.readable = file.error() == QFileDevice::NoError;
      }
      result.read_error = file.error();
    }
    if (metadata(path, follow) != result.metadata) changed(path);
    return result;
  }
  void capture_original() {
    original_entries.emplace("project.json", entry(directory + "/project.json"));
    // This is only a safe filename hint for evidence capture, not manifest
    // acceptance. load_current() still validates the entire format/version.
    const auto& manifest = original_entries.at("project.json");
    auto source = manifest.readable ? QJsonDocument::fromJson(manifest.bytes).object().value("screenplay").toString() : QString();
    if (!source.endsWith(".json") || source.startsWith('.') || source.contains('/') || source.contains('\\'))
      source = QStringLiteral("screenplay.json");
    for (const auto& name : {source, QStringLiteral("imported-material.json"), QStringLiteral("imports"),
                             QStringLiteral("imports/original.fountain")}) {
      original_entries.emplace(name, entry(directory + '/' + name));
    }
  }
  void check_entries() const {
    for (const auto& [name, inspected] : original_entries) {
      if (entry(directory + '/' + name) != inspected) changed(directory + '/' + name);
    }
  }

  static PackageState package_state(const ProjectSession& session) {
    const auto& directory = session.opened_.project.directory;
    return {session.directory_identity_, ProjectSession::read_file(directory + "/project.json"),
            ProjectSession::read_file(directory + "/screenplay.json"), ProjectSession::read_file(directory + "/recovery.json")};
  }
  static std::unique_ptr<ProjectSession> lock(const QString& directory) {
    return std::unique_ptr<ProjectSession>(new ProjectSession(directory, ProjectSession::InspectionOnly{}));
  }
  static std::optional<InspectedPackage> inspect(const QString& path) {
    try {
      // Retention takes the same package lock before unpublishing. An active
      // package editor is not a stable recovery source and is not offered.
      auto session = lock(path);
      const auto before = package_state(*session);
      auto package = RecoveryStore::inspect_copy(session->opened_.project.directory);
      if (!package) return std::nullopt;
      session->check_lock();
      if (before != package_state(*session)) return std::nullopt;
      return InspectedPackage{std::move(*package), before};
    } catch (const SourceError&) { return std::nullopt; }
  }
  void check_original() const {
    if (consumed) throw SourceError(SourceError::Kind::io, QStringLiteral("This project-open decision has already been used"));
    if (original) {
      original->check_lock();
      check_entries();
      if (ready) {
        original->check_destination();
        if (!same_project(original->read_reload(), original->opened_)) changed(directory);
        original->check_destination();
      }
    } else if (!non_directory || QFileInfo(directory).isDir() || entry(directory) != *non_directory ||
               (followed_target && entry(directory, true) != *followed_target)) {
      changed(directory);
    }
  }
  void check_package(ProjectSession& session, const AccessCandidate& selected) const {
    const auto found = packages.find(selected.copy.directory);
    if (found == packages.end() || session.directory_identity_ != found->second.directory) changed(selected.copy.directory);
    if (package_state(session) != found->second) changed(selected.copy.directory);
    const auto package = RecoveryStore::inspect_copy(selected.copy.directory);
    if (!package || package->origin_id != project_id || package->origin_directory != selected.origin_directory ||
        package->copy.reason != selected.copy.reason || package->copy.captured_at != selected.copy.captured_at ||
        package->copy.revision != selected.copy.revision || package->opened.screenplay != selected.screenplay ||
        package->opened.assets != selected.assets) changed(selected.copy.directory);
    session.check_lock();
    if (package_state(session) != found->second) changed(selected.copy.directory);
  }
};

ProjectAccess::ProjectAccess(const QString& directory, const QString& expected_id) : state_(std::make_unique<State>()) {
  auto& state = *state_;
  state.directory = normalized(directory);
  if (!expected_id.isEmpty() && !valid_id(expected_id)) {
    state.error = QStringLiteral("The saved project identity is invalid; no project was opened at %1").arg(state.directory);
    state.error_kind = SourceError::Kind::malformed;
    return;
  }
  state.project_id = expected_id;
  try {
    state.original = State::lock(state.directory);
  } catch (const SourceError& error) {
    state.error = QString::fromUtf8(error.what());
    state.error_kind = error.kind();
    // A failed lock on an existing directory is never bypassable. A missing
    // path or a retained non-directory entry has no directory lock to acquire,
    // but matching emergency evidence can still be recovered into a NEW path.
    try {
      auto inspected = State::entry(state.directory);
      const auto& entry = inspected.metadata;
      if (error.kind() == SourceError::Kind::locked || (!entry.error && S_ISDIR(entry.mode)) ||
          (entry.error && entry.error != ENOENT && entry.error != ENOTDIR)) return;
      if (!entry.error) {
        state.error = QStringLiteral("Project path %1 is not a folder; the existing entry was preserved.").arg(state.directory);
      }
      // A dangling link can regain a file target without changing the link
      // itself. That is a new original-path decision, just like a directory.
      if (!entry.error && S_ISLNK(entry.mode)) state.followed_target = State::entry(state.directory, true);
      state.non_directory = std::move(inspected);
    } catch (const SourceError& changed_error) {
      state.error = QString::fromUtf8(changed_error.what());
      state.error_kind = changed_error.kind();
      return;
    }
  }
  if (state.original) {
    try { state.capture_original(); }
    catch (const SourceError& error) {
      state.error = QString::fromUtf8(error.what());
      state.error_kind = error.kind();
      return;
    }
    try {
      state.original->load_current();
      state.ready = true;
      state.revision = state.original->opened_.screenplay.revision;
    } catch (const SourceError& error) {
      state.error = QString::fromUtf8(error.what());
      state.error_kind = error.kind();
    }
    const auto& manifest_id = state.original->opened_.project.id;
    if (!expected_id.isEmpty() && !manifest_id.isEmpty() && manifest_id != expected_id) {
      state.ready = false;
      state.revision = 0;
      state.error_kind = SourceError::Kind::external_change;
      state.error = QStringLiteral("Project identity at %1 differs from the last opened project. "
                                   "The replacement was not opened; the saved project's evidence is retained.").arg(state.directory);
    } else if (state.project_id.isEmpty()) {
      state.project_id = manifest_id;
    }
    try {
      state.original->check_lock();
      state.check_entries();
    } catch (const SourceError& error) {
      state.ready = false;
      state.revision = 0;
      state.error = QString::fromUtf8(error.what());
      state.error_kind = error.kind();
      return;
    }
  }

  std::vector<State::InspectedPackage> inspected;
  std::set<QString> seen;
  const auto scan = [&](const QString& root) {
    const QFileInfo folder(root);
    if (!folder.isDir() || folder.isSymLink()) return;
    for (const auto& name : QDir(root).entryList({"copy-*"}, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
      const auto path = root + '/' + name;
      if (!seen.insert(path).second) continue;
      if (auto candidate = State::inspect(path)) inspected.push_back(std::move(*candidate));
    }
  };
  if (state.original) scan(state.directory + "/.recovery");
  const auto data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  if (!data.isEmpty()) {
    const auto root = data + "/Apollo/recovery";
    if (QFileInfo(root).isDir() && !QFileInfo(root).isSymLink()) {
      for (const auto& bucket : QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) scan(root + '/' + bucket);
    }
  }
  if (state.project_id.isEmpty()) {
    // A damaged/missing manifest supplies no identity. Only verified metadata
    // tied to the requested original location can establish it; unrelated
    // app-data packages are never guessed to belong to this project.
    std::set<QString> identities;
    for (const auto& item : inspected) {
      if (QDir::cleanPath(item.package.origin_directory) == state.directory) identities.insert(item.package.origin_id);
    }
    if (identities.size() == 1) state.project_id = *identities.begin();
    else if (identities.size() > 1) {
      state.error += QStringLiteral(" Multiple verified project identities refer to this location; the intended project identity is required.");
    }
  }
  for (auto& item : inspected) {
    auto& package = item.package;
    if (state.project_id.isEmpty() || package.origin_id != state.project_id) continue;
    QString relation;
    if (state.ready) {
      const auto& current = state.original->opened_;
      if (package.copy.revision < current.screenplay.revision) continue;
      if (package.copy.revision == current.screenplay.revision) {
        if (package.opened.screenplay == current.screenplay && package.opened.assets == current.assets) continue;
        relation = QStringLiteral("Conflicting writing or imported material at the same revision (%1)").arg(package.copy.revision);
      } else {
        relation = QStringLiteral("Newer revision %1 (current revision %2)").arg(package.copy.revision).arg(current.screenplay.revision);
      }
    } else {
      relation = QStringLiteral("Verified alternative at revision %1; current project is unavailable").arg(package.copy.revision);
    }
    state.packages.emplace(package.copy.directory, std::move(item.state));
    state.candidates.push_back({std::move(package.copy), std::move(package.origin_directory),
                                std::move(package.opened.screenplay), std::move(package.opened.assets), std::move(relation)});
  }
  std::sort(state.candidates.begin(), state.candidates.end(), [](const auto& a, const auto& b) {
    if (a.copy.revision != b.copy.revision) return a.copy.revision > b.copy.revision;
    if (a.copy.captured_at != b.copy.captured_at) return a.copy.captured_at > b.copy.captured_at;
    return a.copy.directory < b.copy.directory;
  });
}

ProjectAccess::~ProjectAccess() = default;
QString ProjectAccess::directory() const { return state_->directory; }
QString ProjectAccess::error() const { return state_->error; }
bool ProjectAccess::current_available() const { return state_->ready && !state_->consumed; }
QString ProjectAccess::project_id() const { return state_->project_id; }
std::uint64_t ProjectAccess::current_revision() const { return state_->revision; }
const std::vector<AccessCandidate>& ProjectAccess::candidates() const { return state_->candidates; }

std::unique_ptr<ProjectSession> ProjectAccess::open_current() {
  if (!current_available()) {
    throw SourceError(state_->error_kind, state_->error.isEmpty() ? QStringLiteral("The inspected current project is unavailable") : state_->error);
  }
  state_->check_original();
  state_->consumed = true;
  return std::move(state_->original);
}

std::unique_ptr<ProjectSession> ProjectAccess::recover(std::size_t index, const QString& destination,
                                                       const std::function<void()>& before_activation) {
  auto& state = *state_;
  state.check_original();
  if (index >= state.candidates.size()) throw SourceError(SourceError::Kind::io, QStringLiteral("No such verified recovery selection"));
  if (!QDir::isAbsolutePath(destination)) throw SourceError(SourceError::Kind::io, QStringLiteral("Recovery destination must be an absolute new project folder"));
  const auto& selected = state.candidates[index];
  auto package_lock = State::lock(selected.copy.directory);
  state.check_package(*package_lock, selected);
  // create_project stages, reads back, and publishes with RENAME_NOREPLACE.
  // It assigns a new identity and never changes the package or original.
  const auto published = create_project(destination, selected.screenplay, selected.assets);
  try {
    if (before_activation) before_activation();
    state.check_original();
    state.check_package(*package_lock, selected);
    auto active = std::make_unique<ProjectSession>(published.directory);
    active->check_destination();
    if (active->opened().project.id != published.id || active->opened().screenplay != selected.screenplay ||
        active->opened().assets != selected.assets) changed(published.directory);
    state.consumed = true;
    state.original.reset();
    return active;
  } catch (const std::exception& error) {
    throw SourceError(SourceError::Kind::io,
                      QStringLiteral("Recovery project retained at %1, but could not activate it: %2")
                          .arg(published.directory, QString::fromUtf8(error.what())));
  }
}
}
