// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"

#include <DApplication>
#include <LogManager.h>

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

int main(int argc, char *argv[])
{
    DApplication app(argc, argv);

    app.setApplicationName("deepin-xdgicon-convert");
    app.setProductIcon(QIcon::fromTheme("deepin-xdgicon-convert"));
    app.setApplicationVersion(VERSION);
    app.loadTranslator();

    DLogManager::registerConsoleAppender();
    DLogManager::registerJournalAppender();

    MainWindow w;
    w.resize(450, 360);
    w.show();

    return app.exec();
}
