#pragma once

#include <QObject>
#include <QHash>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>

namespace apollo::editor {
// Transient, per-document completion. A request is valid only for the exact
// revision/caret/component that produced it; nothing is persisted as identity.
class WritingSuggestions : public QObject {
  Q_OBJECT
  Q_PROPERTY(QStringList choices READ choices NOTIFY changed)
  Q_PROPERTY(int selected READ selected NOTIFY changed)
  Q_PROPERTY(QString component READ component NOTIFY changed)
public:
  explicit WritingSuggestions(QTextDocument* document, QObject* parent = nullptr);
  const QStringList& choices() const { return choices_; }
  int selected() const { return selected_; }
  const QString& component() const { return context_.kind; }
  Q_INVOKABLE void request(int position, int start, int end, bool explicitRequest = false);
  Q_INVOKABLE void dismiss(bool suppress = false);
  Q_INVOKABLE void cursorChanged(int position);
  Q_INVOKABLE void move(int delta);
  Q_INVOKABLE void select(int index);
  Q_INVOKABLE int accept(int position, int start, int end);
signals:
  void changed();
private:
  struct Context {
    QString key, kind, prefix, text, closing;
    int from = 0, to = 0, position = -1, following = 0;
  };
  QTextDocument* document_;
  QTimer scan_;
  QTextBlock next_;
  Context context_;
  QString suppressed_;
  QStringList choices_;
  QHash<QString, int> frequency_;
  int revision_ = -1, selected_ = 0;
  Context contextAt(int position) const;
  void addCandidate(const QString& kind, const QString& text);
  void scanChunk();
};
}
