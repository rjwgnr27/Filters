/****************************************************************************
 ** A program to interactively apply regular expressions to an input file.
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

#include "filters.h"
#include "mainwidget.h"
#include "ui_mainwidget.h"

#include <QCheckBox>
#include <QClipboard>
#include <QtConcurrent>
#include <QDebug>
#include <QFileDialog>
#include <QFontDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPair>
#include <QPushButton>
#include <QStatusBar>
#include <QTextStream>

#include <KAboutData>
#include <KActionCollection>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KSelectAction>
#include <KStandardAction>

#include <exception>
#include <iostream>
#include <utility>

Filters::Filters(const commandLineOptions& opts, QWidget *parent) :
        KXmlGuiWindow(parent), m_ui(new mainWidget(this))
{
        setCentralWidget(m_ui);
    setupActions();
    setStandardToolBarMenuEnabled(true);
    StandardWindowOptions options = Default;
    options.setFlag(ToolBar, false);
    setupGUI(options);
    m_ui->initialLoad(opts);
}


void Filters::closeEvent(QCloseEvent *event)
{
    m_ui->recentFileAction->saveEntries(
            KConfigGroup(KSharedConfig::openConfig(), QStringLiteral("RecentURLs")));
    m_ui->recentFiltersAction->saveEntries(
            KConfigGroup(KSharedConfig::openConfig(), QStringLiteral("RecentFilters")));
    QWidget::closeEvent(event);
}

void Filters::setupActions()
{
    KActionCollection *ac = actionCollection();
    KStandardAction::quit(this, SLOT(close()), ac);
    m_ui->setupActions();

    stateChanged(QStringLiteral("results_save"), StateReverse);
    stateChanged(QStringLiteral("filters_save"), StateReverse);
}

void mainWidget::setupActions()
{
    KActionCollection *ac = mainWindow->actionCollection();

    /***********************/
    /***    File menu    ***/
    KStandardAction::open(this, SLOT(loadSubjectFile()), ac);

    recentFileAction = KStandardAction::openRecent(this,
                                SLOT(loadRecentSubject(const QUrl&)), ac);
    recentFileAction->loadEntries(KConfigGroup(KSharedConfig::openConfig(),
                                QStringLiteral("RecentURLs")));

    actionLoadFromClipboard = ac->addAction("file_load_from_clipboard", this, SLOT(loadSubjectFromCB()));
    actionLoadFromClipboard->setText(i18n("Load from clipboard"));
    actionLoadFromClipboard->setWhatsThis(i18n("Set subject to text contents of the clipboard"));
    actionLoadFromClipboard->setToolTip(i18n("Set subject to clipboard"));
    //actionLoadFromClipboard->setDefaultShortcut();
    //actionLoadFromClipboard->setIcon();
    //actionLoadFromClipboard->setEnabled(false);

    actionSaveResults = ac->addAction(QStringLiteral("save_result"), this, SLOT(saveResult()));
    actionSaveResults->setText(i18n("Save Result..."));
    actionSaveResults->setToolTip(i18n("Save the filtered result."));
    actionSaveResults->setWhatsThis(i18n("Save the results of applying the filters to the source."));
    //actionSaveResults->setDefaultShortcut(actionSaveResults, QKeySequence(QStringLiteral("Ctrl+N"));
    actionSaveResults->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    actionSaveResults->setEnabled(false);

    actionSaveResultsAs = ac->addAction(QStringLiteral("save_result_as"), this, SLOT(saveResultAs()));
    actionSaveResultsAs->setText(i18n("Save Result As..."));
    actionSaveResultsAs->setToolTip(i18n("Save the result as a new file."));
    actionSaveResultsAs->setWhatsThis(i18n("Save the results of applying the filters to the source as a new file."));
    //ac->setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+N"));
    actionSaveResultsAs->setIcon(QIcon::fromTheme(QStringLiteral("document-save-as")));
    actionSaveResultsAs->setEnabled(false);

    /***********************/
    /***   Edit menu     ***/

    /***********************/
    /***   Filters menu  ***/
    actionRun = ac->addAction(QStringLiteral("run_filters"), this, SLOT(runButtonClicked()));
    actionRun->setText(i18n("Run filters"));
    actionRun->setToolTip(i18n("Run the filters against the input"));
    //actionRun->setWhatsThis(i18n(""));
    ac->setDefaultShortcut(actionRun, QKeySequence(QStringLiteral("Ctrl+R")));
    actionRun->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));

    actionAutorun = ac->addAction(QStringLiteral("auto_run_filters"), this, SLOT(autoRunClicked()));
    actionAutorun->setText(i18n("Autorun filters"));
    actionAutorun->setCheckable(true);
    actionAutorun->setChecked(false);
    actionAutorun->setToolTip(i18n("Auto-run the filters on any change"));
    actionAutorun->setWhatsThis(i18n("When set, any change will cause the filters to be re-applied"));
    //ac->setDefaultShortcut(actionAutorun, QKeySequence(QStringLiteral("Ctrl+"));
    actionAutorun->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));

    actionDialect = new KSelectAction(i18n("Dialect"), this);
    actionDialect->addAction(QStringLiteral("QRegularExpression"));
    actionDialect->setCurrentItem(0);
    QAction *action = ac->addAction(QStringLiteral("re_dialect"), actionDialect);
    connect(actionDialect, SIGNAL(triggered(const QString&)), this, SLOT(dialectChanged(const QString&)));
    action->setText(i18n("Dialect"));
    action->setToolTip(i18n("Select the RE dialect used"));
    //action->setWhatsThis(i18n(""));
    //action->setIcon(QIcon::fromTheme(QStringLiteral("")));
    //ac->setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+"));

    action = ac->addAction(QStringLiteral("load_filters"), this, SLOT(loadFilters()));
    action->setText(i18n("Load Filters..."));
    action->setToolTip(i18n("Replace current filter list with contents of a file."));
    action->setWhatsThis(i18n("Load a filter list from a file, replacing the current filter "
                              "set with the contents."));
    //ac->setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+N"));
    //action->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));

    recentFiltersAction = reinterpret_cast<KRecentFilesAction*>(ac->addAction(
        KStandardAction::OpenRecent, QStringLiteral("load_filters_recent"),
                            this, SLOT(loadFiltersTable(const QUrl&))));
    recentFiltersAction->setText(i18n("Load Recent Filters..."));
    recentFiltersAction->setToolTip(i18n("Replace current filter list a recent file."));
    recentFiltersAction->setWhatsThis(i18n("Load a filter list from a recent file, replacing "
                                            "the current filter set with the contents."));
    //ac->setDefaultShortcut(recentFiltersAction, QKeySequence(QStringLiteral("Ctrl+N"));
    //recentFiltersAction->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    recentFiltersAction->loadEntries(KConfigGroup(KSharedConfig::openConfig(),
                                            QStringLiteral("RecentFilters")));

    actionSaveFilters = ac->addAction(QStringLiteral("save_filters"), this, SLOT(saveFilters()));
    actionSaveFilters->setText(i18n("Save Filters..."));
    actionSaveFilters->setToolTip(i18n("Save the filters to a file."));
    actionSaveFilters->setWhatsThis(i18n("Save the filter list to a file."));
    //ac->setDefaultShortcut(actionSaveFilters, QKeySequence(QStringLiteral("Ctrl+N"));
    actionSaveFilters->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));

    actionSaveFiltersAs = ac->addAction(QStringLiteral("save_filters_as"), this, SLOT(saveFiltersAs()));
    actionSaveFiltersAs->setText(i18n("Save Filters As..."));
    actionSaveFiltersAs->setToolTip(i18n("Save the filters as a new file."));
    actionSaveFiltersAs->setWhatsThis(i18n("Save the filter list as a new file."));
    //ac->setDefaultShortcut(actionSaveFiltersAs, QKeySequence(QStringLiteral("Ctrl+N"));
    actionSaveFiltersAs->setIcon(QIcon::fromTheme(QStringLiteral("document-save-as")));

    action = ac->addAction(QStringLiteral("clear_filters"), this, SLOT(clearFilters()));
    action->setText(i18n("Clear"));
    action->setToolTip(i18n("Clears the filters table"));
    //action->setWhatsThis(i18n(""));
    //ac->setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+N"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("delete-table")));

    /**********************/
    /*** Result Menu  ***/
    action = KStandardAction::find(this, SLOT(resultFind()), ac);
    action->setText(i18n("Result &Find..."));
    action->setToolTip(i18n("Find text in result"));

    action = KStandardAction::findNext(this, SLOT(resultFindNext()), ac);
    action->setText(i18n("Result Find &Next"));
    action->setToolTip(i18n("Find next occurrence of the find text in result"));

    action = KStandardAction::findPrev(this, SLOT(resultFindPrev()), ac);
    action->setText(i18n("Result Find &Previous"));
    action->setToolTip(i18n("Find previous occurrence of the find text in result"));

    /**********************/
    /*** Settings Menu  ***/
    actionLineNumbers = ac->addAction(QStringLiteral("show_line_numbers"));
    actionLineNumbers->setIcon(QIcon::fromTheme(QStringLiteral("list-ol")));
    actionLineNumbers->setText(i18n("Show &Line Numbers"));
    actionLineNumbers->setToolTip(i18n("Toggle showing of source line numbers"));
    actionLineNumbers->setCheckable(true);
    actionLineNumbers->setChecked(true);

    action = ac->addAction(QStringLiteral("filter_font"), this, SLOT(selectFilterFont()));
    action->setText(i18n("Filter Font..."));
    action->setToolTip(i18n("Select the font for the filters table"));

    action = ac->addAction(QStringLiteral("result_font"), this, SLOT(selectResultFont()));
    action->setText(i18n("Result Font..."));
    action->setToolTip(i18n("Select the font for the results table"));

    /***********************************/
    /*** Filters table context menu  ***/
    filtersTableMenu = new QMenu(filtersTable);
    filtersTableMenu->setTitle(i18n("Filters..."));

    action = filtersTableMenu->addAction(i18n("Insert Row"), this,
                SLOT(insertEmptyFilterAbove()));
    ac->addAction(QStringLiteral("insert_row"), action);
    action->setToolTip(i18n("Insert an empty entry above current"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("insert-table-row")));

    action = filtersTableMenu->addAction(i18n("Delete Row"), this,
                SLOT(deleteFilterRow()));
    ac->addAction(QStringLiteral("delete_row"), action);
    action->setToolTip(i18n("Delete current row"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-table-delete-row")));

    action = filtersTableMenu->addAction(i18n("Clear Row"), this, SLOT(clearFilterRow()));
    ac->addAction(QStringLiteral("clear_row"), action);
    action->setToolTip(i18n("Clear current row"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-table-delete-row")));

    filtersTableMenu->addSeparator();

    actionMoveFilterUp = filtersTableMenu->addAction(i18n("Move Up"), this,
                SLOT(moveFilterUp()));
    actionMoveFilterUp->setToolTip(i18n("Move current filter up"));
    actionMoveFilterUp->setIcon(QIcon::fromTheme(QStringLiteral("usermenu-up")));

    actionMoveFilterDown = filtersTableMenu->addAction(i18n("Move Down"), this,
                SLOT(moveFilterDown()));
    actionMoveFilterDown->setToolTip(i18n("Move current filter down"));
    actionMoveFilterDown->setIcon(QIcon::fromTheme(QStringLiteral("usermenu-down")));

    filtersTableMenu->addSeparator();

    actionInsertFilters = filtersTableMenu->addAction(i18n("Insert File ..."), this,
                SLOT(insertFiltersAbove()));
    ac->addAction(QStringLiteral("insert_filters"), actionInsertFilters);
    actionInsertFilters->setToolTip(i18n("Insert filter file above the current row"));
    actionInsertFilters->setIcon(QIcon::fromTheme(QStringLiteral("edit-table-insert-row-above")));
    //ac->setDefaultShortcut(action, QKeySequence(QStringLiteral(""));
}

mainWidget::mainWidget(KXmlGuiWindow *main, QWidget *parent) :
    QWidget(parent), mainWindow(main), status(new QLabel)
{
    setupUi(this);

    mainWindow->statusBar()->insertWidget(0, status);

    filtersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    filtersTable->horizontalHeaderItem(ColEnable)->setTextAlignment(Qt::AlignLeft);
    filtersTable->horizontalHeaderItem(ColEnable)->setToolTip(
        i18n("Expression entry is enabled when checked"));

    filtersTable->horizontalHeaderItem(ColExclude)->setTextAlignment(Qt::AlignLeft);
    filtersTable->horizontalHeaderItem(ColExclude)->setToolTip(
        i18n("Exclude matching lines when checked"));

    filtersTable->horizontalHeaderItem(ColCaseIgnore)->setTextAlignment(Qt::AlignLeft);
    filtersTable->horizontalHeaderItem(ColCaseIgnore)->setToolTip(
        i18n("Use case insensitive matching when checked"));

    filtersTable->horizontalHeaderItem(ColRegEx)->setToolTip(i18n("Regular expression string"));
    appendEmptyRow();
}

void mainWidget::updateApplicationTitle()
{
    mainWindow->setCaption(titleFile, subjModified | reModified);
}

bool mainWidget::initialLoad(const commandLineOptions& opts)
{
    if (opts.stdin)
      QMessageBox::warning(this, i18n("Option Not Supported"),
                   i18n("--stdin option not supported in graphic mode"));

    if (!opts.subjectFile.isEmpty()) {
        if (!loadSubjectFile(opts.subjectFile))
            QMessageBox::warning(this, i18n("Could Not Load"),
                    i18n("Subject file '%1' could not be loaded").arg(opts.subjectFile));
    }

    if (!opts.filters.empty()) {
        filterData data;
        bool initial = true;
        for (const QString& fileName : opts.filters) {
            filterData t = loadFiltersFile(fileName);
            if (!t.valid) {
                QMessageBox::critical(this, i18n("Not Valid"),
                                    i18n("Filter file '%1' is not valid").arg(fileName));
                return false;
            }

            if (initial) {
                initial = false;
                data = t;
            } else if (t.dialect != data.dialect) {
                QMessageBox::critical(this, i18n("Dialect Mismatch"),
                    i18n("Dialect of '%1' does not match previous files").arg(fileName));
                return false;
            } else
                data.filters << t.filters;
        }
        if (!loadFiltersTable(data)) {
            QMessageBox::critical(this, i18n("Load Failure"),
                                i18n("Could not load initial filters"));
            return false;
        }
        doInitialApply = !opts.subjectFile.isEmpty();
    }

    actionAutorun->setChecked(opts.autoRun);
    actionRun->setEnabled(!opts.autoRun);

    return true;
}

void mainWidget::showEvent(QShowEvent *ev)
{
    QWidget::showEvent(ev);
    if (doInitialApply) {
        doInitialApply = false;
        QMetaObject::invokeMethod(this, [this](){applyFrom(0);},
                        Qt::ConnectionType::QueuedConnection);
    }
}

void mainWidget::loadSubjectFile()
{
    loadSubjectFile(QFileDialog::getOpenFileName(this, i18n("Open Subject File")));
}

bool mainWidget::loadSubjectFile(const QString& localFile)
{
    if (localFile.isEmpty())
        return true;

    QFile source(localFile);
    if (source.open(QIODevice::ReadOnly | QIODevice::Text)) {
        titleFile = QFileInfo(localFile).fileName();
        subjModified = false;
        resultFileName.clear();
        updateApplicationTitle();
        sourceLineCount = -1;
        clearResultsAfter(0);
        QTextStream stream(&source);
        itemList lines;
        sourceLineCount = 0;
        while(!stream.atEnd())
            lines << textItem{++sourceLineCount, stream.readLine()};
        stepResults[0] = std::move(lines);
        recentFileAction->addUrl(QUrl::fromLocalFile(localFile));
        // FIXME: After loading huge file, this line hangs:
        //status->setText(QStringLiteral("%1: %2 lines").arg(localFile, sourceLineCount));
        maybeAutoApply(0);
        return true;
    } else {
        status->setText(QStringLiteral("open '%1' failed").arg(localFile));
        return false;
    }
}

void mainWidget::loadRecentSubject(const QUrl& url)
{
    loadSubjectFile(url.toLocalFile());
}

void mainWidget::loadSubjectFromCB()
{
    const QClipboard *clipboard = QApplication::clipboard();
    QString text = clipboard->text();
    if (text.isEmpty()) {
        QMessageBox::information(this, i18n("No Data"),
                              i18n("Clipboard does not contain text data"));
        return;
    }

    QTextStream stream(&text, QIODevice::ReadOnly);
    itemList lines;
    int srcLine = 0;
    while (!stream.atEnd())
        lines << textItem{++srcLine, stream.readLine()};

    titleFile = i18n("<clipboard>");
    subjModified = false;
    updateApplicationTitle();
    sourceLineCount = -1;
    clearResultsAfter(0);
    stepResults[0] = lines;
    sourceLineCount = lines.size();
    status->setText(QStringLiteral("%1: %2 lines").arg(titleFile, sourceLineCount ));
    maybeAutoApply(0);
}


itemList mainWidget::applyExpression(size_t entry, itemList src)
{
    if (src.empty())
        return src;

    auto table = filtersTable;
    size_t rows = table->rowCount();
    if (entry >= rows) {
        qWarning() << QStringLiteral("Range failure %1/%2").arg(entry).arg(rows);
        return itemList{};
    }

    if (table->item(entry, ColEnable)->checkState() != Qt::Checked)
        return src;

    const auto item = table->item(entry, ColRegEx);
    if (!item || item->text().isEmpty())
        return src;

    auto reStr = item->text();
    bool exclude = table->item(entry, ColExclude)->checkState() == Qt::Checked;
    QRegularExpression::PatternOptions pOpts = QRegularExpression::NoPatternOption;
    if (table->item(entry, ColCaseIgnore)->checkState() == Qt::Checked)
        pOpts |= QRegularExpression::CaseInsensitiveOption;
    QRegularExpression re(reStr, pOpts);
    if (!re.isValid())
        return src;

    re.optimize();
    return QtConcurrent::blockingFiltered(src,
        [&re, exclude](const textItem& item) -> bool {return re.match(item.text).hasMatch() ^ exclude;});
}


void mainWidget::maybeAutoApply(int entry)
{
    if (actionAutorun->isChecked())
        applyFrom(entry);
}

void mainWidget::applyFrom(size_t start)
{
    clearResultsAfter(start);
    if (stepResults.size() > start) {
        if (!validateExpressions(start))
            return;

        size_t rows = filtersTable->rowCount();
        QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        qApp->processEvents();
        stepResults.resize(rows+1);
        for (size_t row = start; row < rows; ++row) {
            auto result = applyExpression(row, stepResults[row]);
            subjModified |= stepResults[row].size() != result.size();
            stepResults[row+1] = result;
        }
        updateApplicationTitle();
        qApp->processEvents();
        displayResult();
        QGuiApplication::restoreOverrideCursor();
    } else
        qWarning() << QStringLiteral("No source entry %1/%2").arg(start).arg(stepResults.size());
}

bool mainWidget::validateExpressions(int entry)
{
    auto table = filtersTable;
    for (int rows = table->rowCount(); entry < rows; ++entry) {
        if (table->item(entry, ColEnable)->checkState() == Qt::Checked) {
            auto reStr = table->item(entry, ColRegEx)->text();
            if (!reStr.isEmpty()) {
                QRegularExpression re(reStr);
                if (!re.isValid()) {
                    status->setText(QStringLiteral("Invalid RE at %1: '%2'")
                            .arg(entry).arg(re.errorString()));
                    table->setCurrentCell(entry, ColRegEx);
                    return false;
                }
            }
        }
    }
    return true;
}

void mainWidget::clearResultsAfter(size_t entry)
{
    int rowCount = filtersTable->rowCount() + 1;
    stepResults.resize(rowCount);
    for (int i = entry + 1; i < rowCount; ++i)
        stepResults[i].clear();
    clearResults();
    status->clear();
}

void mainWidget::clearResults()
{
    result->clear();
    actionSaveResults->setEnabled(false);
    actionSaveResultsAs->setEnabled(false);
}

void mainWidget::displayResult()
{
    result->setUpdatesEnabled(false);
    result->clear();
    size_t resultLines = 0;
    if (!stepResults.empty()) {
        const itemList& items = stepResults.back();
        sourceLineMap.resize(items.size());
        std::vector<int>::iterator mapIt = sourceLineMap.begin();
#if 1
        QString line;
        if (false && actionLineNumbers->isChecked()) {
            /* joining all the lines first appears to be faster than using
             * appendPlainText() in a loop */
            int width = QString("%1").arg(items.back().srcLineNumber).size();
            for (const auto& item : qAsConst(items)) {
                QString str = QString("%1| %2\n").arg(item.srcLineNumber, width).arg(item.text);
                line += str;
                *(mapIt++) = item.srcLineNumber;
            }
        } else {
            for (auto item : qAsConst(items)) {
                line += item.text + QChar('\n');
                *(mapIt++) = item.srcLineNumber;
            }
        }
        result->setPlainText(line);
#else
        if (false && actionLineNumbers->isChecked()) {
            auto lineNo = items.back().srcLineNumber;
            int width = QString("%1").arg(lineNo).size();
            for (const auto& item : items) {
                result->appendPlainText(QString("%1| ").arg(item.srcLineNumber, width) + item.text);
                *(mapIt++) = item.srcLineNumber;
            }
        } else {
            for (const auto& item : items) {
                result->appendPlainText(item.text);
                *(mapIt++) = item.srcLineNumber;
            }
        }
#endif
        resultLines = items.size();
        actionSaveResults->setEnabled(true);
        actionSaveResultsAs->setEnabled(true);
    }
    status->setText(QStringLiteral("Source: %1, final %2 lines").arg(sourceLineCount).arg(resultLines));
    result->setUpdatesEnabled(true);
}

void mainWidget::clearFilters()
{
    QSignalBlocker blocker(filtersTable);
    filtersFileName.clear();
    filtersTable->setRowCount(0);
    appendEmptyRow();
}

void mainWidget::appendEmptyRow()
{
    insertEmptyRowAt(filtersTable->rowCount());
}

void mainWidget::insertEmptyRowAt(int row)
{
    QSignalBlocker blocker(filtersTable);
    filtersTable->insertRow(row);
    QTableWidgetItem *item = new QTableWidgetItem();
    item->setCheckState(Qt::Checked);
    filtersTable->setItem(row, ColEnable, item);

    item = new QTableWidgetItem();
    item->setCheckState(Qt::Unchecked);
    filtersTable->setItem(row, ColExclude, item);

    item = new QTableWidgetItem();
    item->setCheckState(Qt::Unchecked);
    filtersTable->setItem(row, ColCaseIgnore, item);

    item = new QTableWidgetItem();
    filtersTable->setItem(row, ColRegEx, item);
    filtersTable->setCurrentCell(row, ColRegEx);
}

void mainWidget::insertEmptyFilterAbove()
{
    int row = filtersTable->currentRow();
    if (row >= 0 && row < filtersTable->rowCount()) {
        insertEmptyRowAt(row);
        maybeAutoApply(row);
    }
}

void mainWidget::deleteFilterRow()
{
    int row = filtersTable->currentRow();
    if (row >= 0 && row < filtersTable->rowCount()) {
        QSignalBlocker blocker(filtersTable);
        filtersTable->removeRow(row);
        if (filtersTable->rowCount() == 0)
            appendEmptyRow();
        blocker.unblock();
        maybeAutoApply(row);
    }
}

void mainWidget::clearFilterRow()
{
    int row = filtersTable->currentRow();
    if (row >= 0 && row < filtersTable->rowCount()) {
        setFilterRow(row, filterEntry());
        maybeAutoApply(row);
    }
}

void mainWidget::moveFilterUp()
{
    int row = filtersTable->currentRow();
    if (row < 1 || row >= filtersTable->rowCount())
        return;
    swapFiltersRows(row, row - 1);
}

void mainWidget::moveFilterDown()
{
    int row = filtersTable->currentRow();
    if (row < 0 || row >= filtersTable->rowCount() - 1)
        return;
    swapFiltersRows(row, row + 1);
}

filterEntry mainWidget::getFilterRow(int row)
{
    filterEntry entry;
    entry.enabled = filtersTable->item(row, ColEnable)->checkState() == Qt::Checked;
    entry.exclude = filtersTable->item(row, ColExclude)->checkState() == Qt::Checked;
    entry.ignoreCase = filtersTable->item(row, ColCaseIgnore)->checkState() == Qt::Checked;
    entry.re = filtersTable->item(row, ColRegEx)->text();
    return entry;
}

void mainWidget::setFilterRow(int row, filterEntry entry)
{
    QSignalBlocker blocker(filtersTable);
    filtersTable->item(row, ColEnable)->setCheckState(entry.enabled ? Qt::Checked : Qt::Unchecked);
    filtersTable->item(row, ColExclude)->setCheckState(entry.exclude ? Qt::Checked : Qt::Unchecked);
    filtersTable->item(row, ColCaseIgnore)->setCheckState(entry.ignoreCase ? Qt::Checked : Qt::Unchecked);
    filtersTable->item(row, ColRegEx)->setText(entry.re);
}

void mainWidget::swapFiltersRows(int a, int b)
{
    auto temp = getFilterRow(a);
    setFilterRow(a, getFilterRow(b));
    setFilterRow(b, temp);
    maybeAutoApply(std::min(a, b));
}

void mainWidget::autoRunClicked()
{
    bool checked = actionAutorun->isChecked();
    actionRun->setEnabled(!checked);
    if (checked)
        applyFrom(0);
}

void mainWidget::dialectChanged(QString text)
{
    Q_UNUSED(text)
    maybeAutoApply(0);
}

void mainWidget::runButtonClicked()
{
    applyFrom(0);
}

void mainWidget::tableItemChanged(QTableWidgetItem *item)
{
    QSignalBlocker blocker(filtersTable);
    status->clear();
    auto tbl = filtersTable;
    if (item->column() == ColRegEx) {
        int lastRow = tbl->rowCount() - 1;
        if (item->text().isEmpty()) {
            if (item->row() == (lastRow - 1) && tbl->item(lastRow, ColRegEx)->text().isEmpty())
                tbl->removeRow(lastRow);
        } else {
            QRegularExpression re(item->text());
            if (re.isValid()) {
                item->setForeground(QBrush());
                item->setToolTip(QString());
                item->setIcon(QIcon());
                maybeAutoApply(item->row());
            } else {
                clearResultsAfter(item->row());
                status->setText(QStringLiteral("<span style=\"color:red;\">bad RE: '%1'</span>").arg(re.errorString()));
                QBrush brush = item->foreground();
                brush.setColor(Qt::red);
                item->setForeground(brush);
                item->setToolTip(re.errorString());
                item->setIcon(QIcon::fromTheme(QStringLiteral("error")));
            }
            if (item->row() == lastRow)
                appendEmptyRow();
        }
    } else
        maybeAutoApply(item->row());
}


QString mainWidget::getFilterFile()
{
    return QFileDialog::getOpenFileName(this, i18n("Open Filter File"), QString(),
                QStringLiteral("JSON files (*.json)"));
}

void mainWidget::insertFiltersAbove()
{
    int row = filtersTable->currentRow();
    if (row < 0 || row >= filtersTable->rowCount())
        return;

    QString fileName = getFilterFile();
    if (!fileName.isEmpty()) {
        filterData filters = loadFiltersFile(fileName);
        if (filters.valid) {
            QSignalBlocker blocker(filtersTable);
            insertFiltersAt(row, filters);
            filtersTable->setCurrentCell(row, filtersTable->currentColumn());
            maybeAutoApply(row);
        }
    }
}

void mainWidget::loadFilters()
{
    QString fileName = getFilterFile();
    if (!fileName.isEmpty()) {
        if (loadFiltersTable(fileName))
            recentFiltersAction->addUrl(QUrl::fromLocalFile(fileName));
    }
}

void mainWidget::loadFiltersTable(const QUrl& url)
{
    loadFiltersTable(url.toLocalFile());
}

bool mainWidget::loadFiltersTable(const QString& fileName)
{
    return loadFiltersTable(loadFiltersFile(fileName));
}

bool mainWidget::loadFiltersTable(const filterData& filters)
{
    if (filters.valid) {
        QSignalBlocker blocker(filtersTable);
        filtersTable->setRowCount(0);
        insertFiltersAt(0, filters);
        appendEmptyRow();
        maybeAutoApply(0);
    } else
        qWarning() << "JSON file is not valid";
    return filters.valid;
}

filterData mainWidget::loadFiltersFile(const QString& fileName)
{
    filterData result;

    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc(QJsonDocument::fromJson(data));
        QJsonObject filters = doc.object();
        if (filters.contains("dialect") && filters["dialect"].isString())
            result.dialect = filters["dialect"].toString();

        if (filters.contains("filters") && filters["filters"].isArray()) {
            const QJsonArray filterArray = filters["filters"].toArray();
            for (const auto& entry : qAsConst(filterArray))
                result.filters << filterEntry::fromJson(entry.toObject());
            result.valid = true;
            filtersFileName = fileName;
        }
    } else
        qWarning() << QStringLiteral("Failed to open '%1'").arg(fileName);
    return result;
}

void mainWidget::insertFiltersAt(int at, const filterData& fData)
{
    if (!fData.dialect.isEmpty())
        actionDialect->setCurrentAction(fData.dialect);

    for (const auto& entry : qAsConst(fData.filters)) {
        insertEmptyRowAt(at);
        setFilterRow(at, entry);
        ++at;
    }
}

void mainWidget::saveFilters()
{
    if (filtersFileName.isEmpty())
        saveFiltersAs();
    else
        doSaveFilters(filtersFileName);
}

void mainWidget::saveFiltersAs()
{
    QString localFile = QFileDialog::getSaveFileName(this, "Save Filters To",
                    QString(), i18n("JSON file (*.json);;All files (*.*)"));
    if (!localFile.isEmpty()) {
        if (doSaveFilters(localFile)) {
            filtersFileName = localFile;
            recentFiltersAction->addUrl(QUrl::fromLocalFile(localFile));
        }
    }
}

bool mainWidget::doSaveFilters(const QString& fileName)
{
    QFile dest(fileName);
    if (dest.open(QIODevice::WriteOnly | QIODevice::Text)) {
        int rows = filtersTable->rowCount();
        QJsonArray filterArray;
        for (int row = 0; row < rows; ++row) {
            if (!filtersTable->item(row, ColRegEx)->text().isEmpty()) {
                filterEntry filter = getFilterRow(row);
                filterArray.append(filter.toJson());
            }
        }
        const KAboutData& app = KAboutData::applicationData();
        QJsonObject about;
        about["application"] = app.componentName();
        about["version"] = app.version();
        QJsonObject filters;
        filters["about"] = about;
        filters["dialect"] = actionDialect->currentText();
        filters["filters"] = filterArray;
        auto jDoc = QJsonDocument(filters).toJson();
        bool success = dest.write(jDoc) == jDoc.size();
        reModified &= !success;
        return success;
    }
    return false;
}

void mainWidget::gotoLine()
{
    bool ok;
    auto cursor = result->textCursor();
    int lineNo = QInputDialog::getInt(this, tr("Go to line"), tr("Source line number:"),
                                      cursor.blockNumber(), 1, result->blockCount(), 1, &ok);
    if (ok) {
//        cursor.movePosition(..);
        result->setTextCursor(cursor);
    }
}

void mainWidget::resultFind()
{
    FindDialog dialog(this, lastFoundText);
    dialog.setIgnoreCase(findIgnoreCase);
    if (dialog.exec() == QDialog::Accepted) {
        QString text = dialog.getFindText();
        if (text.size() > 0) {
            lastFoundText = text;
            findIgnoreCase = dialog.getIgnoreCase();
            doResultFind();
        }
    }
}

void mainWidget::resultFindNext()
{
    doResultFind();
}

void mainWidget::resultFindPrev()
{
    doResultFind(QTextDocument::FindBackward);
}

void mainWidget::doResultFind(QTextDocument::FindFlags flags)
{
    if (!lastFoundText.isEmpty())
        if (!result->find(lastFoundText, flags)) {
            if (!findIgnoreCase)
                flags |= QTextDocument::FindFlag::FindCaseSensitively;
            QMessageBox::information(this, tr("Search failed"), tr("Text not found"));
        }
}

void mainWidget::saveResult()
{
    if (resultFileName.isEmpty())
        saveResultAs();
    else
        doSaveResult(resultFileName);
}

void mainWidget::saveResultAs()
{
    QString localFile = QFileDialog::getSaveFileName(this, "Save Results To"/*,
                            QString(), i18n("Log files (*.txt *.log);;All files (*.*)")*/);
    if (!localFile.isEmpty())
        if (doSaveResult(localFile))
            resultFileName = localFile;
}

bool mainWidget::doSaveResult(const QString& fileName)
{
    QFile dest(fileName);
    bool saved = dest.open(QIODevice::WriteOnly | QIODevice::Text) &&
           dest.write((result->toPlainText() + QLatin1Char('\n')).toLatin1()) >= 0;
    subjModified = !saved;
    if (saved) {
        titleFile = QFileInfo(fileName).fileName();
        updateApplicationTitle();
    }
    return saved;
}

void mainWidget::filtersTableMenuRequested(QPoint point)
{
    int row = filtersTable->currentRow();
    if (row < 0 || row >= filtersTable->rowCount())
        return;
    actionMoveFilterUp->setEnabled(row > 0);
    actionMoveFilterDown->setEnabled(row < filtersTable->rowCount()-1);
    filtersTableMenu->exec(filtersTable->mapToGlobal(point));
}

void mainWidget::selectFilterFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, filtersTable->font(), this);
    if (ok)
        filtersTable->setFont(font);
}

void mainWidget::selectResultFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, result->font(), this);
    if (ok)
        result->setFont(font);
}

QJsonObject filterEntry::toJson() const
{
    QJsonObject filter;
    filter["enabled"] = enabled;
    filter["exclude"] = exclude;
    filter["ignore_case"] = ignoreCase;
    filter["regexp"] = re;
    return filter;
}

filterEntry filterEntry::fromJson(const QJsonObject& jentry)
{
    filterEntry entry;
    entry.enabled = jentry["enabled"].toBool();
    entry.exclude = jentry["exclude"].toBool();
    entry.ignoreCase = jentry["ignore_case"].toBool();
    entry.re = jentry["regexp"].toString();
    return entry;
}

#include "moc_mainwidget.cpp"

class batchException : public std::exception
{
protected:
    std::string errorString;

public:
    batchException(QString const& str) : errorString(str.toUtf8().constData()) {}
    virtual ~batchException() {}
    virtual const char* what() const noexcept override {return errorString.c_str();}
};

class filterLoadException : public batchException
{
public:
    filterLoadException(QString const& str) :
        batchException(QStringLiteral("Error loading filter file: %1").arg(str)) {}
    virtual ~filterLoadException() {}
};

class loadDialectException : public batchException
{
public:
    loadDialectException(QString const& str) :
        batchException(QStringLiteral("Dialect mismatch loading filter file: %1").arg(str)) {}
    virtual ~loadDialectException() {}
};

class dialectTypeException : public batchException
{
public:
    dialectTypeException(QString const& str) :
        batchException(QStringLiteral("Unsupported dialect: %1").arg(str)) {}
    virtual ~dialectTypeException() {}
};

class badRegexException : public batchException
{
public:
    badRegexException(QString const& str) :
        batchException(QStringLiteral("bad regular expression: '%1'").arg(str)) {}
    virtual ~badRegexException() {}
};

class subjectLoadException : public batchException
{
public:
    subjectLoadException(QString const& str) :
        batchException(QStringLiteral("Error loading subject file: %1").arg(str)) {}
    virtual ~subjectLoadException() {}
};


static filterData batchLoadFilterFile(const QString& fileName)
{
    filterData result;

    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc(QJsonDocument::fromJson(data));
        QJsonObject filters = doc.object();
        if (filters.contains("dialect") && filters["dialect"].isString())
            result.dialect = filters["dialect"].toString();

        if (filters.contains("filters") && filters["filters"].isArray()) {
            const QJsonArray filterArray = filters["filters"].toArray();
            for (const auto& entry : filterArray)
                result.filters << filterEntry::fromJson(entry.toObject());
            result.valid = true;
        }
    }
    if (!result.valid)
        throw filterLoadException(fileName);
    return result;
}


static filterData batchLoadFilters(const commandLineOptions& opts)
{
    filterData result;
    bool initial = true;
    for (const QString& fileName : opts.filters) {
        filterData t = batchLoadFilterFile(fileName);
        if (initial) {
            initial = false;
            result = t;
        } else if (t.dialect == result.dialect)
            result.filters << t.filters;
        else
            throw loadDialectException(fileName);
    }
    return result;
}


static itemList batchLoadSubjectFile(const commandLineOptions& opts)
{
    QFile source(opts.subjectFile);
    if (source.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&source);
        itemList lines;
        int srcLine = 0;
        while(!stream.atEnd())
          lines << textItem{++srcLine, stream.readLine()};
        return lines;
    }
    throw subjectLoadException(opts.subjectFile);
}

static itemList readStdIn()
{
  itemList lines;
  int srcLine = 0;
  while(std::cin) {
    std::string line;
    std::getline(std::cin, line);
    lines << textItem{++srcLine, QString::fromStdString(line)};
  }
  return lines;
}


static itemList batchApplyQRegularExpressions(const filterData& filters, itemList items)
{
    for (const filterEntry& entry : filters.filters) {
        if (entry.enabled) {
            bool exclude = entry.exclude;
            QRegularExpression::PatternOptions pOpts = QRegularExpression::NoPatternOption;
            if (entry.ignoreCase)
                pOpts |= QRegularExpression::CaseInsensitiveOption;
            QRegularExpression re(entry.re, pOpts);
            if (!re.isValid())
                throw badRegexException(entry.re);
            re.optimize();
            items = QtConcurrent::blockingFiltered(items,
                    [&re, exclude](const textItem& item) -> bool {return re.match(item.text).hasMatch() ^ exclude;});
            if (items.empty())
                break;
        }
    }
    return items;
}

static itemList batchApplyFilters(const filterData& filters, const itemList& lines)
{
    if (filters.dialect == "QRegularExpression")
        return batchApplyQRegularExpressions(filters, itemList(lines));
    throw dialectTypeException(filters.dialect);
}

int doBatch(const commandLineOptions& opts)
{
    filterData filters;
    itemList items;
    try {
        filters = batchLoadFilters(opts);
        if (opts.stdin) 
            items = readStdIn();
        else
            items = batchLoadSubjectFile(opts);
        items = batchApplyFilters(filters, items);
        for (auto const& item : qAsConst(items))
            std::cout << item.text.toUtf8().constData() << '\n';
    }
    catch (std::exception const &except) {
        std::cerr << except.what() << std::endl;
        return -3;
    }
    return 0;
}

FindDialog::FindDialog(QWidget *parent, QString text)
    : QDialog(parent)
{
    lineEdit = new QLineEdit;
    findButton = new QPushButton(tr("&Find"));
    findIgnoreCase = new QCheckBox(tr("&Ignore case:"));

    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget(new QLabel(tr("Find:")));
    layout->addWidget(lineEdit);
    layout->addWidget(findIgnoreCase);
    layout->addWidget(findButton);

    setLayout(layout);
    setWindowTitle(tr("Find text"));
    connect(findButton, &QPushButton::clicked, this, &FindDialog::findClicked);
    connect(findButton, &QPushButton::clicked, this, &FindDialog::accept);
    lineEdit->setText(text);
}

void FindDialog::findClicked()
{
    QString text = lineEdit->text();

    if (text.isEmpty()) {
        QMessageBox::information(this, tr("Empty Search Field"),
                                 tr("Please enter a text to find."));
        return;
    } else {
        findText = text;
        lineEdit->clear();
        hide();
    }
}

bool FindDialog::getIgnoreCase() const
{
    return findIgnoreCase->checkState() == Qt::Checked;
}

void FindDialog::setIgnoreCase(bool ic)
{
    findIgnoreCase->setCheckState(ic ? Qt::Checked : Qt::Unchecked);
}

