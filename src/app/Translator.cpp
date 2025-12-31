#include "Translator.h"

namespace OnlyAir {

Translator& Translator::instance() {
    static Translator instance;
    return instance;
}

Translator::Translator() : m_currentLang("en") {
    loadEnglish();
}

void Translator::initialize() {
    QLocale locale;
    QString lang = locale.name().left(2); // "it_IT" -> "it"

    if (lang == "it") {
        setLanguage("it");
    } else {
        setLanguage("en");
    }
}

void Translator::setLanguage(const QString& langCode) {
    m_currentLang = langCode;
    m_translations.clear();

    if (langCode == "it") {
        loadItalian();
    } else {
        loadEnglish();
    }
}

QString Translator::tr(const QString& key) const {
    return m_translations.value(key, key);
}

void Translator::loadEnglish() {
    // Menu
    m_translations["menu_file"] = "&File";
    m_translations["menu_open_video"] = "&Open Video...";
    m_translations["menu_exit"] = "E&xit";
    m_translations["menu_help"] = "&Help";
    m_translations["menu_about"] = "&About";

    // About dialog
    m_translations["about_title"] = "About OnlyAir";
    m_translations["about_text"] = "<h3>OnlyAir</h3>"
        "<p>Virtual Camera & Audio for Video Calls</p>"
        "<p><b>Version 1.0.0</b></p>"
        "<p>Created by <b>DevDav</b></p>"
        "<p><a href='https://github.com/0xDevDav/OnlyAir'>github.com/0xDevDav/OnlyAir</a></p>"
        "<hr>"
        "<p><b>License:</b> MIT</p>"
        "<p><b>Built with:</b><br>"
        "Qt 6 (LGPL) &bull; FFmpeg (LGPL) &bull; Softcam (MIT) &bull; VB-Cable</p>";

    // Scene panel
    m_translations["scene_black"] = "Black Scene";
    m_translations["scene_video"] = "Video Scene";
    m_translations["scene_live"] = "LIVE";
    m_translations["scene_no_video"] = "No video";

    // Playback controls
    m_translations["play"] = "Play";
    m_translations["pause"] = "Pause";
    m_translations["stop"] = "Stop";
    m_translations["volume"] = "Vol:";

    // Status bar
    m_translations["no_video_loaded"] = "No video loaded";
    m_translations["vcam_on"] = "VCam ON";
    m_translations["vcam_off"] = "VCam OFF";
    m_translations["audio_on"] = "Audio ON";
    m_translations["audio_off"] = "Audio OFF";

    // Dialogs
    m_translations["open_video_title"] = "Open Video File";
    m_translations["video_files_filter"] = "Video Files (*.mp4 *.avi *.mkv *.mov *.wmv *.webm);;All Files (*.*)";
    m_translations["error"] = "Error";
    m_translations["error_init"] = "Failed to initialize application";
    m_translations["error_open_video"] = "Failed to open video:";
}

void Translator::loadItalian() {
    // Menu
    m_translations["menu_file"] = "&File";
    m_translations["menu_open_video"] = "&Apri Video...";
    m_translations["menu_exit"] = "&Esci";
    m_translations["menu_help"] = "&Aiuto";
    m_translations["menu_about"] = "&Informazioni";

    // About dialog
    m_translations["about_title"] = "Informazioni su OnlyAir";
    m_translations["about_text"] = "<h3>OnlyAir</h3>"
        "<p>Camera e Audio Virtuali per Videochiamate</p>"
        "<p><b>Versione 1.0.0</b></p>"
        "<p>Creato da <b>DevDav</b></p>"
        "<p><a href='https://github.com/0xDevDav/OnlyAir'>github.com/0xDevDav/OnlyAir</a></p>"
        "<hr>"
        "<p><b>Licenza:</b> MIT</p>"
        "<p><b>Creato con:</b><br>"
        "Qt 6 (LGPL) &bull; FFmpeg (LGPL) &bull; Softcam (MIT) &bull; VB-Cable</p>";

    // Scene panel
    m_translations["scene_black"] = "Scena Nera";
    m_translations["scene_video"] = "Scena Video";
    m_translations["scene_live"] = "LIVE";
    m_translations["scene_no_video"] = "Nessun video";

    // Playback controls
    m_translations["play"] = "Play";
    m_translations["pause"] = "Pausa";
    m_translations["stop"] = "Stop";
    m_translations["volume"] = "Vol:";

    // Status bar
    m_translations["no_video_loaded"] = "Nessun video caricato";
    m_translations["vcam_on"] = "VCam ON";
    m_translations["vcam_off"] = "VCam OFF";
    m_translations["audio_on"] = "Audio ON";
    m_translations["audio_off"] = "Audio OFF";

    // Dialogs
    m_translations["open_video_title"] = "Apri File Video";
    m_translations["video_files_filter"] = "File Video (*.mp4 *.avi *.mkv *.mov *.wmv *.webm);;Tutti i File (*.*)";
    m_translations["error"] = "Errore";
    m_translations["error_init"] = "Impossibile inizializzare l'applicazione";
    m_translations["error_open_video"] = "Impossibile aprire il video:";
}

} // namespace OnlyAir
