// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"

#include <DApplication>
#include <DWidgetUtil>
#include <LogManager.h>

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

int main(int argc, char *argv[])
{
    DApplication app(argc, argv);

    app.setProductIcon(QIcon::fromTheme("deepin-xdgicon-convert"));
    app.setApplicationVersion(VERSION);
    app.loadTranslator();
    app.setApplicationDisplayName(QObject::tr("Theme Icon Converter"));

    if(!app.setSingleInstance(app.applicationName())) {
        qWarning() << "deepin-xdgicon-convert is running...";
        return 1;
    }

    DLogManager::registerConsoleAppender();
    DLogManager::registerJournalAppender();

    MainWindow w;
    w.resize(450, 360);
    Dtk::Widget::moveToCenter(&w);
    w.show();

    return app.exec();
}
