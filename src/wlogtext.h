/*
 * Copyright (c) 2008-2014, Harmonic Inc. All rights reserved.
 */

/** @file wlogtext.h Definition of the wLogText widget class and API. **/

#ifndef WLOGTEXT_H
#define WLOGTEXT_H

#include <climits>
#include <cstdint>

#include <QAbstractScrollArea>
#include <QColor>
#include <QElapsedTimer>
#include <QFlags>
#include <QPoint>
#include <QPixmap>
#include <QDate>

// STL includes:
#include <vector>
using std::vector;

//#include "ommontypes.h"
using styleId = uint16_t;        //!< Style id within a palette
using lineNumber_t = int32_t;

class QClipboard;
class QContextMenuEvent;
class QCustomEvent;
class QFocusEvent;
class QFont;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QTimerEvent;
class QWheelEvent;

class logTextItemVisitor;
class logTextPalette;
class logTextPaletteEntry;
class wLogText;
class wLogTextPrivate;


/**
 * @brief A class providing character cell (line, column) semantics.
 *
 * An class to represent a character cell position of line and column. It adds the notion
 * screen positional comparison, i.e.
 * if ( cell_a \< cell_b ).  Cell_a \< cell_b if:
 *     ((cell_a.line() \< cell_b.line) ||
 *      ((cell_a.line() == cell_b.line()) && cell_a.col() \< cell_b.col()) );
 **/
class cell {
private:
    int m_line = 0;
    int m_col = 0;

public:
    cell() {};                                  //!< Default constructor
    cell(int l, int c) : m_line{l}, m_col(c) {};   //!< Constructor from line, column
    cell(const cell& p) = default;   //!< Copy constructor

    cell& operator = (cell const& other) = default;

    [[deprecated("Use cell(cell&)")]] explicit cell(const QPoint& p) : cell(p.y(), p.x()) {};   //!< Copy contructor
    [[deprecated("Use cell(cell&)")]] explicit operator QPoint() {return QPoint(columnNumber(), lineNumber());}

    /**
     * Return the line number represented by this cell.
     * @return Line number of this cell
     **/
    int lineNumber() const {return m_line;}            //!< Return line number represented by a cell.

    /**
     * Set the line position represented by this cell.
     * @param l New line number for this cell.
     **/
    void setLineNumber(int l) {m_line = l;}          //!< Set the line number this cell represents

    /**
     * Return the column number represented by this cell.
     * @return column number of this cell
     **/
    int columnNumber() const {return m_col;}               //!< Return the character column this cell represents

    /**
     * Set the column position represented by this cell.
     * @param c New column number for this cell.
     */
    void setColumnNumber(int c) {m_col = c;}         //!< Set the character column this cell represents

    /**
     * Set the line and column position represented by this cell.
     * @param l New line number for this cell.
     * @param c New column number for this cell.
     **/
    void setPos(int l, int c) {setLineNumber(l); setColumnNumber(c);}  //!< Set the cell location

    /**
     * Returns a cell one line after this.  Note, the position strictly mathematical, there is no
     * validation against any document structure.
     * @return A \c cell positition one line down from this.
     **/
    cell nextLine() const {return {lineNumber() + 1, columnNumber()};}  //!< Return a cell one line after this

    /**
     * Returns a cell one column after this.  Note, the position strictly mathematical, there is no
     * validation against any document structure.
     * @return cell location of next column.
     **/
    cell nextCol() const {return {lineNumber(), columnNumber() + 1};}     //!< Return a cell one column after this

    /**
     * Cell relative position comparison.
     * @param c Cell to compare this location to.
     * @return returns <, ==, or > based on line, then cell
     **/
    auto operator <=> (cell const& other) const {
        return lineNumber() == other.lineNumber() ? columnNumber() <=> other.columnNumber() : lineNumber() <=> other.lineNumber();}
        auto operator == (cell const& other) const {
            return lineNumber() == other.lineNumber() && columnNumber() == other.columnNumber();}
};

/**
 * Class to define a line item for to be displayed in a logText widget.
 * It allows access to the text, attributes, and positional information of a
 * text line.
 *
 * This class should not be instantiated by an application.  Creation and
 * access should be done through a logText object.
 *
 * Many attributes are accessable from the object, and some are settable.
 * However, in most cases an object should be modified through the logText
 * object, so that the display will reflect changes in the item.
 */

class logTextItem {
    friend class wLogText;

private:
    QString m_text;                     //!< actual line text
    qint16 m_pixmapId = -1;             //!< ID within the pixmap palette for the gutter pixmap
    styleId m_styleId;                  //!< style ID within the active palette to paint this line

public:
    /**
     * Constructor with attributes.
     *
     * @param text Text of string to be displayed.
     * @param styleNo Style number of the base.
     **/
    logTextItem(const QString& text, styleId styleNo) :
            m_text(text), m_styleId(styleNo) {}

    logTextItem(QString&& text, styleId styleNo) :
            m_text(std::move(text)), m_styleId(styleNo) {}

    /**
     * Copy constructor.
     * @param source Object to copy from.
     */
    logTextItem(const logTextItem&) = default;
    logTextItem(logTextItem&&) = default;

    virtual ~logTextItem() {}

    /**
     * Get the style for a textLog line.
     *
     * @return Style number applied to this text.
     **/
    styleId style() const {return m_styleId;}

    /**
     * @brief set line style
     *
     * Sets the text style of the logText line.
     * @note Use this method to set the style of a newly constructed item, as
     * changing it after it has been appended to the monitorWidget will not
     * result in a redraw with the new style. The application should modify an
     * existing text entry through a wLogText::setStyle(lineNo, style) method
     * so that it can synchronize the on screen view with changes.
     * @param styleNo New style number to be applied to this text.
     */
    void setStyle(styleId styleNo) {m_styleId = styleNo;}

    /**
     * Get the text of the log item.
     *
     * @return const reference to the logTextItem text string.
     **/
    const QString &text() const {return m_text;}

    /**
     * Set a pixmap on a logTextItem.
     *
     * The pixmap is displayed in the text gutter, if enabled.  It is the
     * applications responsibility to ensure the gutter width, line height and
     * pixmap size are compatible.
     *
     * @param pm Pixmap ID in logtext to attach to the logTextItem.
     **/
    void setPixmap(int pm) {m_pixmapId = pm;}

    /**
     * Remove the pixmap from this textLineItem.  If there is a pixmap on this line,
     * it is deleted.
     **/
    void clearPixmap() {m_pixmapId = -1;}

    int pixmapId() const {return m_pixmapId;}
};
using logTextItemPtr = logTextItem *;
using logTextItemCPtr = logTextItem const *;
using logItemsImplList = std::vector<logTextItemPtr>;        //!< list type for the body of logTextItems
using logItemsImplIt = logItemsImplList::iterator;      //!< iterartor for the list of logTextItems
using logItemsImplCIt = logItemsImplList::const_iterator;      //!< iterartor for the list of logTextItems


/**********************************************************
*
* Class logTextItemVisitor:
*
**********************************************************/
/**
 * @brief Visitor action class for iterating over all text items.
 *
 * A class derived and implimented by an application to action performed an
 * action via iteration over all text items.  It is pure virtual, to be
 * implemented in a derived class.  An instance of the derived class is passed
 * to the wLogText visit methods. The implemented visit() function is called
 * on all item elements in the range specified by the visit method.
 *
 * A derived class can implement input and result members for communicating
 * between the application and the visit method.
 *
 * Example:
 * @code
 * class countStrings : public logTextItemVisitor {
 *  private:
 *      int matches;
 *      QString match;
 *  public:
 *      countStrings(QString& text) : matches(0), match(text) {};
 *      int count() { return matches; };
 *      visit(wLogText *logText, logTextItem *item, visitationFlags flags) {
 *          if (item->text().find(match) != -1) {
 *              ++matches;          // Increment counter for showing results
 *              logText->setItemStyle(item, styleTextFound);  // Recolor
 *          }
 *          return true;  // Keep going
 *      };
 * };
 *
 *...
 *      countStrings stringCounter("Hello World");
 *      logText->visitItems(stringCounter);
 *      showMessage("Found %d matches", stringCounter.count());
 * ...
 * @endcode
 *
 * @see wLogText::visitItems
 * @see wLogText::visitSelection
 */
class logTextItemVisitor {
public:
    class visitedItem {
    public:
        wLogText *logText;
        logTextItem *lineItem;
        lineNumber_t lineNumber;
        visitedItem(wLogText *log, logTextItem *item, lineNumber_t firstLine) :
                    logText(log), lineItem(item), lineNumber(firstLine) {}
    };
    virtual ~logTextItemVisitor();
        /**
         * @brief Visit action method
         * This method is called on each item in the log text visit range.
         * @param logText The wLogText instance from which the items visited.
         * @param item An instance of the item in the iteration step.
         * @return true if visiting should continue; else false to terminate
         *         visitation.
         * @see wLogText::visitItems
         * @see wLogText::visitSelection
         */
    virtual bool visit(const visitedItem& item)=0;
};


/**
 * @brief A widget for displaying simple text lines, optimized for speed with few formatting options.
 *
 * This is a text display widget, optimized for speed over formatting.
 * The intended use would be for something like a log viewer, with monospace
 * characters. Simple formatting is available on a per line basis; the
 * entire line uses a single format.  This eliminates the overhead of parsing
 * the content for formatting tags.  The format is specified as a style value,
 * which selects a style from a static palette.  For simplicity and speed,
 * all lines are displayed with the same (monospace) font.  Styles can modify
 * the text attributes other than size.
 *
 * The textual elements are accessable, eliminating the need to maintain
 * separate copies in the application and the widget.  Formatting can be altered
 * on a per-line basis, and will be reflected in the view.
 *
 * Each widget has an active palette.  The palette can be changed.
 * Each palette defines:
 *  - A base style (font) from which the styles are derived.
 *  - A set of styles derived from the base style.
 *
 * The styles are derived from the base style, specifying the modifications in
 * attributes to that base.
 *
 * The modifiers are an or'ed bit mask of attribute modifiers the style.
 *
 * The styles are all based on the same base monospace font, but on a per
 * style basis can modify:
 *  - Text color
 *  - Background color
 *  - visual attribute: Italic, Underline, strike through, bold.
 *
 * Note that a style cannot modify the size or face from the base font.  Using
 * a uniform face and size makes layout simpler and faster.
 *
 * The style-modifiers are a bit-wise modifier to the selected style of the element.
 * Each modifer bit position represents a single style modification.  The same
 * modifier bit position will apply the same effect to any base style (within
 * limits of physical representations and conflicts).
 *
 * So for example, suppose style 0 defines black text, and style 1 blue; and
 * modifier 0 defines underlining.  If line one has selected syle 0, and line
 * two selects style 1 then line one is displayed with black text, and line two
 * with blue.  If then modifier0 is applied to both lines, they will maintain
 * their base colors, but will both have underlining also appied.
 *
 * @author Rick Wagner <Rick.Wagner@Harmonicinc.com>
 */
class wLogText : public QAbstractScrollArea {
    Q_OBJECT

    /** Base font of style for displayed text */
    Q_PROPERTY(QFont font READ font WRITE setFont)

    /** Gutter size property */
    Q_PROPERTY(int gutter READ gutter WRITE setGutter)

    /**
     * Maximum log lines the widget will maintain. When the maximum is
     * exceeded, new lines will cause the top of the log to be trimmed down to
     * the maximum
     **/
    Q_PROPERTY(lineNumber_t maxLogLines READ maxLogLines WRITE setMaxLogLines)

    /** Controls the display of the blinking text cursor in the client area */
    Q_PROPERTY(bool showCaret READ showCaret WRITE setShowCaret)

    /**
     * Amount of time in milliseconds after the mouse stops moving over the
     * text area to emit a hover signal
     **/
    Q_PROPERTY(int m_hoverTime READ hoverTime WRITE setHoverTime)

    /**
     * Behavior of a user escape-key press when scrolling is locked. When the
     * property is true, an escape-key will release the scroll lock, and scroll
     * the text area to display the last text line.
     **/
    Q_PROPERTY(bool escJumpsToEnd READ escJumpsToEnd WRITE setEscJumpsToEnd)

private:
    /** Refresh needed due to changes during an "updatesDisabled" state */
    enum updateLevel {
        noUpdate,           //!< No changes made, update is not needed
        updateCond,         //!< Update is conditionally needed
        updateFull          //!< Update is required
    };

protected:
    friend class wLogTextPrivate;

    wLogText(const wLogText&) = delete;
    wLogText& operator =(const wLogText&) = delete;

    /** Pointer to implementation detail */
    std::unique_ptr<wLogTextPrivate> const d;

    logItemsImplList items;         //!< Vector of text plus attribute items.
    lineNumber_t m_lineCount = 0;   //!< Count of lines
    int m_maxLineChars = 0;         //!< Width in characters of longest line in list of all lines.

    lineNumber_t maximumLogLines = 0;        //!< Maximum lines held.  Above this, and top lines are discarded.
    lineNumber_t maximumLogLinesSlacked = std::numeric_limits<lineNumber_t>::max(); //!< maximumLogLines + slack to go before trimming.

    updateLevel updatesNeeded = updateLevel::noUpdate;  //!< Flag to indicate to idle event to update screen.
    int updatePosted = 0;           //!< Flag indicting that an update event is already pending.

    QPoint mouseRawPos;         //!< Raw position of the last mouse move event
    QElapsedTimer mouseHoverOnTime;     //!< Time at last mouse move event
    bool mouseHovering = false;         //!< If a hover has been detected, and a hover signal emitted.
    int m_hoverTime = 0;            //!< Time in mSeconds to trigger mouse hover event.
    int hoverTimerId = 0;           //!< QObject timer ID for the hover timer
    bool finalized = false;             //!< If finalize() has been called.

    // Widget events; these are overrides of functions in ancestor classes
    // (QScrollView, QWidget, QObject).  Thus thay cannot move to wLogTextPrivate.
    /**
     * @brief Context menu request.
     *
     * Handle context menu request.
     *
     * @param event QContextMenuEvent for the context request.  If the event does not
     * occur on a valid line, it is ignored.  If the event occurs in
     * the gutter area, emit a gutterContextClick() signal, else emit a
     * contextClick() signal.
     */
    void contextMenuEvent(QContextMenuEvent *event) override;

    /**
     * @brief Handle QWidget custom events from the Qt event loop. [virtual, inherited]
     *
     * Inherited from QWidget, handles QEvent events posted to the event
     * queue.  This reimplentation is for signaling the request to update the
     * display when requested by setUpdatesNeeded.
     *
     * @param event The Qt custom event object.
     */
    void customEvent(QEvent *) override;

    /**
     * @brief Handle QWidget focusInEvent. [virtual, inherited]
     *
     * Inherited from QWidget, handles notification of the widget receiving focus.
     *
     * @param event QFocusEvent object passed from QWidget.
     */
    void focusInEvent(QFocusEvent *event) override;

    /**
     * @brief Handle QWidget focusOutEvent. [virtual, inherited]
     *
     * Inherited from QWidget, handles notification of the widget losing focus.
     *
     * @param  event QFocusEvent object passed from QWidget.
     */
    void focusOutEvent(QFocusEvent *event) override;

    /**
     * @brief Handle key press event.
     *
     * Handle keyboard key press events.
     *
     * @param event QKeyEvent describing the event.
     */
    void keyPressEvent (QKeyEvent *event) override;

    /**
     * @brief Mouse double-click event handler. [virtual, overloaded: QAbstractScrollArea]
     *
     * Function called when a double click in the visible region occurs.  Emits a
     * signal based on whether the event occurs in the text area, or the gutter.
     *
     * @param event event describing the mouse position and state.
     *
     */
    void mouseDoubleClickEvent (QMouseEvent* event) override;

    /**
     * @brief Mouse movement event handler. [virtual, overloaded: QAbstractScrollArea]
     *
     * Function is called to handle mouse movements while the mouse is down, having
     * been pressed within the widget region.  Note that the coordinates of the
     * movement may extend outside the widget, and may be negative when outside
     * left and/or above.
     *
     * @param event event describing the mouse position and state.
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * @brief Handle mouse button press event.
     *
     * Handle mouse button press events.  If the event is in the gutter region, a
     * gutterClick signal is emitted.  Otherwise, if the event was a left button
     * click, the caret is moved the clicked cell, and selection setup is initiated.
     * A clicked signal is also emitted.
     *
     * Selection setup clears any current selection, and sets up an new selection.
     *
     * @param event QMouseEvent
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief Handle mouse button release event.
     *
     * Handle mouse button up.
     *
     * @param event QMouseEvent for the mouse release.
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /**
     * @brief Paint a region of the visible text area. [virtual, overload]
     *
     * Overload from @a QWidget.  Paints the specified region.  Called when changes
     * occur on the view; i.e. view area is scrolled, or resized.  The rectangle
     * defined in clipx, clipy,..., may, and often are, just a subset of the
     * physically visible area of the screen.  They are optimize to cause a repaint
     * of only the area which has changed.
     *
     * @param event Event describing the region to be updated.
     */
    void paintEvent(QPaintEvent *event) override;

    /**
     * @brief Visible region size changed event. [virtual, overloaded:
     * QAbstractScrollArea]
     *
     * Event handler for resize events.  Adjusts variables which depend upon the
     * view area size.
     *
     * @param event Resize event data.
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * @brief Reimplementation of QObject::timerEvent [virtual]
     *
     * Reimplementation of QObject::timerEvent.  It is called when a timer created
     * by a QObject::startTimer expires.
     *
     * @param event A QTimerEvent for the fired timer.
     */
    void timerEvent(QTimerEvent *event) override;

    /**
     * @brief Handle mouse wheel event.
     *
     * Handle a mouse wheel event.  This will scroll the display, in various
     * incrementes, or zoom the font size in or out, depending on the state of the
     * keyboard shift, alt, and control keys.
     *
     *  Just wheel: Scroll global line steps.
     *  Ctrl-Wheel: Zoom font.
     *  Shift-Wheel: Scroll page.
     *  Ctrl-Shift-Wheel: Scroll line.
     *  Alt-Wheel: Scroll horizontal 1/4 width.
     *  Alt-Ctrl-Wheel: Scroll horizontal 1 character.
     *
     * @param event Event for the wheel event.
     */
    void wheelEvent(QWheelEvent *event) override;

    /**
     * @brief recalculate the horizontal scrollbar limits
     *
     * Called when the character width or max line length changes, to
     * recalculate the horizontal scroll bad limits.
     **/
    void adjustHorizontalScrollBar();

    /**
     * @brief Signal the need to refresh the display.
     *
     * Conditionally signals the need to refresh the display, setting the detail
     * level of the update to the greater of the current value, or the passed value.
     *
     * Updates are signaled by posting a custom event to the event queue, if updates
     * are enabled.
     *
     * @param level Level of update detail being set.
     */
    void setUpdatesNeeded(updateLevel level);

    /**
     * Remove lines in excess of maximumLogLines from the top of the widget
     **/
    void trimLines();

    cell pointToCell(QPoint const& point) const;

public:
    /**
     * @brief Constructor
     * Constructor
     * @param parent Parent of this widget
     */
    explicit wLogText(QWidget *parent = nullptr);

    /**
     * @brief Destructor
     * 
     * Default destructor.
     */
    virtual ~wLogText();

    /**
     * @brief Get base font
     *
     * Gets the font used to derive a activated palette. When a palette is
     * activated, the styles are redered from this base.
     * @return Font base for the style
     **/
    QFont const& font() const;

    /**
     * @brief Interline spacing, in pixels.
     *
     * Returns the active line height, in scrollView units (pixels).  The height
     * includes adjustment for font zooming.
     *
     * @return Height of a text line, in scrollView units.
     */
    int lineHeight() const;

    /**
     * @brief Text zoom factor
     *
     * Integer increment (+/-) in points from the base font (@see font()) when
     * the text is zoomed in or out. 
     * @return Zoom factor in +/- points from base. Returns 0 when zoom is set to normal.
     **/
    int fontZoom() const;

    /**
     * @brief Returns the number of characters in a line.
     *
     * Return the length in characters of line @p lineNo.
     *
     * @param  lineNumber Line number of which to get the length.
     * @return length of the line if @p lineNumber is a valid line number; else 0.
     **/
    int length(lineNumber_t lineNumber) const;

    /**
     * @brief Return count of lines.
     *
     * Return the number of lines of text in the text buffer.  Because of the slack
     * functionality (see setMaxLogLines()), the number of lines currently in the
     * buffer may exceed the max line count.  If this is a problem, use the trim()
     * function to force a trimming of the slack.
     *
     * @return Number of lines of text.
     */
    lineNumber_t lineCount() const {return m_lineCount;}

    /**
     * @brief get maxLogLines property
     *
     * Returns the maxLogLines property configured for the widget. The property
     * can be set via setMaxLogLines.
     *
     * To prevent excessive trimming, the widget maintains a "slack", which is
     * an additional number of lines which can be in the widget. So appendign a
     * line checks against the slacked value, and once the slack limit is
     * reached, the lines will be trimmed down to the maxLogLine() value. Thus
     * it is possible for lineCount() to return a value (and the widget to
     * display ) greater then maxLogLines() lines of text.
     * 
     * @return Configured maximum number of lines kept by the widget.
     * @sa wLogText::setMaxLogLines().
     **/
    lineNumber_t maxLogLines() const;

    /**
     * @brief Mark begin/end of content update period. [virtual, overload: QWidget]
     *
     * Overload of QWidget function.  Disables refreshing of display while in
     * updates enabled state.  Use this when multiple changes (i.e. adding
     * lines) are expected, to reduce flicker and speed up operations across
     * intermediate changes.
     *
     * @param newState if @c false: screen refreshes are disabled; else if
     *          @c true: re-enable screen refreshes, and apply updates made
     *          while updates are enabled.
     */
    void setUpdatesEnabled(bool newState);

    /**
     * @brief Append a single line of text.
     *
     * Appends a text item pointer to the widget. The item pointer must
     * be allocated via new. Ownership of the pointer transfers to the widget,
     * which will be responsible for its deletion with delete.
     *
     * @param item pointer to logTextItem. Ownership of the pointer transfers
     * @return line number of newly added line
     **/
    lineNumber_t append(logTextItem *item) {
        m_maxLineChars = std::max(m_maxLineChars, item->m_text.length());
        items.push_back(item);
        setUpdatesNeeded(updateCond);

        if (maximumLogLines && (m_lineCount >= maximumLogLinesSlacked))
            trimLines();
        return m_lineCount++;
    }

    /**
     * @brief Erase a range of lines
     *
     * Erases the lines from line number @p top for @p count lines. No signal is
     * emitted in response to this call.
     * @param top Line number to start erasure
     * @param count Number of lines from @p top to clear
     **/
    void clear(int top, int count);
    
    /**
     * @brief Check validity of a line number
     *
     * Verifies that the line number @p lineNo is a valid line number, i.e.
     * >= 0 and < numberOfLines
     *
     * @param lineNo Line number to validate
     * @return @c true if @p lineNo is a valid line number.
     **/
    bool validLineNumber(lineNumber_t lineNo) const {
        return (lineNo >= 0) && (lineNo < m_lineCount); }

    /**
     * @brief Return const pointer to line item for a line
     *
     * Get a reference to a text item (text string + attributes).  Results are
     * unpredictable if the passed line number is out of range.
     *
     * @param lineNo number of line to retrieve.
     * @return const pointer to line item.
     */
    const logTextItem *item(lineNumber_t lineNo) const {
        if (lineNo >= m_lineCount)
            lineNo = m_lineCount - 1;
        return items[lineNo];
    }

    /**
     * @brief Return pointer line item for a line
     * @overload
     * @param lineNo number of line to retrieve.
     * @return pointer to line item.
     **/
    logTextItem *item(lineNumber_t lineNo) {
        if (lineNo >= m_lineCount)
            lineNo = m_lineCount - 1;
        return items[lineNo];
    }

    // Palette operations:
    /**
     * @brief Activate a palette.
     *
     * Activate the named palette.  Converts a palette to a styles array to be used
     * for drawing the text.  If the passed name is null (QString::null), then the
     * current palette is reactived.  Reactivating the current palette is useful
     * when the palette styles have changed since the last activation.
     *
     * @param name Name of palette, allocated by @a newPalette(), to make active.
     * @return true if the activation succeeded; else false;
     */
    bool activatePalette(const QString& name= QString());

    /**
     * @brief Create a named palette.
     *
     * Create a palette with system default values for text and background colors.
     * The operation will fail (returning 0) if the name is the default palette
     * name.  If a palette of the given name already exist, it will be deleted first
     * (using @a wLogText::deletePalette()) and a new one created in its place.
     *
     * @param size Number of palette style items to allocate.
     * @param name Name assigned to the palette.
     * @return Pointer to a palette if creation was successful; 0 on failure.
     */
    logTextPalette *createPalette(size_t size, const QString& name);

    /**
     * @brief Create a clone of an existing palette.
     *
     * Creates a new palette with a given name, as a clone of an existing palette.
     * See wLogText::createPalette(int size, const QString& name).
     *
     * @param name Name assigned to the palette.
     * @param source Source palette to clone.
     * @return Pointer to a palette if creation was successful; 0 on failure.
     */
    logTextPalette *createPalette(const QString& name, const logTextPalette *source);

    /**
     * @brief Return a pointer to a named palette.
     *
     * If the palette named exits, return a pointer to it, else retun 0.
     *
     * @param name  Name of the palette to get.
     * @return Pointer to the named palette.  Zero if it does not exist.
     */
    logTextPalette *palette(const QString& name) const;

    /**
     * @brief Delete a named palette.
     *
     * Deletes the named palette.  If the named palette is the active palette, the
     * default palette is made active.  If the named palette is the default palette,
     * the operation is ignored.
     *
     * @param name Name of the palette to delete.
     */
    void deletePalette(const QString& name);

    /**
     * @brief Return the name of active palette.
     *
     * Returns the name of the currently active palette.
     *
     * @return Name of the active palette.
     */
    QString activePaletteName() const;

    /**
     * @brief Return the name of default palette.
     *
     * Returns the name of the default palette.
     *
     * @return Name of the active palette.
     */
    QString defaultPaletteName() const;

    /**
     * @brief Get a list of palette names.
     *
     * Returns a list of the currently defined palette names.
     *
     * @return String list of palette names.
     */
    QStringList paletteNames() const;

    /**
     * @brief Return the cell (line/column) of the caret.
     *
     * Returns the current cursor postion, in the form of line, column.
     *
     * @return @c cell caret position
     */
    cell cursorPostion() const;

    /**
     * Set the current cursor postion, in the form of line, column.  If line is
     * negative, the position is counted back from the last line.  If column is
     * negative the position counted back from the end of line.  The position does
     * not wrap, so if a column number is more negative than the length of the line,
     * the new position is the beginning of the line, not wrapped back on the
     * previous line.
     *
     * @param lineNumber New line number for cursor position.
     * @param columnNumber New column position.
     **/
    void setCursorPosition(lineNumber_t lineNumber, int columnNumber);

    /**
     * @brief Set the caret to a specified line, column coordinate.
     *
     * Set the current cursor postion from a QPoint (or cell). p.y is line, p.x=col.
     * If line is negative, the new position is "line" lines from the end.  If col
     * is negative, the new column position is "col" characters from the end.
     *
     * To move to the end of the text, use a QPoint(MAXINT, MAXINT). (MAXINT is
     * usually defined in values.h).
     *
     * @param p New cursor position.
     **/
    void setCursorPosition(const cell&);

    /**
     * @brief Get state of showCaret flag.
     *
     * Gets the state of the show caret flag.
     *
     * @return true if the caret will be shown.
     **/
    bool showCaret() const;

    /**
     * @brief Search for text string.
     *
     * Find a line containing a text string.  If the search point pointer is
     * non-zero, it is the starting point of the search; otherwise the search begins
     * at the current caret position.  If the pointer is non-zero, it is updated
     * with found location, if the find is successful.
     *
     * @remarks The search occurs only within whole text lines.  Searches which span
     * multiple lines are not supported.
     *
     * @param str Text string to search for.
     *
     * @param pt Pointer to @a QPoint giving cell location (y() = line, x() = column)
     * to begin the search.
     *
     * @param caseSensitive True if the seach is case sensitive.
     *
     * @param forward If true, the search is from the start position to the end of
     * text; else to beginning of text.
     *
     * @return True if the text is found within the text.  If the search matches,
     * and position pointer is non-zero, the location pointed to is updated with the
     * match position.  If the search does not match, the pointed to location
     * remains unchanged.
     *
     * @return If the parameter @p p is not null, *p will be set to the resultant
     * location of the text.
     */
    bool find(const QString& str, cell *pt,
              Qt::CaseSensitivity=Qt::CaseInsensitive, bool forward=true);

    /**
     * @brief Search for text string.
     *
     * Find a line containing a text string.  This is for compatibility with
     * QTextEdit, without the whole word parameter.  For whole word matching, see
     * the version of this function which takes a @c QRegularExpression parameter.
     *
     * If the line and column pointers are non-zero, they give the starting point
     * of the search; else the search begins at the current caret position.  If
     * either or both pointers are non-zero, they are updated with found location,
     * if the find is successful.
     *
     * @param str Text string to search for.
     *
     * @param caseSensitive True if the seach is case sensitive.
     *
     * @param forward If true, the search is from the start position to the end of
     * text; else to beginning of text.
     *
     * @param line Pointer to the line to start search, and to where the
     * line number of a successful search is put.
     *
     * @param col Pointer to the column number in the line to start the
     * search, and where the coulumn number of a successful search is put.
     *
     * @return True if the text is found within the text.  If the search matches,
     * and either or both line and column pointers are non-zero, the locations
     * pointed to are updated with the match position.  If the search does not match,
     * the pointed to locations remain unchanged.
     *
     * @retval line If the parameter @p line is not null, *line will be set to the
     * resultant line number of the location of the text.
     *
     * @retval col If the parameter @p col is not null, *col will be set to the
     * resultant column number of the location of the text.
     */
    bool find(const QString&, Qt::CaseSensitivity=Qt::CaseInsensitive,
              bool forward=true, lineNumber_t *line=nullptr, int *col=nullptr);

    /**
     * @brief Search for text matching a regular expression.
     *
     * Find a line containing a text string.  If the search point pointer is
     * non-zero, it is the starting point of the search; otherwise the search begins
     * at the current caret position.  If the pointer is non-zero, it is updated
     * with found location, if the find is successful.
     *
     * @param re @a QRegularExpression to match in the search.
     *
     * @param p Pointer to @c cell giving cell location (y() = line,
     * x() = column) to begin the search.
     *
     * @param forward If true, the search is from the start position to the end of
     * text; else to beginning of text.
     *
     * @return @c True if the text is found within the text.  If the search matches,
     * and position pointer is non-zero, the location pointed to is updated with the
     * match position.  If the search does not match, the pointed to location
     * remains unchanged.
     *
     * @retval p If the parameter @p p is not null, *p will be set to the resultant
     * location of the text.
     */
    bool find(const QRegExp& re, cell *at, bool forward = true);

    /**
     * @brief Ensure the caret is visible, scrolling if needed.
     *
     * Ensure that the caret position is visible, scrolling the visible region if
     * needed.
     **/
    void ensureCursorVisible();

    /**
     * @brief Check for text selection.
     *
     * Checks for having selected text.
     *
     * @return Returns @c TRUE if some text is selected; otherwise returns @c FALSE.
     **/
    bool hasSelectedText() const;

    /**
     * @brief Get coordinates of the selection.
     *
     * If there is a selection, *lineFrom is set to the number of the linegraph in
     * which the selection begins and *lineTo is set to the number of the linegraph
     * in which the selection ends. (They could be the same.) *colFrom is set to
     * the col at which the selection begins within *lineFrom, and *colTo is set
     * to the col at which the selection ends within *lineTo.
     *
     * If there is no selection, *lineFrom, *colFrom, *lineTo and *colTo are all
     * set to -1. If lineFrom, colFrom, lineTo or colTo is 0 this function does
     * nothing.
     *
     * @param lineFrom  Pointer to store line number of selection start
     * @param colFrom   Pointer to store column number of selection start
     * @param lineTo    Pointer to store line number of selection end
     * @param colTo     Pointer to store column number of selection end
     **/
    void getSelection(lineNumber_t *lineFrom, int *colFrom,
                      lineNumber_t *lineTo, int *colTo) const;

    QString toPlainText(QLatin1Char sep = QLatin1Char('\n')) const;

    /**
     * @brief Returns a single string of text selection.
     *
     * Get the selected text as a single string.  If the selection spans more than
     * a line, the lines are separated with a new line ('\\n') character.
     *
     * @return Selection region, concatenated with new-line separation.
     **/
    QString selectedText() const;

    /**
     * @brief Get the current mouse hover delay time.
     *
     * Returns the value set after adjustments by \c setHoverTime.
     * \see setHoverTime
     *
     * @return hover time in milliseconds
     **/
    int hoverTime() const {return m_hoverTime;}

    /**
     * @brief Set time for mouse hover signal
     *
     * Set the time in milliseconds for a mouse move to trigger a hover signal.
     * A value of <= 0 shuts off the hover notifications. The value is rounded up
     * to a multiple of 50mS, with a maximum of 2500 mS.
     *
     * @param t Mouse hover time in milliseconds.
     **/
    void setHoverTime(int t);

    /**
     * @brief Return current state of scroll lock.
     *
     * Return current state of scroll lock
     *
     * @return Current scroll lock state.
     */
    bool scrollLock() const;

    /**
     * @brief behavior of esc key when the view is scrolled
     *
     * Get the state of behavior for escape key presses when the view is
     * scrolled.
     *
     * @return @c true when escape key will release the scroll lock, and the
     * view will jump to the last line.
     * @return @c false escape key will not release a scroll lock
     **/
    bool escJumpsToEnd() const;

    /**
     * @brief set behavior of escape key press when the view is scroll-locked
     *
     * Set the mode indicating whether pressing the escape key will release a
     * scroll lock, and scroll the view to the bottom line.
     * 
     * @param jump new state for the escJumpsToEnd property
     **/
    void setEscJumpsToEnd(bool jump);

    /**
     * @brief Returns the current gutter width.
     *
     * Gets the width of the gutter.  This space does not include the gutter
     * border, if any.
     *
     * @return Width in pixels of the gutter.  The vertical space per line is the
     *  same as the text height, @a lineHeight().
     */
    int gutter() const;

    /**
     * @brief map a pixmap to a pixmap ID
     *
     * Maps the pixmap to a pixmap identifier. Gutter pixmaps are identified
     * per line in the logTextLineItem by pixmap ID. This call maps the pixmap
     * ID to the pixmap to be displayed.
     *
     * @see wLogText::setLinePixmap
     * @param pixmapId ID of the pixmap to map
     * @param pixmap visual pixmap assigned to the @p pixmapId
     **/
    void setPixmap(int pixmapId, QPixmap& pixmap);

    /**
     * @brief unmap a pixmap ID
     *
     * Removes the mapping assigned in wLogText::setPixmap for the ID.
     * 
     * @param pixmapId pixmap ID to unmap
     **/
    void clearPixmap(int pixmapId);
    
    /**
     * @brief Assign a gutter pixmap to a line.
     *
     * Assign a pixmap to be displayed in the gutter of a line.  The pixmap is
     * assigned regardless of the state of the gutter.
     *
     * @p pixmapId is an arbitrary application integer value. The ID is mapped
     * to a visual pixmap with wLogText::setPixmap().
     *
     * @param lineNo Number of the line to receive the pixmap.
     * @param pixmapId identifier for the pixmap on this line.
     */
    void setLinePixmap(lineNumber_t lineNo, int pixmapId);

    /**
     * @brief Remove a pixmap for the line
     *
     * Remove the pixmap (if any) assigned to the specified line.
     *
     * @param lineNo Line number of line on which to remove the pixmap.
     */
    void clearLinePixmap(lineNumber_t lineNo);

    /**
     * @brief Set the style on an item.
     *
     * Alters the style value on a logTextItem.  This method is used in place of
     * directly altering the item, to ensure an update of items currently on screen
     * are redrawn.  If updates are enabled, the screen refresh (if any) is deferred
     * until updates are re-enabled.
     *
     * @param item pointer to the item to be updated.
     * @param style New style value for the item.
     */
    void setLineStyle( lineNumber_t line, int style);

    /**
     * @brief Apply a function to items in the item list.
     *
     * Walks the list of all the @c logTextItem items in the widget, and applies
     * the @c logTextItemVisitor::visit method to each item.
     *
     * The logTextItemVisitor::visit() function is appied to each line from line
     * @p firstLine (default 0), until visit() returns false or the last list
     * has visited reached.
     *
     * Updates are disabled across all visitations, and returns to the
     * same enabled state as on entry. Thus updates will not be shown until all
     * the visitations are complete, or longer if updates were disabled on entry.
     *
     * @param v Visitor object to be applied to each item.
     * @param firstLine Line number of the first element to be applied.
     */
    void visitItems(logTextItemVisitor &v, lineNumber_t firstLine=0);

    /**
     * @brief Apply a function to items in the item list.
     *
     * Walks the list of all the @c logTextItem items in the widget, and applies
     * the @p op method to each item.
     *
     * The logTextItemVisitor::visit() function is appied to each line from the
     * first line of the view, until visit() returns false or the last list
     * has visited reached.
     *
     * Updates are disabled across all visitations, and returns to the
     * same enabled state as on entry. Thus updates will not be shown until all
     * the visitations are complete, or longer if updates were disabled on entry.
     *
     * @param op Visitor function to be applied to each item.
     */
    void visitItems(bool (*op)(const logTextItemVisitor::visitedItem&));

    
    /**
     * @brief Apply a function to all selected lines in the item list.
     *
     * Walks the list of all the \a logTextItem items in the selection range, and applies
     * the \a logTextItemVisitor::visit method to each item.
     *
     * The visit() function is appied from the first line of the selection range,
     * even if the selection starts after the start of the line.  It visits each in
     * the selection until visit() returns false, or the end of the selection range
     * is reached.  Last line of the selection is visited, even if the selection
     * does extend to the end of that line.
     *
     * Updates are disabled across all visitations, and returns to the
     * same enabled state as on entry.  Thus updates will not be shown until all
     * the visitations are complete, or longer if updates were disabled on entry.
     *
     * @param v Visitor object to be applied to each item.
     */
    void visitSelection(logTextItemVisitor &v);
    void visitSelection(bool (*v)(const logTextItemVisitor::visitedItem& item));

protected Q_SLOTS:
    /**
     * @brief slot called when the vertical scrollbar scroll position is changed
     * @param value new scrollbar position value
     **/
    void vScrollChange(int value);

    /**
     * @brief slot called when the horizontal scrollbar scroll position is changed
     * @param value new scrollbar position value
     **/
    void hScrollChange(int value);

public Q_SLOTS:
    /**
     * @brief Sets the maximum number of lines.
     *
     * Sets the maximum number of lines to keep.  A value of 0 means unlimited.  A
     * slack is added in to allow trimming in chunks, which is more efficient than
     * trimming one at a time.  Once the slack capacity is reached, (or at select
     * other times), the list will be trimmed down to the maximum lines.  So at
     * times, the count returned may exceed the 'setMaxLogLines' value.
     *
     * @param mll Maximum number of lines.  Use 0 for unlimited.
     */
    void setMaxLogLines(lineNumber_t mll);

    /**
     * @brief Clear display and delete all line d->items.
     *
     * Deletes the line items, and clears the display area.  Resets sizes and
     * counts apropriately.
     */
    void clear();

    /**
     * @brief advisory that no new items are expected
     *
     * Call to indicate that no new items are expected, so resources can be
     * released. The call is an expectation, however, new items may be received.
     * The view is not obligated to display them, but must remain stable (not
     * crash) if any new lines are received after finalize(). Note that accesses
     * and line deletions are still possible.
     */
    void finalize();
    
    /**
     * @brief Assign a new fixed-pitch base font.
     *
     * Sets the base font to the requested font. The requested font must be a
     * fixed pitch font. If it is not, then the request is ignored, and no
     * changes occur.
     *
     * @param font New fixed pitch QFont to use for text display.
     **/
    void setFont(QFont font);

    /**
     * @brief Increment the font magnification.
     *
     * Increments the displayed font zoom increment by one point. Same as doing
     * setFontZoom(fontZoom() + 1);
     **/
    void enlargeFont();

    /**
     * @brief Decrement the font magnification.
     *
     * Decrement the displayed font zoom increment by one point. Same as doing
     * setFontZoom(fontZoom() - 1), when fontZoom > 2;
     **/
    void shrinkFont();

    /**
     * @brief Set the mouse zoom increment.
     *
     * Set the font zoom increment, in points.  The displayed text is incremented
     * (zoom > 0) or decremented (zoom < 0) by the specified number of points.
     *
     * @param zoom Font size increment in points.
     */
    void setFontZoom(int zoom);

    /**
     * @brief Reset font magnification.
     *
     * Resets the displayed text size to the base font size.
     **/
    void resetFontZoom();

    /**
     * @brief Select all text.
     *
     * Select all the text.  Selected text is copied to the global clipboard. Emits
     * a copyAvailable(true) signal.
     **/
    void selectAll();

    /**
     * @brief Remove selection
     *
     * Unselect the text selection, and removes the text from the global clipboard.
     * Emits a copyAvailable(false) signal.
     *
     **/
    void clearSelection();

    /**
     * @brief Copy selection to the clipboard.
     *
     * Request to copy the current selection, if there is one, to the clipboard.  If
     * there is no current selection, no action is taken.
     */
    void copy() const;

    /**
     * @brief Set the gutter width.
     *
     * Sets the width of the gutter in pixels.  This space does not include the gutter
     * border, if any.  A value of zero disables the gutter and gutter border.  When
     * set to a positive value, a space along the left edge is left available for
     * line pixmaps.
     *
     * @param width Width in pixels for the gutter.  Zero disables gutter.
     * @sa wLogText::setLinePixmap() for setting a pixmap into the gutter of a line.
     */
    void setGutter(int);

    /**
     * @brief Set scroll lock state.
     *
     * Set the scroll lock state.  Emits a scrollLockChange if the state changes.
     *
     * @param state New scroll lock state.
     **/
    void setScrollLock(bool state);

    /**
     * @brief Set whether the caret will be shown.
     *
     * Sets the show caret flag.
     *
     * @param show New state of the show caret flag.
     **/
    void setShowCaret(bool show);

Q_SIGNALS:
    /**
     * @brief Signal click at line/column position.
     *
     * This signal is emitted when a click in the text area occurs.
     *
     * @param event Mouse event context.
     */
    void clicked(QMouseEvent *event);

    /**
     * @brief Context menu request in log body.
     *
     * Signal the request for a context menu (i.e. right mouse click) in the text
     * area.
     *
     * @param lineNo The text line number the event ocurred on.  This is the line
     *          number relative to the start of the text, not the start of the
     *          display area.
     * @param point The QPoint location of the mouse pointer when the click ocurred.
     * @param event Mouse event context.
     */
    void contextClick(lineNumber_t lineNo, QPoint& point, QContextMenuEvent *event);

    /**
     * @brief When selection state changes.
     *
     * Signal emitted when a copyable selection change has occurred.  When the
     * parameter is true, hasSelectedText() will return true, and text would be
     * returned by a selectedText() call.
     *
     * @param avail true when hasSelectedText() would return true; i.e. when text is
     * available to be copied.
     */
    void copyAvailable(bool avail);

    /**
     * @brief Double click at line/column position.
     *
     * A mouse button double click in the text area.  While the parameter value
     * is a QPoint, internally it is a cell, which represents a line and column pair.
     *
     * @param cell The line (\p cell.y()) and column (\p cell.x()) the event
     *          ocurred on.
     */
    void doubleClicked(cell cell);

    /**
     * @fn void wLogText::gutterClick(int lineNo, ButtonState btn, QPoint point)
     * @brief Click in gutter of for a line.
     *
     * @param lineNo The text line number the event ocurred on.  This is the line
     *          number relative to the start of the text, not the start of the
     *          display area.
     * @param event Mouse event context.
     */
    void gutterClick(lineNumber_t lineNo, QMouseEvent *event);

    /**
     * @fn void wLogText::gutterContextClick(int lineNo, ButtonState btn, QPoint point)
     * @brief Context menu request in the gutter of line.
     *
     * @param lineNo The text line number the event ocurred on.  This is the line
     *          number relative to the start of the text, not the start of the
     *          display area.
     * @param point The QPoint location of the mouse pointer when the click ocurred.
     * @param event Event context
     */
    void gutterContextClick(lineNumber_t lineNo, QPoint& point, QContextMenuEvent* event);

    /**
     * @brief Double-click in gutter of line.
     *
     * @param lineNo The text line number the event ocurred on.  This is the line
     *          number relative to the start of the text, not the start of the
     *          display area.
     */
    void gutterDoubleClicked(lineNumber_t lineNo);

    /**
     * @brief signal mouse hovering
     *
     * Signal emitted when the mouse has stopped moving for the hover period
     * over text area
     *
     * @param lineNo Line number of the text being hovered on
     * @param col Column number in the text line of mouse
     **/
    void hover(lineNumber_t lineNo, int col);

    /**
     * @brief Handle key press event.
     *
     * Signal that a key press which is not handled internally by the widget has
     * occurred.  This allows the application to handle custom keyboard actions.
     *
     * @param evt The Qt QKeyEvent for the key press.
     */
    void keyPress(QKeyEvent *evt);

    /**
     * @brief Signal a scroll lock/unlock
     *
     * The scroll lock is used to prevent scrolling of the text region when new
     * lines are added.  When the @p lock state is true, automatic scrolling is
     * inhibited.  Note scroll locking does not prevent the user from scrolling
     * using mouse or key actions.
     *
     * @param lock New scroll lock state.
     */
    void scrollLockChange(bool lock);

    /**
     * @brief Line trimming has ocurred through line 'lines'
     *
     * This signal is emitted when line trimming has ocurred.  Line triming
     * is the removal of line from the beginning of the buffer to reduce the
     * total number of lines to the 'maxLogLines' setting.
     *
     * The parameter is the number of lines (from line 0) deleted from the
     * head of the line list.  This will allow the application to remove
     * references to the line range, and adjust references to later line
     * numbers by the given amount.  Since the line numbers are contiguous from
     * zero for \p lines , the value of \p lines can be subtracted from any list of
     * line numbers.  Note the any line number less than \p lines has become invalid.
     *
     * For example, if an application keeps a list of bookmarks by line
     * number, on this signal, bookmarks for the designated lines can be
     * deleted, and the remaining bookmark line numbers reduced by the line
     * count.
     *
     * @param lines Number of lines trimmed from the beginning.
     */
    void trimmed(int lines);

    /**
     * @brief signal line height change
     *
     * Signals a chang in line height, due to events such as a change inf font
     * or zoom. Signaled from the repaint code prior to drawing, when the text
     * line height gets recalculated.
     *
     * @param oldHeight former heigh in pixels of a text line
     * @param newHeight newly calculated text line height
     **/
    void lineSpacingChange(int oldHeight, int newHeight);
};


/**
 * @brief Text attributes for a palette entry.
 *
 * Define a logTextPalette entry.  This is an application accessable class to
 * define a logTextPalette entry base style or modifier.  A logTextPaletteEntry is stored
 * in a palette, which can later be actived.
 */
class logTextPaletteEntry {
public: //types
    /** Bitmap attributes which get applied to the base font to form a style entry */
    enum textAttribute {
        attrNone=0,                     //!< no style modifier; default
        attrItalic=0x01,                //!< add italic formatting to font
        attrBold=0x02,                  //!< add bold attribute to base font
        attrUnderline=0x04,             //!< underline
        attrOverLine=0x08,              //!< overline
        attrStrikeOut=0x10};            //!< strike through the font
    Q_DECLARE_FLAGS(textAttributes, textAttribute)

protected:
    QColor m_backgroundColor;           //!< text line background color
    QColor m_clBackgroundColor;         //!< caret line background color
    QColor m_textColor;                 //!< text font color
    textAttributes m_attributes;        //!< font modifier attribute flags

public:
    /**
     * @brief Default constructor.
     *
     * Default constructor, using widget default font, color, and no attributes.
     */
    logTextPaletteEntry();

    logTextPaletteEntry(logTextPaletteEntry const&) = default;

    /**
     * @brief Constructor with full attributes.
     *
     * Constructor with background color, and text color and attributes.
     *
     * @param textColor Text color.
     * @param backgroundColor Background color for normal lines.
     * @param caretBackgroundColor Background color caret line.
     * @param attributes Text attributes.
     **/
    logTextPaletteEntry(const QColor& textColor, const QColor& backgroundColor,
                        const QColor& caretBackgroundColor,
                        textAttributes attributes=attrNone);

    /**
     * @brief Simplified constructor for text color and attributes.
     *
     * Simplification of the full constructor. Sets the text color, and
     * attributes. The background is set to the default active background.
     *
     * @param textColor  Text color.
     * @param attributes Text attributes.
     **/
    explicit logTextPaletteEntry(const QColor& textColor,
                        textAttributes attributes=attrNone);

    /**
     * @brief Get background color.
     *
     * Return the current defined background color.
     *
     * @return Current background color.
     **/
    QColor backgroundColor() const;

    /**
     * @brief Get background color for caret line
     * @return Color for the background of the caret line
     */
    QColor caretLineBackgroundColor() const;

    /**
     * @brief Set background color.
     *
     * Setter function to set the background color.
     *
     * @param c New color for the background.
     **/
    void setBackgroundColor(const QColor& c);

    void setCaretLineBackgroundColor(const QColor& c);

    /**
     * @brief Get text color.
     *
     * Get the currently defined text color.
     *
     * @return The defined text color.
     **/
    QColor textColor() const;

    /**
     * @brief Set text color.
     *
     * Setter function to define the text color of a palette entry.
     *
     * @param c New text color for the style.
     **/
    void setTextColor(const QColor& c);

    /**
     * @brief Get palette item attributes.
     *
     * Get the current set of attribute bits.
     *
     * @return Attribute bits for this logTextPaletteEntry.
     **/
    textAttributes attributes() const;

    /**
     * @brief Set palette item attributes.
     *
     * Set the state of the attributes for this logTextPaletteEntry.  Whether a bit affects
     * the final style is controlled by the mask.  See also @a setAttributes().
     *
     * @param a New attribute state for this entry.
     **/
    void setAttributes(logTextPaletteEntry::textAttributes a);
};
Q_DECLARE_OPERATORS_FOR_FLAGS(logTextPaletteEntry::textAttributes)
Q_DECLARE_TYPEINFO(logTextPaletteEntry, Q_MOVABLE_TYPE);        //!< Declare as movable type for Qt containers

/** Implementation of logTextPalette */
class logTextPalette {
private:
    QString name;               //!< Palette name
    QVector<logTextPaletteEntry> styles; //!< array of styles
    
#ifndef DOXYGEN_EXCLUDE         // Exclude from DOXYGEN generation
    logTextPalette(logTextPalette&) = delete;
    logTextPalette& operator=(logTextPalette&) = delete;
#endif  // DOXYGEN_EXCLUDE
    
public:
    /**
     * @brief Constructor with default colors.
     *
     * Create a palette, using the specified background and foreground colors as
     * the initial value for all entries.
     *
     * @param name Name assigned to the new palette.
     * @param numEntries Number of styles to allocate.
     * @param textColor Default foreground color for all style entries.
     * @param bgColor Default background color for all style entries.
     **/
    logTextPalette(const QString& name, int numEntries, QColor const& textColor,
                   QColor const& bgColor, QColor const& clBgColor);
    
    /**
     * @brief Copy constructor.
     *
     * Copy constructor. Create a clone of another palette.
     *
     * @param name Name to assign to the clone.
     * @param source Pointer to the palette to clone.
     */
    logTextPalette(const QString& name, logTextPalette const& source);

    /**
     * Get a reference to a specified palette style entry.
     *
     * @param id Style number in the palette.
     * @return Reference to a style entry.  If the style number @p s is out of
     *         range, a reference to the last entry is returned.
     **/
    logTextPaletteEntry const& style(styleId id) const;
    
    /**
     * Returns the (compile time defined) number of styles in a palette.
     *
     * @return Number of styles available in a palette.
     **/
    int numStyles() const {return styles.count();}
};

#endif // WLOGTEXT_H
