#include "filters.h"
#include <QApplication>
#include <QCommandLineParser>
#include <KAboutData>
#include <KDBusService>
#include <KLocalizedString>

#include "filters_config.h"
#include "mainwidget.h"

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

    QCommandLineOption autoRunOption(i18n("auto"), i18n("Set auto-run mode"));
    parser.addOption(autoRunOption);

    QCommandLineOption reOption(QStringList() << "r" << i18n("refile"),
                                i18n("regex file to load"), i18n("REFILE"));
    parser.addOption(reOption);

    QCommandLineOption subjectOption(QStringList() << "s" << i18n("subject"),
                       i18n("subject file to load"), i18n("SUBJECTFILE"));
    parser.addOption(subjectOption);

    parser.process(app);
    aboutData.processCommandLine(&parser);

    KDBusService service(KDBusService::Multiple, &app);

    commandLineOptions opts;
    opts.autoRun = parser.isSet(autoRunOption);
    opts.filters = parser.values(reOption);
    opts.subjectFile = parser.value(subjectOption);
    Filters *w = new Filters(opts);
    w->show();

    return app.exec();
}

