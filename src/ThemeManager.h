#pragma once

#include <QObject>
#include <QColor>
#include <QString>

// Single source of truth for the app's accent color. Persisted via
// QSettings ("browser/accentColor") and shared by every widget that needs
// to look consistent — toolbar, menu, tabs, and the custom TitleBar (a
// native Linux window manager won't let Qt recolor its title bar, which is
// exactly why MainWindow draws its own and keeps it wired to this class).
//
// It also owns the light/dark decision — see ColorMode below — because the
// accent and the surface are the same palette.
//
// What the *modern* chrome paints itself with. The classic chrome is a fixed
// 2009 palette by design and ignores this entirely (see ClassicTheme.h), so a
// user on Classic who picks "Dark" gets a truthful answer: the setting is
// stored, and the dialog says the classic chrome does not use it.
enum class ColorMode {
    System,   // follow the OS; re-evaluated when the OS setting changes
    Light,
    Dark
};

class ThemeManager : public QObject
{
    Q_OBJECT
public:
    static ThemeManager &instance();

    QColor accentColor() const;
    void setAccentColor(const QColor &color);

    // --- light / dark ------------------------------------------------------
    // System is the default and the interesting case: Qt 6.5+ exposes
    // QStyleHints::colorScheme() and a colorSchemeChanged() signal, so the
    // window can repaint when the OS flips at sunset without a restart. Qt
    // older than 6.5 has no such API, and there System resolves to light
    // rather than claiming to follow something it cannot observe.
    ColorMode colorMode() const;                 // the stored preference
    void setColorMode(ColorMode mode);
    bool followsSystemColorMode() const { return colorMode() == ColorMode::System; }

    // What is actually on screen right now: the preference, with System
    // resolved to the OS answer. Everything that paints asks this.
    bool prefersDark() const;
    QString colorModeKey() const;                // "system" | "light" | "dark"

    // The OS colour scheme, and whether this Qt build can observe it at all.
    static bool systemColorSchemeQueryable();
    static bool systemPrefersDark();

    // Black or white, whichever reads better on top of `bg`.
    static QColor readableTextOn(const QColor &bg);

    // Full app QSS for whichever chrome is active (see ClassicTheme.h).
    // Re-apply via qApp->setStyleSheet() whenever accentChanged() fires.
    QString appStyleSheet() const;

    // The original dark sheet, exposed so Theme::appStyleSheet() can
    // delegate to it when the Modern style is selected instead of a second
    // copy drifting out of sync with this one.
    QString modernStyleSheet(const QColor &accent) const;

signals:
    void accentChanged(const QColor &color);
    // Emitted when what is on screen changes: the user picked a mode, or the OS
    // flipped while the preference is System. Consumers re-read prefersDark().
    void colorModeChanged(bool dark);

private:
    explicit ThemeManager(QObject *parent = nullptr);
    Q_DISABLE_COPY(ThemeManager)

    // One definition of the light/dark palette, shared by the stylesheet and by
    // every widget that paints itself. The painted widgets used to hard-code
    // the dark values, which is why the tab close button was #c7cede — i.e.
    // white-on-white — in light mode.
    struct Palette {
        QColor window;
        QColor chrome;
        QColor border;
        QColor tab;
        QColor tabHover;
        QColor hover;
        QColor input;
        QColor text;
        QColor dim;
        QColor onAccent;
    };
    // The palette in force right now (the preference with System resolved).
    Palette palette() const;
    // The same palette for an explicit dark/light answer.
    static Palette paletteFor(bool dark);

    QColor m_accent;
    ColorMode m_mode = ColorMode::System;
    bool m_lastDark = false;   // last value announced, so no redundant repaints
};
