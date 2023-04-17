#include "mainwidget.h"
#include "filters.h"

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
#include <KFind>
#include <KFindDialog>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KSelectAction>
#include <KStandardAction>
#include <KXmlGuiWindow>

mainWidget::mainWidget(KXmlGuiWindow *main, QWidget *parent) :
    QWidget(parent), mainWindow(main)
{
    setupUi();
    appendEmptyRow();
}


void mainWidget::setupUi()
{
    QVBoxLayout *verticalLayout_3 = new QVBoxLayout(this);
    verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));

    QSplitter *splitter = new QSplitter(this);
    splitter->setObjectName(QString::fromUtf8("splitter"));
    splitter->setOrientation(Qt::Vertical);

    QGroupBox *groupBox_2 = new QGroupBox(splitter);
    groupBox_2->setObjectName(QString::fromUtf8("groupBox_2"));

    QVBoxLayout *verticalLayout = new QVBoxLayout(groupBox_2);
    verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));

    filtersTable = new QTableWidget(groupBox_2);
    filtersTable->setObjectName(QString::fromUtf8("filtersTable"));
    filtersTable->setMouseTracking(true);
    filtersTable->setColumnCount(NumCol);
    filtersTable->setContextMenuPolicy(Qt::CustomContextMenu);
    filtersTable->horizontalHeader()->setStretchLastSection(true);
    filtersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto item = new QTableWidgetItem;
    item->setTextAlignment(Qt::AlignLeft);
    item->setText(QCoreApplication::translate("mainwidget", "En", nullptr));
    item->setToolTip(i18n("Expression entry is enabled when checked"));
    item->setWhatsThis(i18n("When checked, this filter is enabled, and will be applied "
    "to the result. When unchecked, this entry is not used."));
    filtersTable->setHorizontalHeaderItem(ColEnable, item);

    item = new QTableWidgetItem;
    item->setText(QCoreApplication::translate("mainwidget", "Ex", nullptr));
    item->setTextAlignment(Qt::AlignLeft);
    item->setToolTip(i18n("Exclude matching lines"));
    item->setWhatsThis(i18n("When not checked, lines matching the regular expression "
    "will be included in the result. When checked, those which "
    "do not match will be included."));
    filtersTable->setHorizontalHeaderItem(ColExclude, item);

    item = new QTableWidgetItem;
    item->setText(QCoreApplication::translate("mainwidget", "IC", nullptr));
    item->setTextAlignment(Qt::AlignLeft);
    item->setToolTip(i18n("Use case insensitive matching"));
    item->setWhatsThis(i18n("When checked, the regular expression match will ignore text case."));
    filtersTable->setHorizontalHeaderItem(ColCaseIgnore, item);

    item = new QTableWidgetItem;
    item->setText(QCoreApplication::translate("mainwidget", "Regular Expression", nullptr));
    item->setToolTip(i18n("Regular expression string"));
    filtersTable->setHorizontalHeaderItem(ColRegEx, item);

    verticalLayout->addWidget(filtersTable);

    splitter->addWidget(groupBox_2);

    QGroupBox *groupBox_3 = new QGroupBox(splitter);
    groupBox_3->setObjectName(QStringLiteral("groupBox_3"));

    result = new wLogText(groupBox_3);
    result->setObjectName(QStringLiteral("result"));
    result->setGutter(32);
    result->setFont(QFont{QStringLiteral("Monospace")});
    QVBoxLayout *verticalLayout_2 = new QVBoxLayout(groupBox_3);
    verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
    verticalLayout_2->addWidget(result);

    splitter->addWidget(groupBox_3);

    verticalLayout_3->addWidget(splitter);

    status = new QLabel;
    mainWindow->statusBar()->insertWidget(0, status);

    //pixBmUser = KIconLoader::global()->loadIcon(QStringLiteral("bookmarks"), KIconLoader::Small);
    pixBmUser = QIcon::fromTheme(QStringLiteral("status-note")).pixmap(16,16);
    pixBmUser.scaledToHeight(16);
    result->setPixmap(pixmapIdBookMark, pixBmUser);
    pixBmAnnotation = QIcon::fromTheme(QStringLiteral("status-note")).pixmap(16,16);
    result->setPixmap(pixmapIdAnnotation,  pixBmAnnotation);
    result->setGutter(std::max(pixBmUser.width(), pixBmAnnotation.width()));

    KActionCollection *ac = mainWindow->actionCollection();
    QAction *action;

    /***********************/
    /***    File menu    ***/
    KStandardAction::open(this, SLOT(loadSubjectFile()), ac);

    recentFileAction = KStandardAction::openRecent(this,
                                SLOT(loadRecentSubject(const QUrl&)), ac);
    recentFileAction->loadEntries(KConfigGroup(KSharedConfig::openConfig(),
                                QStringLiteral("RecentURLs")));

    actionLoadFromClipboard = ac->addAction(QStringLiteral("file_load_from_clipboard"),
                                            this, SLOT(loadSubjectFromCB()));
    actionLoadFromClipboard->setText(i18n("Load from clipboard"));
    actionLoadFromClipboard->setWhatsThis(i18n("Set subject to text contents of the clipboard"));
    actionLoadFromClipboard->setToolTip(i18n("Set subject to clipboard"));

    actionSaveResults = ac->addAction(QStringLiteral("save_result"), this, SLOT(saveResult()));
    actionSaveResults->setText(i18n("Save Result..."));
    actionSaveResults->setToolTip(i18n("Save the filtered result."));
    actionSaveResults->setWhatsThis(i18n("Save the results of applying the filters to the source."));
    actionSaveResults->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    actionSaveResults->setEnabled(false);

    actionSaveResultsAs = ac->addAction(QStringLiteral("save_result_as"), this, SLOT(saveResultAs()));
    actionSaveResultsAs->setText(i18n("Save Result As..."));
    actionSaveResultsAs->setToolTip(i18n("Save the result as a new file."));
    actionSaveResultsAs->setWhatsThis(i18n("Save the results of applying the filters to the source as a new file."));
    actionSaveResultsAs->setIcon(QIcon::fromTheme(QStringLiteral("document-save-as")));
    actionSaveResultsAs->setEnabled(false);

    /***********************/
    /***   Edit menu     ***/
    actionGotoLine = ac->addAction(QStringLiteral("goto_line"), this, SLOT(gotoLine()));
    actionGotoLine->setText(i18nc("edit menu", "&Go to line"));
    actionGotoLine->setToolTip(i18n("Jump to subject line number"));
    actionGotoLine->setWhatsThis(i18n("Jump to the displayed line for the given subject line number, or the preceding line closest."));
    actionGotoLine->setIcon(QIcon::fromTheme(QStringLiteral("goto-line")));
    ac->setDefaultShortcut(actionGotoLine, QKeySequence(QStringLiteral("Ctrl+G")));

    actionBookmarkMenu = new KSelectAction(QIcon::fromTheme(QStringLiteral("bookmarks")),
                                           QStringLiteral("Jump to bookmark"), ac);
    action = ac->addAction(QStringLiteral("goto_bookmark"), actionBookmarkMenu);
    action->setIcon(QIcon::fromTheme(QStringLiteral("bookmarks")));
    connect(actionBookmarkMenu, SIGNAL(triggered(int)), SLOT(gotoBookmark(int)));
    ac->setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+J")));

    /***********************/
    /***   View menu     ***/
    action = ac->addAction(QStringLiteral("zoom_in"), result, SLOT(enlargeFont()));
    action->setText(i18nc("view menu", "Increase Font Size"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in")));
    ac->setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl++")));

    action = ac->addAction(QStringLiteral("zoom_out"), result, SLOT(shrinkFont()));
    action->setText(i18nc("view menu", "Decrease Font Size"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("zoom-out")));
    ac->setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+-")));

    action = ac->addAction(QStringLiteral("actual_size"), result, SLOT(resetFontZoom()));
    action->setText(i18nc("view menu", "Reset Font Size"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("actual-size")));
    ac->setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+0")));

    /***********************/
    /***   Filters menu  ***/
    actionRun = ac->addAction(QStringLiteral("run_filters"), this, [this](){applyFrom(0);}
                              /*SLOT(runButtonClicked())*/);
    actionRun->setText(i18n("Run filters"));
    actionRun->setToolTip(i18n("Run the filters against the input"));
    ac->setDefaultShortcut(actionRun, QKeySequence(QStringLiteral("Ctrl+R")));
    actionRun->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));

    actionAutorun = ac->addAction(QStringLiteral("auto_run_filters"), this, SLOT(autoRunClicked()));
    actionAutorun->setText(i18n("Autorun filters"));
    actionAutorun->setCheckable(true);
    actionAutorun->setChecked(false);
    actionAutorun->setToolTip(i18n("Auto-run the filters on any change"));
    actionAutorun->setWhatsThis(i18n("When set, any change will cause the filters to be re-applied"));
    actionAutorun->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));

    actionDialect = new KSelectAction(i18n("Dialect"), this);
    actionDialect->addAction(QStringLiteral("QRegularExpression"));
    actionDialect->setCurrentItem(0);
    action = ac->addAction(QStringLiteral("re_dialect"), actionDialect);
    connect(actionDialect, SIGNAL(triggered(const QString&)), this, SLOT(dialectChanged(const QString&)));
    action->setText(i18n("Dialect"));
    action->setToolTip(i18n("Select the RE dialect used"));

    action = ac->addAction(QStringLiteral("load_filters"), this, SLOT(loadFilters()));
    action->setText(i18n("Load Filters..."));
    action->setToolTip(i18n("Replace current filter list with contents of a file."));
    action->setWhatsThis(i18n("Load a filter list from a file, replacing the current filter "
                              "set with the contents."));

    recentFiltersAction = reinterpret_cast<KRecentFilesAction*>(ac->addAction(
        KStandardAction::OpenRecent, QStringLiteral("load_filters_recent"),
                            this, SLOT(loadFiltersTable(const QUrl&))));
    recentFiltersAction->setText(i18n("Load Recent Filters..."));
    recentFiltersAction->setToolTip(i18n("Replace current filter list a recent file."));
    recentFiltersAction->setWhatsThis(i18n("Load a filter list from a recent file, replacing "
                                            "the current filter set with the contents."));
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
    actionLineNumbers->setIcon(QIcon::fromTheme(QStringLiteral("line-number")));
    actionLineNumbers->setText(i18n("Show &Line Numbers"));
    actionLineNumbers->setToolTip(i18n("Toggle showing of source line numbers"));
    actionLineNumbers->setCheckable(true);

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

    ctxtMenu = new QMenu(this);
    ctxtMenu->addAction(KStandardAction::selectAll(result, SLOT(selectAll()), ac));
    actionClearSelection = KStandardAction::deselect(result, SLOT(clearSelection()), ac);
    ctxtMenu->addAction(actionClearSelection);

    actionCopySelection = KStandardAction::copy(result, SLOT(copy()), ac);
    ctxtMenu->addAction(actionCopySelection);

    // Annotation menu items:
    // ctxtMenu->addSeparator();
    // action = ac->addAction(QStringLiteral("annotation"), this, SLOT(addAnnotationAction()));
    // action->setIcon(QIcon::fromTheme(QStringLiteral("bkmkinfo")));
    // action->setText(QStringLiteral("Annotate Line"));
    // action->setWhatsThis(QStringLiteral("Add, change or remove line annotation."));
    // ctxtMenu->addAction(action);

    // Bookmarks menu items:
    actionToggleBookmark = ac->addAction(QStringLiteral("toggle_bookmark"), this,
                                         SLOT(toggleBookmark()));
    actionToggleBookmark->setText(QStringLiteral("Toggle Bookmark"));
    actionToggleBookmark->setWhatsThis(QStringLiteral("Set/clear a bookmark on the current line."));
    actionToggleBookmark->setIcon(QIcon::fromTheme(QStringLiteral("bookmark-new")));
    ctxtMenu->addAction(actionToggleBookmark);
    ctxtMenu->addAction(actionBookmarkMenu);

    connect(filtersTable, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(tableItemChanged(QTableWidgetItem*)));
    connect(filtersTable, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(filtersTableMenuRequested(QPoint)));
    connect(actionLineNumbers, SIGNAL(triggered(bool)), this, SLOT(actionLineNumbersTriggerd(bool)));

    if (objectName().isEmpty())
        setObjectName(QStringLiteral("mainWidget"));
    setWindowTitle(QCoreApplication::translate("mainWidget", "Form", nullptr));

    connect(result, SIGNAL(contextClick(lineNumber_t,QPoint,QContextMenuEvent*)),
            SLOT(resultContextClick(lineNumber_t,QPoint,QContextMenuEvent*)));
    connect(result, SIGNAL(gutterContextClick(lineNumber_t,QPoint,QContextMenuEvent*)),
            SLOT(resultContextClick(lineNumber_t,QPoint,QContextMenuEvent*)));
    connect(result, SIGNAL(fontMetricsChanged(int,int)), SLOT(fontMetricsChanged(int,int)));

    QMetaObject::connectSlotsByName(this);

    /* settings related to the general application */
    KConfigGroup generalConfig{KSharedConfig::openConfig(), generalConfigName};

    /* settings related to the filters section */
    KConfigGroup filtersConfig{KSharedConfig::openConfig(), filtersConfigName};
    filtersTable->setFont(filtersConfig.readEntry(QStringLiteral("font"), QFont{}));

    /* settings related to the results section */
    KConfigGroup resultsConfig{KSharedConfig::openConfig(), resultsConfigName};
    result->setFont(resultsConfig.readEntry(QStringLiteral("font"), QFont{QStringLiteral("monospace")}));

    findHistory = resultsConfig.readEntry(QStringLiteral("findHistory"), QStringList{});
    findHistorySize = resultsConfig.readEntry(QStringLiteral("findHistorySize"), findHistorySize);

    actionLineNumbers->setChecked(resultsConfig.readEntry(QStringLiteral("showLineNumbers"), false));
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
    loadSubjectFile(QFileDialog::getOpenFileName(this,
                            i18nc("@title:window load subject file dialog title", "Open Subject File")));
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
        bookmarkedLines.clear();
        sourceItems.clear();
        sourceLineCount = 0;
        for(QTextStream stream(&source); !stream.atEnd(); )
            sourceItems.emplace_back(++sourceLineCount, stream.readLine());
        sourceItems.shrink_to_fit();

        stepList steps;
        steps.reserve(sourceItems.size());
        std::for_each(sourceItems.begin(), sourceItems.end(),
                      [&steps](auto& item) mutable {steps.push_back(&item);});
        stepResults.resize(1);
        stepResults[0] = std::move(steps);
        recentFileAction->addUrl(QUrl::fromLocalFile(localFile));
        status->setText(QStringLiteral("%1: %2 lines").arg(localFile).arg(sourceItems.size()));
        maybeAutoApply(0);
        return true;
    }
    status->setText(QStringLiteral("open '%1' failed").arg(localFile));
    return false;
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
        QMessageBox::information(this,
                        i18nc("@title:window title of no data in clipboard information dialog", "No Data"),
                        i18nc("@info:status informational message of no data in clipboard information dialog", "Clipboard does not contain text data"));
        return;
    }

    bookmarkedLines.clear();
    sourceItems.clear();
    int srcLine = 0;
    for(QTextStream stream(&text, QIODevice::ReadOnly); !stream.atEnd(); )
        sourceItems.emplace_back(++srcLine, stream.readLine());
    sourceItems.shrink_to_fit();

    stepList steps;
    steps.reserve(sourceItems.size());
    std::for_each(sourceItems.begin(), sourceItems.end(),
                  [&steps](auto& item) mutable {steps.push_back(&item);});
    stepResults.resize(1);
    stepResults[0] = std::move(steps);

    titleFile = i18nc("@info:status title bar file-name when loaded from clipboard", "<clipboard>");
    subjModified = false;
    updateApplicationTitle();
    sourceLineCount = -1;
    clearResultsAfter(0);
    sourceLineCount = sourceItems.size();
    status->setText(QStringLiteral("%1: %2 lines").arg(titleFile).arg(sourceLineCount));
    maybeAutoApply(0);
}


stepList mainWidget::applyExpression(size_t entry, stepList src)
{
    if (src.empty())
        return src;

    size_t rows = filtersTable->rowCount();
    if (entry >= rows) {
        qWarning() << QStringLiteral("Range failure %1/%2").arg(entry).arg(rows);
        return stepList{};
    }

    auto* item = filtersTable->item(entry, ColRegEx);
    if (!item)
        return src;

    if (filtersTable->item(entry, ColEnable)->checkState() != Qt::Checked) {
        item->setToolTip(i18nc("@info:tooltip filter table entry when expression is disabled", "disabled"));
        return src;
    }

    if (item->text().isEmpty()) {
        item->setToolTip(QString{});
        return src;
    }

    auto reStr = item->text();
    bool exclude = filtersTable->item(entry, ColExclude)->checkState() == Qt::Checked;
    QRegularExpression::PatternOptions pOpts = QRegularExpression::NoPatternOption;
    if (filtersTable->item(entry, ColCaseIgnore)->checkState() == Qt::Checked)
        pOpts |= QRegularExpression::CaseInsensitiveOption;
    QRegularExpression re(reStr, pOpts);
    if (!re.isValid())
        return src;
    re.optimize();

    QElapsedTimer timer;
    timer.start();
    auto result = QtConcurrent::blockingFiltered(src,
        [&re, exclude](const textItem* item) -> bool {return re.match(item->text).hasMatch() ^ exclude;});
    item->setToolTip(QStringLiteral("%L1 of %L2 -- %L3us").arg(result.size())
            .arg(src.size()).arg(timer.nsecsElapsed()/1000));
    return result;
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
        stepResults.resize(rows+1);
        for (size_t row = start; row < rows; ++row) {
            auto result = applyExpression(row, stepResults[row]);
            subjModified |= stepResults[row].size() != result.size();
            stepResults[row+1] = std::move(result);
            qApp->processEvents();
        }
        updateApplicationTitle();
        displayResult();
        QGuiApplication::restoreOverrideCursor();
    } else
        qWarning() << QStringLiteral("No source entry %1/%2").arg(start).arg(stepResults.size());
}

bool mainWidget::validateExpressions(int entry) const
{
    auto table = filtersTable;
    for (int rows = table->rowCount(); entry < rows; ++entry) {
        if (table->item(entry, ColEnable)->checkState() == Qt::Checked) {
            auto* reItem = table->item(entry, ColRegEx);
            auto reStr = reItem->text();
            if (!reStr.isEmpty()) {
                QRegularExpression re(reStr);
                if (!re.isValid()) {
                    status->setText(QStringLiteral("Invalid RE at %1: '%2'")
                            .arg(entry).arg(re.errorString()));
                    table->setCurrentCell(entry, ColRegEx);
                    reItem->setToolTip(status->text());
                    return false;
                }
                reItem->setToolTip(QString{});
            }
        }
    }
    return true;
}

void mainWidget::clearResultsAfter(size_t startIndex)
{
    /* startIndex is zero based item rows. The items in the table start
     * at one, with zero being the header. */
    int const rowLast = filtersTable->rowCount() + 1;
    stepResults.resize(rowLast);
    for (int rowNumber = startIndex + 1; rowNumber < rowLast; ++rowNumber) {
        stepResults[rowNumber].clear();
        if (const auto item = filtersTable->item(rowNumber, ColRegEx); item)
            item->setToolTip(QString{});
    }
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
    size_t resultLines = 0;

    if (!stepResults.empty() && !stepResults.back().empty()) {
        QSignalBlocker disabler{result};
        result->clear();
        stepList& items = stepResults.back();
        std::vector<int> lineMap(items.size());
        auto mapIt = lineMap.begin();
        if (actionLineNumbers->isChecked()) {
            int width = QStringLiteral("%1").arg(items.back()->srcLineNumber).size();
            for (auto const& item : items) {
                QString line = QStringLiteral("%1| ").arg(item->srcLineNumber, width) + item->text;
                result->append(new resultTextItem(item, std::move(line), 0));
                *(mapIt++) = item->srcLineNumber;
            }
        } else {
            for (auto const& item : items) {
                result->append(new resultTextItem(item, item->text, 0));
                *(mapIt++) = item->srcLineNumber;
            }
        }
        sourceLineMap = std::move(lineMap);
        resultLines = items.size();
    } else {
        result->clear();
        sourceLineMap.clear();
    }
    result->setCaretPosition(10, 10);
    result->ensureCaretVisible();
    actionSaveResults->setEnabled(resultLines != 0);
    actionSaveResultsAs->setEnabled(resultLines != 0);
    status->setText(QStringLiteral("Source: %L1, final %L2 lines").arg(sourceLineCount).arg(resultLines));
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
    item->setFlags(item->flags() & ~(Qt::ItemIsEditable));
    filtersTable->setItem(row, ColEnable, item);

    item = new QTableWidgetItem();
    item->setCheckState(Qt::Unchecked);
    item->setFlags(item->flags() & ~(Qt::ItemIsEditable));
    filtersTable->setItem(row, ColExclude, item);

    item = new QTableWidgetItem();
    item->setCheckState(Qt::Unchecked);
    item->setFlags(item->flags() & ~(Qt::ItemIsEditable));
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

void mainWidget::setFilterRow(int row, filterEntry const& entry)
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

void mainWidget::dialectChanged([[maybe_unused]] QString const& text)
{
    maybeAutoApply(0);
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
        qWarning() << QStringLiteral("JSON file is not valid");
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
        if (auto it = filters.find(QStringLiteral("dialect")); it != filters.end() && it->isString())
            result.dialect = it->toString();

        if (auto it = filters.find(QStringLiteral("filters")); it != filters.end() && it->isArray()) {
            const QJsonArray filterArray = filters[QStringLiteral("filters")].toArray();
            for (const auto& entry : it->toArray())
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
    QString localFile = QFileDialog::getSaveFileName(this,
                    i18nc("@title:window save filters file name dialog", "Save Filters To"),
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
        about[QStringLiteral("application")] = app.componentName();
        about[QStringLiteral("version")] = app.version();
        QJsonObject filters;
        filters[QStringLiteral("about")] = about;
        filters[QStringLiteral("dialect")] = actionDialect->currentText();
        filters[QStringLiteral("filters")] = filterArray;
        auto jDoc = QJsonDocument(filters).toJson();
        bool success = dest.write(jDoc) == jDoc.size();
        reModified &= !success;
        return success;
    }
    return false;
}

void mainWidget::toggleBookmark()
{
    if (sourceLineMap.empty())
        return;

    auto lineNumber = result->caretPosition().lineNumber();      /*!< line number in results */
    if (lineNumber > result->lineCount())   /* shouldn't happen, but be safe */
        return;
    auto sourceItem = resultTextItem::asResultTextItem(result->item(lineNumber))->sourceItem();
    if (!sourceItem->bookmarked) {
        sourceItem->bookmarked = true;
        QString const& sourceText = sourceItem->text;
        if (auto sel = result->getSelection().normalized(); sel.singleLine()) {
            auto [start, end] = sel;
            sourceItem->bmText = sourceText.midRef(
                start.columnNumber(), end.columnNumber() - start.columnNumber());
        } else
            sourceItem->bmText = sourceText.leftRef(40);
        bookmarkedLines.insert(sourceItem->srcLineNumber);
        result->setLinePixmap(lineNumber, pixmapIdBookMark);
    } else {
        sourceItem->bookmarked = false;
        bookmarkedLines.remove(sourceItem->srcLineNumber);
        result->clearLinePixmap(lineNumber);
    }

    auto lineNums = bookmarkedLines.values();
    std::sort(lineNums.begin(), lineNums.end());
    QStringList bms;
    bmLineNums.clear();
    bmLineNums.reserve(bmLineNums.size());
    bms.reserve(bmLineNums.size());

    auto const& items = stepResults[0];
    for(lineNumber_t lineNo : lineNums) {
        if (lineNo < items.size()) [[likely]] {/* should always be true, but check */
            auto srcIdx = std::max(0, lineNo - 1);
            bms.push_back(QStringLiteral("%1: %2").arg(lineNo).arg(items[srcIdx]->bmText));
            bmLineNums.push_back(lineNo);
        } else
            bookmarkedLines.remove(lineNo);
    }
    actionBookmarkMenu->setItems(bms);
}

void mainWidget::actionLineNumbersTriggerd(bool checked)
{
    KConfigGroup resultsConfig{KSharedConfig::openConfig(), resultsConfigName};
    resultsConfig.writeEntry(QStringLiteral("showLineNumbers"), checked);
}

void mainWidget::gotoLine()
{
    if (sourceLineMap.empty())
        return;

    auto currentPos = result->caretPosition();
    if (currentPos.lineNumber() > result->lineCount())   /* shouldn't happen, but be safe */
        return;
    auto sourceLineNo = sourceLineMap[currentPos.lineNumber()];
    bool ok;
    sourceLineNo = QInputDialog::getInt(this,
                        i18nc("@title:window title of go to line number dialog", "Go to line"),
                        i18nc("@label:textbox label for line number input field", "Source line number:"),
                        sourceLineNo, 1, sourceLineMap.back(), 1, &ok);
    if (ok)
        jumpToSourceLine(sourceLineNo);
}

void mainWidget::jumpToSourceLine(int lineNumber)
{
    auto it = std::lower_bound(sourceLineMap.cbegin(), sourceLineMap.cend(), lineNumber);
    auto currentPos = result->caretPosition();
    currentPos.setLineNumber(std::distance(sourceLineMap.cbegin(), it));
    result->setCaretPosition(currentPos);
    result->ensureCaretVisible();
}


void mainWidget::resultFind()
{
    KFindDialog findDlg(this, findOptions, findHistory);
    findDlg.setPattern(lastFoundText);
    findDlg.setHasCursor(true);
    findDlg.setSupportsBackwardsFind(true);
    findDlg.setSupportsCaseSensitiveFind(true);
    findDlg.setSupportsWholeWordsFind(false);
    findDlg.setSupportsRegularExpressionFind(true);
    if (findDlg.exec() != QDialog::Accepted)
        return;

    QString text = findDlg.pattern();
    if (text.size() > 0) {
        lastFoundText = text;
        findHistory.removeAll(lastFoundText);
        findHistory.push_back(lastFoundText);
        if (findHistory.size() > findHistorySize)
            findHistory.erase(findHistory.begin(), findHistory.begin() + (findHistory.size() - findHistorySize));
        KConfigGroup resultsConfig{KSharedConfig::openConfig(), resultsConfigName};
        resultsConfig.writeEntry(QStringLiteral("findHistory"), findHistory);

        findOptions = findDlg.options();
        doResultFind(findOptions);
    }
}

void mainWidget::resultFindNext()
{
    doResultFind((findOptions | KFind::FromCursor) & ~KFind::FindBackwards);
}

void mainWidget::resultFindPrev()
{
    doResultFind(findOptions | KFind::FromCursor | KFind::FindBackwards);
}

void mainWidget::doResultFind(long options)
{
    if (lastFoundText.isEmpty())
        return;
    if (!result->find(lastFoundText, options)) {
        QMessageBox::information(this,
                    i18nc("@title:window title of search fail pop-up", "Search Not Found"),
                    i18nc("@info:status text of search string not found", "Search text not found"));
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
    QString localFile = QFileDialog::getSaveFileName(this,
                i18nc("@title:window title of save to file dialog", "Save Results To")
                /*,QString(), i18n("Log files (*.txt *.log);;All files (*.*)")*/);
    if (!localFile.isEmpty())
        if (doSaveResult(localFile))
            resultFileName = localFile;
}

bool mainWidget::doSaveResult(const QString& fileName)
{
    QFile dest(fileName);
    bool saved = dest.open(QIODevice::WriteOnly | QIODevice::Text) &&
           dest.write(result->toPlainText().toLatin1()) >= 0;
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

void mainWidget::resultContextClick(lineNumber_t lineNo, QPoint pos, [[maybe_unused]] QContextMenuEvent *e)
{
    if (std::cmp_greater_equal(lineNo, sourceLineMap.size()))
        return;

    bool hasSelectedText = result->hasSelectedText();
    actionCopySelection->setEnabled(hasSelectedText);
    actionClearSelection->setEnabled(hasSelectedText);

    auto lineNumber = result->caretPosition().lineNumber();      /*!< line number in results */
    if (lineNumber > result->lineCount())   /* shouldn't happen, but be safe */
        return;
    auto sourceItem = resultTextItem::asResultTextItem(result->item(lineNumber))->sourceItem();
    if (sourceItem->bookmarked) {
        actionToggleBookmark->setText(i18nc("@action:inmenu remove bookmark", "Clear bookmark"));
        actionToggleBookmark->setToolTip(i18nc("@info:tooltip remove bookmark", "Remove the bookmark on the current line."));
	actionToggleBookmark->setIcon(QIcon::fromTheme(QStringLiteral("bookmark-remove")));
    } else {
        actionToggleBookmark->setText(i18nc("@action:inmenu set bookmark", "Set bookmark"));
        actionToggleBookmark->setToolTip(i18nc("@info:tooltip set bookmark", "Place a bookmark on the current line."));
	actionToggleBookmark->setIcon(QIcon::fromTheme(QStringLiteral("bookmark-new")));
    }
    ctxtMenu->exec(pos);
}

void mainWidget::gotoBookmark(int entry)
{
    if (std::cmp_less(entry, bmLineNums.size()))
        jumpToSourceLine(bmLineNums[entry]);
}

void mainWidget::fontMetricsChanged(int lineHeight, [[maybe_unused]] int charWidth)
{
    QPixmap pmU = pixBmUser.scaledToHeight(lineHeight);
    auto pmUWidth = pmU.width();
    result->setPixmap(pixmapIdBookMark, std::move(pmU));

    QPixmap pmA = pixBmAnnotation.scaledToHeight(lineHeight);
    auto pmAWidth = pmA.width();
    result->setPixmap(pixmapIdAnnotation, std::move(pmA));
    result->setGutter(std::max(pmUWidth, pmAWidth));
}

void mainWidget::selectFilterFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, filtersTable->font(), this);
    if (ok) {
        KConfigGroup filtersConfig{KSharedConfig::openConfig(), filtersConfigName};
        filtersConfig.writeEntry(QStringLiteral("font"), font);
        filtersTable->setFont(font);
    }
}

void mainWidget::selectResultFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, result->font(), this);
    if (ok) {
        KConfigGroup resultessConfig{KSharedConfig::openConfig(), resultsConfigName};
        resultessConfig.writeEntry(QStringLiteral("font"), font);
        result->setFont(font);
    }
}
