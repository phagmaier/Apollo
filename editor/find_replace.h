#pragma once

#include <QObject>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <vector>

namespace apollo::editor {
// Transient literal search over authored text. Search never changes source
// formatting; replacements are confined to text inside one element.
class FindReplace : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(int count READ count NOTIFY changed)
  Q_PROPERTY(int current READ current NOTIFY changed)
  Q_PROPERTY(bool hasScope READ hasScope NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
public:
  explicit FindReplace(QTextDocument*, QObject* parent = nullptr);
  bool busy() const { return busy_; }
  int count() const { return static_cast<int>(matches_.size()); }
  int current() const { return selected_ + 1; }
  bool hasScope() const { return captured_scope_; }
  QString status() const;
  Q_INVOKABLE void begin(int position, int start, int end);
  Q_INVOKABLE void configure(const QString& query, bool matchCase, bool wholeWords, bool selectionOnly);
  Q_INVOKABLE void dismiss();
  Q_INVOKABLE void next(bool backward = false);
  Q_INVOKABLE int replace(const QString& replacement, bool all = false);
signals:
  void changed();
  void selectionRequested(int start, int end);
private:
  struct Match { int from, to; };
  QTextDocument* document_;
  QTextCursor scope_start_, scope_end_, origin_;
  QTimer timer_;
  QTextBlock block_;
  QString query_, message_;
  std::vector<Match> matches_, pending_;
  int revision_ = -1, selected_ = -1, offset_ = 0;
  bool active_ = false, busy_ = false, changing_ = false;
  bool captured_scope_ = false;
  bool match_case_ = false, whole_words_ = false, selection_only_ = false;
  void restart();
  void scan();
  void select();
  bool ready() const;
};
}
