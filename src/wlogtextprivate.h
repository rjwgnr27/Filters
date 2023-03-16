/*
 * Copyright (c) 2008-2012, Harmonic Inc. All rights reserved.
 */

#ifndef LOGTEXTPRIVATE_H
#define LOGTEXTPRIVATE_H

/** @file wlogtextprivate.h Implementation details of wlogtext UI widget */

#include "wlogtext.h"

#include <QHash>
#include <QMap>
#include <QClipboard>
//KDE4 #include <QColorGroup>
#include <QPixmap>
#include <QReadLocker>

/** macro to identify the likely (usually true) path of a conditional */
#define likely(x)       __builtin_expect((x),1)
/** macro to identify the unlikely (usually false) path of a conditional */
#define unlikely(x)     __builtin_expect((x),0)

/** State of text drag */
enum class dragStates {
    dragNone,           //!< No text dragging in progress
    dragMaybe,          //!< Text is selected, and mouse is down, but not yet dragging
    dragDragging        //!< Selected text is dragging
    };

typedef QHash<QString,logTextPalette*> paletteMap;  //!< Map type for styles by name


/**
 * @brief A class similar to a QPoint, providing character cell (line, column) semantics.
 * 
 * An internal class derived from QPoint to represent a character cell
 * position: line/column.  It adds the notion screen positional comparison, i.e.
 * if ( cell_a > cell_b ).  Cell_a \< cell_b if:
 *     ((cell_a.line() \< cell_b.line) ||
 *      ((cell_a.line() == cell_b.line()) && cell_a.col() \< cell_b.col()) );
 **/
class cell : public QPoint {
    public:
        cell() {};                                  //!< Default constructor
        cell(int l, int c) : QPoint(c, l) {};   //!< Constructor from line, column
        explicit cell(const QPoint& p) : QPoint(p) {};   //!< Copy contructor

        /**
         * Return the line number represented by this cell.
         * @return Line number of this cell
         **/
        int line() const {return y();}            //!< Return line number represented by a cell.

        /**
         * Set the line position represented by this cell.
         * @param l New line number for this cell.
         **/
        void setLine(int l) {setY(l);}          //!< Set the line number this cell represents

        /**
         * Return the column number represented by this cell.
         * @return column number of this cell
         **/
        int col() const {return x();}               //!< Return the character column this cell represents

        /**
         * Set the column position represented by this cell.
         * @param c New column number for this cell.
         */
        void setCol(int c) {setX(c);}         //!< Set the character column this cell represents

        /**
         * Set the line and column position represented by this cell.
         * @param l New line number for this cell.
         * @param c New column number for this cell.
         **/
        void setPos(int l, int c) {setLine(l); setCol(c);}  //!< Set the cell location

        /**
         * Returns a cell one line after this.  Note, the position strictly mathmatical, there is no
         * validation against any document structure.
         * @return A \c cell postition one line down from this.
         **/
        cell nextLine() const {cell c(*this); c.setLine(c.line() + 1); return c;}  //!< Return a cell one line after this

        /**
         * Returns a cell one column after this.  Note, the position strictly mathmatical, there is no
         * validation against any document structure.
         * @return cell location of next column.
         **/
        cell nextCol() const {cell c(*this); c.setCol(c.col() + 1); return c;}     //!< Return a cell one column after this

        /**
         * Cell relative position comparison.
         * @param c Cell to compare this location to.
         * @return @c true if cell @p c has a position < this.
         **/
        bool operator < (const cell& c) const {return (x() < c.x()) || ((x() == c.x()) && (y() < c.y()));}    //!< Compare a cell less than (earlier on the screen) this

        /**
         * Cell relative position comparison.
         * @param c Cell to compare this location to.
         * @return @c true if cell @p c has a position <= this.
         **/
        bool operator <= (const cell& c) const {return (x() < c.x()) || ((x() == c.x()) && (y() <= c.y()));}    //!< Compare a cell less than (earlier on the screen) this

        /**
         * Cell relative position comparison.
         * @param c Cell to compare this location to.
         * @return @c true if cell @p c has a position == this.
         **/
        bool operator == (const cell& c) const {return (y() == c.y()) && (x() == c.x());}   //!< Compare a cell to be the same location as this

        /**
         * Cell relative position comparison.
         * @param c Cell to compare this location to.
         * @return @c true if cell @p c has a position != this.
         **/
        bool operator != (const cell& c) const {return !(operator ==(c));}   //!< Compare a cell location not the same as this.

        /**
         * Cell relative position comparison.
         * @param c Cell to compare this location to.
         * @return @c true if cell @p c has a position > this.
         **/
        bool operator > (const cell& c) const {return (x() > c.x()) || ((x() == c.x()) && (y() > c.y()));}    //!< Compare a cell less than (earlier on the screen) this
        
        /**
         * Cell relative position comparison.
         * @param c Cell to compare this location to.
         * @return @c true if cell @p c has a position >= this.
         **/
        bool operator >= (const cell& c) const {return (x() > c.x()) || ((x() == c.x()) && (y() >= c.y()));}    //!< Compare a cell less than (earlier on the screen) this
};

/**
 * @brief A style item of a style palette
 *
 * Defines the text attributes for a style: font, foreground and background colors.
 **/
class styleItem {
public:
    QFont font;                 //!< Font derived from the base font for this style
    QColor textColor;           //!< Text color for this style
    QColor backgroundColor;     //!< Background color for this style
    styleItem() = default;
    styleItem(const styleItem&) = default;
    styleItem(styleItem&&) = default;
};
#ifndef DOXYGEN_EXCLUDE
Q_DECLARE_TYPEINFO(styleItem, Q_MOVABLE_TYPE);
#endif  // DOXYGEN_EXCLUDE

/**
 * @brief An activate collection of styleItem's
 * 
 * A collection of styleItem's which has the font attributes applied to each
 * style entry to form an active palette.
 **/
class activatedPalette {
private:
    std::vector<styleItem> styles;                           //!< Array of style items

#ifndef DOXYGEN_EXCLUDE         // Exlude from DOXYGEN generation
    activatedPalette(activatedPalette&) = delete;             /** Declare no copy constructor */
    activatedPalette& operator =(activatedPalette) = delete;  /** Declare no "operator =()" */
#endif  // DOXYGEN_EXCLUDE

public:
    /**
     * Create a drawing style array from a palette.  The drawing array is a
     * permutation of the styles and attributes, formed into a linear array.  The
     * array is indexed by style+attribute in the draw function.  Each style entry
     * is a sum of the palette base style and every combination of modifier.  This
     * speeds drawing, by referencing a style number for the line, rather than
     * applying (multiple) attributes at draw time.
     *
     * @param f Base font for the styles.  If the base font changes, even in size,
     * the whole palette must be reactivated.
     *
     * @param p Palette, containing the style and modifiers which control the view
     * of the display.
     **/
    activatedPalette(const QFont& f, logTextPalette const& p);

    /**
     * @brief number of styles in the palette
     *
     * Returns the number of styles defined for the palette
     * @return number of styles in the palette
     **/
    inline int numStyles() const { return styles.size(); };

    /**
     * @brief get style by ID
     *
     * Returns the style for the given style ID. If the style ID is invalid,
     * i.e. greater than the number of styles, the default (ID 0) style is
     * returned.
     *
     * @param styleId ID of the style to return
     * @return const reference to the style identified
     **/
    inline const styleItem& style(styleId styleId) const {
            return styles[(styleId < numStyles()) ? styleId : 0]; };
};


/**
 * @brief Private implementation details of wLogText widget
 *
 * Each wLogText object constructs a private detail (*d) to implement the
 * private details of the object.
 **/
class wLogTextPrivate {
protected:
    friend class wLogText;
    
    wLogText *const q = nullptr;    //!< Public wLogText which this implements

    QPixmap *drawPixMap = nullptr;  //!< Rendering pixmap for the draw event
    QSize pmSize{0,0};              //!< Allocation size of the drawPixMap

    bool timeDraw = false;          //!< Diagnostic to enable draw timing
    QTime drawTimer;                //!< Diagnostic timer used for timing draw events

    QPalette m_qpalette;            //!< System style palette for the widget
    QMap<int, QPixmap> itemPixMaps; //!< Map from pixmapId to QPixmap for the gutter pixmaps.

    int gutterWidth = 0;            //!< Space on left edge left of pixmaps.
    int m_gutterOffset;             //!< gutterWidth + gutterBorder, or zero.

    int m_textLineHeight = 0;       //!< Calculated text line height, based on current (zommed) fint size.
    int textLineBaselineOffset = 0; //!< Physical offset of line bottom from baseline, font.descent().
    int m_characterWidth = 0;       //!< calculated character width,  based on current (zommed) fint size.
    int fontBaseSize = 10;          //!< Selected base font size in pixels
    int magnify = 0;                //!< Increment from base font size for font zooming

    uint32_t visibleLines = 0;      //!< Number of lines visible based on font size and physical viweport size.
    bool m_mouseIsEditCursor = true; //!< If the last set of the mouse cursor was edit, as opposed to arrow cursor.
    int m_maxVScroll = 0;           //!< Maximum value for the vertical scroll bar, indicating last line showing.

    bool selecting = false;         //!< Whether text is being or is selected.
    cell selectionOrigin;           //!< Character cell (line, col) of selection origin
    cell selectTop;                 //!< Top of the selection region, regardless of selection direction.
    cell selectBottom;              //!< Bottom of the selection region, regardless of selection direction.
    int selectScrollTimerId = 0;    //!< Timer when mouse-down and shift selecting out of range
    int timedVScrollStep = 0;       //!< Last scroll step: <0 for up, >0 for down, == 0 none

    dragStates dragState = dragStates::dragNone; //!< Current state of dragging
    cell dragStart;                 //!< Mouse position of drag start

    bool m_ShowCaret = false;       //!< Should caret be shown?

    cell caretPosition = {0,0};     //!< QPoint caret position. QPoint.y() is line, QPoint.x() is column.
    int caretBlinkTimer = -1;       //!< QWidget timer number for caret blink
    bool caretBlinkOn = false;      //!< If the current blnk state of the caret is on

    bool hardLocked = false;        //!< Are new lines are prevented from scrolling view due to ScrlLk key.
    bool softLocked = false;        //!< Are new lines are prevented from scrolling view due to soft things.
    bool escJump = false;           //!< Flag whether the ESC key will unlock and jump to end

    activatedPalette *activePalette = nullptr;    //!< Pointer to current activated palette
    paletteMap palettes;            //!< Map by name of available palettes.
    QString activatedPaletteName;   //!< name of active palette.

    /**
     * Constructor for private data of a wLogText
     * @param base public interface widget
     **/
    explicit wLogTextPrivate(wLogText *base);

    /** @brief destructor **/
    ~wLogTextPrivate();

    /**
     * @brief Request a screen update on a bounded region.
     *
     * Request a view update on the region bounded by two cells. If the region spans
     * multiple lines, then the width is expanded to enclose the entire lines.
     *
     * @param top Top cell (line/column) of region to update.
     * @param bottom Bottom cell of region to update.
     **/
    void updateCellRange(const cell&, const cell&);

    /**
     * @brief return line number of first visible line
     *
     * Returns the top most visable line of the client region.
     * 
     * @return linenumber of the top most line visible in the client area
     **/
    inline lineNumber_t firstVisibleLine() const;
    
    /**
     * @brief return number of lines visible on the painted region
     *
     * @return linenumber lines displayable for the current client region size
     **/
    inline lineNumber_t linesPerPage() const;
    
    /**
     * @brief return line number of last visible line
     *
     * Returns the bottom most visable line of the client region.
     *
     * @return linenumber of the bottom most line visible in the client area
     **/
    inline lineNumber_t lastVisibleLine() const;

    // Miscellaneous
    /**
     * Internal function used to prepare a find.
     *
     * @param forward Direction of search.
     * @param pos @c cell to be set with the initial serch location.
     * @param at @c QPoint from caller of find, which may be non-zero, in which case
     * it is the cell position from which to start the find.
     * @return true if the initial cell location is valid, and the find may proceed.
     * Otherwise false, in which case the find will fail.
     **/
    bool prepareFind(bool forward, cell& pos, const QPoint *at) const;

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
    bool activatePalette(const QString& name);

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
    logTextPalette *createPalette( int, const QString& );


    /**
     * @brief return soft-lock state
     * @return @c true if view scrolling is soft locked, else @c false
     **/
    inline bool isSoftLocked() const { return softLocked; };

    /**
     * @brief Set soft scroll lock state.
     *
     * Set the scroll lock state.  Emits a scrollLockChange if the state changes.
     *
     * @param state New scroll lock state.
     **/
    void setSoftLock(bool state);

    /**
     * @brief Get hardlock state
     *
     * Returns the state of the hard lock (i.e. SrclLck key)
     * @return hard-lock state
     **/
    inline bool isHardLocked() const { return hardLocked; };

    /**
     * @brief Set hard scroll lock state.
     *
     * Set the scroll lock state.  Emits a scrollLockChange if the state changes.
     *
     * @param state New scroll lock state.
     **/
    void setHardLock(bool state);

    /**
     * @brief Check scrollability of new text.
     *
     * Checks the various locking conditions to determine if new text may scroll
     * the text.
     *
     * @return true if scrolling is unlocked.
     **/
    inline bool isScrollable() const;

    // Caret methods
    /**
     * @brief Move caret with update.
     *
     * Set new caret postion, erasing old and drawing new.
     *
     * @param line New caret line.
     * @param col New caret column.
     */
    void updateCaretPos(lineNumber_t line, int col);

    /**
     * @brief Move caret with update.
     *
     * Set new caret postion, erasing old and drawing new.
     *
     * @param pos New caret position.
     */
    void updateCaretPos(const cell& pos);

    // Selection and clipboard methods
    /**
     * @brief Extend selection range from d->selectionOrigin
     *
     * Extend the selection range from the current origin to the given cell.
     *
     * @param sel Cell location (line/column) to extend the selection from the
     * origin.
     **/
    void setSelection(const cell& sel);

    /**
     * @brief Set selection range.
     *
     * Sets the selection range.
     *
     * @param a Cell location (line/column) the selection origin.
     * @param b Cell location (line/column) the selection end.
     **/
    void setSelection(const cell& a, const cell& b);

    /**
     * @brief Copy the selected text to a clipboard.
     *
     * Copy the selected region, if any.  The region can be copied to either the
     * global selection or the clipboard.
     *
     * @param cbMode Specifies the target of the copy, either the global
     * selection or the clipboard.
     **/
    void copySelToClipboard(QClipboard::Mode cbMode) const;

    /**
     * @brief Make internal adjustment to calculated metrics.
     *
     * Caclculate the element metrics when the visible area is resized, or the font
     * is changed.  These metrics are used for the conversion to/from character to
     * pixel position.
     */
    void adjustTextMetrics();

    /**
     * @brief Set QAbstractScrollArea content size.
     *
     * Set QAbstractScrollArea content size based on text content size and metrics.
     */
    inline void setContentSize();

    /**
     * @brief draw the text area
     *
     * Paint event to draw the client area contents.
     * 
     * @param bounds Bounding area of the client which is invalidated, and needs
     * drawing.
     **/
    void drawContents(const QRect& bounds);

    /**
     * Draw the caret onto a QPainter.
     *
     * @param painter QPainter on which to draw the caret image:
     * @param x x offset in painter coordinates.
     * @param top top offset in painter coordinates.
     * @param bot bottom offset in painter coordinates.
     **/
    void drawCaret(QPainter& painter, int x, int top, int bot) const;

    /**
     * @brief Check if location is in the gutter area.
     *
     * Test to see if @p x (scrollView coordinate) is in the gutter area.
     *
     * @param x x-axis point in scroll view coordinates.
     * @return @c true is point is in the gutter region.
     **/
    inline bool inTheGutter(int x) const;

    /**
     * @brief Convert line/column cell to pixel postion.
     *
     * Internal utility function to convert a cell (line, column) to a pixel point
     * x,y.  This only converts a point on the cell grid.  It does not validate that
     * text actually exists at that coordinate.
     *
     * @param c cell coordinate to convert.
     * @return Pixel point position.
     **/
    inline QPoint cellToPoint(const cell&) const;

    /**
     * @brief Convert view pixel postion to character column.
     *
     * Calculate character column based on scroll view x coordinate.  The returned
     * value is just the calculated column.  It is not verified to be a valid
     * character position for the line; i.e. the calculated column may be beyond
     * the end of the line.
     *
     * If the position is in the gutter area, zero is
     * returned.  See @a inTheGutter() to test if the point is in the gutter.
     *
     * @param x Scroll view x-coordinate to translate.
     * @return Character column number on text view.
     **/
    inline int xToCharColumn(int x) const;

    /**
     * @brief scrollView y pixel coordinate to line number
     *
     * Calculate line number from scroll view y location.  The line number is just a
     * caclulation based on point y and the various metrics.  It is not guaranteed
     * to be a valid line number; i.e., it can extend beyond the valid lines.  See
     * @a validLineNumber() to verify the validity of the line number.
     *
     * @param y viewport visible y coordinate to translate.
     * @return Calculated line number based on scroll view coordinate.
     **/
    inline lineNumber_t yToLine(int y) const;
};

/**
 * Object to disable widget updates for the duraction of the object lifetime.
 * Manages update disable/enable in a block scope.
 */
class [[nodiscard]] updatesDisableScope {
private:
    QWidget *widget;                //!< Pointer to the QWidget to manage
    bool initial;                   //!< State of updatesEnabled at ctor

    #ifndef DOXYGEN_EXCLUDE         // Exlude from DOXYGEN generation
    updatesDisableScope(const updatesDisableScope&) = delete;          //!< Copy ctor
    updatesDisableScope& operator =(const updatesDisableScope&) = delete;    //!< Assignment operator
    #endif  // DOXYGEN_EXCLUDE

public:
    /**
     * @brief Constructor
     * Construct object, managing update enable state for widget @p w. The
     * widget's updatesEnabled() state is noted on construction. The
     * initial state is restored on destruction, or on call to enableUpdates().
     * @param w Pointer to QWidget to manage
     */
    updatesDisableScope(QWidget *w) : widget(w), initial(w->updatesEnabled())
    {widget->setUpdatesEnabled(false);}

    /**
     * @brief Destructor; restore widget's update state to state at ctor.
     * Destroys the object, reseting the widget's update enable state to
     * the initial state at construction.
     */
    ~updatesDisableScope() {widget->setUpdatesEnabled(initial);}

    /**
     * @brief retrieve the widgets initial update enabled state.
     * Retrieve the update enable state of the widget at the time of the
     * object construction.
     * @return initial update enabled state of the widget
     */
    bool initiallyEnabled() const {return initial;}

    /**
     * @brief reset widget's update enable state to its initial state
     * Set the widget's update enable state, to the state it was in at the
     * time of this objects contruction. Used to temporarilly re-enable
     * updates, while the scope disable object is still in scope.
     */
    void enableUpdates() {widget->setUpdatesEnabled(initial);}

    /**
     * @brief force widgets update enable state to enabled
     * Sets the widget's update enable state to enabled, regardless of the
     * initial state of this objects construction. Used to temporarilly re-enable
     * updates, while the scope disable object is still in scope.
     */
    void enableUpdatesForced() {widget->setUpdatesEnabled(true);}

    /**
     * @brief disable widget's updates
     * Sets the widget update enable state to disabled. Used within this
     * objects scope, to disable updates after a call to enableUpdates() or
     * enableUpdatesForced().
     */
    void disableUpdates() {widget->setUpdatesEnabled(false);}
};


#endif //ifndef LOGTEXTPRIVATE_H
