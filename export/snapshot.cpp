#include "export/snapshot.h"

#include <QDataStream>
#include <QIODevice>
#include <stdexcept>

namespace apollo {
QByteArray encode_export_snapshot(const ExportSnapshot& snapshot) {
  QByteArray bytes;
  QDataStream out(&bytes, QIODevice::WriteOnly);
  out.setVersion(QDataStream::Qt_6_0);
  // The profile is compiled into this experiment; v1 pins that profile too.
  out << QByteArray("Apollo PDF snapshot v1") << snapshot.include_title << quint32(snapshot.elements.size());
  for (const auto& element : snapshot.elements) {
    out << quint8(element.type) << QByteArray::fromStdString(element.text) << element.dual_right << quint32(element.styles.size());
    for (const auto& span : element.styles) out << quint64(span.start) << quint64(span.end) << quint8(span.style);
  }
  if (out.status() != QDataStream::Ok) throw std::runtime_error("cannot encode export snapshot");
  return bytes;
}

ExportSnapshot decode_export_snapshot(const QByteArray& bytes) {
  QDataStream in(bytes);
  in.setVersion(QDataStream::Qt_6_0);
  auto require = [](bool valid) { if (!valid) throw std::runtime_error("invalid export snapshot"); };
  QByteArray marker;
  ExportSnapshot snapshot;
  quint32 count;
  in >> marker >> snapshot.include_title >> count;
  require(marker == "Apollo PDF snapshot v1" && in.status() == QDataStream::Ok && count <= bytes.size() / 10);
  for (quint32 i = 0; i < count; ++i) {
    quint8 type;
    QByteArray text;
    bool dual;
    quint32 styles;
    in >> type >> text >> dual >> styles;
    require(in.status() == QDataStream::Ok && type <= quint8(ElementType::draft_date) && styles <= bytes.size() / 17);
    Element element{ElementType(type), text.toStdString(), dual};
    for (quint32 j = 0; j < styles; ++j) {
      quint64 start, end;
      quint8 style;
      in >> start >> end >> style;
      require(in.status() == QDataStream::Ok && start <= end && end <= quint64(text.size()) && style <= quint8(Style::underline));
      element.styles.push_back({std::size_t(start), std::size_t(end), Style(style)});
    }
    snapshot.elements.push_back(std::move(element));
  }
  require(in.status() == QDataStream::Ok && in.atEnd());
  return snapshot;
}
}
