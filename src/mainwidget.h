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

#include <QDialog>
#include <QFile>
#include <QScopedPointer>
#include <QStringList>
#include <vector>

#include "ui_mainwidget.h"

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
    static filterEntry fromJson(const QJsonObject& jentry);
};

struct filterData {
    bool valid = false;
    QString dialect;
    QList<filterEntry> filters;
};

struct textItem {
    int srcLineNumber;
    QString text;
};
using itemList = QList<textItem>;

class mainWidget : public QWidget, public Ui::mainwidget {
    friend class Filters;
    Q_OBJECT

private:
    enum {ColEnable = 0, ColExclude=1, ColCaseIgnore=2, ColRegEx=3};

public:
    mainWidget(KXmlGuiWindow *main, QWidget *parent = nullptr);
    void setupActions();
    bool initialLoad(const commandLineOptions& opts);

private Q_SLOTS:
    void appendEmptyRow();
    void autoRunClicked();
    void clearFilterRow();
    void clearFilters();
    void deleteFilterRow();
    void dialectChanged(QString text);
    void filtersTableMenuRequested(QPoint point);
    void insertEmptyFilterAbove();
    void insertEmptyRowAt(int row);
    void insertFiltersAbove();
    void loadFilters();
    void loadFiltersTable(const QUrl&);
    void moveFilterDown();
    void moveFilterUp();
    void loadSubjectFile();
    bool loadSubjectFile(const QString& localFile);
    void loadRecentSubject(const QUrl& url);
    void loadSubjectFromCB();

    void runButtonClicked();
    void saveFilters();
    void saveFiltersAs();
    void saveResult();
    void saveResultAs();
    void selectFilterFont();
    void selectResultFont();
    void tableItemChanged(QTableWidgetItem *item);

    void gotoLine();

    void resultFind();
    void resultFindNext();
    void resultFindPrev();

private:
    KXmlGuiWindow *mainWindow;

    bool doInitialApply = false;

    bool reModified = false;
    bool subjModified = false;

    /** Vector of results for each step. The input file is read into results[0].
     * Each results[n] is the input to filter(n), and the filter result goes
     * to results[n+1]. The final displayed result is at results.back(). */
    std::vector<itemList> stepResults;

    /**
     * Map of display line number to source line number. The index into the
     * vector is the display line number, and the entry is the source line
     * number. Since this array is filled from monotonically increasing source
     * line numbers, the vector will be automatically sorted; thus, looking up
     * the source line number becomes a binary search. The display line number
     * is the index+1 of the matched source line number.
     */
    std::vector<int> sourceLineMap;

    /** label widget placed in the status bar */
    QLabel *status;

    /** lines read from the source file */
    int sourceLineCount = -1;

    KRecentFilesAction *recentFileAction;

    /** subject file name to display in the application title; latest of last loaded or saved */
    QString titleFile;

    QAction *actionLoadFromClipboard;
    QAction *actionSaveResults;
    QAction *actionSaveResultsAs;
    QString resultFileName;

    QAction *actionSaveFilters;
    QAction *actionSaveFiltersAs;
    QString filtersFileName;
    KRecentFilesAction *recentFiltersAction;
    QAction *actionRun;
    QAction *actionAutorun;
    KSelectAction *actionDialect;
    QAction *actionLineNumbers;
    QMenu *filtersTableMenu = nullptr;
    QAction *actionMoveFilterUp = nullptr;
    QAction *actionMoveFilterDown = nullptr;
    QAction *actionInsertFilters = nullptr;

    QString lastFoundText;
    bool findIgnoreCase = false;

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
    itemList applyExpression(size_t entry, itemList src);

    /**
     * @brief apply filter chain from point and those following
     * @param entry table entry (row) number to start applying from
     */
    void applyFrom(size_t entry);

    /**
     * @brief clear the interim and final results from a given entry
     * @param entry entry number to clear from
     */
    void clearResultsAfter(size_t entry);

    /**
     * @brief clear the results field
     */
    void clearResults();

    /**
     * @brief update result display with the results of the final evaluation
     */
    void displayResult();

    /**
     * @brief common perform find
     *
     * Helper function for resultFind/resultFindNext/resultFindPrev
     * @param flags QTextDocument::FindFlag for search; default={} for forward
     */
    void doResultFind(QTextDocument::FindFlags flags={});

    /**
     * @brief save the filters to a JSON file
     * @param fileName file name to save result text to
     * @return @c true if the file was opened, and the filters saved; otherwise @c false
     */
    bool doSaveFilters(const QString& fileName);

    /**
     * @brief save the result text to a file
     * @param fileName file name to save result text to
     * @return @c true if the file was opened, and the text saved; otherwise @c false
     */
    bool doSaveResult(const QString& fileName);

    void insertFiltersAt(int at, const filterData& fData);

    QString getFilterFile();
    bool loadFiltersTable(const QString& localFile);
    bool loadFiltersTable(const filterData& filters);
    filterData loadFiltersFile(const QString& fileName);

    /**
     * @brief run expressions, if auto-apply option is enabled
     * @param entry table entry number to start applying from
     */
    void maybeAutoApply(int entry);

    void swapFiltersRows(int a, int b);
    filterEntry getFilterRow(int row);
    void setFilterRow(int row, filterEntry entry);

    /**
     * override base paintEvent
     */
    void showEvent(QShowEvent *ev) override;

    void updateApplicationTitle();

    /**
     * @brief validate expressions in the expressions table
     * @param entry table entry number to validate from
     * @return @c true if all expressions (from @p entry) are valid; @c false if any
     * expressions are invalid
     */
    bool validateExpressions(int entry=0);
};

class FindDialog : public QDialog
{
    Q_OBJECT

public:
    FindDialog(QWidget *parent = nullptr, QString text={});
    QString getFindText() const {return findText;}
    bool getIgnoreCase() const;
    void setIgnoreCase(bool ic);
    // void setFindText(QString text);

public slots:
    void findClicked();

private:
    QPushButton *findButton;
    QCheckBox *findIgnoreCase;
    QLineEdit *lineEdit;
    QString findText;
};

#endif // MAINWIDGET_H
