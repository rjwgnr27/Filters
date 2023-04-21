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

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QtConcurrent>

#include <KActionCollection>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <exception>
#include <iostream>
#include <utility>

QString const generalConfigName = QStringLiteral("general");
QString const filtersConfigName = QStringLiteral("filters");
QString const resultsConfigName = QStringLiteral("results");

Filters::Filters(const commandLineOptions& opts, QWidget *parent) :
        KXmlGuiWindow(parent), m_ui(new mainWidget(this))
{
    setCentralWidget(m_ui);
    setupActions();
    setStandardToolBarMenuEnabled(true);
    StandardWindowOptions options = Default;
    options.setFlag(ToolBar, false);
    setupGUI(options);
    setAutoSaveSettings();
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

    stateChanged(QStringLiteral("results_save"), StateReverse);
    stateChanged(QStringLiteral("filters_save"), StateReverse);
}

QJsonObject filterEntry::toJson() const
{
    QJsonObject filter;
    filter[QStringLiteral("enabled")] = enabled;
    filter[QStringLiteral("exclude")] = exclude;
    filter[QStringLiteral("ignore_case")] = ignoreCase;
    filter[QStringLiteral("regexp")] = re;
    return filter;
}

auto filterEntry::fromJson(const QJsonObject& jentry) -> filterEntry
{
    filterEntry entry;
    entry.enabled = jentry[QStringLiteral("enabled")].toBool();
    entry.exclude = jentry[QStringLiteral("exclude")].toBool();
    entry.ignoreCase = jentry[QStringLiteral("ignore_case")].toBool();
    entry.re = jentry[QStringLiteral("regexp")].toString();
    return entry;
}

#include "moc_mainwidget.cpp"

class batchException : public std::exception
{
protected:
    std::string errorString;

public:
    batchException(QString const& str) : errorString(str.toUtf8().constData()) {}
    auto what() const noexcept -> const char* override {return errorString.c_str();}
};

class filterLoadException : public batchException
{
public:
    filterLoadException(QString const& str) :
        batchException(QStringLiteral("Error loading filter file: %1").arg(str)) {}
};

class loadDialectException : public batchException
{
public:
    loadDialectException(QString const& str) :
        batchException(QStringLiteral("Dialect mismatch loading filter file: %1").arg(str)) {}
};

class dialectTypeException : public batchException
{
public:
    dialectTypeException(QString const& str) :
        batchException(QStringLiteral("Unsupported dialect: %1").arg(str)) {}
};

class badRegexException : public batchException
{
public:
    badRegexException(QString const& str) :
        batchException(QStringLiteral("bad regular expression: '%1'").arg(str)) {}
};

class subjectLoadException : public batchException
{
public:
    subjectLoadException(QString const& str) :
        batchException(QStringLiteral("Error loading subject file: %1").arg(str)) {}
};


static auto batchLoadFilterFile(const QString& fileName) -> filterData
{
    filterData result;

    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc(QJsonDocument::fromJson(data));
        QJsonObject filters = doc.object();
        if (auto it = filters.find(QStringLiteral("dialect")); it != filters.end() && it->isString())
            result.dialect = it->toString();

        if (auto const it = filters.find(QStringLiteral("filters")); it != filters.end() && it->isArray()) {
            for (const auto& entry : it->toArray())
                result.filters << filterEntry::fromJson(entry.toObject());
            result.valid = true;
        }
    }
    if (!result.valid)
        throw filterLoadException(fileName);
    return result;
}


static auto batchLoadFilters(const commandLineOptions& opts) -> filterData
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


static itemsList batchLoadSubjectFile(const commandLineOptions& opts)
{
    QFile source(opts.subjectFile);
    if (source.open(QIODevice::ReadOnly | QIODevice::Text)) {
        itemsList lines;
        int srcLine = 0;
        for(QTextStream stream(&source); !stream.atEnd(); )
          lines.emplace_back(++srcLine, stream.readLine());
        lines.shrink_to_fit();
        return lines;
    }
    throw subjectLoadException(opts.subjectFile);
}

static itemsList readStdIn()
{
    itemsList lines;
    for(int srcLine = 0; std::cin;) {
        std::string line;
        std::getline(std::cin, line);
        lines.emplace_back(++srcLine, QString::fromStdString(line));
    }
    return lines;
}


static stepList batchApplyQRegularExpressions(const filterData& filters, stepList items)
{
    for (const filterEntry& entry : filters.filters) {
        if (entry.enabled) {
            bool const exclude = entry.exclude;
            QRegularExpression::PatternOptions pOpts = QRegularExpression::NoPatternOption;
            if (entry.ignoreCase)
                pOpts |= QRegularExpression::CaseInsensitiveOption;
            QRegularExpression re(entry.re, pOpts);
            if (!re.isValid())
                throw badRegexException(entry.re);
            re.optimize();
            items = QtConcurrent::blockingFiltered(items,
                    [&re, exclude](const textItem* item) -> bool {return re.match(item->text).hasMatch() ^ exclude;});
            if (items.empty())
                break;
        }
    }
    return items;
}

static stepList batchApplyFilters(const filterData& filters, const stepList& lines)
{
    if (filters.dialect == QStringLiteral("QRegularExpression"))
        return batchApplyQRegularExpressions(filters, stepList{lines});
    throw dialectTypeException(filters.dialect);
}

auto doBatch(const commandLineOptions& opts) -> int
{
    try {
        filterData filters = batchLoadFilters(opts);
        itemsList sourceItems = opts.stdin ? readStdIn() : batchLoadSubjectFile(opts);
        stepList steps;
        steps.reserve(sourceItems.size());
        std::for_each(sourceItems.begin(), sourceItems.end(),
                      [&steps](auto& item) mutable {steps.push_back(&item);});

        steps = batchApplyFilters(filters, steps);
        for (auto const& item : qAsConst(sourceItems))
            std::cout << item.text.toUtf8().constData() << '\n';
    }
    catch (std::exception const &except) {
        std::cerr << except.what() << std::endl;
        return -3;
    }
    return 0;
}
