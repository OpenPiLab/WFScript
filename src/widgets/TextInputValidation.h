#pragma once
#include <QString>
#include <QStringList>
#include <QtGlobal>

// Strict IPv4 literal: four dotted decimal octets in canonical form.
// No hostnames, abbreviated forms, hexadecimal, octal or IPv6.
namespace TextInputValidation {
inline bool ipv4(const QString& text) {
    const QStringList parts = text.split(QLatin1Char('.'), Qt::KeepEmptyParts);
    if (parts.size() != 4) return false;
    for (const QString& part : parts) {
        if (part.isEmpty() || part.size() > 3) return false;
        if (part.size() > 1 && part.startsWith(QLatin1Char('0'))) return false;
        int octet = 0;
        for (const QChar digit : part) {
            if (digit < QLatin1Char('0') || digit > QLatin1Char('9')) return false;
            octet = octet * 10 + digit.unicode() - QLatin1Char('0').unicode();
        }
        if (octet > 255) return false;
    }
    return true;
}
inline bool allowedValue(bool isIpv4, const QString& text, int maxLength) {
    return text.size() <= qBound(1, maxLength, 4096) &&
           (!isIpv4 || text.isEmpty() || ipv4(text));
}
}
