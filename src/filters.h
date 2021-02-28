#ifndef FILTERS_H
#define FILTERS_H

#include <KXmlGuiWindow>
#include <QStringList>

class mainWidget;

struct commandLineOptions {
    QStringList filters;
    QString subjectFile;
    bool autoRun = false;
};

class Filters : public KXmlGuiWindow
{
public:
    explicit Filters(const commandLineOptions& opts, QWidget *parent=nullptr);
    virtual ~Filters();

private:
    /** application widget placed in the main window */
    mainWidget *m_ui;

    void setupActions();

    /**
     * @brief inherited close event
     *
     * Notification of close, allowing the save of the confiuration
     * @param event event for the close event
     **/
    virtual void closeEvent(QCloseEvent *event) override;
};

#endif // FILTERS_H
