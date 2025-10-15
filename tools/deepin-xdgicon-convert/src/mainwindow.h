// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "filechooserwidget.h"

#include <QPushButton>
#include <QStackedWidget>

#include <DMainWindow>
#include <DFileChooserEdit>
#include <DWaterProgress>
#include <DLabel>

DWIDGET_USE_NAMESPACE

class MainWindow : public DMainWindow
{
    Q_OBJECT
public:
    enum {
        PAGE_FILE_CHOOSER = 0,
        PAGE_CONVERTING,
        PAGE_CONVERT_SUCCESS,
        PAGE_CONVERT_FAIL
    };

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void initUI();
    void initFileChooserPage();
    void initConvertingPage();
    void initConvertSuccessPage();
    void initConvertFailPage();

private:
    QStackedWidget *m_stackedWidget = nullptr;
    ConvertWorker *m_worker = nullptr;

    // File chooser page
    QWidget *m_fileChooserPage = nullptr;
    FileChooserWidget *m_fileChooserWidget = nullptr;
    DFileChooserEdit *m_fileChooserEdit = nullptr;
    QPushButton *m_convertButton = nullptr;

    // converting page
    QWidget *m_convertingPage = nullptr;
    DWaterProgress *m_loadingIndicator = nullptr;
    DLabel *m_convertingLabel = nullptr;

    // convert success page
    QWidget *m_convertSuccessPage = nullptr;
    QLabel *m_convertSuccessIcon = nullptr;
    DLabel *m_convertSuccessLabel = nullptr;
    QPushButton *m_openFileButton = nullptr;
    QPushButton *m_finishButton = nullptr;

    // convert fail page
    QWidget *m_convertFailPage = nullptr;
    QLabel *m_convertFailIcon = nullptr;
    DLabel *m_convertFailLabel = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_retryButton = nullptr;
};
