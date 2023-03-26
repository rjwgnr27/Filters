/*
 * Copyright (c) 2008-2023, Rick Wagner. All rights reserved.
 */

/**
 * @file wlogtext.cpp
 * Implementation of a QWidget display of simple formatted text. The use of
 * wLogTextWidget is intended to be low overhead with fast rendering of line
 * oriented text, as one would find in viewing a log file. The lines are not
 * wrapped, thus one logical line occupies one physical line in the widget
 * client area.
 *
 * To simplify (speed up) loading and drawing, formatting is per line, with the
 * exception of a single selection region. This eliminates having to interpret
 * embedded formatting codes or having to make font shifts on a line. Lines are
 * displayed with a fixed pitch font, with all lines having the same height.
 * This simpifies position calculation and rendering.
 **/
//#define TIME_DRAW_REQUEST

#include "wlogtextprivate.h"

// Qt includes
#include <QActionEvent>
#include <QApplication>
#include <QCharRef>
#include <QChildEvent>
#include <QDebug>
#include <QDrag>
#include <QFontDatabase>
#include <QLabel>
#include <QMimeData>
#include <QRegularExpression>
#include <QPainter>
#include <QScrollBar>

#include <algorithm>
#include <ranges>

/** Application specific QEvent type to signal a need for screen refresh */
const QEvent::Type updatesNeededEvent = QEvent::User;

/** Text string of default palette name */
static const QString defaultPaletteNameString = QStringLiteral("default");

/** Size in pixels of the border of a gutter */
static constexpr int gutterBorder = 1;
/** Size in pixels of the border between the gutter and the text */
static constexpr int textBorder = 1;


wLogText::wLogText(QWidget *parent) :
        QAbstractScrollArea{parent},
        d{new wLogTextPrivate(this)}
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_StaticContents, true);
    viewport()->setFocusPolicy(Qt::StrongFocus);
    viewport()->setMouseTracking(true); //Enable to make cursor changes

    setCursor(Qt::IBeamCursor);
    verticalScrollBar()->setCursor(Qt::ArrowCursor);
    horizontalScrollBar()->setCursor(Qt::ArrowCursor);

    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    // Don't let QAbstractScrollArea mess with scroll bars.  We will
    // deal with them.
    disconnect(verticalScrollBar(), SIGNAL(valueChanged(int)));
    disconnect(horizontalScrollBar(), SIGNAL(valueChanged(int)));

    connect(verticalScrollBar(), SIGNAL(valueChanged(int)),
             SLOT(vScrollChange(int)));
    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)),
             SLOT(hScrollChange(int)));
}


wLogText::~wLogText()
{
    qDeleteAll(items);
}


cell wLogText::pointToCell(QPoint const& point) const
{
    return {std::min(d->yToLine(point.y()), m_lineCount), d->xToCharColumn(point.x())};
}

void wLogText::finalize()
{
    trimLines();
    items.shrink_to_fit();
    finalized = true;
}


wLogTextPrivate::wLogTextPrivate(wLogText *base) :
        q(base), m_gutterOffset{textBorder}
{
    palettes[defaultPaletteNameString] =  new logTextPalette(
            defaultPaletteNameString, 1,
            m_qpalette.color(QPalette::Active, QPalette::WindowText),
            m_qpalette.color(QPalette::Active, QPalette::Base),
            m_qpalette.color(QPalette::Active, QPalette::AlternateBase));
}


wLogTextPrivate::~wLogTextPrivate()
{
    qDeleteAll(palettes);
}


void wLogText::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == d->caretBlinkTimer) {
        d->caretBlinkOn = !d->caretBlinkOn;
        d->updateCellRange(d->caretPosition,
                cell(d->caretPosition.lineNumber(), d->caretPosition.columnNumber() + 1));
    } else if (event->timerId() == hoverTimerId) {
        if (m_hoverTime) {
            int const elapsed = mouseHoverOnTime.elapsed();
            if (  !mouseHovering && (elapsed - 5) > m_hoverTime) {
                int const line = std::min(d->yToLine(mouseRawPos.y()), m_lineCount);
                if (validLineNumber(line)) {
                    if (d->inTheGutter(mouseRawPos.x())) {
                        mouseHovering = true;
                        Q_EMIT hover(line, -1);
                    } else {
                        int col = d->xToCharColumn(mouseRawPos.x());
                        if (col < items[line]->m_text.length()) {
                            mouseHovering = true;
                            Q_EMIT hover(line, col);
                        }
                    }
                }
            }
            if (elapsed > 5000) {
                /* If mouse has been idle for a while, shut off the timers. */
                killTimer(hoverTimerId);
                hoverTimerId = 0;
            }
        } else {
            killTimer(hoverTimerId);
            hoverTimerId = 0;
            if (mouseHovering)
                Q_EMIT hover(-1,-1);
        }
    } else if (event->timerId() == d->selectScrollTimerId) {
        verticalScrollBar()->setValue(verticalScrollBar()->value() + d->timedVScrollStep );
    }
}


void wLogText::setUpdatesNeeded(updateLevel level)
{
    if (level > updatesNeeded) {
        updatesNeeded = level;
        // It's OK to err on the side of extra posts; we just don't
        // want to overdose on them.
        if (updatesEnabled() && (updatePosted++ == 0)) {
            QApplication::postEvent(this, new QEvent(updatesNeededEvent));
        }
    }
}


void wLogText::customEvent(QEvent *event)
{
    Q_UNUSED(event);
    updatePosted = 0;
    if (updatesNeeded && updatesEnabled()) {
        d->setContentSize();

        if (d->isScrollable()) {
            QScrollBar *vsb = verticalScrollBar();
            if ((m_lineCount * d->m_textLineHeight) <= vsb->maximum()) {
                vsb->setValue(0);
            } else {
                int showable = viewport()->size().height() / d->m_textLineHeight;
                if (m_lineCount <= showable) {
                    vsb->setValue(0);
                } else
                    vsb->setValue(vsb->maximum());
            }

            horizontalScrollBar()->setValue(0);
            viewport()->update();
        }
        d->updateCaretPos(m_lineCount, 0);
        updatesNeeded = noUpdate;
    }
}


void wLogText::focusInEvent(QFocusEvent *evt)
{
    if (d->m_ShowCaret && (d->caretBlinkTimer == -1)) {
        d->caretBlinkTimer = startTimer(500);
    }
    QAbstractScrollArea::focusInEvent(evt);
}


void wLogText::focusOutEvent(QFocusEvent *evt)
{
    if (d->caretBlinkTimer != -1) {
        killTimer(d->caretBlinkTimer);
        d->caretBlinkTimer = -1;
        d->caretBlinkOn = true;
        d->updateCellRange(d->caretPosition, d->caretPosition.nextCol());
    }
    QAbstractScrollArea::focusOutEvent(evt);
}


void wLogText::paintEvent(QPaintEvent *event)
{
    d->drawContents(event->rect());
}


void wLogTextPrivate::drawContents(const QRect& bounds)
{
#ifdef TIME_DRAW_REQUEST
    if (timeDraw) {
        qDebug() << "Draw start dT " << drawTimer.elapsed();
    }
#endif

    if (!activePalette)
        activatePalette(defaultPaletteNameString);

    int const verticalClip = q->viewport()->size().height();

    // On first paint, or if the viewport has resized, allocate a pixmap on
    // which to paint:
    if (!drawPixMap || pmSize != q->viewport()->size()) {
        pmSize = q->viewport()->size();
        drawPixMap.reset(new QPixmap(pmSize));
        if (!drawPixMap || drawPixMap->isNull()) {
            qCritical() << "could not allocate pixmap";
            return;
        }
    }

    // As of Qt4 (4.4.3), a 'fontMetrics().width' is not valid unless attached
    // to a painter.  So after a font change, set the width to unknown, and
    // find it here in the paint event.
    QPainter pixmapPainter(drawPixMap.get());
    pixmapPainter.setRenderHint(QPainter::Antialiasing, false);
    if (m_characterWidth == 0) {
        pixmapPainter.setFont(activePalette->style(0).font);
        m_characterWidth = pixmapPainter.fontMetrics().horizontalAdvance(QLatin1Char('X'));
        m_textLineHeight = pixmapPainter.fontMetrics().lineSpacing();
        pixmapPainter.end();
        q->adjustHorizontalScrollBar();
        q->setUpdatesNeeded(wLogText::updateFull); // Force repaint because scroll bars may show
        return;
    }

    QColor const defBGColor = activePalette->style(0).backgroundColor;

    pixmapPainter.setBackground(defBGColor);
    pixmapPainter.eraseRect(bounds);

    if (gutterWidth > 0) {
        pixmapPainter.setBackground(m_qpalette.color(QPalette::Active, QPalette::ToolTipBase));
        pixmapPainter.eraseRect(0, 0, gutterWidth, pmSize.height());
        if (gutterBorder > 0) {   // Draw gutter edge?
            QPen save(pixmapPainter.pen());
            QPen pen(save);
            pen.setColor(QPalette::Shadow);
            pen.setWidth(gutterBorder);
            pixmapPainter.setPen(pen);
            pixmapPainter.drawLine(gutterWidth, 0, gutterWidth, pmSize.height());
            pixmapPainter.setPen(save);
        }
        pixmapPainter.setBackground(defBGColor);
    }

    /* TODO: Account for top and/or left viewport clipping in paintEvent.
     * Currently we do not over-draw past the visble bounds right or
     * or bottom. However, we do not yet take into account the visible top or
     * left clipping. So, if the viewport is obscured on the right or bottom,
     * the number of characters and lines painted are reduced accordingly.
     * However, painting is always done from the first viewport line 0,
     * character 0; event if the top or left is obscured. That will be further
     * optimization.
     */
    // Get the line number of the first line touched by the top of the clip.
    // We may actually only need a frament of the line (or even the interline
    // white-space).  But we'll draw the whole first line, and let the clipping
    // region sort it out.  Same for the last line.
    int const visibleLines = (verticalClip + m_textLineHeight-1) / m_textLineHeight;
    lineNumber_t line, lastDrawLine;
    if (visibleLines < q->m_lineCount) [[likely]] {
        line = (q->verticalScrollBar()->value() /  m_textLineHeight);
        lastDrawLine = line + visibleLines;
        if (lastDrawLine > q->m_lineCount) {
            line = q->m_lineCount - visibleLines;
            lastDrawLine = q->m_lineCount;
        }
    } else {
        line = 0;
        lastDrawLine = q->m_lineCount;
        //last = std::max(q->m_lineCount-1, 0);
    }
    const int lastPaintLine = std::min(lastDrawLine,
            (line + (bounds.height() + m_textLineHeight + 1) / m_textLineHeight));

    // If drawing at first line, paint it flush to top; else paint the final
    // pixmap so the bottom line is flush to bottom.
    int pmYTop = (line == 0) ? 0 : (verticalClip - (visibleLines * m_textLineHeight)); // Top of text line
    int pmYBase = pmYTop + m_textLineHeight - textLineBaselineOffset;           // Baseline for text
    int const eraseWidth = std::min(pmSize.width(), bounds.width()) - m_gutterOffset;
    int const pmChars = (eraseWidth + m_characterWidth - 1) / m_characterWidth;   //!< Displayable characters in pixma
    logItemsImplCIt it = q->items.cbegin() + line;
    logItemsImplCIt itemsEnd = q->items.cbegin() + lastPaintLine;
    int const firstChar = q->horizontalScrollBar()->value();

    // Initially set out of range, to skip selection drawing.  If we determine
    // that this redraw event is in the selection region, set these values in
    // range, until we're done; then we'll set them out of range again:
    lineNumber_t firstSelectionLine = std::numeric_limits<lineNumber_t>::max();
    lineNumber_t lastSelectionLine  = std::numeric_limits<lineNumber_t>::max();
    int selectionLeftCol = firstChar;
    int selectionRightCol = firstChar;
    if (selecting) {
        if (selectTop.lineNumber() <= selectBottom.lineNumber()) {
            firstSelectionLine = std::max(selectTop.lineNumber(), line);
            selectionLeftCol = std::max(0, selectTop.columnNumber() - firstChar);
            lastSelectionLine = std::min(selectBottom.lineNumber(), lastDrawLine);
            selectionRightCol = std::max(0, selectBottom.columnNumber() - firstChar);
        } else {
            firstSelectionLine = std::max(selectBottom.lineNumber(), line);
            selectionLeftCol = std::max(0, selectBottom.columnNumber() - firstChar);
            lastSelectionLine = std::min(selectTop.lineNumber(), lastDrawLine);
            selectionRightCol = std::max(0, selectTop.columnNumber() - firstChar);
        }
    }

    // Remember last style, so we don't do so many font changes:
    styleId_t lastStyleId = activePalette->numStyles() + 1;
    const styleItem *style= &activePalette->style(0);
    for (; it != itemsEnd;  ++it, ++line,
                pmYTop += m_textLineHeight, pmYBase += m_textLineHeight) {
        logTextItemCPtr item = *it;
        const QString text(item->text().mid(firstChar, pmChars));
        int lineCharacters = text.length();

        // Can we avoid a font change?
        if (lastStyleId != item->styleId()) [[unlikely]] {
            lastStyleId = item->styleId();
            style = &activePalette->style(item->styleId());
            pixmapPainter.setFont(style->font);
        }

        // See if the current line is in the selection range:
        if (line >= firstSelectionLine) [[unlikely]] {
            // Draw selection background:
            int right = lineCharacters;
            if (line == lastSelectionLine) {
                right = selectionRightCol;
                // After this one, we're done drawing selection for this event,
                // so set the values out of range as a flag.
                firstSelectionLine = lastSelectionLine = std::numeric_limits<lineNumber_t>::max();
            }
            QStringRef const leftChars(text.midRef(0, selectionLeftCol));
            QStringRef const selChars(text.midRef(selectionLeftCol, right - selectionLeftCol));
            QStringRef const rightChars(text.midRef(right));

            if (leftChars.size() != 0 || rightChars.size() != 0) {
                // There may be normal text to left of first selection line,
                // and to the right of the last selection line (first and last
                // may be the same line):
                // FIXME: this causes only partial painting of line background, up to length of text:
                auto bgColor = (caretPosition.lineNumber() == line)  ?
                        style->clBackgroundColor : style->backgroundColor;

                if (bgColor != defBGColor) {
                    pixmapPainter.setBackground(bgColor);
                    pixmapPainter.eraseRect(m_gutterOffset, pmYTop,
                            pmSize.width() - m_gutterOffset, m_textLineHeight);
                }
                pixmapPainter.setPen(style->textColor);
                if (leftChars.size() != 0)
                    pixmapPainter.drawText(m_gutterOffset, pmYBase, leftChars.toString());

                if (rightChars.size() != 0) {
                    int x = (leftChars.size() + selChars.size()) * m_characterWidth + m_gutterOffset;
                    pixmapPainter.drawText(x, pmYBase, rightChars.toString());
                }
            }

            if (selChars.size() != 0) {
                int x = leftChars.size() * m_characterWidth + m_gutterOffset;
                pixmapPainter.setBackground(m_qpalette.color(QPalette::Active,
                                                             QPalette::Highlight));
                pixmapPainter.setPen(m_qpalette.color(QPalette::Active,
                                                      QPalette::HighlightedText));

                pixmapPainter.eraseRect(x, pmYTop, selChars.size() * m_characterWidth,
                                        m_textLineHeight);
                pixmapPainter.drawText(x, pmYBase, selChars.toString());
            }
            selectionLeftCol = 0;
        } else {
            // Outside selection, this is easy:
            auto const bgColor = (caretPosition.lineNumber() == line)  ?
                    style->clBackgroundColor : style->backgroundColor;

            if (bgColor != defBGColor) {
                pixmapPainter.setBackground(bgColor);
                pixmapPainter.eraseRect(m_gutterOffset, pmYTop,
                                        pmSize.width() - m_gutterOffset, m_textLineHeight);
            }
            pixmapPainter.setPen(style->textColor);
            pixmapPainter.drawText(m_gutterOffset, pmYBase, text);
            if (bgColor != defBGColor)
                pixmapPainter.setBackground(defBGColor);
        }

        // If the gutter is showing, and this item has a pixmap, draw it now:
        if (gutterWidth > 0 && item->hasPixmap()) [[unlikely]] {
            if (auto it = itemPixMaps.constFind(item->pixmapId()); it != itemPixMaps.cend()) {
                int y = pmYTop;       // Start at top of line
                int dy = 0;
                QPixmap const& pm = *it;
                // Center the pixmap on the line, if pixmap is shorter than the line.
                // If longer, clip the bottom:
                if (pm.height() < m_textLineHeight)
                    y += (m_textLineHeight - pm.height()) / 2;
                else
                    dy = (pm.height() - m_textLineHeight) / 2;
                pixmapPainter.drawPixmap(0, y, pm, 0, dy, gutterWidth,
                                        m_textLineHeight);
            }
        }

        // If this is the caret line and the caret is blinkOn state, draw it here:
        if ((caretPosition.lineNumber() == line) && caretBlinkOn) [[unlikely]] {
            drawCaret(pixmapPainter, caretPosition.columnNumber() * m_characterWidth + m_gutterOffset,
                    pmYTop + textLineBaselineOffset,
                    pmYBase + textLineBaselineOffset);
        }
    }       // for items...

    // Handle the caret follows last line case:
    if ((caretPosition.lineNumber() == line) && caretBlinkOn) {
        drawCaret(pixmapPainter, m_gutterOffset, pmYTop + textLineBaselineOffset,
                pmYBase + textLineBaselineOffset);

    }

    // Close off pixmap for blt onto screen:
    pixmapPainter.end();

    // Draw the off-screen pixmap onto the screen:
    QPainter(q->viewport()).drawPixmap(0, 0, *drawPixMap);

#ifdef TIME_DRAW_REQUEST
    if (timeDraw) {
        timeDraw = false;
        qDebug() << "Draw complete dT " << drawTimer.elapsed();
    }
#endif
}


void wLogTextPrivate::drawCaret(QPainter& painter, int x, int top, int bot) const
{
    if (m_ShowCaret) {
        painter.save();
        QPen const pen(m_qpalette.color(QPalette::Active, QPalette::Base), 2,
                 Qt::SolidLine);
        painter.setPen(pen);
        painter.setCompositionMode(QPainter::CompositionMode_Xor);
        painter.drawLine(x, top, x, bot);
        painter.restore();
    }
}


void wLogTextPrivate::updateCellRange(const cell& top, const cell& bottom)
{
#if 0
    const cell *t=&top, *b=&bottom;
    if (top > bottom) {
        t = &bottom;
        b = &top;
    }
    int x, y, w, h;
    if (t->line() == b->line()) {
        // The update is on a single line, so update only the rectangle bounded
        // by the first and last characters, and line height.
        x = (m_gutterOffset + t->col() * m_characterWidth) - q->horizontalScrollBar()->value();
        y = (t->line() * m_textLineHeight) - q->verticalScrollBar()->value();
        w = (b->col() - t->col()) * m_characterWidth;
        h = m_textLineHeight;
        qDebug() << m_textLineHeight << q->verticalScrollBar()->value();
    } else {
        // The line spans multiple lines, so update a rectangle which encloses
        // all of all the lines from top.line() to bottom.line();
        x = m_gutterOffset;
        y = (t->line() * m_textLineHeight) - q->verticalScrollBar()->value();
        w = q->m_maxLineChars * m_characterWidth;
        h = (b->line() - t->line() + 1)  * m_textLineHeight;
    }
    qDebug() << __FUNCTION__ << top << bottom << x << y << w << h;
    q->viewport()->update(x, y, w, h);
#else
    Q_UNUSED(top); Q_UNUSED(bottom);
    q->viewport()->update();
#endif
}


void wLogText::setUpdatesEnabled(bool newState)
{
    bool const oldState = updatesEnabled();
    QAbstractScrollArea::setUpdatesEnabled(newState);
    viewport()->setUpdatesEnabled(newState);
    if (!oldState && newState) {
        // if going from disabled to enabled, refresh view:
        if (updatesNeeded) {
            QApplication::postEvent(this, new QEvent(updatesNeededEvent));
        }
    }
}


inline void wLogTextPrivate::setContentSize()
{
    int const i = q->viewport()->size().height() / m_textLineHeight;
    int const j =  (q->m_lineCount > i) ? ((q->m_lineCount - i) * m_textLineHeight) : 0;
    q->verticalScrollBar()->setMaximum(j);
    q->adjustHorizontalScrollBar();
}


void wLogTextPrivate::adjustTextMetrics()
{
    const int oldLineHeight = m_textLineHeight;
    const QFontMetrics &fm =  q->QWidget::fontMetrics();
    m_textLineHeight = fm.lineSpacing();
    if (m_textLineHeight != oldLineHeight) {
        Q_EMIT q->lineSpacingChange(oldLineHeight, m_textLineHeight);
    }

    // As of Qt4 (4.4.3), a 'fontMetrics().width' is not valid unless attached
    // to a painter.  So after a font change, set the width to unknown, and
    // find it in the paint event.
    m_characterWidth = 0;
    textLineBaselineOffset = fm.descent();

    visibleLines = q->viewport()->size().height() / m_textLineHeight;
    q->verticalScrollBar()->setSingleStep(m_textLineHeight);
    q->verticalScrollBar()->setPageStep(visibleLines * m_textLineHeight);
    q->adjustHorizontalScrollBar();
    setContentSize();
}


inline int wLogTextPrivate::firstVisibleLine() const
{
    return q->verticalScrollBar()->value() / m_textLineHeight;
}


inline int wLogTextPrivate::linesPerPage() const
{
    return q->viewport()->size().height() / m_textLineHeight;
}


inline int wLogTextPrivate::lastVisibleLine() const
{
    return (q->verticalScrollBar()->value() + q->viewport()->size().height())
              / m_textLineHeight;
}


void wLogText::adjustHorizontalScrollBar()
{
    if (d->m_characterWidth) {
        int const visibleChars = (viewport()->size().width() - d->m_gutterOffset) / d->m_characterWidth;
        int const j = (m_maxLineChars > visibleChars ) ? (m_maxLineChars - visibleChars) : 0;
        horizontalScrollBar()->setMaximum(j);
        horizontalScrollBar()->setPageStep( visibleChars );
        horizontalScrollBar()->setSingleStep(1);
    }
}


void wLogText::setFont(QFont font)
{
    if (!QFontInfo(font).fixedPitch()) {
        qWarning() << "Ignoring non fixed pitch font" <<  font.family();
        return;
    }
    font.setKerning(false);
    font.setFixedPitch(true);
    QWidget::setFont(font);

    d->fontBaseSize = font.pointSize();
    d->adjustTextMetrics();
    activatePalette();      // Reactivate the current palette with altered font.
    ensureCaretVisible();
    setUpdatesNeeded(updateFull);
}


QFont const& wLogText::font() const
{
    return QWidget::font();
}


void wLogText::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    d->visibleLines = viewport()->size().height() / d->m_textLineHeight;
    verticalScrollBar()->setPageStep(d->visibleLines * d->m_textLineHeight);
}


// Defining our word break characters: not (alpha, numa, '_', '-'):
static const QRegularExpression wordBreakRe(QLatin1String("[^\\w-]"));

void wLogText::mouseDoubleClickEvent(QMouseEvent *event)
{
    int const line = d->yToLine(event->y());

    if (validLineNumber(line)) {
        if (d->inTheGutter(event->x())) {
            Q_EMIT gutterDoubleClicked(line);
        } else {
            const QString& str = items[line]->m_text;
            const int textLen = str.length();
            const int col=std::min(d->xToCharColumn(event->x()), textLen-1);
            d->updateCaretPos(line, col);
            if (textLen) {
                int selLeft=0, selRight=textLen-1;
                bool haveWord = false;
                // Rules on double click selection:
                //   Word characters are letters, numbers, '_', '-'.
                //
                //   If click char is a word character, look left for
                //   start of word, then right for end of word.  Line boundaries
                //   are also word boundaries.
                //
                //   If click char a non word character, look left one character,
                //   and if it is a word character, then it is the right end of
                //   the word.  Serch left from there for the left end of the
                //   word.

                // Note in the lines below, we can use "selLeft = foundAt + 1"
                // because find and lastIndexOf return -1 for not found, and in
                // these cases, -1+1 = 0, which is the first column.

                if (str[col].isLetterOrNumber()) {
                    // We're starting on a word character
                    // Search left for left word boundary
                    selLeft = str.lastIndexOf(wordBreakRe, col) + 1;
                    int foundAt = str.indexOf(wordBreakRe, col);
                    selRight = (foundAt > -1) ? foundAt : textLen;
                    haveWord = true;
                } else if ((col > 1) && str[col-1].isLetterOrNumber()) {
                    // So we are starting on a white space.  Search left for
                    // the left-most non-whitespace before click
                    selRight = col;
                    selLeft = str.lastIndexOf(wordBreakRe, selRight-1) + 1;
                    haveWord = true;
                }
                if (haveWord) {
                    cell r(line, selRight);
                    d->updateCaretPos(r);
                    d->setSelection(cell(line, selLeft), r);
                }
            }
            Q_EMIT doubleClicked(cell(line, col));
        }
    }
}


void wLogTextPrivate::setSelection(const cell& sel)
{
    cell const oldSelTop(selectTop), oldSelBot(selectBottom);
    if (sel == selectionOrigin) {
        selecting= false;
    } else {
        if (sel < selectionOrigin) {
            selectTop = sel;
            selectBottom =selectionOrigin;
        } else {
            selectTop = selectionOrigin;
            selectBottom = sel;
        }
        selecting= true;
    }
    // Cause a repaint covering the old area and new area, so the old gets
    // cleared if needed, and the new gets drawn.
    if ((oldSelTop != selectTop) || (oldSelBot != selectBottom)) {
        updateCellRange(std::min(oldSelTop, selectTop), std::max(oldSelBot, selectBottom));
    }
}


void wLogTextPrivate::setSelection(const cell& a, const cell& b)
{
    cell const oldSelTop(selectTop), oldSelBot(selectBottom);
    selectionOrigin = a;
    if (a == b) {
        selecting= false;
    } else {
        if (a < b) {
            selectTop = a;
            selectBottom = b;
        } else {
            selectTop = b;
            selectBottom = a;
        }
        selecting= true;
    }

    // TODO Optimize here; if they are separate areas, update them separately,
    // otherwise they overlap, so just update the overlapping region.

    // Cause a repaint covering the old area and new area, so the old gets
    // cleared if needed, and the new gets drawn.
    if ((oldSelTop != selectTop) || (oldSelBot != selectBottom)) {
        updateCellRange(std::min(oldSelTop, selectTop), std::max(oldSelBot, selectBottom));
    }
}


void wLogText::mouseMoveEvent(QMouseEvent *event)
{
    mouseRawPos = event->pos();
    if (d->dragState != dragStates::dragNone) {
        // If this is a move following a mouse down in a selection, then start a
        // a drag operation.
        if (d->dragState == dragStates::dragMaybe) {
            /* FIXME: This is mixing mouse QPoint coordinates (pixel) with
             * cell (line, column) coordinates. The mouse point needs to be
             * mapped to a cell.
             */
            if ((event->pos() - QPoint(d->dragStart)).manhattanLength()
                < QApplication::startDragDistance())
                return;
            d->dragState = dragStates::dragDragging;

            // Do not delete the mimeData or drag objects; that is handled by Qt.
            QMimeData *mimeData = new QMimeData;
            mimeData->setText(selectedText());
            QDrag *drag = new QDrag(this);
            drag->setMimeData(mimeData);
            drag->exec(Qt::CopyAction);
        }
        event->accept();
        return;
    }
    if (mouseHovering) {
        mouseHovering = false;
        Q_EMIT hover(-1,-1);
    }

    if (event->buttons() == 0) {
        // Handle a clickless move: if the move is into/out of the gutter,
        // change the mouse cursor as appropriate:
        if (d->inTheGutter(event->x())) {
            // If the mouse is in the gutter, but cursor is currently an edit
            // "I-beam", change to an arrow:
            if (d->m_mouseIsEditCursor) {
                d->m_mouseIsEditCursor = false;
                setCursor(Qt::ArrowCursor);
            }
        } else if (!d->m_mouseIsEditCursor) {
            // The mouse is not in the gutter, thus it must be in the edit
            // area.  If the mouse is not already the "I-beam", change it so:
            d->m_mouseIsEditCursor = true;
            setCursor(Qt::IBeamCursor);
        }
        if (m_hoverTime) {
            if (!hoverTimerId) {
                hoverTimerId = startTimer(m_hoverTime);
            }
            mouseHoverOnTime.restart();
        }
    } else if ((event->buttons() & Qt::LeftButton) != 0) {
        // Handle move out of the widget.  If the mouse is above the widget,
        // scroll down.  If below, scroll up.
        // TODO Handle outside left/right scroll.
        if (d->selectScrollTimerId != 0)
            killTimer(d->selectScrollTimerId);
        int delta = 0;
        if (event->y() < 0)
            delta = -verticalScrollBar()->singleStep();
        else if (event->y() > viewport()->size().height())
            delta = verticalScrollBar()->singleStep();
        verticalScrollBar()->setValue(verticalScrollBar()->value() + delta);
        d->timedVScrollStep = delta;
        if (delta)
            d->selectScrollTimerId = startTimer(200);

        const int line = std::min(d->yToLine(event->y()), m_lineCount);
        int col=0;
        if (validLineNumber(line))
            col = std::min(d->xToCharColumn(event->x()), items[line]->m_text.length());
        cell at(line, col);
        d->updateCaretPos(at);
        d->setSelection(at);
        event->accept();
    } else {
        QAbstractScrollArea::mouseMoveEvent(event);
    }
}


void wLogText::setHoverTime(int t)
{
    t = qBound(0, t, 2000);
    m_hoverTime = ((t + 49) / 50) * 50;
    if (hoverTimerId) {
        /* Kill the current timer. Next mouse move will restart it. */
        killTimer(hoverTimerId);
        hoverTimerId = 0;
    }
}


void wLogText::mousePressEvent(QMouseEvent *event)
{
    if (mouseHovering) {
        mouseHovering = false;
        Q_EMIT hover(-1, -1);
    }
    cell at = pointToCell(event->pos());
    if (d->inTheGutter(event->x())) {
        int line = at.lineNumber();
        if (validLineNumber(line)) {
            Q_EMIT gutterClick(line, event);
        }
    } else {
        if (m_lineCount > 0) {
            d->updateCaretPos(at);
            if (validLineNumber(at.lineNumber())) {
                if (at.columnNumber() > items[at.lineNumber()]->m_text.length())
                    at.setColumnNumber(items[at.lineNumber()]->m_text.length());
            } else
                at.setColumnNumber(0);

            if (event->button() == Qt::LeftButton) {
                if ((event->modifiers() & Qt::ShiftModifier) == 0) {
                    if (d->selecting &&
                         (at >= d->selectTop) && (at <= d->selectBottom)) {
                        d->dragState = dragStates::dragMaybe;
                        d->dragStart = at;
                    } else {
                        if (d->selecting) {
                            d->selecting= false;
                            d->updateCellRange(d->selectTop, d->selectBottom);
                            Q_EMIT copyAvailable(false);
                        }
                        d->selectionOrigin = d->selectTop = d->selectBottom = at;
                    }
                } else
                    d->setSelection(at);
            }
            d->updateCaretPos(at);
            d->updateCellRange({at.lineNumber(), 0}, {at.lineNumber(), at.columnNumber()});

            Q_EMIT clicked(event);
        }
    }
    QAbstractScrollArea::mousePressEvent(event);
}


void wLogText::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (d->selectScrollTimerId) {
            killTimer(d->selectScrollTimerId);
            d->selectScrollTimerId = 0;
        }

        if (d->dragState != dragStates::dragNone) {
            // A mouse down in a selection set drageMaybe.  If this was a click,
            // clear the selection.
            if (d->dragState == dragStates::dragMaybe) {
                clearSelection();
            }
            // In any case, the drag is done.
            d->dragState = dragStates::dragNone;
        }

        // If we were in the process of d->selecting, finalize it here:
        if (d->selecting) {
            d->copySelToClipboard(QClipboard::Selection);
            Q_EMIT copyAvailable(true);
        }
    }
    QAbstractScrollArea::mouseReleaseEvent(event);
}


void wLogText::contextMenuEvent(QContextMenuEvent *event)
{
    cell const at = pointToCell(event->pos());
    if (validLineNumber(at.lineNumber())) {
        QPoint gPos = viewport()->mapToGlobal(event->pos());
        if (d->inTheGutter(event->x())) {
            Q_EMIT gutterContextClick(at.lineNumber(), gPos, event);
        } else {
            if (!d->selecting)
                d->caretPosition = at;
            Q_EMIT contextClick(at.lineNumber(), gPos, event);
        }
        event->accept();
    } else {
        QAbstractScrollArea::contextMenuEvent(event);
    }
}


void wLogText::wheelEvent(QWheelEvent *event)
{
    int dir=(event->angleDelta().y() < 0) ? 1 : -1;
    QString state;
    QScrollBar *vsb = verticalScrollBar();
    QScrollBar *hsb = horizontalScrollBar();
    int vDelta = 0;
    int hDelta = 0;
    const Qt::KeyboardModifiers modifiers = event->modifiers();

    if (mouseHovering) {
        mouseHovering = false;
        Q_EMIT hover(-1, -1);
    }

    if (modifiers == 0) {
        // Normal scroll:
        vDelta = dir * qApp->wheelScrollLines() * d->m_textLineHeight;
    } else if (modifiers == Qt::ShiftModifier) {
        // Scroll page:
        vDelta = dir * vsb->pageStep();
#ifdef TIME_DRAW_REQUEST
        d->timeDraw = true;
        d->drawTimer.start();
#endif
    } else if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
        // Scroll line:
        vDelta = dir * vsb->singleStep();
    } else if (modifiers == Qt::AltModifier) {
        // Scroll horizontal 1/4 width:
        hDelta = dir * hsb->pageStep();
    } else if (modifiers == (Qt::ControlModifier | Qt::AltModifier)) {
        // Scroll horizontal 1 character:
        if (d->m_characterWidth)
            hDelta = dir * d->m_characterWidth;
    } else if (modifiers == Qt::ControlModifier) {
        // Zoom font:
        if (dir < 0) {
            enlargeFont();
        } else {
            shrinkFont();
        }
    }
    if (vDelta) {
        vsb->setValue(qBound(0, vsb->value() + vDelta, vsb->maximum()));
    } else if (hDelta) {
        hsb->setValue(qBound(0, hsb->value() + hDelta, hsb->maximum()));
    }
    event->accept();
}


void wLogText::keyPressEvent (QKeyEvent *event)
{
    QScrollBar *vsb=verticalScrollBar();
    int const currentVSb = vsb->value();
    QScrollBar *hsb=horizontalScrollBar();
    bool accept = false;

    if (mouseHovering) {
        mouseHovering = false;
        Q_EMIT hover(-1, -1);
    }

    if (event->modifiers() == Qt::NoModifier) {
        accept = true;
        switch (event->key()) {
        case Qt::Key_Home:
            vsb->setValue(vsb->minimum());
            hsb->setValue(hsb->minimum());
            break;

        case Qt::Key_End:
            vsb->setValue(vsb->maximum());
            hsb->setValue(hsb->minimum());
            break;

        case Qt::Key_Up:
            vsb->setValue(currentVSb - vsb->singleStep());
            break;

        case Qt::Key_Down:
            vsb->setValue(currentVSb + vsb->singleStep());
            break;

        case Qt::Key_PageUp:
            vsb->setValue(currentVSb - vsb->pageStep());
            break;

        case Qt::Key_PageDown:
            vsb->setValue(currentVSb + vsb->pageStep());
            break;

        case Qt::Key_ScrollLock:
            setScrollLock(!d->isHardLocked());
            break;

        case Qt::Key_Escape:
            clearSelection();
            d->setSoftLock(false);
            d->setHardLock(false);
            d->m_maxVScroll = 0;
            if (d->escJump) {
                vsb->setValue(vsb->maximum());
                hsb->setValue(hsb->minimum());
            }
            break;

        default:
            accept = false;
        }
    }

    if (accept)
        event->accept();
    Q_EMIT keyPress(event);
    if (!accept)
        QAbstractScrollArea::keyPressEvent(event);
}


void wLogTextPrivate::copySelToClipboard(QClipboard::Mode cbMode) const
{
    if (selecting)
        QApplication::clipboard()->setText(q->selectedText(), cbMode);
}


inline void wLogTextPrivate::updateCaretPos(lineNumber_t line, int col)
{
    updateCaretPos(cell(line, col));
}


inline void wLogTextPrivate::updateCaretPos(const cell& pos)
{
    if (m_ShowCaret) {
        cell o(caretPosition);
        caretPosition = pos;

        updateCellRange(o, o.nextCol());
        updateCellRange(caretPosition, caretPosition.nextCol());
    } else {
        caretPosition = pos;
    }
}


cell wLogText::caretPosition() const
{
    return d->caretPosition;
}


void wLogText::setCaretPosition(lineNumber_t line, int col)
{
    // If line (p.y()) is > number of lines, use number of lines.
    int l = std::min(line, m_lineCount);

    // if new pos is < 0, count back from the end:
    if (l < 0) {
        // Don't go any further back than the first line (0):
        l = std::max(0, m_lineCount + l);
    }

    // If l is a valid line number (i.e. not lastLine + 1), get its text length.
    // Otherwise use zero.
    const int textlen = validLineNumber(l) ? items[l]->m_text.length() : 0;

    // If new column (p.x()) is beyond the end of the line, use line length.
    int c = std::min(col, textlen);

    // If the new position is negative, count back from the end.
    if (c < 0) {
        // But don't go back before the first column.
        c = std::max(0, textlen + c);
    }

    // Update the cursor.
    d->updateCaretPos(cell(l, c));
    ensureCaretVisible();
}


void wLogText::setCaretPosition(const cell &p)
{
    setCaretPosition(p.lineNumber(), p.columnNumber());
}


void wLogText::selectAll()
{
    if (m_lineCount) {
        d->selecting= true;
        d->selectTop = cell(0, 0);
        d->selectBottom = cell(m_lineCount, 0);
        // Draw selection highlight:
        viewport()->update();
        d->copySelToClipboard(QClipboard::Selection);
        Q_EMIT copyAvailable(true);
    }
}


void wLogText::clearSelection()
{
    d->selecting= false;
    if (d->selecting) {
        QApplication::clipboard()->clear(QClipboard::Selection);
        // Clear selection highlight:
        viewport()->update();
        Q_EMIT copyAvailable(false);
    }
}



bool wLogText::hasSelectedText() const
{
    return d->selecting;
}

region wLogText::getSelection() const
{
    region t;
    if (d->selecting)
        t = region(d->selectTop, d->selectBottom);
    return t;
}

void wLogText::getSelection(lineNumber_t *lineFrom, int *colFrom,
                            lineNumber_t *lineTo, int *colTo) const
{
    cell sTop, sBot;
    if (d->selecting) {
        sTop = d->selectTop;
        sBot = d->selectBottom;
    } else
        sTop = sBot = cell(-1, -1);

    if (lineFrom) {
        *lineFrom = sTop.lineNumber();
    }
    if (colFrom) {
        *colFrom =  sTop.columnNumber();
    }
    if (lineTo) {
        *lineTo = sBot.lineNumber();
    }
    if (colTo) {
        *colTo = sBot.columnNumber();
    }
}


QString wLogText::selectedText() const
{
    if (!d->selecting) {
        return QString();
    }

    QString selText;
    int line = d->selectTop.lineNumber();
    int last = std::min(d->selectBottom.lineNumber(), m_lineCount);
    if (last < line)
        qSwap(last, line);
    if (line == last) {
        selText = items[line]->m_text.mid(d->selectTop.columnNumber(), d->selectBottom.columnNumber() - d->selectTop.columnNumber());
        ++line;
    } else {
        logItemsImplCIt it, end;
        int nItems = items.size();
        if (line < nItems) {
            it = items.cbegin() + line;
            end = (last < nItems) ? (items.cbegin() + last) : items.cend();
        } else {
            end = items.cend();
            it = end;
        }
        if ((it != items.cend()) && d->selectTop.columnNumber() > 0) {
            selText = (*it)->m_text.mid(d->selectTop.columnNumber()) + QLatin1Char('\n');
            ++it;
        }

        for (; it != end; ++it) {
            selText += (*it)->m_text + QLatin1Char('\n');
        }
        if ((it != items.cend()) && last < m_lineCount) {
            selText += (*it)->m_text.left(d->selectBottom.columnNumber());
        }
    }
    return selText;
}

QString wLogText::toPlainText(QLatin1Char sep) const
{
    QString ret;
    for (auto const& item : items)
        ret += item->m_text + sep;
    return ret;
}

void wLogText::copy() const
{
    if (d->selecting) {
        d->copySelToClipboard(QClipboard::Clipboard);
    }
}


/******************************************************************************/
/******************************************************************************/
/*                                                                            */
/*   Public interface functions                                               */
/*                                                                            */
/******************************************************************************/
logTextPalette *wLogText::createPalette(size_t size, const QString& name)
{
    return d->createPalette(size, name);
}


logTextPalette *wLogTextPrivate::createPalette(int size, const QString& name)
{
    if (name == QString(defaultPaletteNameString)) {
        qWarning() << "Can not delete default palette '" <<  defaultPaletteNameString << "''";
        return nullptr;
    }
    q->deletePalette(name);
    logTextPalette *np = new logTextPalette(name, size,
		    m_qpalette.color(QPalette::Active, QPalette::WindowText),
                    m_qpalette.color(QPalette::Active, QPalette::Base),
                    m_qpalette.color(QPalette::Active, QPalette::AlternateBase));
    palettes[name] = np;            // Add it to the list of palettes.
    return np;
}


logTextPalette *wLogText::createPalette(const QString& name,
                                        const logTextPalette *source)
{
    if (name == QString(defaultPaletteNameString)) {
        qWarning() << "Can not delete default palette '" <<  defaultPaletteNameString << "''";
        return nullptr;
    }
    if (name.isEmpty()) {
        qWarning() << "Cannot create unnamed palette.";
        return nullptr;
    }

    if (auto it = d->palettes.find(name); it != d->palettes.end()) {
        if (*it == source) {
            qWarning() << "Attempt to clone palette over itself: '" << name << "'";
            return *it;
        }
        deletePalette(name);
    }
    logTextPalette *np = new logTextPalette(name, *source);
    d->palettes[name] = np;            // Add it to the list of palettes.
    return np;
}


logTextPalette *wLogText::palette(const QString& name) const
{
    return d->palettes.value(name, nullptr);
}


void wLogText::deletePalette(const QString& name)
{
    if (name == defaultPaletteNameString) {
        qWarning() << "Can not delete default palette '" << defaultPaletteNameString << "'";
        return;
    }
    if (name == d->activatedPaletteName) {
        activatePalette(defaultPaletteNameString);
    }

    if (paletteMap::iterator pit = d->palettes.find(name); pit != d->palettes.end()) {
        delete *pit;
        d->palettes.erase(pit);
    }
}

QString wLogText::activePaletteName() const
{
    return d->activatedPaletteName;
}


QString wLogText::defaultPaletteName() const
{
    return defaultPaletteNameString;
}


QStringList wLogText::paletteNames() const
{
    return d->palettes.keys();
}


bool wLogText::activatePalette(const QString& name)
{
    return d->activatePalette(name);
}


bool wLogTextPrivate::activatePalette(const QString& name)
{
    // Null string reactivates (realizes changes to) the current palette.
    if (!name.isEmpty()) {
        if (!palettes.contains(name)) {
            return false;
        }
        activatedPaletteName = name;
    } else {
        if (!activePalette) {
            // No current palette to reactivate.
            return false;
        }
    }

    activePalette = std::make_unique<activatedPalette>(q->font(), *palettes[activatedPaletteName]);
    if (activePalette) {
        q->viewport()->update();
    } else {
        activatedPaletteName.clear();
    }
    return (bool)activePalette;
}



/******************************************************************************/
/******************************************************************************/
/*                                                                            */
/*   Text manipulation functions:                                             */
/*                                                                            */
/******************************************************************************/

int wLogText::lineHeight() const
{
    return d->m_textLineHeight;
}


int wLogText::fontZoom() const
{
    return d->magnify;
}


void wLogText::setFontZoom(int zoom)
{
    if (d->magnify != zoom) {
        d->magnify = zoom;

        QFont f = QWidget::font();
        f.setPointSize(d->fontBaseSize + d->magnify);

        // Don't call our setFont, because that sets 'fontBaseSize' which needs
        // to stay constant for across zoom changes.
        QWidget::setFont(f);

        d->adjustTextMetrics();
        activatePalette();      // Reactivate the current palette with altered font.
        ensureCaretVisible();
        setUpdatesNeeded(updateFull);
    }
}


void wLogText::resetFontZoom()
{
    setFontZoom(0);
}


void wLogText::enlargeFont()
{
    setFontZoom(d->magnify + 1);
}


void wLogText::shrinkFont()
{
    // Don't shrink below resultant 2 points in size
    if ((d->fontBaseSize + d->magnify) > 2)
        setFontZoom(d->magnify - 1);
}


int wLogText::length(lineNumber_t lineNumber) const
{
    return validLineNumber(lineNumber) ? items[lineNumber]->m_text.length() : 0;
}


int wLogText::maxLogLines() const
{
    return maximumLogLines;
}


void wLogText::setMaxLogLines(lineNumber_t mll)
{
    // Treat <= 0 as unlimited:
    maximumLogLines = mll;

    if (mll <= 0) {
        maximumLogLinesSlacked = std::numeric_limits<lineNumber_t>::max();
    } else {
        // Calculate a slack value:
        maximumLogLinesSlacked = mll + qBound(10, mll/10, 1000);

        // Check for overflow maxint:
        if (maximumLogLinesSlacked <= mll)
            maximumLogLinesSlacked = std::numeric_limits<lineNumber_t>::max();
        // Now would be a good time to trim:
        trimLines();
    }
}


void wLogText::trimLines()
{
    if (maximumLogLines > 0 && static_cast<int>(items.size()) > maximumLogLines) {
        int const toRemove = items.size() - maximumLogLines;
        auto const begin = items.begin();
        auto const end = begin + toRemove;
        std::for_each(begin, end, [](auto item) {delete item;});
        items.erase(begin, end);
        m_lineCount = items.size();
        m_maxLineChars = (m_lineCount == 0) ? 0 :
            (*std::ranges::max_element(items, [](auto x, auto y) {
                return x->text().length() < y->text().length();}))->text().length();

        // Adjust selection:
        if (d->selecting) {
            if (d->selectTop.lineNumber() < toRemove)
                d->selectTop = cell(0,0);
            else
                d->selectTop.setLineNumber(d->selectTop.lineNumber() - toRemove);

            if (d->selectBottom.lineNumber() < toRemove)
                d->selectBottom = cell(0,0);
            else
                d->selectBottom.setLineNumber(d->selectBottom.lineNumber() - toRemove);

            if (d->selectTop == d->selectBottom) {
                clearSelection();
            }
        }

        Q_EMIT trimmed(toRemove);
        d->m_maxVScroll = 0;
        d->setContentSize();
    }
}


inline bool wLogTextPrivate::isScrollable() const
{
    return !isHardLocked() && !isSoftLocked() && (!selecting);
}


void wLogText::vScrollChange(int value)
{
    int const currentMaxScroll = verticalScrollBar()->maximum();
    if (currentMaxScroll < d->m_maxVScroll) {
        // Handle shrinking line count, ie, a trim() operation:
        d->m_maxVScroll = currentMaxScroll;
    } else {
        // If the new vertical position is less than the std::max position seen, it
        // is a scroll up.  Lock the postion to prevent new lines from scrolling
        // the view.  Else, if we have scrolled back to the bottom, unlock the
        // scroll.
        if (value < d->m_maxVScroll) {
            d->setSoftLock(true);
        } else {
            d->m_maxVScroll = value;
            d->setSoftLock(false);
        }
    }
    viewport()->update();
}


void wLogText::hScrollChange(int value)
{
    Q_UNUSED(value);
    viewport()->update();
}


inline QPoint wLogTextPrivate::cellToPoint(const cell& c) const
{
    int const y = c.lineNumber() * m_textLineHeight;
    int const x = c.columnNumber() * m_characterWidth + m_gutterOffset;
    return QPoint(x, y);
}


inline bool wLogTextPrivate::inTheGutter(int x) const
{
    return (x < m_gutterOffset);
}


inline int wLogTextPrivate::xToCharColumn(int x) const
{
    int col=0;
    if (m_characterWidth != 0)
        col = inTheGutter(x) ? 0 :
            ((x - m_gutterOffset) / m_characterWidth + q->horizontalScrollBar()->value());
    return col;
}


inline int wLogTextPrivate::yToLine(int y) const
{
    return (q->verticalScrollBar()->value() + std::max(y, 0)) / m_textLineHeight;
}


void wLogText::ensureCaretVisible()
{
    int const line = d->caretPosition.lineNumber();
    if ((line < d->firstVisibleLine()) || (line > d->lastVisibleLine())) {
        // The ' - (d->linesPerPage()/2)' seems counter intuitive at first
        // (it is intended to scroll the cursor to the center of the page),
        // however the scroll bar maximum is based on total lines - lines per
        // page.  So while intuitively you want '+ linesPerPage/2', when
        // you subtract linesPerPage from the total, you get '- linesPerPage/2'.
        verticalScrollBar()->setValue(
                (line - (d->linesPerPage()/2)) * d->m_textLineHeight);
    }
}


bool wLogText::showCaret() const
{
    return d->m_ShowCaret;
}


void wLogText::setShowCaret(bool show)
{
    Q_UNUSED(show);
#if 0 // FIXME: Caret on causes full screen updates!
    d->m_ShowCaret = show;
    if (!show && (d->caretBlinkTimer != -1)) {
        killTimer(d->caretBlinkTimer);
        d->caretBlinkTimer = -1;
    }
#endif
}


void wLogText::clear()
{
    d->selecting= false;
    qDeleteAll(items);
    items.clear();
    m_lineCount = 0;
    m_maxLineChars = 0;
    d->m_maxVScroll = 0;
    d->setSoftLock(false);
    d->setHardLock(false);
    d->caretPosition = cell(0, 0);
    d->setContentSize();
}


void wLogText::clear(int top, int count)
{
    d->selecting = false;

    if (top < 0 || count < 1 || top >= m_lineCount)
        return;
    if ((top + count) > m_lineCount)
        count = m_lineCount - top;

    auto const begin = items.begin() + top;
    auto const end = begin + count;
    std::for_each(begin, end, [](auto item) { delete item; });
    items.erase(begin, end);
    if (finalized)
        items.shrink_to_fit();

    d->m_maxVScroll = 0;
    m_lineCount = items.size();
    m_maxLineChars = (m_lineCount == 0) ? 0 :
        (*std::ranges::max_element(items, [](auto x, auto y) {
            return x->text().size() < y->text().size();}))->text().size();

    d->caretPosition = cell(0, 0);
    d->setContentSize();
}


bool wLogText::find(const QString& str, cell *at,
                    Qt::CaseSensitivity caseSensitive, bool forward)
{
    //Anything to search?
    if (m_lineCount == 0)
        return false;

    cell pos;
    if (!d->prepareFind(forward, pos, at))
        return false;

    bool match = false;
    lineNumber_t lineNumber = pos.lineNumber();
    int col = pos.columnNumber();
    auto it = items.cbegin() + lineNumber;
    if (forward) {
        for (auto end=items.cend(); it != end; ++it, ++lineNumber) {
            col = (*it)->m_text.indexOf(str, col, caseSensitive);
            match = (col != -1);
            if (match) {
                int span = col + str.length();
                d->updateCaretPos(cell(lineNumber, span));
                d->setSelection(cell(lineNumber, col), d->caretPosition);
                break;
            }
            col = 0;
        }
    } else {
        for (auto end = items.cbegin(); it != end; --it, --lineNumber) {
            col = (*it)->m_text.lastIndexOf(str, col, caseSensitive);
            match = (col != -1);
            if (match) {
                int span = col + str.length();
                d->updateCaretPos(cell(lineNumber, col));
                d->setSelection(d->caretPosition, cell(lineNumber, span));
                break;
            }
            // col is already == -1 from find fail
        }
    }
    if (match) {
        pos = cell(lineNumber, col);
        if (at) *at = pos;
        ensureCaretVisible();
    }
    return match;
}


bool wLogText::find(const QString& str, Qt::CaseSensitivity caseSensitive,
                    bool forward, lineNumber_t *line, int *col)
{
    cell pos = d->caretPosition;
    if (line)
        pos.setLineNumber(*line);
    if (col)
        pos.setColumnNumber(*col);
    bool result = find(str, &pos, caseSensitive, forward);
    if (result) {
        if (line)
            *line = pos.lineNumber();
        if (col)
            *col = pos.columnNumber();
    }
    return result;
}


bool wLogText::find(const QRegExp& re, cell *at, bool forward)
{
    // Anything to search?
    if (m_lineCount == 0)
        return false;

    // Is the RegExp valid?
    if (!re.isValid())
        return false;

    cell pos;
    if (!d->prepareFind(forward, pos, at))
        return false;

    bool match=false;
    int line = pos.lineNumber(), col = pos.columnNumber();
    if (forward) {
        for (auto const &item : items) {
            col = item->m_text.indexOf(re, col);
            match = (col != -1);
            if (match) {
                int span = col + re.matchedLength();
                d->updateCaretPos(cell(line, span));
                d->setSelection(cell(line, col), d->caretPosition);
                break;
            }
            col = 0;
            ++line;
        }
    } else {
        for (auto it=items.cbegin() + line; line >= 0; --it, --line) {
            col = (*it)->m_text.lastIndexOf(re, col);
            match = (col != -1);
            if (match) {
                int span = col + re.matchedLength();
                d->updateCaretPos(cell(line, col));
                d->setSelection(d->caretPosition, cell(line, span));
                break;
            }
            // col is already == -1 from find fail
        }
    }
    if (match) {
        pos=cell(line, col);
        if (at) *at = pos;
        ensureCaretVisible();
    }
    return match;
}


bool wLogTextPrivate::prepareFind(bool forward, cell& pos, const cell *at) const
{
    pos = at ? cell(*at) : caretPosition;
    if (!q->validLineNumber(pos.lineNumber())) {
        if (forward)
            return false;
        pos.setLineNumber(q->m_lineCount - 1);
        pos.setColumnNumber(-1);
    }
    int lineLen = q->items[pos.lineNumber()]->text().length();
    if (pos.columnNumber() >= lineLen)
        pos.setColumnNumber(lineLen - 1);
    return true;
}


void wLogText::setPixmap(int pixmapId, QPixmap const& pixmap)
{
    if (pixmapId >= 0)
        d->itemPixMaps[pixmapId] = pixmap;
}

void wLogText::setPixmap(int pixmapId, QPixmap&& pixmap)
{
    if (pixmapId >= 0)
        d->itemPixMaps[pixmapId] = std::move(pixmap);
}


void wLogText::clearPixmap(int pixmapId)
{
    d->itemPixMaps.remove(pixmapId);
}

void wLogText::clearLinePixmap(lineNumber_t lineNo)
{
    if (validLineNumber(lineNo)) [[likely]] {
        items[lineNo]->clearPixmap();
        if (updatesEnabled() && d->gutterWidth > 0) {
            setUpdatesNeeded(updateFull);
            update(0, lineNo * d->m_textLineHeight, d->gutterWidth,
                    (lineNo+1) * d->m_textLineHeight);
            update(); // FIXME bug in single line update, invalid entire view area
        }
    }
}


void wLogText::setLinePixmap(lineNumber_t lineNo, int pixmapId)
{
    if (validLineNumber(lineNo)) [[likely]] {
        items[lineNo]->m_pixmapId = pixmapId;
        if (updatesEnabled() && d->gutterWidth > 0) {
            setUpdatesNeeded(updateFull);
            update(0, lineNo * d->m_textLineHeight, d->gutterWidth,
                            (lineNo+1) * d->m_textLineHeight);
            update(); // FIXME bug in single line update, invalid entire view area
        }
    }
}


int wLogText::gutter() const
{
    return d->gutterWidth;
}


void wLogText::setGutter(int width)
{
    if (width < 0) {
        width = 0;
    }
    d->gutterWidth = width;
    if (width != 0) {
        d->m_gutterOffset = width + gutterBorder + textBorder;
    } else {
        d->m_gutterOffset = textBorder;
    }
    if (updatesEnabled()) {
        viewport()->update();
    }
}


void wLogTextPrivate::setSoftLock(bool state)
{
    if (softLocked != state) {
        softLocked = state;
        bool now = isScrollable();
        if (now) {
            q->viewport()->update();
        }
        Q_EMIT q->scrollLockChange(!now);
    }
}


void wLogTextPrivate::setHardLock(bool state)
{
    if (hardLocked != state) {
        hardLocked = state;
        bool now = isScrollable();
        if (now) {
            q->viewport()->update();
        }
        Q_EMIT q->scrollLockChange(!now);
    }
}


void wLogText::setScrollLock(bool state)
{
    d->setHardLock(state);
}


bool wLogText::scrollLock() const
{
    return d->isHardLocked();
}

bool wLogText::escJumpsToEnd() const
{
    return d->escJump;
}

void wLogText::setEscJumpsToEnd(bool state)
{
    d->escJump = state;
}


void wLogText::setLineStyle(lineNumber_t line, int style)
{
    logTextItemPtr const item = items[line];
    if (item && item->styleId() != style) {
        item->setStyleId(style);
        // TODO Only refresh if item is in view
        setUpdatesNeeded(updateFull);
    }
}


void wLogText::visitItems(logTextItemVisitor& v, lineNumber_t firstLine)
{
    if (firstLine >= m_lineCount)
        return;

    QSignalBlocker disabler(this);
    for (lineNumber_t lineNumber = firstLine; auto& item : items) {
        if (!v.visit({this, item, lineNumber++}))
            break;
    }
}


void wLogText::visitItems(bool (*v)(const logTextItemVisitor::visitedItem& item))
{
    QSignalBlocker disabler(this);
    for (lineNumber_t lineNumber = 1; auto& item : items) {
        if (!v({this, item, lineNumber++}))
            break;
    }
}


void wLogText::visitSelection(logTextItemVisitor &v)
{
    if (d->selecting) {
        auto it  = items.cbegin() + d->selectTop.lineNumber();
        auto end = items.cbegin() + std::min(d->selectBottom.lineNumber(), m_lineCount);

        QSignalBlocker disabler(this);
        for (lineNumber_t lineNumber = d->selectTop.lineNumber(); it != end; ++it) {
            if (!v.visit({this, *it, lineNumber++}))
                break;
        }
    }
}


void wLogText::visitSelection(bool (*v)(const logTextItemVisitor::visitedItem&))
{
    if (d->selecting) {
        auto it  = items.cbegin() + d->selectTop.lineNumber();
        auto end = items.cbegin() + std::min(d->selectBottom.lineNumber(), m_lineCount);

        QSignalBlocker signalBlocker(this);
        for (lineNumber_t lineNumber = d->selectTop.lineNumber(); it != end; ++it) {
            if (!v({this, *it, lineNumber++}))
                break;
        }
    }
}


logTextItemVisitor::~logTextItemVisitor()
{
}


/******************************************************************************
 *
 *    palette class:
 *
 ******************************************************************************/

logTextPalette::logTextPalette(const QString& nm, int ns,
                               QColor const& defText, QColor const& defBack,
                               QColor const& defClBack) : name(nm)
{
    styles.fill(logTextPaletteEntry(defText, defBack, defClBack), std::max(ns, 1));
}


logTextPalette::logTextPalette(const QString& nm, logTextPalette const& source)
{
    name = nm;
    styles = source.styles;
}

styleId_t logTextPalette::addStyle(logTextPaletteEntry style)
{
    styleId_t id = styles.size();
    styles.push_back(std::move(style));
    return id;
}

styleId_t logTextPalette::addStyle(QColor const& textColor, QColor const& bgColor,
                                   QColor const& clBgColor)
{
    return addStyle({textColor, bgColor, clBgColor});
}


logTextPaletteEntry& logTextPalette::style(styleId_t id)
{
    return (id >= styles.count()) ? styles.last() : styles[id];
}


/******************************************************************************
 *
 *    logTextPaletteEntry class:
 *
 ******************************************************************************/

logTextPaletteEntry::logTextPaletteEntry() :
        logTextPaletteEntry(logTextPaletteEntry::attrNone)
{
    QPalette const m_qpalette;
    m_backgroundColor = m_qpalette.color(QPalette::Active, QPalette::Base);
    m_clBackgroundColor = m_qpalette.color(QPalette::Active, QPalette::AlternateBase);
    m_textColor = m_qpalette.color(QPalette::Active, QPalette::WindowText);
}


logTextPaletteEntry::logTextPaletteEntry(const QColor& tc, const QColor& bc,
                    const QColor& clbc, textAttributes a) :
            m_backgroundColor(bc), m_clBackgroundColor(clbc), m_textColor(tc), m_attributes(a)
{
}

logTextPaletteEntry::logTextPaletteEntry(const QColor& tc, textAttributes a) :
        m_textColor(tc), m_attributes(a)
{
    QPalette const m_qpalette;
    m_backgroundColor = m_qpalette.color(QPalette::Active, QPalette::Base);
    m_clBackgroundColor = m_qpalette.color(QPalette::Active, QPalette::AlternateBase);
}


QColor const& logTextPaletteEntry::backgroundColor() const
{
    return m_backgroundColor;
}

void logTextPaletteEntry::setBackgroundColor(const QColor& c)
{
    m_backgroundColor = c;
}


QColor const& logTextPaletteEntry::caretLineBackgroundColor() const
{
    return m_clBackgroundColor;
}

void logTextPaletteEntry::setCaretLineBackgroundColor(const QColor& c)
{
    m_clBackgroundColor = c;
}


QColor const& logTextPaletteEntry::textColor() const
{
    return m_textColor;
}


void logTextPaletteEntry::setTextColor(const QColor& c)
{
    m_textColor = c;
}


logTextPaletteEntry::textAttributes logTextPaletteEntry::attributes() const
{
    return m_attributes;
}


void logTextPaletteEntry::setAttributes( textAttributes a )
{
    m_attributes = a;
}


/******************************************************************************
 ******************************************************************************
 *
 *    activatedPalette class:
 *
 ******************************************************************************
 ******************************************************************************/

activatedPalette::activatedPalette(const QFont& f, logTextPalette& p)
{
    const int nStyles = std::max(1, p.numStyles());
    std::vector<styleItem> tStyles(nStyles);
    for (int i = 0; i < nStyles; ++i) {
        styleItem& style = tStyles[i];
        logTextPaletteEntry& pe = p.style(i);

        QFont font{f};
        logTextPaletteEntry::textAttributes attrs = pe.attributes();
        font.setItalic(attrs & logTextPaletteEntry::attrItalic);
        font.setBold(attrs & logTextPaletteEntry::attrBold);
        font.setUnderline(attrs & logTextPaletteEntry::attrUnderline);
        font.setOverline(attrs & logTextPaletteEntry::attrOverLine);
        font.setStrikeOut(attrs & logTextPaletteEntry::attrStrikeOut);
        style.font = std::move(font);

        style.backgroundColor = pe.backgroundColor();
        style.clBackgroundColor = pe.caretLineBackgroundColor();
        style.textColor = pe.textColor();

    }
    styles = std::move(tStyles);
}

