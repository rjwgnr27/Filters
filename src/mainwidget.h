/****************************************************************************
 * * A program to interactively apply regular expressions to an input file.
 ** Copyright (C) 2021 Rick Wagner
 **
 ** This program is free software: you can redistribute it and/or modify it under the terms of
 ** the GNU General Public License as published by the Free Software Foundation, either
 ** version 3 of the License, or (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 ** without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 ** See the GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License along with this program.
 ** If not, see <https://www.gnu.org/licenses/>.
 **/

#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QGroupBox>
#include <QHeaderView>
#include <QPlainTextEdit>
#include <QScopedPointer>
#include <QSplitter>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <KFind>

#include <vector>

#include "wlogtext.h"

class QCheckBox;
class QLabel;
class QMenu;
class QTableWidgetItem;
class KXmlGuiWindow;
class KRecentFilesAction;
class KSelectAction;
struct commandLineOptions;

struct filterEntry {
    bool enabled = false;
    bool exclude = false;
    bool ignoreCase = false;
    QString re;

    QJsonObject toJson() const;
    static auto fromJson(const QJsonObject& jentry) -> filterEntry;
};

struct filterData {
    bool valid = false;
    QString dialect;
    QList<filterEntry> filters;
};

struct textItem {
    int srcLineNumber = 0;
    bool bookmarked = false;
    QString text;
    QStringRef bmText;

    textItem(int lineNo, QString const& txt) : srcLineNumber{lineNo}, text{txt} {}
    textItem(int lineNo, QString&& txt) noexcept : srcLineNumber{lineNo}, text{std::move(txt)} {}
    textItem(textItem const&) = default;
    textItem(textItem&&) noexcept = default;
    auto operator=(textItem const&) -> textItem& = default;
    auto operator=(textItem&&) noexcept -> textItem& = default;

    auto isBoomkmarked() const {return bookmarked;}
};
using itemsList = std::vector<textItem>;
using stepList = QList<textItem*>;

class mainWidget : public QWidget {
    friend class Filters;
    Q_OBJECT

private:
    /** Constants for column addressing */
    enum {ColEnable = 0, ColExclude, ColCaseIgnore, ColRegEx, NumCol};
    /** Pixmap ID values */
    enum : pixmapId_t {pixmapIdBookMark = 0, pixmapIdAnnotation = 1};
    /** style IDs */
    enum : styleId_t {styleBase = 0};

    class resultTextItem : public logTextItem {
    private:
        textItem* const srcItem;

    public:
        resultTextItem(textItem* item, const QString& text, styleId_t styleNo = styleBase) :
                logTextItem{text, styleNo}, srcItem{item} {}

        resultTextItem(textItem* item, QString&& text, styleId_t styleNo = styleBase) :
                logTextItem{text, styleNo}, srcItem{item} {}

        resultTextItem(resultTextItem const&) = default;
        resultTextItem(resultTextItem&&) = default;

        static auto asResultTextItem(logTextItem* item) -> resultTextItem* {
            return static_cast<resultTextItem *>(item);}
        static auto asResultTextItem(logTextItem const* item) -> resultTextItem const* {
            return static_cast<resultTextItem const*>(item);}

        auto sourceItem() const {return srcItem;}
    };

public:
    explicit mainWidget(KXmlGuiWindow *main, QWidget *parent = nullptr);

    auto initialLoad(const commandLineOptions& opts) -> bool;

private Q_SLOTS:
    auto appendEmptyRow() -> void;
    auto actionLineNumbersTriggerd(bool checked) -> void;
    auto autoRunClicked() -> void;
    auto clearFilterRow() -> void;
    auto clearFilters() -> void;
    auto deleteFilterRow() -> void;
    auto dialectChanged(QString const& text) -> void;
    auto filtersTableMenuRequested(QPoint point) -> void;
    auto gotoBookmark(int entry) -> void;
    auto gotoLine() -> void;
    auto insertEmptyFilterAbove() -> void;
    auto insertEmptyRowAt(int row) -> void;
    auto insertFiltersAbove() -> void;
    auto loadFilters() -> void;
    auto loadFiltersTable(const QUrl&) -> void;
    auto fontMetricsChanged(int lineHeight, int charWidth) -> void;
    auto loadRecentSubject(const QUrl& url) -> void;
    auto loadSubjectFile() -> void;
    auto loadSubjectFile(const QString& localFile) -> bool;
    auto loadSubjectFromCB() -> void;
    auto moveFilterDown() -> void;
    auto moveFilterUp() -> void;
    auto resultContextClick(lineNumber_t,QPoint,QContextMenuEvent*) -> void;
    auto resultFind() -> void;
    auto resultFindNext() -> void;
    auto resultFindPrev() -> void;
    auto saveFilters() -> void;
    auto saveFiltersAs() -> void;
    auto saveResult() -> void;
    auto saveResultAs() -> void;
    auto selectFilterFont() -> void;
    auto selectResultFont() -> void;
    auto tableItemChanged(QTableWidgetItem *item) -> void;
    auto toggleBookmark() -> void;

private:
    KXmlGuiWindow *mainWindow = nullptr;

    QTableWidget *filtersTable = nullptr;
    wLogText *result = nullptr;

    bool doInitialApply = false;

    bool reModified = false;
    bool subjModified = false;

    /** Vector of text originally sourced text items */
    itemsList sourceItems;

    /** Vector of results for each step. The input file is read into results[0].
     * Each results[n] is the input to filter(n), and the filter result goes
     * to results[n+1]. The final displayed result is at results.back(). */
    std::vector<stepList> stepResults;

    /**
     * Map of display line number to source line number. The index into the
     * vector is the display line number, and the entry is the source line
     * number. Since this array is filled from monotonically increasing source
     * line numbers, the vector will be automatically sorted; thus, looking up
     * the source line number becomes a binary search. The display line number
     * is the index+1 of the matched source line number.
     */
    std::vector<int> sourceLineMap;

    /**
     * set containing the source line numbers of source lines with bookmarks
     */
    QSet<int> bookmarkedLines;
    std::vector<int> bmLineNums;

    /** label widget placed in the status bar */
    QLabel *status = nullptr;

    /** lines read from the source file */
    int sourceLineCount = -1;

    /** number of columns in the painted line number; used for mapping wLogText cell
     * positions to source columns */
    int lineNoColCount = 0;

    KRecentFilesAction *recentFileAction = nullptr;

    /** subject file name to display in the application title; latest of last loaded or saved */
    QString titleFile;

    QAction *actionLoadFromClipboard = nullptr;
    QAction *actionSaveResults = nullptr;
    QAction *actionSaveResultsAs = nullptr;
    QString resultFileName;

    QAction *actionSaveFilters = nullptr;
    QAction *actionSaveFiltersAs = nullptr;
    QString filtersFileName;
    KRecentFilesAction *recentFiltersAction = nullptr;
    QAction *actionGotoLine = nullptr;
    QAction *actionRun = nullptr;
    QAction *actionAutorun = nullptr;
    KSelectAction *actionDialect = nullptr;
    QAction *actionLineNumbers = nullptr;
    QMenu *filtersTableMenu = nullptr;
    QAction *actionMoveFilterUp = nullptr;
    QAction *actionMoveFilterDown = nullptr;
    QAction *actionInsertFilters = nullptr;

    QPixmap pixBmUser;         //!< Pixmap to display in the gutter for user bookmarks
    QPixmap pixBmAnnotation;

    QMenu *ctxtMenu = nullptr;
    QAction *actionToggleBookmark;
    KSelectAction *actionBookmarkMenu;
    QAction *actionClearSelection;
    QAction *actionCopySelection;

    int findHistorySize = 10;
    QString lastFoundText;
    long findOptions = 0;
    QStringList findHistory;

    auto setupUi() -> void;

    /**
     * @brief apply an expression entry
     * Apply the expression at table[@p entry] to input string list @p src,
     * returning the resultant string list. If the expression is empty or
     * disabled, just return @p src.
     *
     * @param entry entry number to apply
     * @param src input string list to apply
     * @return result of the filter applied to the input list @p entry
     */
    auto applyExpression(size_t entry, stepList src) -> stepList;

    /**
     * @brief apply filter chain from point and those following
     * @param entry table entry (row) number to start applying from
     */
    auto applyFrom(size_t entry) -> void;;

    /**
     * @brief clear the interim and final results from a given entry
     * @param entry entry number to clear from
     */
    auto clearResultsAfter(size_t entry) -> void;;

    /**
     * @brief clear the results field
     */
    auto clearResults() -> void;;

    /**
     * @brief update result display with the results of the final evaluation
     */
    auto displayResult() -> void;;

    /**
     * @brief common perform find
     *
     * Helper function for resultFind/resultFindNext/resultFindPrev
     * @param flags QTextDocument::FindFlag for search; default={} for forward
     */
    auto doResultFind(long options) -> void;

    /**
     * @brief save the filters to a JSON file
     * @param fileName file name to save result text to
     * @return @c true if the file was opened, and the filters saved; otherwise @c false
     */
    auto doSaveFilters(const QString& fileName) -> bool;

    /**
     * @brief save the result text to a file
     * @param fileName file name to save result text to
     * @return @c true if the file was opened, and the text saved; otherwise @c false
     */
    auto doSaveResult(const QString& fileName) -> bool;

    auto insertFiltersAt(int at, const filterData& fData) -> void;

    /**
     * go to displayed line for, or nearest previous line displayed for, a source line
     * @param lineNumber source line number to display
     */
    auto jumpToSourceLine(int lineNumber) -> void;

    auto getFilterFile() -> QString;
    auto loadFiltersTable(const QString& localFile) -> bool;
    auto loadFiltersTable(const filterData& filters) -> bool;
    auto loadFiltersFile(const QString& fileName) -> filterData;

    /**
     * @brief run expressions, if auto-apply option is enabled
     * @param entry table entry number to start applying from
     */
    auto maybeAutoApply(int entry) -> void;

    auto swapFiltersRows(int a, int b) -> void;
    auto getFilterRow(int row) -> filterEntry;
    auto setFilterRow(int row, filterEntry const& entry) -> void;

    /**
     * override base paintEvent
     */
    auto showEvent(QShowEvent *ev) -> void override;

    auto updateApplicationTitle() -> void;

    /**
     * @brief validate expressions in the expressions table
     * @param entry table entry number to validate from
     * @return @c true if all expressions (from @p entry) are valid; @c false if any
     * expressions are invalid
     */
    auto validateExpressions(int entry=0) const -> bool;
};

#endif // MAINWIDGET_H
