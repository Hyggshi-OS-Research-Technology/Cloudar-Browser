#pragma once

#include <QWidget>
#include <QPixmap>
#include <QUrl>

class QLabel;
class QFrame;
class QPropertyAnimation;
class QGraphicsOpacityEffect;

class TabPreviewPopup : public QWidget
{
    Q_OBJECT
public:
    explicit TabPreviewPopup(QWidget *parent = nullptr);

    // `state` is a short, accurate description of the tab's memory state
    // ("Active", "Background", "Sleeping"). It replaces the old per-tab
    // "Memory usage: N MB" line, which was a hash of the URL rather than a
    // measurement and misled users into thinking tabs were expensive when
    // they were not.
    void setContent(const QPixmap &thumbnail, const QString &title, const QUrl &url = QUrl(),
                    const QString &state = QString());
    void fadeIn();
    void fadeOut();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QFrame *m_card;
    QGraphicsOpacityEffect *m_opacityEffect;
    QPropertyAnimation *m_fadeAnimation;
    QLabel *m_titleLabel;
    QLabel *m_domainLabel;
    QLabel *m_thumbnailLabel;
    QLabel *m_stateIcon;
    QLabel *m_stateLabel;
};
