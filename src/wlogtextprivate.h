/*
 * Copyright (c) 2008-2023, Rick Wagner. All rights reserved.
 */

#ifndef LOGTEXTPRIVATE_H
#define LOGTEXTPRIVATE_H

/** @file wlogtextprivate.h Implementation details of wlogtext UI widget */

#include "wlogtext.h"

#include <QHash>
#include <QMap>
#include <QClipboard>
#include <QPixmap>
#include <QReadLocker>

/** State of text drag */
enum class dragStates {
    dragNone,           //!< No text dragging in progress
    dragMaybe,          //!< Text is selected, and mouse is down, but not yet dragging
    dragDragging        //!< Selected text is dragging
    };

using paletteMap = QHash<QString,logTextPalette*>;  //!< Map type for styles by name


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
    QColor clBackgroundColor;   //!< Caret line background color for this style
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

public:
#ifndef DOXYGEN_EXCLUDE         // Exlude from DOXYGEN generation
    activatedPalette(activatedPalette&) = delete;             /** Declare no copy constructor */
    auto operator =(activatedPalette) -> activatedPalette& = delete;  /** Declare no "operator =()"*/
#endif  // DOXYGEN_EXCLUDE

    /**
     * Create a drawing style array from a palette.  The drawing array is a
     * permutation of the styles and attributes, formed into a linear array.  The
     * array is indexed by style+attribute in the draw function.  Each style entry
     * is a sum of the palette base style and every combination of modifiers.  This
     * speeds drawing, by referencing a style number for the line, rather than
     * applying (multiple) attributes at draw time.
     *
     * @param f Base font for the styles.  If the base font changes, even in size,
     * the whole palette must be reactivated.
     *
     * @param p Palette, containing the style and modifiers which control the view
     * of the display.
     **/
    activatedPalette(const QFont& f, logTextPalette& p);

    activatedPalette(activatedPalette&&) noexcept = default;
    activatedPalette& operator =(activatedPalette&&) noexcept = default;

    /**
     * @brief number of styles in the palette
     *
     * Returns the number of styles defined for the palette
     * @return number of styles in the palette
     **/
    inline auto numStyles() const -> int { return styles.size(); };

    /**
     * @brief get style by ID
     *
     * Returns the style for the given style ID. If the style ID is invalid,
     * i.e. greater than the number of styles, the default (ID 0) style is
     * returned.
     *
     * @param styleId ID of the style to return
     * @return const reference to the style requested
     **/
    inline auto style(styleId_t styleId) const -> const styleItem& {
            return styles[(styleId < numStyles()) ? styleId : 0]; };
};


/**
 * @brief Private implementation details of wLogText widget
 *
 * Each wLogText object constructs a private detail (*d) to implement the
 * private details of the object.
 **/
class wLogTextPrivate {
public:
        /** @brief destructor **/
    ~wLogTextPrivate();

protected:
    friend class wLogText;
    
    wLogText *const q = nullptr;    //!< Public wLogText which this implements

    std::unique_ptr<QPixmap> drawPixMap;  //!< Rendering pixmap for the draw event
    QSize pmSize{0,0};              //!< Allocation size of the drawPixMap

    bool timeDraw = false;          //!< Diagnostic to enable draw timing
    QTime drawTimer;                //!< Diagnostic timer used for timing draw events

    QPalette m_qpalette;            //!< System style palette for the widget
    QMap<int, QPixmap> itemPixMaps; //!< Map from pixmapId to QPixmap for the gutter pixmaps.

    int gutterWidth = 0;            //!< Space on left edge left of pixmaps.
    int m_gutterOffset;             //!< gutterWidth + gutterBorder, or zero.

    int m_textLineHeight = 10;      //!< Calculated text line height, based on current (zoomed) font size.
    int textLineBaselineOffset = 0; //!< Physical offset of line bottom from baseline, font.descent().

    /** calculated character width,  based on current (zoomed) font size. Note
     * that the character width for a font cannot be determine unless attached
     * to a paint event, so is zero initially and after a font change, until a
     * paint event occurs.
     */
    int m_characterWidth = 0;
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
    bool caretBlinkOn = false;      //!< If the current blink state of the caret is on

    bool hardLocked = false;        //!< Are new lines are prevented from scrolling view due to ScrlLk key.
    bool softLocked = false;        //!< Are new lines are prevented from scrolling view due to soft things.
    bool escJump = false;           //!< Flag whether the ESC key will unlock and jump to end

    std::unique_ptr<activatedPalette> activePalette;    //!< Pointer to current activated palette
    paletteMap palettes;            //!< Map by name of available palettes.
    QString activatedPaletteName;   //!< name of active palette.

    /**
     * Constructor for private data of a wLogText
     * @param base public interface widget
     **/
    explicit wLogTextPrivate(wLogText *base);

    /**
     * @brief Request a screen update on a bounded region.
     *
     * Request a view update on the region bounded by two cells. If the region spans
     * multiple lines, then the width is expanded to enclose the entire lines.
     *
     * @param top Top cell (line/column) of region to update.
     * @param bottom Bottom cell of region to update.
     **/
    auto updateCellRange(const cell&, const cell&) -> void;

    /**
     * @brief return line number of first visible line
     *
     * Returns the top most visible line of the client region.
     * 
     * @return linenumber of the top most line visible in the client area
     **/
    auto firstVisibleLine() const -> lineNumber_t;
    
    /**
     * @brief return number of lines visible on the painted region
     *
     * @return linenumber lines displayable for the current client region size
     **/
    auto linesPerPage() const -> lineNumber_t;
    
    /**
     * @brief return line number of last visible line
     *
     * Returns the bottom most visible line of the client region.
     *
     * @return linenumber of the bottom most line visible in the client area
     **/
    auto lastVisibleLine() const -> lineNumber_t;

    // Miscellaneous
    /**
     * Internal function used to prepare a find.
     *
     * @param forward Direction of search.
     * @param pos @c cell to be set with the initial search location.
     * @param at @c QPoint from caller of find, which may be non-zero, in which case
     * it is the cell position from which to start the find.
     * @return true if the initial cell location is valid, and the find may proceed.
     * Otherwise false, in which case the find will fail.
     **/
    auto prepareFind(bool forward, cell& pos, const cell *at) const -> bool;

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
    auto activatePalette(const QString& name) -> bool;

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
    auto createPalette( int, const QString& ) -> logTextPalette *;


    /**
     * @brief return soft-lock state
     * @return @c true if view scrolling is soft locked, else @c false
     **/
    auto isSoftLocked() const -> bool { return softLocked; };

    /**
     * @brief Set soft scroll lock state.
     *
     * Set the scroll lock state.  Emits a scrollLockChange if the state changes.
     *
     * @param state New scroll lock state.
     **/
    auto setSoftLock(bool state) -> void;

    /**
     * @brief Get hardlock state
     *
     * Returns the state of the hard lock (i.e. SrclLck key)
     * @return hard-lock state
     **/
    auto isHardLocked() const -> bool { return hardLocked; };

    /**
     * @brief Set hard scroll lock state.
     *
     * Set the scroll lock state.  Emits a scrollLockChange if the state changes.
     *
     * @param state New scroll lock state.
     **/
    auto setHardLock(bool state) -> void;

    /**
     * @brief Check scrollability of new text.
     *
     * Checks the various locking conditions to determine if new text may scroll
     * the text.
     *
     * @return true if scrolling is unlocked.
     **/
    auto isScrollable() const -> bool;

    // Caret methods
    /**
     * @brief Move caret with update.
     *
     * Set new caret position, erasing old and drawing new.
     *
     * @param line New caret line.
     * @param col New caret column.
     */
    auto updateCaretPos(lineNumber_t line, int col) -> void;

    /**
     * @brief Move caret with update.
     *
     * Set new caret position, erasing old and drawing new.
     *
     * @param pos New caret position.
     */
    auto updateCaretPos(const cell& pos) -> void;

    // Selection and clipboard methods
    /**
     * @brief Extend selection range from d->selectionOrigin
     *
     * Extend the selection range from the current origin to the given cell.
     *
     * @param sel Cell location (line/column) to extend the selection from the
     * origin.
     **/
    auto setSelection(const cell& sel) -> void;

    /**
     * @brief Set selection range.
     *
     * Sets the selection range.
     *
     * @param a Cell location (line/column) the selection origin.
     * @param b Cell location (line/column) the selection end.
     **/
    auto setSelection(const cell& a, const cell& b) -> void;

    /**
     * @brief Copy the selected text to a clipboard.
     *
     * Copy the selected region, if any.  The region can be copied to either the
     * global selection or the clipboard.
     *
     * @param cbMode Specifies the target of the copy, either the global
     * selection or the clipboard.
     **/
    auto copySelToClipboard(QClipboard::Mode cbMode) const -> void;

    /**
     * @brief Make internal adjustment to calculated metrics.
     *
     * Calculate the element metrics when the visible area is resized, or the font
     * is changed.  These metrics are used for the conversion to/from character to
     * pixel position.
     */
    auto adjustTextMetrics() -> void;

    /**
     * @brief Set QAbstractScrollArea content size.
     *
     * Set QAbstractScrollArea content size based on text content size and metrics.
     */
    auto setContentSize() -> void;

    /**
     * @brief draw the text area
     *
     * Paint event to draw the client area contents.
     * 
     * @param bounds Bounding area of the client which is invalidated, and needs
     * drawing.
     **/
    auto drawContents(const QRect& bounds) -> void;

    /**
     * Draw the caret onto a QPainter.
     *
     * @param painter QPainter on which to draw the caret image:
     * @param x x offset in painter coordinates.
     * @param top top offset in painter coordinates.
     * @param bot bottom offset in painter coordinates.
     **/
    auto drawCaret(QPainter& painter, int x, int top, int bot) const -> void;

    /**
     * @brief Check if location is in the gutter area.
     *
     * Test to see if @p x (scrollView coordinate) is in the gutter area.
     *
     * @param x x-axis point in scroll view coordinates.
     * @return @c true is point is in the gutter region.
     **/
    auto inTheGutter(int x) const -> bool;

    /**
     * @brief Convert line/column cell to pixel position.
     *
     * Internal utility function to convert a cell (line, column) to a pixel point
     * x,y.  This only converts a point on the cell grid.  It does not validate that
     * text actually exists at that coordinate.
     *
     * @param c cell coordinate to convert.
     * @return Pixel point position.
     **/
    QPoint cellToPoint(const cell&) const;

    /**
     * @brief Convert view pixel position to character column.
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
    auto xToCharColumn(int x) const -> int;

    /**
     * @brief scrollView y pixel coordinate to line number
     *
     * Calculate line number from scroll view y location.  The line number is just a
     * calculation based on point y and the various metrics.  It is not guaranteed
     * to be a valid line number; i.e., it can extend beyond the valid lines.  See
     * @a validLineNumber() to verify the validity of the line number.
     *
     * @param y viewport visible y coordinate to translate.
     * @return Calculated line number based on scroll view coordinate.
     **/
    auto yToLine(int y) const -> lineNumber_t;
};

#endif //ifndef LOGTEXTPRIVATE_H
