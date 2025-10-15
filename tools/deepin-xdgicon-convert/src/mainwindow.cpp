// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QTimer>
#include <QStandardPaths>
#include <QDesktopServices>

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
{
    m_worker = new ConvertWorker(this);
    initUI();

    connect(m_fileChooserWidget, &FileChooserWidget::checkStatusChanged, this, [this](bool ok){
        m_convertButton->setEnabled(ok);
    });

    connect(m_fileChooserWidget, &FileChooserWidget::fileChanged, this, [this](const QString &filePath){
        m_convertButton->setEnabled(!filePath.isEmpty());
    });

    connect(m_worker, &ConvertWorker::convertProgressChanged, this, [this](int value){
        m_loadingIndicator->setValue(value);
    });

    connect(m_worker, &ConvertWorker::convertFinished, this, [this](bool ok){
        if (ok) {
            m_stackedWidget->setCurrentIndex(PAGE_CONVERT_SUCCESS);
        } else {
            m_stackedWidget->setCurrentIndex(PAGE_CONVERT_FAIL);
        }
        m_worker->clear();
    });
}

MainWindow::~MainWindow()
{

}

void MainWindow::initUI()
{
    m_stackedWidget = new QStackedWidget(this);
    setCentralWidget(m_stackedWidget);
    initFileChooserPage();
    initConvertingPage();
    initConvertSuccessPage();
    initConvertFailPage();

    // 默认显示文件选择页面
    m_stackedWidget->setCurrentIndex(PAGE_FILE_CHOOSER);
}

void MainWindow::initFileChooserPage()
{
    m_fileChooserPage = new QWidget();

    QVBoxLayout *mainLayout = new QVBoxLayout(m_fileChooserPage);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 文件选择区域
    m_fileChooserWidget = new FileChooserWidget(m_worker);
    m_fileChooserWidget->setFixedSize(410, 190);
    mainLayout->addWidget(m_fileChooserWidget);

    QHBoxLayout *dirChooserLayout = new QHBoxLayout();
    m_fileChooserEdit = new DFileChooserEdit();
    m_fileChooserEdit->setFileMode(QFileDialog::Directory);
    m_fileChooserEdit->setDialogDisplayPosition(DFileChooserEdit::CurrentMonitorCenter);
    m_fileChooserEdit->lineEdit()->setText(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    dirChooserLayout->addWidget(new QLabel(tr("Save to:")));
    dirChooserLayout->addWidget(m_fileChooserEdit);
    m_convertButton = new QPushButton(tr("Start Conversion"));
    m_convertButton->setFixedWidth(220);
    m_convertButton->setEnabled(false);

    mainLayout->addWidget(m_fileChooserWidget, 0, Qt::AlignHCenter);
    mainLayout->addLayout(dirChooserLayout);
    mainLayout->addWidget(m_convertButton, 0, Qt::AlignHCenter);

    connect(m_convertButton, &QPushButton::clicked, this, [this]() {
        m_stackedWidget->setCurrentIndex(PAGE_CONVERTING);
        m_worker->setDebFilePath(m_fileChooserWidget->getFilePath());
        m_worker->requestConvertDeb(m_fileChooserEdit->lineEdit()->text());
    });
    m_stackedWidget->addWidget(m_fileChooserPage);
}

void MainWindow::initConvertingPage()
{
    m_convertingPage = new QWidget();

    QVBoxLayout *layout = new QVBoxLayout(m_convertingPage);
    layout->setContentsMargins(50, 90, 50, 50);
    layout->setSpacing(20);

    m_loadingIndicator = new DWaterProgress();
    m_loadingIndicator->setFixedSize(84, 84);
    m_loadingIndicator->start();
    m_loadingIndicator->setValue(0);

    layout->addWidget(m_loadingIndicator, 0, Qt::AlignHCenter);

    m_convertingLabel = new DLabel(tr("Converting..."));
    m_convertingLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_convertingLabel);
    layout->addStretch();

    m_stackedWidget->addWidget(m_convertingPage);
}

void MainWindow::initConvertSuccessPage()
{
    m_convertSuccessPage = new QWidget();

    QVBoxLayout *layout = new QVBoxLayout(m_convertSuccessPage);
    layout->setContentsMargins(10, 50, 10, 30);
    layout->setSpacing(10);

    m_convertSuccessIcon = new QLabel();
    m_convertSuccessIcon->setPixmap(QIcon::fromTheme("icon_success").pixmap(96, 96));
    layout->addWidget(m_convertSuccessIcon, 0, Qt::AlignHCenter);

    m_convertSuccessLabel = new DLabel(tr("Theme converted successfully!"));
    m_convertSuccessLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_convertSuccessLabel);

    m_openFileButton = new QPushButton(tr("Open File Location"));
    m_openFileButton->setFixedWidth(180);

    m_finishButton = new QPushButton(tr("Done"));
    m_finishButton->setFixedWidth(180);

    layout->addStretch();
    layout->addWidget(m_openFileButton, 0, Qt::AlignHCenter);
    layout->addWidget(m_finishButton, 0, Qt::AlignHCenter);

    connect(m_finishButton, &QPushButton::clicked, this, [this]() {
        m_stackedWidget->setCurrentIndex(PAGE_FILE_CHOOSER);
    });

    connect(m_openFileButton, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_fileChooserEdit->lineEdit()->text()));
    });
    m_stackedWidget->addWidget(m_convertSuccessPage);
}

void MainWindow::initConvertFailPage()
{
    m_convertFailPage = new QWidget();

    QVBoxLayout *layout = new QVBoxLayout(m_convertFailPage);
    layout->setContentsMargins(10, 50, 10, 30);
    layout->setSpacing(10);

    QLabel *failIcon = new QLabel();
    failIcon->setPixmap(QIcon::fromTheme("icon_fail").pixmap(96, 96));
    layout->addWidget(failIcon, 0, Qt::AlignHCenter);

    m_convertFailLabel = new DLabel(tr("Theme conversion failed, please try again"));
    m_convertFailLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_convertFailLabel);

    m_retryButton = new QPushButton(tr("Retry"));
    m_retryButton->setFixedWidth(180);
    
    m_cancelButton = new QPushButton(tr("Cancel"));
    m_cancelButton->setFixedWidth(180);

    layout->addStretch();
    layout->addWidget(m_retryButton, 0, Qt::AlignHCenter);
    layout->addWidget(m_cancelButton, 0, Qt::AlignHCenter);

    connect(m_retryButton, &QPushButton::clicked, this, [this]() {
        m_stackedWidget->setCurrentIndex(PAGE_CONVERTING);
        m_worker->setDebFilePath(m_fileChooserWidget->getFilePath());
        m_worker->requestConvertDeb(m_fileChooserEdit->lineEdit()->text());
    });

    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        m_stackedWidget->setCurrentIndex(PAGE_FILE_CHOOSER);
    });
    m_stackedWidget->addWidget(m_convertFailPage);
}
