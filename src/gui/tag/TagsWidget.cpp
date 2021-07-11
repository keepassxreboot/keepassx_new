/*
  MIT License

  Copyright (c) 2019 Nicolai Trandafil
  Modified in 2021 by the KeePassXC Team <team@keepassxc.org>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "TagsWidget.h"

#include <QApplication>
#include <QCompleter>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOptionFrame>
#include <QTextLayout>

#include <cassert>

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
#define FONT_METRICS_WIDTH(fmt, ...) fmt.width(__VA_ARGS__)
#else
#define FONT_METRICS_WIDTH(fmt, ...) fmt.horizontalAdvance(__VA_ARGS__)
#endif

namespace
{

    constexpr int top_text_margin = 1;
    constexpr int bottom_text_margin = 1;
    constexpr int left_text_margin = 1;
    constexpr int right_text_margin = 1;

    constexpr int vertical_margin = 3;
    constexpr int bottommargin = 1;
    constexpr int topmargin = 1;

    constexpr int horizontal_margin = 3;
    constexpr int leftmargin = 1;
    constexpr int rightmargin = 1;

    constexpr int tag_spacing = 3;
    constexpr int tag_inner_name_padding = 5;
    constexpr int tag_cross_width = 5;
    constexpr float tag_cross_radius = tag_cross_width / 2;
    constexpr int tag_cross_padding = 5;

    struct Tag
    {
        QString text;
        QRect rect;
    };

    /// Non empty string filtering iterator
    template <class It> struct EmptySkipIterator
    {
        EmptySkipIterator() = default;

        // skip until `end`
        explicit EmptySkipIterator(It it, It end)
            : it(it)
            , end(end)
        {
            while (this->it != end && this->it->text.isEmpty()) {
                ++this->it;
            }
        }

        explicit EmptySkipIterator(It it)
            : it(it)
        {
        }

        using difference_type = typename std::iterator_traits<It>::difference_type;
        using value_type = typename std::iterator_traits<It>::value_type;
        using pointer = typename std::iterator_traits<It>::pointer;
        using reference = typename std::iterator_traits<It>::reference;
        using iterator_category = std::output_iterator_tag;

        EmptySkipIterator& operator++()
        {
            while (++it != end && it->text.isEmpty())
                ;
            return *this;
        }

        value_type& operator*()
        {
            return *it;
        }

        pointer operator->()
        {
            return &(*it);
        }

        bool operator!=(EmptySkipIterator const& rhs) const
        {
            return it != rhs.it;
        }

    private:
        It it;
        It end;
    };

    template <class It> EmptySkipIterator(It, It) -> EmptySkipIterator<It>;

} // namespace

struct TagsWidget::Impl
{
    explicit Impl(TagsWidget* const& ifce)
        : ifce(ifce)
        , tags{Tag()}
        , editing_index(0)
        , cursor(0)
        , blink_timer(0)
        , blink_status(true)
        , select_start(0)
        , select_size(0)
        , cross_deleter(true)
        , completer(std::make_unique<QCompleter>())
    {
    }

    void initStyleOption(QStyleOptionFrame* option) const
    {
        assert(option);
        option->initFrom(ifce);
        option->rect = ifce->contentsRect();
        option->lineWidth = ifce->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, option, ifce);
        option->midLineWidth = 0;
        option->state |= QStyle::State_Sunken;
        option->features = QStyleOptionFrame::None;
    }

    inline QRectF crossRect(QRectF const& r) const
    {
        QRectF cross(QPointF{0, 0}, QSizeF{tag_cross_width + tag_cross_padding * 2, r.top() - r.bottom()});
        cross.moveCenter(QPointF(r.right() - tag_cross_radius - tag_cross_padding, r.center().y()));
        return cross;
    }

    bool inCrossArea(int tag_index, QPoint const& point) const
    {
        return cross_deleter
                   ? crossRect(tags[tag_index].rect).adjusted(-2, 0, 0, 0).translated(-hscroll, 0).contains(point)
                         && (!cursorVisible() || tag_index != editing_index)
                   : false;
    }

    template <class It> void drawTagsWidget(QPainter& p, std::pair<It, It> range) const
    {
        for (auto it = range.first; it != range.second; ++it) {
            QRect const& i_r = it->rect.translated(-hscroll, 0);
            auto const text_pos =
                i_r.topLeft()
                + QPointF(tag_inner_name_padding,
                          ifce->fontMetrics().ascent() + ((i_r.height() - ifce->fontMetrics().height()) / 2));

            // drag tag rect
            QColor const blue(0, 96, 100, 150);
            QPainterPath path;
            auto cornerRadius = 4;
            path.addRoundedRect(i_r, cornerRadius, cornerRadius);
            p.fillPath(path, blue);

            // draw text
            p.drawText(text_pos, it->text);

            if (cross_deleter) {
                // calc cross rect
                auto const i_cross_r = crossRect(i_r);

                QPainterPath crossRectBg1, crossRectBg2;
                crossRectBg1.addRoundedRect(i_cross_r, cornerRadius, cornerRadius);
                // cover left rounded corners
                crossRectBg2.addRect(
                    i_cross_r.left(), i_cross_r.bottom(), tag_cross_radius, i_cross_r.top() - i_cross_r.bottom());
                p.fillPath(crossRectBg1, QColorConstants::Cyan);
                p.fillPath(crossRectBg2, QColorConstants::Cyan);

                QPen pen = p.pen();
                pen.setWidth(2);

                p.save();
                p.setPen(pen);
                p.setRenderHint(QPainter::Antialiasing);
                p.drawLine(QLineF(i_cross_r.center() - QPointF(tag_cross_radius, tag_cross_radius),
                                  i_cross_r.center() + QPointF(tag_cross_radius, tag_cross_radius)));
                p.drawLine(QLineF(i_cross_r.center() - QPointF(-tag_cross_radius, tag_cross_radius),
                                  i_cross_r.center() + QPointF(-tag_cross_radius, tag_cross_radius)));
                p.restore();
            }
        }
    }

    QRect cRect() const
    {
        QStyleOptionFrame panel;
        initStyleOption(&panel);
        QRect r = ifce->style()->subElementRect(QStyle::SE_LineEditContents, &panel, ifce);
        r.adjust(left_text_margin, top_text_margin, -right_text_margin, -bottom_text_margin);
        return r;
    }

    void calcRects()
    {
        auto const r = cRect();
        auto lt = r.topLeft();

        if (cursorVisible()) {
            calcRects(lt, r.height(), std::make_pair(tags.begin(), tags.begin() + std::ptrdiff_t(editing_index)));
            calcEditorRect(lt, r.height());
            calcRects(lt, r.height(), std::make_pair(tags.begin() + std::ptrdiff_t(editing_index + 1), tags.end()));
        } else {
            calcRects(lt,
                      r.height(),
                      std::make_pair(EmptySkipIterator(tags.begin(), tags.end()), EmptySkipIterator(tags.end())));
        }
    }

    template <class It> void calcRects(QPoint& lt, int height, std::pair<It, It> range)
    {
        for (auto it = range.first; it != range.second; ++it) {
            // calc text rect
            const auto i_width = FONT_METRICS_WIDTH(ifce->fontMetrics(), it->text);
            QRect i_r(lt, QSize(i_width, height));
            i_r.translate(tag_inner_name_padding, 0);
            if (cross_deleter) {
                i_r.adjust(
                    -tag_inner_name_padding, 0, tag_inner_name_padding + tag_cross_padding * 2 + tag_cross_width, 0);
            } else {
                i_r.adjust(-tag_inner_name_padding, 0, tag_inner_name_padding, 0);
            }
            it->rect = i_r;
            lt.setX(i_r.right() + tag_spacing);
        }
    }

    void calcEditorRect(QPoint& lt, int height)
    {
        auto const w = FONT_METRICS_WIDTH(ifce->fontMetrics(), text_layout.text()) + tag_inner_name_padding
                       + tag_inner_name_padding;
        currentRect() = QRect(lt, QSize(w, height));
        lt += QPoint(w + tag_spacing, 0);
    }

    void setCursorVisible(bool visible)
    {
        if (blink_timer) {
            ifce->killTimer(blink_timer);
            blink_timer = 0;
            blink_status = true;
        }

        if (visible) {
            int flashTime = QGuiApplication::styleHints()->cursorFlashTime();
            if (flashTime >= 2) {
                blink_timer = ifce->startTimer(flashTime / 2);
            }
        } else {
            blink_status = false;
        }
    }

    bool cursorVisible() const
    {
        return blink_timer;
    }

    void updateCursorBlinking()
    {
        setCursorVisible(cursorVisible());
    }

    void updateDisplayText()
    {
        text_layout.clearLayout();
        text_layout.setText(currentText());
        text_layout.beginLayout();
        text_layout.createLine();
        text_layout.endLayout();
    }

    void setEditingIndex(int i)
    {
        assert(i <= tags.size());
        if (currentText().isEmpty()) {
            tags.erase(std::next(tags.begin(), std::ptrdiff_t(editing_index)));
            if (editing_index <= i) {
                --i;
            }
        }
        editing_index = i;
    }

    void currentText(QString const& text)
    {
        currentText() = text;
        moveCursor(currentText().length(), false);
        updateDisplayText();
        calcRects();
        ifce->update();
    }

    QString const& currentText() const
    {
        return tags[editing_index].text;
    }

    QString& currentText()
    {
        return tags[editing_index].text;
    }

    QRect const& currentRect() const
    {
        return tags[editing_index].rect;
    }

    QRect& currentRect()
    {
        return tags[editing_index].rect;
    }

    void editNewTag()
    {
        tags.push_back(Tag());
        setEditingIndex(tags.size() - 1);
        moveCursor(0, false);
    }

    void setupCompleter()
    {
        completer->setWidget(ifce);
        connect(completer.get(), qOverload<QString const&>(&QCompleter::activated), [this](QString const& text) {
            currentText(text);
        });
    }

    QVector<QTextLayout::FormatRange> formatting() const
    {
        if (select_size == 0) {
            return {};
        }

        QTextLayout::FormatRange selection;
        selection.start = select_start;
        selection.length = select_size;
        selection.format.setBackground(ifce->palette().brush(QPalette::Highlight));
        selection.format.setForeground(ifce->palette().brush(QPalette::HighlightedText));
        return {selection};
    }

    bool hasSelection() const noexcept
    {
        return select_size > 0;
    }

    void removeSelection()
    {
        cursor = select_start;
        currentText().remove(cursor, select_size);
        deselectAll();
    }

    void removeBackwardOne()
    {
        if (hasSelection()) {
            removeSelection();
        } else {
            currentText().remove(--cursor, 1);
        }
    }

    void selectAll()
    {
        select_start = 0;
        select_size = currentText().size();
    }

    void deselectAll()
    {
        select_start = 0;
        select_size = 0;
    }

    void moveCursor(int pos, bool mark)
    {
        if (mark) {
            auto e = select_start + select_size;
            int anchor = select_size > 0 && cursor == select_start ? e
                         : select_size > 0 && cursor == e          ? select_start
                                                                   : cursor;
            select_start = qMin(anchor, pos);
            select_size = qMax(anchor, pos) - select_start;
        } else {
            deselectAll();
        }

        cursor = pos;
    }

    qreal natrualWidth() const
    {
        return tags.back().rect.right() - tags.front().rect.left();
    }

    qreal cursorToX()
    {
        return text_layout.lineAt(0).cursorToX(cursor);
    }

    void calcHScroll(QRect const& r)
    {
        auto const rect = cRect();
        auto const width_used = qRound(natrualWidth()) + 1;
        int const cix = r.x() + qRound(cursorToX());
        if (width_used <= rect.width()) {
            // text fit
            hscroll = 0;
        } else if (cix - hscroll >= rect.width()) {
            // text doesn't fit, cursor is to the right of lineRect (scroll right)
            hscroll = cix - rect.width() + 1;
        } else if (cix - hscroll < 0 && hscroll < width_used) {
            // text doesn't fit, cursor is to the left of lineRect (scroll left)
            hscroll = cix;
        } else if (width_used - hscroll < rect.width()) {
            // text doesn't fit, text document is to the left of lineRect; align
            // right
            hscroll = width_used - rect.width() + 1;
        } else {
            // in case the text is bigger than the lineedit, the hscroll can never be negative
            hscroll = qMax(0, hscroll);
        }
    }

    void editPreviousTag()
    {
        if (editing_index > 0) {
            setEditingIndex(editing_index - 1);
            moveCursor(currentText().size(), false);
        }
    }

    void editNextTag()
    {
        if (editing_index < tags.size() - 1) {
            setEditingIndex(editing_index + 1);
            moveCursor(0, false);
        }
    }

    void editTag(int i)
    {
        assert(i >= 0 && i < tags.size());
        setEditingIndex(i);
        moveCursor(currentText().size(), false);
    }

    TagsWidget* const ifce;
    QList<Tag> tags;
    int editing_index;
    int cursor;
    int blink_timer;
    bool blink_status;
    QTextLayout text_layout;
    int select_start;
    int select_size;
    bool cross_deleter;
    std::unique_ptr<QCompleter> completer;
    int hscroll{0};
};

TagsWidget::TagsWidget(QWidget* parent)
    : QWidget(parent)
    , impl(std::make_unique<Impl>(this))
    , m_readOnly(false)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::IBeamCursor);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setMouseTracking(true);

    impl->setupCompleter();
    impl->setCursorVisible(hasFocus());
    impl->updateDisplayText();
}

void TagsWidget::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    if (m_readOnly) {
        setFocusPolicy(Qt::NoFocus);
        setCursor(Qt::ArrowCursor);
        setAttribute(Qt::WA_InputMethodEnabled, false);
        impl->cross_deleter = false;
    } else {
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::IBeamCursor);
        setAttribute(Qt::WA_InputMethodEnabled, true);
        impl->cross_deleter = true;
    }
}

TagsWidget::~TagsWidget() = default;

void TagsWidget::resizeEvent(QResizeEvent*)
{
    impl->calcRects();
}

void TagsWidget::focusInEvent(QFocusEvent*)
{
    impl->setCursorVisible(true);
    impl->updateDisplayText();
    impl->calcRects();
    update();
}

void TagsWidget::focusOutEvent(QFocusEvent*)
{
    impl->setCursorVisible(false);
    impl->updateDisplayText();
    impl->calcRects();
    update();
}

void TagsWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);

    // opt
    auto const panel = [this] {
        QStyleOptionFrame panel;
        impl->initStyleOption(&panel);
        return panel;
    }();

    // draw frame
    if (!m_readOnly) {
        style()->drawPrimitive(QStyle::PE_PanelLineEdit, &panel, &p, this);
    }

    // clip
    auto const rect = impl->cRect();
    p.setClipRect(rect);

    if (impl->cursorVisible()) {
        // not terminated tag pos
        auto const& r = impl->currentRect();
        auto const& txt_p = r.topLeft() + QPointF(tag_inner_name_padding, ((r.height() - fontMetrics().height()) / 2));

        // scroll
        impl->calcHScroll(r);

        // tags
        impl->drawTagsWidget(
            p,
            std::make_pair(impl->tags.cbegin(), std::next(impl->tags.cbegin(), std::ptrdiff_t(impl->editing_index))));

        // draw not terminated tag
        auto const formatting = impl->formatting();
        impl->text_layout.draw(&p, txt_p - QPointF(impl->hscroll, 0), formatting);

        // draw cursor
        if (impl->blink_status) {
            impl->text_layout.drawCursor(&p, txt_p - QPointF(impl->hscroll, 0), impl->cursor);
        }

        // tags
        impl->drawTagsWidget(
            p,
            std::make_pair(std::next(impl->tags.cbegin(), std::ptrdiff_t(impl->editing_index + 1)), impl->tags.cend()));
    } else {
        impl->drawTagsWidget(p,
                             std::make_pair(EmptySkipIterator(impl->tags.begin(), impl->tags.end()),
                                            EmptySkipIterator(impl->tags.end())));
    }
}

void TagsWidget::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == impl->blink_timer) {
        impl->blink_status = !impl->blink_status;
        update();
    }
}

void TagsWidget::mousePressEvent(QMouseEvent* event)
{
    bool found = false;
    for (int i = 0; i < impl->tags.size(); ++i) {
        if (impl->inCrossArea(i, event->pos())) {
            impl->tags.erase(impl->tags.begin() + std::ptrdiff_t(i));
            if (i <= impl->editing_index) {
                --impl->editing_index;
            }
            found = true;
            break;
        }

        if (!impl->tags[i].rect.translated(-impl->hscroll, 0).contains(event->pos())) {
            continue;
        }

        if (impl->editing_index == i) {
            impl->moveCursor(impl->text_layout.lineAt(0).xToCursor(
                                 (event->pos() - impl->currentRect().translated(-impl->hscroll, 0).topLeft()).x()),
                             false);
        } else {
            impl->editTag(i);
        }

        found = true;
        break;
    }

    if (!found) {
        impl->editNewTag();
        event->accept();
    }

    if (event->isAccepted()) {
        impl->updateDisplayText();
        impl->calcRects();
        impl->updateCursorBlinking();
        update();
    }
}

QSize TagsWidget::sizeHint() const
{
    ensurePolished();
    QFontMetrics fm(font());
    int h = fm.height() + 2 * vertical_margin + top_text_margin + bottom_text_margin + topmargin + bottommargin;
    int w = fm.boundingRect(QLatin1Char('x')).width() * 17 + 2 * horizontal_margin + leftmargin + rightmargin; // "some"
    QStyleOptionFrame opt;
    impl->initStyleOption(&opt);
    return (style()->sizeFromContents(
        QStyle::CT_LineEdit, &opt, QSize(w, h).expandedTo(QApplication::globalStrut()), this));
}

QSize TagsWidget::minimumSizeHint() const
{
    ensurePolished();
    QFontMetrics fm = fontMetrics();
    int h = fm.height() + qMax(2 * vertical_margin, fm.leading()) + top_text_margin + bottom_text_margin + topmargin
            + bottommargin;
    int w = fm.maxWidth() + leftmargin + rightmargin;
    QStyleOptionFrame opt;
    impl->initStyleOption(&opt);
    return (style()->sizeFromContents(
        QStyle::CT_LineEdit, &opt, QSize(w, h).expandedTo(QApplication::globalStrut()), this));
}

void TagsWidget::keyPressEvent(QKeyEvent* event)
{
    event->setAccepted(false);
    bool unknown = false;

    if (event == QKeySequence::SelectAll) {
        impl->selectAll();
        event->accept();
    } else if (event == QKeySequence::SelectPreviousChar) {
        impl->moveCursor(impl->text_layout.previousCursorPosition(impl->cursor), true);
        event->accept();
    } else if (event == QKeySequence::SelectNextChar) {
        impl->moveCursor(impl->text_layout.nextCursorPosition(impl->cursor), true);
        event->accept();
    } else {
        switch (event->key()) {
        case Qt::Key_Left:
            if (impl->cursor == 0) {
                impl->editPreviousTag();
            } else {
                impl->moveCursor(impl->text_layout.previousCursorPosition(impl->cursor), false);
            }
            event->accept();
            break;
        case Qt::Key_Right:
            if (impl->cursor == impl->currentText().size()) {
                impl->editNextTag();
            } else {
                impl->moveCursor(impl->text_layout.nextCursorPosition(impl->cursor), false);
            }
            event->accept();
            break;
        case Qt::Key_Home:
            if (impl->cursor == 0) {
                impl->editTag(0);
            } else {
                impl->moveCursor(0, false);
            }
            event->accept();
            break;
        case Qt::Key_End:
            if (impl->cursor == impl->currentText().size()) {
                impl->editTag(impl->tags.size() - 1);
            } else {
                impl->moveCursor(impl->currentText().length(), false);
            }
            event->accept();
            break;
        case Qt::Key_Backspace:
            if (!impl->currentText().isEmpty()) {
                impl->removeBackwardOne();
            } else if (impl->editing_index > 0) {
                impl->editPreviousTag();
            }
            event->accept();
            break;
        case Qt::Key_Space:
            if (!impl->currentText().isEmpty()) {
                impl->tags.insert(impl->tags.begin() + std::ptrdiff_t(impl->editing_index + 1), Tag());
                impl->editNextTag();
            }
            event->accept();
            break;
        default:
            unknown = true;
        }
    }

    if (unknown && isAcceptableInput(event)) {
        if (impl->hasSelection()) {
            impl->removeSelection();
        }
        impl->currentText().insert(impl->cursor, event->text());
        impl->cursor += event->text().length();
        event->accept();
        unknown = false;
    }

    if (event->isAccepted()) {
        // update content
        impl->updateDisplayText();
        impl->calcRects();
        impl->updateCursorBlinking();

        // complete
        impl->completer->setCompletionPrefix(impl->currentText());
        impl->completer->complete();

        update();

        emit tagsEdited();
    }
}

void TagsWidget::completion(QStringList const& completions)
{
    impl->completer = std::make_unique<QCompleter>([&] {
        QStringList ret;
        std::copy(completions.begin(), completions.end(), std::back_inserter(ret));
        return ret;
    }());
    impl->setupCompleter();
}

void TagsWidget::tags(QStringList const& tags)
{
    QList<Tag> t{Tag()};
    std::transform(tags.begin(), tags.end(), std::back_inserter(t), [](QString const& text) {
        return Tag{text, QRect()};
    });
    impl->tags = std::move(t);
    impl->editing_index = 0;
    impl->moveCursor(0, false);

    impl->editNewTag();
    impl->updateDisplayText();
    impl->calcRects();

    update();
}

QStringList TagsWidget::tags() const
{
    QStringList ret;
    std::transform(EmptySkipIterator(impl->tags.begin(), impl->tags.end()),
                   EmptySkipIterator(impl->tags.end()),
                   std::back_inserter(ret),
                   [](Tag const& tag) { return tag.text; });
    return ret;
}

void TagsWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_readOnly) {
        for (int i = 0; i < impl->tags.size(); ++i) {
            if (impl->inCrossArea(i, event->pos())) {
                setCursor(Qt::ArrowCursor);
                return;
            }
        }
        setCursor(Qt::IBeamCursor);
    }
}

bool TagsWidget::isAcceptableInput(const QKeyEvent* event)
{
    const QString text = event->text();
    if (text.isEmpty())
        return false;

    const QChar c = text.at(0);

    // Formatting characters such as ZWNJ, ZWJ, RLM, etc. This needs to go before the
    // next test, since CTRL+SHIFT is sometimes used to input it on Windows.
    if (c.category() == QChar::Other_Format)
        return true;

    // QTBUG-35734: ignore Ctrl/Ctrl+Shift; accept only AltGr (Alt+Ctrl) on German keyboards
    if (event->modifiers() == Qt::ControlModifier || event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier)) {
        return false;
    }

    if (c.isPrint())
        return true;

    if (c.category() == QChar::Other_PrivateUse)
        return true;

    return false;
}
