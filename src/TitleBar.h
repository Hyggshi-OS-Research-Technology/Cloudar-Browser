#pragma once

#include <QWidget>
#include <QColor>

class QLabel;
class QToolButton;

// Drawn in place of the OS title bar (MainWindow runs with
// Qt::FramelessWindowHint) so its color can actually follow the app's
// accent color — window managers on Linux own the native decoration and
// give Qt no portable way to recolor it. Drag-to-move and double-click-to-
// maximize are implemented here; the window itself stays resizable via the
// QSizeGrip MainWindow places in its status bar.
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setTitle(const QString &title);

signals:
    void minimizeRequested();
    void maximizeRestoreRequested();
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void applyTheme(const QColor &accent);

private:
    QLabel *m_iconLabel;
    QLabel *m_titleLabel;
    QToolButton *m_minBtn;
    QToolButton *m_maxBtn;
    QToolButton *m_closeBtn;
};
