#pragma once

#include <QString>
#include <QLocale>
#include <QMap>

namespace OnlyAir {

class Translator {
public:
    static Translator& instance();

    // Initialize with system locale
    void initialize();

    // Set language manually (en, it)
    void setLanguage(const QString& langCode);

    // Get translated string
    QString tr(const QString& key) const;

    // Get current language code
    QString currentLanguage() const { return m_currentLang; }

private:
    Translator();
    void loadEnglish();
    void loadItalian();

    QString m_currentLang;
    QMap<QString, QString> m_translations;
};

// Convenience macro
#define TR(key) OnlyAir::Translator::instance().tr(key)

} // namespace OnlyAir
