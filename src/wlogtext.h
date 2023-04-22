/*
 * Copyright (c) 2008-2023, Rick Wagner. All rights reserved.
 */

/** @file wlogtext.h Definition of the wLogText widget class and API. **/

#ifndef WLOGTEXT_H
#define WLOGTEXT_H

#include <climits>
#include <cstdint>
#include <optional>
#include <vector>

#include <QAbstractScrollArea>
#include <QColor>
#include <QElapsedTimer>
#include <QFlags>
#include <QPoint>
#include <QPixmap>
#include <QDate>

using styleId_t = uint16_t;        //!< Style id within a palette
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
 * A class to represent a character cell position of line and column. It has the notion
 * screen positional comparison, i.e.
 * if ( cell_a \< cell_b ). Cell_a \< cell_b if:
 *     ((cell_a.line() \< cell_b.line) ||
 *      ((cell_a.line() == cell_b.line()) && cell_a.col() \< cell_b.col()) );
 **/
class cell {
private:
    lineNumber_t m_line = 0;
    int m_col = 0;

public:
    cell() = default;                                  //!< Default constructor
    cell(int l, int c) : m_line{l}, m_col(c) {};   //!< Constructor from line, column
    cell(const cell& p) = default;   //!< Copy constructor

    auto operator = (cell const& other) -> cell& = default;

    [[deprecated("Use cell(cell&)")]] explicit cell(const QPoint& p) : cell(p.y(), p.x()) {};   //!< Copy contructor
    [[deprecated("Use cell(cell&)")]] explicit operator QPoint() {return QPoint(columnNumber(), lineNumber());}

    /**
     * Return the line number represented by this cell.
     * @return Line number of this cell
     **/
    auto lineNumber() const -> decltype(m_line) {return m_line;}    //!< Return line number represented by a cell.

    /**
     * Set the line position represented by this cell.
     * @param l New line number for this cell.
     **/
    auto setLineNumber(lineNumber_t l) -> void {m_line = l;}                //!< Set the line number this cell represents

    /**
     * Return the column number represented by this cell.
     * @return column number of this cell
     **/
    auto columnNumber() const -> decltype(m_col) {return m_col;}    //!< Return the character column this cell represents

    /**
     * Set the column position represented by this cell.
     * @param c New column number for this cell.
     */
    auto setColumnNumber(int c) -> void {m_col = c;}         //!< Set the character column this cell represents

    auto manhattanLength() const -> int {return ::abs(lineNumber()) + ::abs(columnNumber());}

    /**
     * Set the line and column position represented by this cell.
     * @param l New line number for this cell.
     * @param c New column number for this cell.
     **/
    auto setPos(lineNumber_t l, int c) -> void {setLineNumber(l); setColumnNumber(c);}  //!< Set the cell location

    /**
     * Returns a cell one line after this.  Note, the position strictly mathematical, there is no
     * validation against any document structure.
     * @return A \c cell position one line down from this.
     **/
    [[nodiscard]]
    auto nextLine() const -> cell {return {lineNumber() + 1, columnNumber()};}  //!< Return a cell one line after this

    auto advanceLine(lineNumber_t inc = 1) -> void {setLineNumber(lineNumber() + inc);}

    /**
     * Returns a cell one column after this.  Note, the position strictly mathematical, there is no
     * validation against any document structure.
     * @return cell location of next column.
     **/
    [[nodiscard]]
    auto nextCol() const -> cell {return {lineNumber(), columnNumber() + 1};}     //!< Return a cell one column after this
    auto advanceColumn(int inc = 1) -> void {setColumnNumber(columnNumber() + inc);}

    /**
     * @brief compare to cells for equality
     *
     * @param other @c cell to compare to
     * @return @c true if the cells are equal
     */
    auto operator == (cell const& other) const {
        return lineNumber() == other.lineNumber() && columnNumber() == other.columnNumber();}

    /**
     * Cell relative position comparison.
     * @param c Cell to compare this location to.
     * @return returns <, ==, or > based on line, then cell
     **/
    auto operator <=> (cell const& other) const {
        return lineNumber() == other.lineNumber() ?
                columnNumber() <=> other.columnNumber() : lineNumber() <=> other.lineNumber();}

    auto swap(cell &other) noexcept -> void {std::swap(m_line, other.m_line); std::swap(m_col, other.m_col);}

    [[nodiscard]]
    auto operator +(cell const& other) const ->cell{
        return {lineNumber() + other.lineNumber(), columnNumber() + other.columnNumber()};}

    [[nodiscard]]
    auto operator -(cell const& other) const ->cell {
        return {lineNumber() - other.lineNumber(), columnNumber() - other.columnNumber()};}

    [[nodiscard]]
    auto abs() const ->cell {return {::abs(lineNumber()), ::abs(columnNumber())};}
    [[nodiscard]] friend auto abs(cell c) {return c.abs();}

    auto operator +=(cell const& other) -> cell& {
        setLineNumber(lineNumber() + other.lineNumber());
        setColumnNumber(columnNumber() + other.columnNumber());
        return *this;}
};
template<> inline void std::swap<cell>(cell &x, cell &y) noexcept {x.swap(y);}
QDebug operator<<(QDebug dbg, cell const& c);


/* structured bindings API */
template<> struct std::tuple_size<cell> {static constexpr int value = 2;};
template<size_t N> struct std::tuple_element<N, cell>;
template<> struct std::tuple_element<0, cell> {using type = decltype(cell{}.lineNumber());};
template<> struct std::tuple_element<1, cell> {using type = decltype(cell{}.columnNumber());};
template<int N> auto get(cell const& c) {
    if constexpr(N == 0) return c.lineNumber(); else return c.columnNumber();
}

/**
 * @brief a class representing a region of cells
 * A class representing a start and end-point of a region. The raw first and
 * second may be a reverse range if the source selection is from bottom/right to
 * top/left. The @c normalize() and @c normalized() functions will assure that
 * @c first() precedes @c second().
 */
class region {
    cell m_first;
    cell m_second;

public:
    region() = default;
    region(cell first, cell second) : m_first{first}, m_second{second} {}
    region(region const&) = default;
    auto operator =(region const&) -> region& = default;

    /**
     * test for an empty region
     * @return @c true if start == end
     */
    [[nodiscard]]
    auto empty() const {return m_first == m_second;}

    /**
     * @brief get the start of the selection
     * Returns the start (not necessarily the top) of the selection region
     * @return @c cell of the selection start
     */
    [[nodiscard]]
    auto first() const {return m_first;}

    /**
     * @brief assure first() precedes second()
     * Assures that first() precedes second(), tat is that the line/column of first
     * < the line/column of second.
     */
    auto normalize() {if (m_second < m_first) std::swap(m_first, m_second);}

    /**
     * returns a copy of this region, assuring it is normalized
     * @return a copy of this, assuring first() precedes second()
     */
    [[nodiscard]]
    auto normalized() const {region t{m_first, m_second}; t.normalize(); return t;}

    auto reverse() {std::swap(m_first, m_second);}

    /**
     * @brief get the end of the selection
     * Returns the end (not necessarily the bottom) of the selection region
     * @return @c cell of the selection end
     */
    [[nodiscard]]
    auto second() const {return m_second;}

    /**
     * @brief test if first() and second() have the same line number
     * Test if the region is not empty, and first line number equals the
     * second line number.
     * @return @c true if line number of first is the same as second
     */
    [[nodiscard]]
    auto singleLine() const {return !empty() && m_first.lineNumber() == m_second.lineNumber();}

    /**
     * @brief swap this region with another
     *
     * @param other @c region to swap with
     */
    auto swap(region &other) {
        std::swap(m_first, other.m_first); std::swap(m_second, other.m_second);}

    /**
     * @brief simple approximation of the distance from first() to second()
     * @return sum of the difference of line numbers and column numbers
     */
    auto manhattanLength() const {return (m_second - m_first).manhattanLength();}

    auto operator ==(region const& other) const {
        return first() == other.first() && second() == other.second();}

    auto operator +(cell const& offs) const -> region {return {first() + offs, second() + offs};}
    auto operator +=(cell const& offs) ->region& {m_first += offs; m_second += offs; return *this;}

    [[nodiscard]] auto span() const -> cell {return first() - second();}
};
template<> inline void std::swap<region>(region &x, region &y) noexcept {x.swap(y);}
QDebug operator<<(QDebug dbg, region const& r);

/* structured bindings API: */
template<> struct std::tuple_size<region> {static constexpr int value = 2;};
template<size_t N> struct std::tuple_element<N, region> {using type = cell;};
template<int N> auto get(region const& r) -> decltype(auto) {
    if constexpr(N == 0) return r.first(); else return r.second();}

/**
 * Class to define a line item for to be displayed in a logText widget.
 * It allows access to the text, attributes, and positional information of a
 * text line.
 *
 * This class should not be instantiated by an application.  Creation and
 * access should be done through a logText object.
 *
 * Many attributes are accessible from the object, and some are settable.
 * However, in most cases an object should be modified through the logText
 * object, so that the display will reflect changes in the item.
 */

class logTextItem {
    friend class wLogText;

private:
    QString m_text;                     //!< actual line text
    qint16 m_pixmapId = -1;             //!< ID within the pixmap palette for the gutter pixmap
    styleId_t m_styleId = 0;            //!< style ID within the active palette to paint this line

public:
    /**
     * Constructor with attributes.
     *
     * @param text Text of string to be displayed.
     * @param styleNo Style number of the base.
     **/
    logTextItem(const QString& text, styleId_t styleNo) :
            m_text(text), m_styleId(styleNo) {}

    logTextItem(QString&& text, styleId_t styleNo) :
            m_text(std::move(text)), m_styleId(styleNo) {}

    logTextItem& operator =(logTextItem const&) = delete;
    logTextItem& operator =(logTextItem&&) noexcept = default;

    /**
     * Copy constructor.
     * @param source Object to copy from.
     */
    logTextItem(const logTextItem&) noexcept = default;
    logTextItem(logTextItem&&) noexcept = default;

    virtual ~logTextItem() = default;

    static auto newItem(const QString& text, styleId_t styleNo) {return new logTextItem{text, styleNo};}
    static auto newItem(QString&& text, styleId_t styleNo) {return new logTextItem{std::move(text), styleNo};}

    /**
     * Get the style for a logText line.
     *
     * @return Style number applied to this text.
     **/
    auto styleId() const ->styleId_t {return m_styleId;}

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
    auto setStyleId(styleId_t styleNo) -> void {m_styleId = styleNo;}

    /**
     * Get the text of the log item.
     *
     * @return const reference to the logTextItem text string.
     **/
    auto text() const ->QString const& {return m_text;}

    /**
     * Set a pixmap on a logTextItem.
     *
     * The pixmap is displayed in the text gutter, if enabled.  It is the
     * applications responsibility to ensure the gutter width, line height and
     * pixmap sizes are compatible.
     *
     * @param pm Pixmap ID in logtext to attach to the logTextItem.
     **/
    auto setPixmap(int pm) {m_pixmapId = pm;}

    /**
     * Remove the pixmap selector from this text item
     **/
    auto clearPixmap() {m_pixmapId = -1;}

    auto hasPixmap() const {return m_pixmapId != -1;}
    auto pixmapId() const {return m_pixmapId;}
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
 * A class derived and implemented by an application to action performed an
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
    virtual ~logTextItemVisitor() = default;

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
    virtual auto visit(const visitedItem& item) -> bool=0;
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
 * The textual elements are accessible, eliminating the need to maintain
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
 * Each modifier bit position represents a single style modification.  The same
 * modifier bit position will apply the same effect to any base style (within
 * limits of physical representations and conflicts).
 *
 * So for example, suppose style 0 defines black text, and style 1 blue; and
 * modifier 0 defines underlining.  If line one has selected syle 0, and line
 * two selects style 1 then line one is displayed with black text, and line two
 * with blue.  If then modifier0 is applied to both lines, they will maintain
 * their base colors, but will both have underlining also applied.
 *
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
    // (QScrollView, QWidget, QObject).  Thus they cannot move to wLogTextPrivate.
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
    auto contextMenuEvent(QContextMenuEvent *event) -> void override;

    /**
     * @brief Handle QWidget custom events from the Qt event loop. [virtual, inherited]
     *
     * Inherited from QWidget, handles QEvent events posted to the event
     * queue.  This reimplentation is for signaling the request to update the
     * display when requested by setUpdatesNeeded.
     *
     * @param event The Qt custom event object.
     */
    auto customEvent(QEvent *) -> void override;

    /**
     * @brief Handle QWidget focusInEvent. [virtual, inherited]
     *
     * Inherited from QWidget, handles notification of the widget receiving focus.
     *
     * @param event QFocusEvent object passed from QWidget.
     */
    auto focusInEvent(QFocusEvent *event) -> void override;

    /**
     * @brief Handle QWidget focusOutEvent. [virtual, inherited]
     *
     * Inherited from QWidget, handles notification of the widget losing focus.
     *
     * @param  event QFocusEvent object passed from QWidget.
     */
    auto focusOutEvent(QFocusEvent *event) -> void override;

    /**
     * @brief Handle key press event.
     *
     * Handle keyboard key press events.
     *
     * @param event QKeyEvent describing the event.
     */
    auto keyPressEvent (QKeyEvent *event) -> void override;

    /**
     * @brief Mouse double-click event handler. [virtual, overloaded: QAbstractScrollArea]
     *
     * Function called when a double click in the visible region occurs.  Emits a
     * signal based on whether the event occurs in the text area, or the gutter.
     *
     * @param event event describing the mouse position and state.
     *
     */
    auto mouseDoubleClickEvent (QMouseEvent* event) -> void override;

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
    auto mouseMoveEvent(QMouseEvent *event) -> void override;

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
    auto mousePressEvent(QMouseEvent *event) -> void override;

    /**
     * @brief Handle mouse button release event.
     *
     * Handle mouse button up.
     *
     * @param event QMouseEvent for the mouse release.
     */
    auto mouseReleaseEvent(QMouseEvent *event) -> void override;

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
    auto paintEvent(QPaintEvent *event) -> void override;

    /**
     * @brief translate an event QPoint position to character position
     *
     * Translates a QPoint position from an event, such as from mouseEvent(),
     * to a @c cell{} (line and column numbers) position. The position is
     * absolute relative to all the items, not relative to the display area.
     *
     * @param point mouse point from the event
     * @return character position within the virtual display
     */
    auto pointToCell(QPoint const& point) const -> cell;

    /**
     * @brief Visible region size changed event. [virtual, overloaded:
     * QAbstractScrollArea]
     *
     * Event handler for resize events.  Adjusts variables which depend upon the
     * view area size.
     *
     * @param event Resize event data.
     */
    auto resizeEvent(QResizeEvent *event) -> void override;

    /**
     * @brief Reimplementation of QObject::timerEvent [virtual]
     *
     * Reimplementation of QObject::timerEvent.  It is called when a timer created
     * by a QObject::startTimer expires.
     *
     * @param event A QTimerEvent for the fired timer.
     */
    auto timerEvent(QTimerEvent *event) -> void override;

    /**
     * @brief Handle mouse wheel event.
     *
     * Handle a mouse wheel event.  This will scroll the display, in various
     * increments, or zoom the font size in or out, depending on the state of the
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
    auto wheelEvent(QWheelEvent *event) -> void override;

    /**
     * @brief recalculate the horizontal scrollbar limits
     *
     * Called when the character width or max line length changes, to
     * recalculate the horizontal scroll bad limits.
     **/
    auto adjustHorizontalScrollBar() const -> void;

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
    auto setUpdatesNeeded(updateLevel level) -> void;

    /**
     * Remove lines in excess of maximumLogLines from the top of the widget
     **/
    auto trimLines() -> void;

    auto updateFontMetrics(int lineHeight, int charWidth) -> void {Q_EMIT(fontMetricsChanged(lineHeight, charWidth));}

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
     * activated, the styles are rendered from this base.
     * @return Font base for the style
     **/
    auto font() const -> QFont const&;

    /**
     * @brief Interline spacing, in pixels.
     *
     * Returns the active line height, in scrollView units (pixels).  The height
     * includes adjustment for font zooming.
     *
     * @return Height of a text line, in scrollView units.
     */
    auto lineHeight() const -> int;

    /**
     * @brief Text zoom factor
     *
     * Integer increment (+/-) in points from the base font (@see font()) when
     * the text is zoomed in or out.
     * @return Zoom factor in +/- points from base. Returns 0 when zoom is set to normal.
     **/
    auto fontZoom() const -> int;

    /**
     * @brief Returns the number of characters in a line.
     *
     * Return the length in characters of line @p lineNo.
     *
     * @param  lineNumber Line number of which to get the length.
     * @return length of the line if @p lineNumber is a valid line number; else 0.
     **/
    auto length(lineNumber_t lineNumber) const -> int;

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
    auto lineCount() const -> lineNumber_t {return m_lineCount;}

    /**
     * @brief get maxLogLines property
     *
     * Returns the maxLogLines property configured for the widget. The property
     * can be set via setMaxLogLines.
     *
     * To prevent excessive trimming, the widget maintains a "slack", which is
     * an additional number of lines which can be in the widget. So appending a
     * line checks against the slacked value, and once the slack limit is
     * reached, the lines will be trimmed down to the maxLogLine() value. Thus
     * it is possible for lineCount() to return a value (and the widget to
     * display ) greater then maxLogLines() lines of text.
     *
     * @return Configured maximum number of lines kept by the widget.
     * @sa wLogText::setMaxLogLines().
     **/
    auto maxLogLines() const -> lineNumber_t;

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
    auto setUpdatesEnabled(bool newState) -> void;

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
    auto append(logTextItem *item) {
        m_maxLineChars = std::max(m_maxLineChars, item->m_text.length());
        items.push_back(item);
        setUpdatesNeeded(updateCond);

        if (maximumLogLines && (m_lineCount >= maximumLogLinesSlacked))
            trimLines();
        return m_lineCount++;
    }

    auto append(QString const& text, styleId_t styleNo) {
        return append(logTextItem::newItem(text, styleNo));}

    auto append(QString&& text, styleId_t styleNo) {
        return append(logTextItem::newItem(std::move(text), styleNo));}

    /**
     * @brief Erase a range of lines
     *
     * Erases the lines from line number @p top for @p count lines. No signal is
     * emitted in response to this call.
     * @param top Line number to start erasure
     * @param count Number of lines from @p top to clear
     **/
    auto clear(int top, int count) -> void;

    /**
     * @brief Check validity of a line number
     *
     * Verifies that the line number @p lineNo is a valid line number, i.e.
     * >= 0 and < numberOfLines
     *
     * @param lineNo Line number to validate
     * @return @c true if @p lineNo is a valid line number.
     **/
    auto validLineNumber(lineNumber_t lineNo) const {
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
    auto item(lineNumber_t lineNo) const -> logTextItem const* {
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
     * current palette is reactivated.  Reactivating the current palette is useful
     * when the palette styles have changed since the last activation.
     *
     * @param name Name of palette, allocated by @a newPalette(), to make active.
     * @return true if the activation succeeded; else false;
     */
    auto activatePalette(const QString& name= QString()) -> bool;

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
    auto createPalette(size_t size, const QString& name) ->logTextPalette *;

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
    auto createPalette(const QString& name, const logTextPalette *source) ->logTextPalette *;

    /**
     * @brief Return a pointer to a named palette.
     *
     * If the palette named exits, return a pointer to it, else return 0.
     *
     * @param name  Name of the palette to get.
     * @return Pointer to the named palette.  Zero if it does not exist.
     */
    auto palette(const QString& name) const ->logTextPalette*;

    /**
     * @brief Delete a named palette.
     *
     * Deletes the named palette.  If the named palette is the active palette, the
     * default palette is made active.  If the named palette is the default palette,
     * the operation is ignored.
     *
     * @param name Name of the palette to delete.
     */
    auto deletePalette(const QString& name) -> void;

    /**
     * @brief Return the name of active palette.
     *
     * Returns the name of the currently active palette.
     *
     * @return Name of the active palette.
     */
    auto activePaletteName() const ->QString;

    /**
     * @brief Return the name of default palette.
     *
     * Returns the name of the default palette.
     *
     * @return Name of the active palette.
     */
    auto defaultPaletteName() const ->QString;

    /**
     * @brief Get a list of palette names.
     *
     * Returns a list of the currently defined palette names.
     *
     * @return String list of palette names.
     */
    auto paletteNames() const ->QStringList;

    /**
     * @brief Return the cell (line/column) of the caret.
     *
     * Returns the current cursor position, in the form of line, column.
     *
     * @return @c cell caret position
     */
    auto caretPosition() const ->cell;

    /**
     * Set the current cursor position, in the form of line, column.  If line is
     * negative, the position is counted back from the last line.  If column is
     * negative the position counted back from the end of line.  The position does
     * not wrap, so if a column number is more negative than the length of the line,
     * the new position is the beginning of the line, not wrapped back on the
     * previous line.
     *
     * @param lineNumber New line number for cursor position.
     * @param columnNumber New column position.
     **/
    auto setCaretPosition(lineNumber_t lineNumber, int columnNumber) -> void;

    /**
     * @brief Set the caret to a specified line, column coordinate.
     *
     * Set the current cursor position from a cell. p.y is line, p.x=col.
     * If line is negative, the new position is "line" lines from the end.  If col
     * is negative, the new column position is "col" characters from the end.
     *
     * To move to the end of the text, use a QPoint(MAXINT, MAXINT). (MAXINT is
     * usually defined in values.h).
     *
     * @param p New cursor position.
     **/
    auto setCaretPosition(const cell&) -> void;

    /**
     * @brief Get state of showCaret flag.
     *
     * Gets the state of the show caret flag.
     *
     * @return true if the caret will be shown.
     **/
    auto showCaret() const -> bool;


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
     * @param options KFind::Options
     * @return a @c std::optional; if not set, the search failed. If set, the
     * contained @c cell holds the location of the found text
     */
    auto find(QString const& str, long options=0) ->std::optional<cell>;

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
     * @param pt Pointer to @c cell giving cell location (line, column)
     * to begin the search.
     *
     * @param caseSensitive True if the search is case sensitive.
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
    auto find(const QString& str, cell *pt,
              Qt::CaseSensitivity=Qt::CaseInsensitive, bool forward=true) -> bool;

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
     * @param caseSensitive True if the search is case sensitive.
     *
     * @param forward If true, the search is from the start position to the end of
     * text; else to beginning of text.
     *
     * @param line Pointer to the line to start search, and to where the
     * line number of a successful search is put.
     *
     * @param col Pointer to the column number in the line to start the
     * search, and where the column number of a successful search is put.
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
    auto find(const QString&, Qt::CaseSensitivity=Qt::CaseInsensitive,
              bool forward=true, lineNumber_t *line=nullptr, int *col=nullptr) -> bool;

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
    auto find(QRegularExpression const& re, cell *at, bool forward = true) -> bool;

    /**
     * @brief Ensure the caret is visible, scrolling if needed.
     *
     * Ensure that the caret position is visible, scrolling the visible region if
     * needed.
     **/
    auto ensureCaretVisible() -> void;

    /**
     * @brief Check for text selection.
     *
     * Checks for having selected text.
     *
     * @return Returns @c TRUE if some text is selected; otherwise returns @c FALSE.
     **/
    auto hasSelectedText() const -> bool;

    /**
     * @brief Get coordinates of the selection.
     *
     * If there is a selection, *lineFrom is set to the number of the line in
     * which the selection begins and *lineTo is set to the number of the line
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
     * @deprecated use @c getSelection()
     **/
    [[deprecated("use: region getSelection()")]]
    auto getSelection(lineNumber_t *lineFrom, int *colFrom,
                      lineNumber_t *lineTo, int *colTo) const -> void;
    /**
     * @brief get the region of the selected text
     * Returns a region describing the currently selected text. If there is
     * no selection, the returned @c region::empty() returns @c true.
     *
     * @Note: If the underlying selection is a reverse selection, that is from
     * bottom right to top left, the returned region may not be 'normalized'. See
     * @c regions::normalize().
     */
    auto getSelection() const -> region;

    /**
     * @brief return all lines as a single string, with new-line separations.
     * @return A new-line separated string of all lines.
     */
    auto toPlainText(QLatin1Char sep = QLatin1Char('\n')) const -> QString;

    /**
     * @brief Returns a single string of text selection.
     *
     * Get the selected text as a single string.  If the selection spans more than
     * a line, the lines are separated with a new line ('\\n') character.
     *
     * @return Selection region, concatenated with new-line separation.
     **/
    auto selectedText() const -> QString;

    /**
     * @brief Get the current mouse hover delay time.
     *
     * Returns the value set after adjustments by \c setHoverTime.
     * \see setHoverTime
     *
     * @return hover time in milliseconds
     **/
    auto hoverTime() const {return m_hoverTime;}

    /**
     * @brief Set time for mouse hover signal
     *
     * Set the time in milliseconds for a mouse move to trigger a hover signal.
     * A value of <= 0 shuts off the hover notifications. The value is rounded up
     * to a multiple of 50mS, with a maximum of 2500mS.
     *
     * @param t Mouse hover time in milliseconds.
     **/
    auto setHoverTime(int t) -> void;

    /**
     * @brief Return current state of scroll lock.
     *
     * Return current state of scroll lock
     *
     * @return Current scroll lock state.
     */
    [[nodiscard]]
    auto scrollLock() const -> bool;

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
    auto escJumpsToEnd() const -> bool;

    /**
     * @brief set behavior of escape key press when the view is scroll-locked
     *
     * Set the mode indicating whether pressing the escape key will release a
     * scroll lock, and scroll the view to the bottom line.
     *
     * @param jump new state for the escJumpsToEnd property
     **/
    auto setEscJumpsToEnd(bool jump) -> void;

    /**
     * @brief Returns the current gutter width.
     *
     * Gets the width of the gutter.  This space does not include the gutter
     * border, if any.
     *
     * @return Width in pixels of the gutter.  The vertical space per line is the
     *  same as the text height, @a lineHeight().
     */
    auto gutter() const -> int;

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
    auto setPixmap(int pixmapId, QPixmap const& pixmap) -> void;
    auto setPixmap(int pixmapId, QPixmap&& pixmap) -> void;

    /**
     * @brief unmap a pixmap ID
     *
     * Removes the mapping assigned in wLogText::setPixmap for the ID.
     *
     * @param pixmapId pixmap ID to unmap
     **/
    auto clearPixmap(int pixmapId) -> void;

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
    auto setLinePixmap(lineNumber_t lineNo, int pixmapId) -> void;

    /**
     * @brief Remove a pixmap for the line
     *
     * Remove the pixmap (if any) assigned to the specified line.
     *
     * @param lineNo Line number of line on which to remove the pixmap.
     */
    auto clearLinePixmap(lineNumber_t lineNo) -> void;

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
    auto setLineStyle( lineNumber_t line, int style) -> void;

    /**
     * @brief Apply a function to items in the item list.
     *
     * Walks the list of all the @c logTextItem items in the widget, and applies
     * the @c logTextItemVisitor::visit method to each item.
     *
     * The logTextItemVisitor::visit() function is applied to each line from line
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
    auto visitItems(logTextItemVisitor &v, lineNumber_t firstLine=0) -> void;

    /**
     * @brief Apply a function to items in the item list.
     *
     * Walks the list of all the @c logTextItem items in the widget, and applies
     * the @p op method to each item.
     *
     * The logTextItemVisitor::visit() function is applied to each line from the
     * first line of the view, until visit() returns false or the last list
     * has visited reached.
     *
     * Updates are disabled across all visitations, and returns to the
     * same enabled state as on entry. Thus updates will not be shown until all
     * the visitations are complete, or longer if updates were disabled on entry.
     *
     * @param op Visitor function to be applied to each item.
     */
    auto visitItems(bool (*op)(const logTextItemVisitor::visitedItem&)) -> void;


    /**
     * @brief Apply a function to all selected lines in the item list.
     *
     * Walks the list of all the \a logTextItem items in the selection range, and applies
     * the \a logTextItemVisitor::visit method to each item.
     *
     * The visit() function is applied from the first line of the selection range,
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
    auto visitSelection(logTextItemVisitor &v) -> void;
    auto visitSelection(bool (*v)(const logTextItemVisitor::visitedItem& item)) -> void;

protected Q_SLOTS:
    /**
     * @brief slot called when the vertical scrollbar scroll position is changed
     * @param value new scrollbar position value
     **/
    auto vScrollChange(int value) -> void;

    /**
     * @brief slot called when the horizontal scrollbar scroll position is changed
     * @param value new scrollbar position value
     **/
    auto hScrollChange(int value) -> void;

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
    auto setMaxLogLines(lineNumber_t mll) -> void;

    /**
     * @brief Clear display and delete all line d->items.
     *
     * Deletes the line items, and clears the display area.  Resets sizes and
     * counts apropriately.
     */
    auto clear() -> void;

    /**
     * @brief advisory that no new items are expected
     *
     * Call to indicate that no new items are expected, so resources can be
     * released. The call is an expectation, however, new items may be received.
     * The view is not obligated to display them, but must remain stable (not
     * crash) if any new lines are received after finalize(). Note that accesses
     * and line deletions are still possible.
     */
    auto finalize() -> void;

    /**
     * @brief Assign a new fixed-pitch base font.
     *
     * Sets the base font to the requested font. The requested font must be a
     * fixed pitch font. If it is not, then the request is ignored, and no
     * changes occur.
     *
     * @param font New fixed pitch QFont to use for text display.
     **/
    auto setFont(QFont font) -> void;

    /**
     * @brief Increment the font magnification.
     *
     * Increments the displayed font zoom increment by one point. Same as doing
     * setFontZoom(fontZoom() + 1);
     **/
    auto enlargeFont() -> void;

    /**
     * @brief Decrement the font magnification.
     *
     * Decrement the displayed font zoom increment by one point. Same as doing
     * setFontZoom(fontZoom() - 1), when fontZoom > 2;
     **/
    auto shrinkFont() -> void;

    /**
     * @brief Set the mouse zoom increment.
     *
     * Set the font zoom increment, in points.  The displayed text is incremented
     * (zoom > 0) or decremented (zoom < 0) by the specified number of points.
     *
     * @param zoom Font size increment in points.
     */
    auto setFontZoom(int zoom) -> void;

    /**
     * @brief Reset font magnification.
     *
     * Resets the displayed text size to the base font size.
     **/
    auto resetFontZoom() -> void;

    /**
     * @brief Select all text.
     *
     * Select all the text.  Selected text is copied to the global clipboard. Emits
     * a copyAvailable(true) signal.
     **/
    auto selectAll() -> void;

    /**
     * @brief Remove selection
     *
     * Unselect the text selection, and removes the text from the global clipboard.
     * Emits a copyAvailable(false) signal.
     *
     **/
    auto clearSelection() -> void;

    /**
     * @brief Copy selection to the clipboard.
     *
     * Request to copy the current selection, if there is one, to the clipboard.  If
     * there is no current selection, no action is taken.
     */
    auto copy() const -> void;

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
    auto setGutter(int) -> void;

    /**
     * @brief Set scroll lock state.
     *
     * Set the scroll lock state.  Emits a scrollLockChange if the state changes.
     *
     * @param state New scroll lock state.
     **/
    auto setScrollLock(bool state) -> void;

    /**
     * @brief Set whether the caret will be shown.
     *
     * Sets the show caret flag.
     *
     * @param show New state of the show caret flag.
     **/
    auto setShowCaret(bool show) -> void;

Q_SIGNALS:
    /**
     * @brief Signal click at line/column position.
     *
     * This signal is emitted when a click in the text area occurs.
     *
     * @param event Mouse event context.
     */
    auto clicked(QMouseEvent *event) -> void;

    /**
     * @brief Context menu request in log body.
     *
     * Signal the request for a context menu (i.e. right mouse click) in the text
     * area.
     *
     * @param lineNo The text line number the event occurred on.  This is the line
     *          number relative to the start of the text, not the start of the
     *          display area.
     * @param globalPos The @c QPoint global location of the mouse pointer when the click occurred.
     * @param event Mouse event context.
     */
    auto contextClick(lineNumber_t lineNo, QPoint globalPos, QContextMenuEvent *event) -> void;

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
    auto copyAvailable(bool avail) -> void;

    /**
     * @brief Double click at line/column position.
     *
     * A mouse button double click in the text area.  While the parameter value
     * is a QPoint, internally it is a cell, which represents a line and column pair.
     *
     * @param cell The line (\p cell.y()) and column (\p cell.x()) the event
     *          occurred on.
     */
    auto doubleClicked(cell cell) -> void;

    /**
     * @brief notification when font metrics have changed
     *
     * Called when the font character width and/or line spacing have changed.
     * This happens in a (initial) paint event after the font has changed.
     * Note that after a font change, the font metrics are not knowable until
     * a paint event to which the font can be attached.
     *
     * @param lineHeight line-to-line spacing
     * @param charWidth fixed width of a single character
     */
    auto fontMetricsChanged(int lineHeight, int charWidth) -> void;

    /**
     * @fn void wLogText::gutterClick(int lineNo, ButtonState btn, QPoint point)
     * @brief Click in gutter of for a line.
     *
     * @param lineNo The text line number the event occurred on.  This is the line
     *          number relative to the start of the text, not the start of the
     *          display area.
     * @param event Mouse event context.
     */
    auto gutterClick(lineNumber_t lineNo, QMouseEvent *event) -> void;

    /**
     * @fn void wLogText::gutterContextClick(int lineNo, ButtonState btn, QPoint point)
     * @brief Context menu request in the gutter of line.
     *
     * @param lineNo The text line number the event occurred on.  This is the line
     *          number relative to the start of the text, not the start of the
     *          display area.
     * @param globalPos The @c QPoint global location of the mouse pointer when the click occurred.
     * @param event Event context
     */
    auto gutterContextClick(lineNumber_t lineNo, QPoint globalPos, QContextMenuEvent* event) -> void;

    /**
     * @brief Double-click in gutter of line.
     *
     * @param lineNo The text line number the event occurred on.  This is the line
     *          number relative to the start of the text, not the start of the
     *          display area.
     */
    auto gutterDoubleClicked(lineNumber_t lineNo) -> void;

    /**
     * @brief signal mouse hovering
     *
     * Signal emitted when the mouse has stopped moving for the hover period
     * over text area
     *
     * @param lineNo Line number of the text being hovered on
     * @param col Column number in the text line of mouse
     **/
    auto hover(lineNumber_t lineNo, int col) -> void;

    /**
     * @brief Handle key press event.
     *
     * Signal that a key press which is not handled internally by the widget has
     * occurred.  This allows the application to handle custom keyboard actions.
     *
     * @param evt The Qt QKeyEvent for the key press.
     */
    auto keyPress(QKeyEvent *evt) -> void;

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
    auto lineSpacingChange(int oldHeight, int newHeight) -> void;

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
    auto scrollLockChange(bool lock) -> void;

    /**
     * @brief Line trimming has occurred through line @p 'lines'
     *
     * This signal is emitted when line trimming has occurred.  Line trimming
     * is the removal of lines from the beginning of the buffer to reduce the
     * total number of lines to the 'maxLogLines' setting.
     *
     * The parameter is the number of lines (from line 0) deleted from the
     * head of the line list.  This will allow the application to remove
     * references to the line range, and adjust references to later line
     * numbers by the given amount.  Since the line numbers are contiguous from
     * zero for \p lines , the value of \p lines can be subtracted from any list of
     * line numbers. Any line number less than \p lines has become invalid.
     *
     * For example, if an application keeps a list of bookmarks by line
     * number, on this signal, bookmarks for the designated lines can be
     * deleted, and the remaining bookmark line numbers reduced by the line
     * count.
     *
     * @param lines Number of lines trimmed from the beginning.
     */
    auto trimmed(int lines) -> void;
};


/**
 * @brief Text attributes for a palette entry.
 *
 * Define a logTextPalette entry.  This is an application accessible class to
 * define a logTextPalette entry base style or modifier.  A logTextPaletteEntry is stored
 * in a palette, which can later be activated.
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
    textAttributes m_attributes = attrNone;        //!< font modifier attribute flags

public:
    /**
     * @brief Default constructor.
     *
     * Default constructor, using widget default font, color, and no attributes.
     */
    logTextPaletteEntry(logTextPaletteEntry const&) = default;

    explicit logTextPaletteEntry(QPalette::ColorGroup cg = QPalette::Active);
    explicit logTextPaletteEntry(QPalette const& palette);

    void fromPalette(QPalette const& palette);

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
    auto backgroundColor() const -> QColor const&;

    /**
     * @brief Get background color for caret line
     * @return Color for the background of the caret line
     */
    auto caretLineBackgroundColor() const -> QColor const&;

    /**
     * @brief Set background color.
     *
     * Setter function to set the background color.
     *
     * @param c New color for the background.
     **/
    auto setBackgroundColor(const QColor& c) -> void;

    auto setCaretLineBackgroundColor(const QColor& c) -> void;

    /**
     * @brief Get text color.
     *
     * Get the currently defined text color.
     *
     * @return The defined text color.
     **/
    auto textColor() const -> QColor const&;

    /**
     * @brief Set text color.
     *
     * Setter function to define the text color of a palette entry.
     *
     * @param c New text color for the style.
     **/
    auto setTextColor(const QColor& c) -> void;

    /**
     * @brief Get palette item attributes.
     *
     * Get the current set of attribute bits.
     *
     * @return Attribute bits for this logTextPaletteEntry.
     **/
    auto attributes() const -> textAttributes;

    /**
     * @brief Set palette item attributes.
     *
     * Set the state of the attributes for this logTextPaletteEntry.  Whether a bit affects
     * the final style is controlled by the mask.  See also @a setAttributes().
     *
     * @param a New attribute state for this entry.
     **/
    auto setAttributes(logTextPaletteEntry::textAttributes a) -> void;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(logTextPaletteEntry::textAttributes)
Q_DECLARE_TYPEINFO(logTextPaletteEntry, Q_MOVABLE_TYPE);        //!< Declare as movable type for Qt containers

/** Implementation of logTextPalette */
class logTextPalette {
private:
    QString name;               //!< Palette name
    QVector<logTextPaletteEntry> styles; //!< array of styles

public:
#ifndef DOXYGEN_EXCLUDE         // Exclude from DOXYGEN generation
    logTextPalette(logTextPalette&) = delete;
    auto operator=(logTextPalette&) -> logTextPalette& = delete;
#endif  // DOXYGEN_EXCLUDE

    /**
     * @brief Constructor with default colors
     *
     * Create a palette, using the palette of the active style
     *
     * @param name Name assigned to the new palette.
     * @param numEntries Number of styles to allocate.
     **/
    logTextPalette(const QString& name, int numEntries);

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
     * @param clBgColor Default background color of caret line
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
     * @brief add a new style to the palette
     *
     * @param textColor foreground color for all style entries.
     * @param bgColor background color for all style entries.
     * @param clBgColor background color of caret line
     * @return @c styleId_t of the added style
     */
     auto addStyle(QColor const& textColor, QColor const& bgColor, QColor const& clBgColor) -> styleId_t;

     /**
      * @brief add a new style to the palette
      *
      * @param style style to add
      * @return @c styleId_t of the added style
      */
     auto addStyle(logTextPaletteEntry style) -> styleId_t;

    /**
     * Get a reference to a specified palette style entry.
     *
     * @param id Style number in the palette.
     * @return Reference to a style entry.  If the style number @p s is out of
     *         range, a reference to the last entry is returned.
     **/
    auto style(styleId_t id) -> logTextPaletteEntry&;
    auto style(styleId_t id) const -> logTextPaletteEntry const&;

    /**
     * Returns the (compile time defined) number of styles in a palette.
     *
     * @return Number of styles available in a palette.
     **/
    auto numStyles() const {return styles.count();}
};

#endif // WLOGTEXT_H
