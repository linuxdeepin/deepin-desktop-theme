// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "convertworker.h"

#include <QWidget>
#include <QStackedWidget>

#include <DSpinner>
#include <DLabel>
#include <DToolButton>
#include <DCommandLinkButton>

DWIDGET_USE_NAMESPACE

class FileChooserWidget : public QWidget
{
    Q_OBJECT
public:
    enum {
        CHOOSER_PAGE_CHOOSE_FILE = 0,
        CHOOSER_PAGE_CHECKING,
        CHOOSER_PAGE_SELECTED_FILE,
        CHOOSER_PAGE_CHECK_ERROR
    };

    explicit FileChooserWidget(ConvertWorker *worker, QWidget *parent = nullptr);
    ~FileChooserWidget() override;

    inline QString getFilePath() const { return m_filePath; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

signals:
    void fileChanged(const QString &filePath);
    void checkStatusChanged(bool ok);

private:
    void initUI();
    void selectFile(const QString &filePath);
    void clearFile();

private:
    QStackedWidget *m_stackedWidget = nullptr;

    bool m_isDragOver = false;
    bool m_isPressed = false;
    bool m_isHover = false;
    QString m_filePath;
    ConvertWorker *m_worker = nullptr;

    // choose file page
    QWidget *m_chooseFilePage = nullptr;
    QLabel *m_convertIcon = nullptr;
    QLabel *m_chooseTitleLabel = nullptr;
    QLabel *m_chooseDescLabel = nullptr;

    // checking page
    QWidget *m_checkingPage = nullptr;
    DSpinner *m_checkingSpinner = nullptr;
    QLabel *m_checkingLabel = nullptr;

    // selected file page
    QWidget *m_selectedFilePage = nullptr;
    QToolButton *m_delButton = nullptr;
    QLabel *m_debFileIcon = nullptr;
    QLabel *m_debFileNameLabel = nullptr;

    // check error page
    QWidget *m_checkErrorPage = nullptr;
    QLabel *m_errorIcon = nullptr;
    QLabel *m_errorLabel = nullptr;
    DCommandLinkButton *m_reselectButton = nullptr;
};
