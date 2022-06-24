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

#include "filters.h"
#include <QApplication>
#include <QCommandLineParser>
#include <KAboutData>
#include <KDBusService>
#include <KLocalizedString>

#include <iostream>

#include "filters_config.h"
#include "mainwidget.h"

template <typename T>
std::basic_ostream<char, T>& operator << (std::basic_ostream<char, T>& os, QString const& str)
{
    return os << str.toLatin1().constData();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("Filters");

    KAboutData aboutData(
        // The program name used internally. (componentName)
        QStringLiteral("filters"),
        // A displayable program name string. (displayName)
        i18n("Interactive file filtering"),
        // The program version string. (version)
        QStringLiteral(APP_VERSION_STRING),
        // Short description of what the app does. (shortDescription)
        i18n("Utility to interactively filter a file against a series of regular expressions"),
        // The license this code is released under
        KAboutLicense::GPL,
        // Copyright Statement (copyrightStatement = QString())
        i18n("(c) 2020"));
    aboutData.addAuthor(i18n("Rick Wagner"), i18n("Author"));
    aboutData.addAuthor(i18n("Rick Wagner"), i18n("Author"),
                        QStringLiteral("Rick.Wagner@HarmonicInc.com"));
    KAboutData::setApplicationData(aboutData);

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);

    QCommandLineOption autoRunOption(i18n("auto"),
                                     i18n("Auto-run mode; changes in a filter automatically runs filter chain"));
    parser.addOption(autoRunOption);

    QCommandLineOption batchOption(QStringList{} << "b" << i18n("batch"),
                                   i18n("batch mode; does not open GUI"));
    parser.addOption(batchOption);

    QCommandLineOption reOption(QStringList() << "r" << i18n("refile"),
                                i18n("regex file to load"), i18n("REFILE"));
    parser.addOption(reOption);

    QCommandLineOption stdinOption(i18n("stdin"), i18n("Load subject from stdin; only applies to batch-mode."));
    parser.addOption(stdinOption);

    QCommandLineOption subjectOption(QStringList() << "s" << i18n("subject"),
                       i18n("subject file to load"), i18n("SUBJECTFILE"));
    parser.addOption(subjectOption);

    parser.process(app);
    aboutData.processCommandLine(&parser);

    if (!parser.positionalArguments().empty()) {
        std::cerr << i18n("Extra parameters on command line: ");
        for (const auto& opt : parser.positionalArguments())
            std::cerr << '\'' << opt << "' ";
        std::cerr << '\n';
        return -2;
    }

    KDBusService service(KDBusService::Multiple, &app);

    commandLineOptions opts;
    opts.autoRun = parser.isSet(autoRunOption);
    opts.filters = parser.values(reOption);
    opts.subjectFile = parser.value(subjectOption);
    opts.stdin = parser.isSet(stdinOption);

    if (parser.isSet(batchOption)) {
        if (opts.filters.empty()) {
            std::cerr << i18n("No filters file specified in batch mode") << '\n';
            return -2;
        }
        if (opts.subjectFile.isEmpty() && !opts.stdin) {
            std::cerr << i18n("No subject source specified in batch mode") << '\n';
            return -2;
        }
        if (!opts.subjectFile.isEmpty() && opts.stdin) {
            std::cerr << i18n("Can not specify both subject file and stdin") << '\n';
            return -2;
        }
        return doBatch(opts);
    } else {
        if (opts.stdin) {
            std::cerr << i18n("Can not specify 'stdin' without 'batch'") << '\n';
            return -2;
        }
        Filters *w = new Filters(opts);
        w->show();
        return app.exec();
    }
}
