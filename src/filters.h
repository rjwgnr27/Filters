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

#ifndef FILTERS_H
#define FILTERS_H

#include <KXmlGuiWindow>
#include <QStringList>

extern QString const generalConfigName;
extern QString const filtersConfigName;
extern QString const resultsConfigName;

class mainWidget;

struct commandLineOptions {
    QStringList filters;
    QString subjectFile;
    bool autoRun = false;
    bool batchMode = false;
    bool stdin = false;
};

class Filters : public KXmlGuiWindow
{
public:
    explicit Filters(const commandLineOptions& opts, QWidget *parent=nullptr);

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

int doBatch(const commandLineOptions& opts);

#endif // FILTERS_H
