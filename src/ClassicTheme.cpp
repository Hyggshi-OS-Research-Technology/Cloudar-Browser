#include "ClassicTheme.h"
#include "ThemeManager.h"

#include <QSettings>
#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QPen>
#include <QBrush>
#include <QLinearGradient>
#include <QFont>
#include <QStyle>
#include <QHash>
#include <QVector>
#include <QPair>

#include <cmath>

namespace {

const char *kStyleKey = "browser/themeStyle";

// Windows 7 Aero / Windows 10 grey, and the Windows Classic beige that
// Chrome 0.2 still looked at home on. Kept as named constants because the
// classic sheet references several of them more than once.
const char *kClassicFrame     = "#e8e8e8";
const char *kClassicFrameDark = "#c8c8c8";
const char *kClassicTabBar    = "#d4d0c8";
const char *kClassicTabActive = "#f4f4f4";
const char *kClassicBorder    = "#a0a0a0";
const char *kClassicText      = "#1a1a1a";
const char *kClassicDim       = "#6b6b6b";

QPixmap makePixmap(int size)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    return pix;
}

void paintArrow(QPainter &p, const QColor &color, qreal cx, qreal cy, qreal w, qreal h)
{
    QPolygonF tri;
    tri << QPointF(cx, cy - h / 2)
        << QPointF(cx + w / 2, cy + h / 2)
        << QPointF(cx - w / 2, cy + h / 2);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawPolygon(tri);
}

// --- Classic Chrome 2009 glyphs ------------------------------------------

void paintClassicBack(QPainter &p, const QColor &c, int s)
{
    QPen pen(c, 1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const qreal cy = s * 0.52;
    p.drawArc(QRectF(s * 0.34, s * 0.26, s * 0.44, s * 0.44), 35 * 16, 250 * 16);
    paintArrow(p, c, s * 0.31, cy, s * 0.26, s * 0.24);
}

void paintClassicForward(QPainter &p, const QColor &c, int s)
{
    QPen pen(c, 1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const qreal cy = s * 0.52;
    p.drawArc(QRectF(s * 0.22, s * 0.26, s * 0.44, s * 0.44), 295 * 16, 250 * 16);
    paintArrow(p, c, s * 0.69, cy, s * 0.26, s * 0.24);
}

void paintClassicReload(QPainter &p, const QColor &c, int s)
{
    QPen pen(c, 1.7);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    // Open circle with a gap at the top-right, plus a short arrow head —
    // this is the same "reload" shape Chrome used before it adopted the
    // Material circular arrow.
    p.drawArc(QRectF(s * 0.2, s * 0.2, s * 0.6, s * 0.6), 120 * 16, 285 * 16);
    QPolygonF head;
    head << QPointF(s * 0.62, s * 0.14)
         << QPointF(s * 0.80, s * 0.24)
         << QPointF(s * 0.62, s * 0.32);
    p.setBrush(c);
    p.setPen(Qt::NoPen);
    p.drawPolygon(head);
}

void paintClassicHome(QPainter &p, const QColor &c, int s)
{
    QPen pen(c, 1.5);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPolygonF roof;
    roof << QPointF(s * 0.5, s * 0.16)
         << QPointF(s * 0.86, s * 0.46)
         << QPointF(s * 0.14, s * 0.46);
    p.drawPolyline(roof);
    QPolygonF body;
    body << QPointF(s * 0.24, s * 0.44)
         << QPointF(s * 0.76, s * 0.44)
         << QPointF(s * 0.76, s * 0.84)
         << QPointF(s * 0.24, s * 0.84);
    p.drawPolygon(body);
}

void paintClassicStar(QPainter &p, const QColor &c, int s, bool filled)
{
    constexpr qreal kPi = 3.14159265358979323846;
    QPainterPath star;
    const QPointF center(s / 2.0, s / 2.0);
    const qreal outer = s * 0.40;
    const qreal inner = outer * 0.42;
    for (int i = 0; i < 10; ++i) {
        const qreal radius = (i % 2 == 0) ? outer : inner;
        const qreal angle = -kPi / 2 + i * kPi / 5;
        const QPointF point(center.x() + radius * std::cos(angle),
                            center.y() + radius * std::sin(angle));
        if (i == 0)
            star.moveTo(point);
        else
            star.lineTo(point);
    }
    star.closeSubpath();

    if (filled) {
        p.setBrush(QColor("#f5b301"));
        p.setPen(QPen(QColor("#a37a00"), 1.0));
    } else {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(c, 1.3));
    }
    p.drawPath(star);
}

void paintClassicPage(QPainter &p, const QColor &c, int s)
{
    QPen pen(c, 1.3);
    p.setPen(pen);
    p.setBrush(QColor(c.red(), c.green(), c.blue(), 28));
    QPainterPath page;
    page.moveTo(s * 0.26, s * 0.18);
    page.lineTo(s * 0.62, s * 0.18);
    page.lineTo(s * 0.76, s * 0.32);
    page.lineTo(s * 0.76, s * 0.82);
    page.lineTo(s * 0.26, s * 0.82);
    page.closeSubpath();
    p.drawPath(page);
    pen.setWidthF(1.0);
    p.setPen(pen);
    p.drawLine(QPointF(s * 0.34, s * 0.48), QPointF(s * 0.68, s * 0.48));
    p.drawLine(QPointF(s * 0.34, s * 0.60), QPointF(s * 0.68, s * 0.60));
}

void paintClassicGo(QPainter &p, const QColor &c, int s)
{
    QPen pen(c, 1.8);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(s * 0.24, s * 0.5), QPointF(s * 0.68, s * 0.5));
    paintArrow(p, c, s * 0.70, s * 0.5, s * 0.28, s * 0.28);
}

void paintClassicMenu(QPainter &p, const QColor &c, int s)
{
    QPen pen(c, 1.6);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    for (int i = 0; i < 3; ++i) {
        const qreal y = s * (0.3 + i * 0.2);
        p.drawLine(QPointF(s * 0.2, y), QPointF(s * 0.8, y));
    }
}

void paintClassicStop(QPainter &p, const QColor &c, int s)
{
    QPen pen(c, 1.7);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.drawLine(QPointF(s * 0.28, s * 0.28), QPointF(s * 0.72, s * 0.72));
    p.drawLine(QPointF(s * 0.72, s * 0.28), QPointF(s * 0.28, s * 0.72));
}

void paintClassicPlus(QPainter &p, const QColor &c, int s)
{
    QPen pen(c, 1.7);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.drawLine(QPointF(s * 0.5, s * 0.26), QPointF(s * 0.5, s * 0.74));
    p.drawLine(QPointF(s * 0.26, s * 0.5), QPointF(s * 0.74, s * 0.5));
}

void paintClassicReader(QPainter &p, const QColor &c, int s)
{
    QPen pen(c, 1.2);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(s * 0.26, s * 0.16, s * 0.48, s * 0.68));
    p.drawLine(QPointF(s * 0.34, s * 0.40), QPointF(s * 0.66, s * 0.40));
    p.drawLine(QPointF(s * 0.34, s * 0.52), QPointF(s * 0.66, s * 0.52));
    p.drawLine(QPointF(s * 0.34, s * 0.64), QPointF(s * 0.56, s * 0.64));
}

} // namespace

namespace Theme {

namespace {

// -1 = not resolved yet, 0 = modern, 1 = classic. Resolved once from QSettings
// on first use; QSettings needs an application identity, so a miss there simply
// means "the default", and the next call after QApplication exists re-reads it.
int g_styleCache = -1;

} // namespace

ThemeStyle current()
{
    if (g_styleCache < 0) {
        // Before QApplication exists QSettings cannot be resolved; falling
        // through to the default keeps startup deterministic instead of
        // caching a wrong answer read from an unintended file.
        if (QCoreApplication::instance() == nullptr) {
            const QString override = qEnvironmentVariable("CLOUDAR_THEME").toLower();
            if (override == "modern")
                return ThemeStyle::Modern;
            if (override == "classic")
                return ThemeStyle::Classic;
            return ThemeStyle::Classic;
        }
        const QString key = QSettings().value(kStyleKey, "classic").toString().toLower();
        g_styleCache = (key == "modern") ? 0 : 1;
    }
    return g_styleCache == 0 ? ThemeStyle::Modern : ThemeStyle::Classic;
}

ThemeStyle savedStyle()
{
    if (QCoreApplication::instance() == nullptr)
        return ThemeStyle::Classic;
    const QString key = QSettings().value(kStyleKey, "classic").toString().toLower();
    return key == "modern" ? ThemeStyle::Modern : ThemeStyle::Classic;
}

QString keyFor(ThemeStyle style)
{
    return style == ThemeStyle::Classic ? QStringLiteral("classic") : QStringLiteral("modern");
}

bool styleFromKey(const QString &key, ThemeStyle *out)
{
    const QString normalized = key.trimmed().toLower();
    if (normalized == "classic") { if (out) *out = ThemeStyle::Classic; return true; }
    if (normalized == "modern")  { if (out) *out = ThemeStyle::Modern;  return true; }
    return false;
}

void setCurrent(ThemeStyle style)
{
    // Save only. The running chrome is deliberately left alone: an interface
    // style decides the window model, the tab surface and the menu bar, none of
    // which can be swapped underneath a live window. Half-applying it (new QSS,
    // old layout) is worse than not applying it at all, so the change lands on
    // the next launch and restartRequired() reports that it is pending.
    if (QCoreApplication::instance() != nullptr)
        QSettings().setValue(kStyleKey, keyFor(style));
}

void applyForRun(ThemeStyle style)
{
    // Deliberately no QSettings write: --classic on the command line must not
    // become the persisted preference.
    g_styleCache = (style == ThemeStyle::Modern) ? 0 : 1;
}

Metrics metrics()
{
    Metrics m;
    if (current() == ThemeStyle::Classic) {
        m.titleBarHeight = 26;
        m.toolbarHeight = 34;
        m.menuBarHeight = 20;
        m.tabHeight = 24;
        // The classic chrome has no vertical tab surface at all; the values are
        // zero so callers that only need "is there a sidebar" can test for it.
        m.sidebarCollapsedWidth = 0;
        m.sidebarExpandedWidth = 0;
        m.frameless = false;
        m.verticalTabs = false;
        m.statusBarVisible = true;
        m.bookmarkBarVisible = true;
    } else {
        m.titleBarHeight = 36;
        m.toolbarHeight = 44;
        m.menuBarHeight = 24;
        m.tabHeight = 30;
        m.sidebarCollapsedWidth = 54;
        m.sidebarExpandedWidth = 240;
        m.frameless = true;
        m.verticalTabs = true;
        m.statusBarVisible = false;
        m.bookmarkBarVisible = true;
    }
    return m;
}

QColor textColor()
{
    return current() == ThemeStyle::Classic ? QColor(kClassicText) : QColor("#e8ecf5");
}

QColor dimTextColor()
{
    return current() == ThemeStyle::Classic ? QColor(kClassicDim) : QColor("#8b93a7");
}

QColor accentTextColor(const QColor &accent)
{
    return current() == ThemeStyle::Classic ? QColor(kClassicText) : accent;
}

PopupColors popupColors()
{
    PopupColors c;
    if (current() == ThemeStyle::Classic) {
        // A quiet Win32 tooltip: warm white, one grey hairline, dark text.
        c.surface = QColor(QStringLiteral("#fdfdfb"));
        c.border = QColor(QString::fromLatin1(kClassicBorder));
        c.text = QColor(QString::fromLatin1(kClassicText));
        c.dim = QColor(QString::fromLatin1(kClassicDim));
        c.placeholder = QColor(QStringLiteral("#e6e3dd"));
    } else {
        c.surface = QColor(QStringLiteral("#16181f"));
        c.border = QColor(255, 255, 255, 20);
        c.text = QColor(QStringLiteral("#f8fafc"));
        c.dim = QColor(QStringLiteral("#94a3b8"));
        c.placeholder = QColor(QStringLiteral("#20222a"));
    }
    return c;
}

static void paintGlyph(QPainter &p, const QString &name, const QColor &color, int s)
{
    if (name == "back")            paintClassicBack(p, color, s);
    else if (name == "forward")    paintClassicForward(p, color, s);
    else if (name == "reload")     paintClassicReload(p, color, s);
    else if (name == "stop")       paintClassicStop(p, color, s);
    else if (name == "home")       paintClassicHome(p, color, s);
    else if (name == "star")       paintClassicStar(p, color, s, false);
    else if (name == "star-filled")paintClassicStar(p, color, s, true);
    else if (name == "page")       paintClassicPage(p, color, s);
    else if (name == "go")         paintClassicGo(p, color, s);
    else if (name == "menu")       paintClassicMenu(p, color, s);
    else if (name == "plus")       paintClassicPlus(p, color, s);
    else if (name == "reader")     paintClassicReader(p, color, s);
}

QIcon icon(const QString &name, int size)
{
    // These icons are rebuilt on every toolbar rebuild, so cache them: a
    // QIcon is shared-pointer-copied and QToolButton keeps one per action,
    // but repainting 12 pixmaps on each theme switch is still wasted work.
    static QHash<QString, QIcon> classicCache;
    static QHash<QString, QIcon> modernCache;
    const bool classic = current() == ThemeStyle::Classic;
    QHash<QString, QIcon> &cache = classic ? classicCache : modernCache;
    const QString cacheKey = QStringLiteral("%1@%2").arg(name).arg(size);
    const auto hit = cache.constFind(cacheKey);
    if (hit != cache.constEnd())
        return hit.value();

    if (classic) {
        QPixmap pix = makePixmap(size);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing, true);
        // Classic Chrome's toolbar glyphs are near-black on light grey,
        // dimming to a disabled grey through QIcon's Disabled mode below.
        paintGlyph(p, name, QColor(kClassicText), size);
        p.end();

        QPixmap disabled = makePixmap(size);
        QPainter dp(&disabled);
        dp.setRenderHint(QPainter::Antialiasing, true);
        paintGlyph(dp, name, QColor("#a8a8a8"), size);
        dp.end();

        QIcon result;
        result.addPixmap(pix, QIcon::Normal);
        result.addPixmap(disabled, QIcon::Disabled);
        result.addPixmap(pix, QIcon::Active);
        cache.insert(cacheKey, result);
        return result;
    }

    // Modern: colored glyphs on dark chrome.
    QPixmap pix = makePixmap(size);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    paintGlyph(p, name, QColor("#d7dce8"), size);
    p.end();

    QPixmap disabled = makePixmap(size);
    QPainter dp(&disabled);
    dp.setRenderHint(QPainter::Antialiasing, true);
    paintGlyph(dp, name, QColor("#4b5262"), size);
    dp.end();

    QIcon result;
    result.addPixmap(pix, QIcon::Normal);
    result.addPixmap(disabled, QIcon::Disabled);
    cache.insert(cacheKey, result);
    return result;
}

QString appStyleSheet(const QColor &accent)
{
    if (current() == ThemeStyle::Modern) {
        // The modern sheet lives in ThemeManager (it predates this file);
        // delegate rather than duplicating 80 lines of QSS here.
        return ThemeManager::instance().modernStyleSheet(accent);
    }

    // Every value below is measured off a Chrome 1.0 screenshot: #d4d0c8 tab
    // strip, #f4f4f4 active tab, 1px #a0a0a0 borders, and a 23px omnibox with a
    // 3px inset. Nothing is rounded except the tab shoulders.
    //
    // Values are substituted with named tokens instead of QString::arg(%1..
    // %11): that overload only accepts nine arguments in a single call, and
    // %1/%10 look-alikes make the numbered form easy to get wrong.
    static const char kClassicSheet[] = R"QSS(
        QMainWindow, QWidget#ClassicFrame { background: @FRAME@; }
        QWidget { color: @TEXT@; font-size: 12px; }

        /* ---- Menu bar: classic Windows menu look ---- */
        QMenuBar { background: @TABBAR@; color: @TEXT@; border-bottom: 1px solid @BORDER@;
                   padding: 0px; }
        QMenuBar::item { background: transparent; padding: 2px 8px; }
        QMenuBar::item:selected { background: #316ac5; color: #ffffff; }
        QMenu { background: #ffffff; color: @TEXT@; border: 1px solid @BORDER@; padding: 2px; }
        QMenu::item { padding: 4px 24px 4px 20px; }
        QMenu::item:selected { background: #316ac5; color: #ffffff; }
        QMenu::separator { height: 1px; background: #d0d0d0; margin: 4px 6px; }

        /* ---- Toolbar ---- */
        QToolBar { background: @TABBAR@; border: none; border-bottom: 1px solid @BORDER@;
                   spacing: 1px; padding: 2px 4px; }
        QToolBar::separator { width: 1px; background: @BORDER@; margin: 4px 3px; }
        QToolButton { background: transparent; border: 1px solid transparent;
                      border-radius: 2px; padding: 2px; color: @TEXT@; }
        QToolButton:hover { background: @HOVER@; border: 1px solid #9fc3ef; }
        QToolButton:pressed { background: #c3daf5; border: 1px solid #7da2ce; }
        QToolButton:checked { background: #c3daf5; border: 1px solid #7da2ce; }

        /* ---- Omnibox: the signature 2009 control ---- */
        QLineEdit#ClassicOmnibox {
            background: #ffffff;
            border: 1px solid @BORDER@;
            border-top-color: #8a8a8a;
            border-radius: 2px;
            padding: 2px 22px 2px 22px;   /* reserves room for page icon and go arrow */
            selection-background-color: #316ac5;
            selection-color: #ffffff;
            font-size: 12px;
            min-height: 17px;
        }
        QLineEdit#ClassicOmnibox:focus { border: 1px solid @ACCENT@; }

        /* ---- Tabs: grey gradient strip, square shoulders ---- */
        QTabWidget::pane { border: none; }
        QTabBar { background: @TABBAR@; }
        QTabBar::tab {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 #ececec, stop:0.5 #dcdcdc, stop:1 #cfcfcf);
            color: @TEXT@;
            border: 1px solid @BORDER@;
            border-bottom: none;
            border-top-left-radius: 3px;
            border-top-right-radius: 3px;
            padding: 2px 10px 3px 8px;
            margin-right: 1px;
            margin-top: 3px;
            min-height: 18px;
        }
        QTabBar::tab:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                                         stop:0 #f4f4f4, stop:1 #dedede); }
        QTabBar::tab:selected {
            background: @TABACTIVE@;
            border-bottom: 1px solid @TABACTIVE@;
            margin-top: 1px;
            padding-bottom: 4px;
            font-weight: 600;
        }
        /* Qt draws its own close glyph; the classic look is the flat 2px
           hover wash rather than the modern rounded pill. */
        QTabBar::close-button { subcontrol-position: right; margin-right: 2px; padding: 1px; }
        QTabBar::close-button:hover { background: #d8a0a0; border-radius: 2px; }

        /* ---- Bookmark bar: flat, Chrome 2009 (matched by object name,
               because QSS cannot select a QToolBar by its title) ---- */
        QToolBar#BookmarkToolbar { background: @TABBAR@; border-bottom: 1px solid @BORDER@;
                                   padding: 1px 3px; }
        QToolBar#BookmarkToolbar QToolButton { padding: 1px 4px; font-size: 11px; }

        /* ---- Side panel (hidden by default in the classic chrome) ---- */
        #BrowserSidebar { background: @FRAME@; border-right: 1px solid @BORDER@; }
        #SidebarTitle { color: @TEXT@; font-size: 12px; font-weight: bold; }
        #SidebarTabs { background: #ffffff; border: 1px solid @BORDER@; outline: none; }
        #SidebarTabs::item { min-height: 22px; }
        #SidebarTabs::item:selected { background: #316ac5; color: #ffffff; }
        #SidebarTabs::item:hover { background: @HOVER@; }
        #SidebarNewTabButton { background: @TABBAR@; border: 1px solid @BORDER@;
                               border-radius: 2px; padding: 4px 8px; color: @TEXT@; }
        #SidebarNewTabButton:hover { background: @HOVER@; }

        /* ---- Status bar ---- */
        QStatusBar { background: @TABBAR@; border-top: 1px solid @BORDER@; color: @DIM@; }
        QStatusBar::item { border: none; }
        QProgressBar { background: #ffffff; border: 1px solid @BORDER@; border-radius: 0px;
                       max-height: 10px; text-align: center; }
        QProgressBar::chunk { background: #3d8f3d; }

        /* ---- Splitters ---- */
        QSplitter::handle { background: @BORDER@; }
        QSplitter::handle:hover { background: @ACCENT@; }

        /* ---- Scrollbars: flat 14px classic Windows bars ---- */
        QScrollBar:vertical { background: @FRAME@; width: 14px; margin: 0; }
        QScrollBar::handle:vertical { background: #c0c0c0; min-height: 24px;
                                      border: 1px solid @BORDER@; border-radius: 0px; }
        QScrollBar::handle:vertical:hover { background: #b0b0b0; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background: @FRAME@; height: 14px; margin: 0; }
        QScrollBar::handle:horizontal { background: #c0c0c0; min-width: 24px;
                                        border: 1px solid @BORDER@; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

        /* ---- Dialogs keep the system look ---- */
        QDialog { background: @FRAME@; }
        QDialog QLabel, QDialog QCheckBox { color: @TEXT@; }
        QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                                  stop:0 #f4f4f4, stop:1 #dcdcdc);
                      border: 1px solid @BORDER@; border-radius: 2px; padding: 3px 14px;
                      color: @TEXT@; }
        QPushButton:hover { background: @HOVER@; border-color: #7da2ce; }
        QPushButton:pressed { background: #c3daf5; }
        QLineEdit, QComboBox, QSpinBox { background: #ffffff; border: 1px solid @BORDER@;
                                         border-radius: 0px; padding: 2px 4px; color: @TEXT@; }
        QLineEdit:focus, QComboBox:focus { border-color: @ACCENT@; }
        QListWidget, QTreeWidget, QTableWidget { background: #ffffff; border: 1px solid @BORDER@;
                                                 color: @TEXT@; }
        QListWidget::item:selected { background: #316ac5; color: #ffffff; }

        /* ---- Classic chrome object names ---- */
        #NewTabStripButton { background: transparent; border: 1px solid transparent;
                             border-radius: 2px; color: @TEXT@; font-size: 14px; }
        #NewTabStripButton:hover { background: @HOVER@; border-color: #9fc3ef; }
        #ClassicOmniboxGo { background: transparent; border: 1px solid transparent;
                            border-radius: 2px; }
        #ClassicOmniboxGo:hover { background: @HOVER@; border-color: #9fc3ef; }
        #ClassicPageIcon { background: transparent; border: none; }
    )QSS";

    QString sheet = QString::fromLatin1(kClassicSheet);
    const QVector<QPair<const char *, QString>> tokens = {
        {"@FRAME@", QString::fromLatin1(kClassicFrame)},
        {"@TABBAR@", QString::fromLatin1(kClassicTabBar)},
        {"@BORDER@", QString::fromLatin1(kClassicBorder)},
        {"@TABACTIVE@", QString::fromLatin1(kClassicTabActive)},
        {"@HOVER@", QStringLiteral("#e4eefb")},   // classic Windows hover wash
        {"@ACCENT@", accent.name()},
        {"@TEXT@", QString::fromLatin1(kClassicText)},
        {"@DIM@", QString::fromLatin1(kClassicDim)},
    };
    for (const auto &token : tokens)
        sheet.replace(QString::fromLatin1(token.first), token.second);
    return sheet;
}

} // namespace Theme
