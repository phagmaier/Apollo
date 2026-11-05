#pragma once
#include <QObject>
#include <QTextDocument>
#include <QTextBlock>
#include <QThread>
#include <QTimer>
#include <QSet>
#include <QVariantList>
#include <memory>
class Hunspell;
namespace apollo::editor {
// Assistance owns no authored formatting. The worker receives immutable text;
// only results for the current generation can reach the document's UI.
class Spelling : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QString word READ word NOTIFY menuChanged)
  Q_PROPERTY(QStringList choices READ choices NOTIFY menuChanged)
  Q_PROPERTY(QString message READ message NOTIFY menuChanged)
public:
  explicit Spelling(QTextDocument*, const QString& personalPath = {}, QObject* parent = nullptr);
  ~Spelling() override;
  bool busy() const { return busy_; }
  QString error() const { return error_; }
  QString word() const { return word_; }
  QString message() const { return message_; }
  QStringList choices() const { return choices_; }
  const QVariantList& misspellings() const { return issues_; }
  Q_INVOKABLE QVariantList marks(qreal top, qreal bottom) const;
  Q_INVOKABLE void request(int position, int start, int end);
  Q_INVOKABLE void dismiss();
  Q_INVOKABLE int replace(const QString& replacement);
  Q_INVOKABLE bool addWord();
signals:
  void changed();
  void menuChanged();
private:
  QTextDocument* document_;
  QThread thread_;
  QObject* worker_;
  std::shared_ptr<Hunspell> checker_;
  QTimer debounce_, reload_;
  QString path_, error_, word_, message_;
  QByteArray personal_bytes_;
  QSet<QString> personal_;
  QVariantList issues_, pending_;
  QStringList choices_;
  QTextBlock next_;
  int published_revision_ = -1;
  int generation_ = 0, menu_generation_ = 0, menu_revision_ = -1, from_ = -1;
  bool busy_ = true, ready_ = false, in_flight_ = false;
  void restart();
  void scanNext();
  bool loadPersonal();
  bool validMenu() const;
};
}
