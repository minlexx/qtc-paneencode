#include <QDebug>

#include "Utils.h"


#  include "charsetdetect.h"
QTextCodec *detectCodec (const QByteArray &source) {
  csd_t detector = csd_open ();
  if (detector == nullptr) {
    qCritical () << "codec detector open failed";
    return nullptr;
  }
  csd_consider (detector, source.data (), source.size ());
  QByteArray name = csd_close (detector);
  QTextCodec *codec = QTextCodec::codecForName (name);
  return codec;
}


namespace {
  const QChar separator = QLatin1Char (' ');
  const int minLength = 3; // To avoid of too short strings detection
}

QString QtcPaneEncode::Internal::reencode (const QString &source, const QTextCodec *codec) {
  if (codec != nullptr) {
    return codec->toUnicode (source.toLocal8Bit ());
  }
  // Detectors use only part of string to detect encoding.
  // So function splits source string into some parts.
  QString detectBuffer;
  QString result;
  for (int i = 0, end = source.size (); i <= end; ++i) {
    QChar curChar;
    if (i < end) {
      curChar = source.at (i);
      if (curChar != separator || detectBuffer.length () < minLength) {
        detectBuffer += curChar;
        continue;
      }
    }

    if (!detectBuffer.isEmpty ()) {
      QByteArray rawSource = detectBuffer.toLocal8Bit ();
      QTextCodec *codec = detectCodec (rawSource);
      result += (codec != nullptr) ? codec->toUnicode (rawSource)
                : detectBuffer;
      detectBuffer.clear ();
    }

    if (!curChar.isNull ()) { // ignore last one(i == end)
      result += curChar;
    }
  }
  return result;
}
