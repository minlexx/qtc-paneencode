#ifndef QTCPANEENCODE_GLOBAL_H
#define QTCPANEENCODE_GLOBAL_H

#include <QtGlobal>

#if defined(QTCPANEENCODE_LIBRARY)
#  define QTCPANEENCODESHARED_EXPORT Q_DECL_EXPORT
#else
#  define QTCPANEENCODESHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // QTCPANEENCODE_GLOBAL_H
