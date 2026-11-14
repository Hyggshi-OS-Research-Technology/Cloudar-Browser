#pragma once

#include <QObject>
#include <QColor>
#include <QString>

// Single source of truth for the app's accent color. Persisted via
// QSettings ("browser/accentColor") and shared by every widget that needs
// to look consistent — toolbar, menu, tabs, and the custom TitleBar (a
// native Linux window manager won't let Qt recolor its title bar, which is
// exactly why MainWindow draws its own and keeps it wired to this class).
class ThemeManager : public QObject
{
    Q_OBJECT
public:
    static ThemeManager &instance();

    QColor accentColor() const;
    void setAccentColor(const QColor &color);

    // Black or white, whichever reads better on top of `bg`.
    static QColor readableTextOn(const QColor &bg);

    // Full app QSS built from the current accent color. Re-apply via
    // qApp->setStyleSheet() whenever accentChanged() fires.
    QString appStyleSheet() const;

signals:
    void accentChanged(const QColor &color);

private:
    explicit ThemeManager(QObject *parent = nullptr);
    Q_DISABLE_COPY(ThemeManager)

    QColor m_accent;
};
