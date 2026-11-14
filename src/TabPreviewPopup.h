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

    void setContent(const QPixmap &thumbnail, const QString &title, const QUrl &url = QUrl(), int tabIndex = 0);
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
    QLabel *m_memoryIcon;
    QLabel *m_memoryLabel;
};
