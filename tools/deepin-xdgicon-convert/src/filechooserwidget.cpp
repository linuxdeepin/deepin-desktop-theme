// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filechooserwidget.h"
#include "convertworker.h"

#include <QLayout>
#include <QLabel>
#include <QPainter>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileDialog>

#include <DFontSizeManager>
#include <DGuiApplicationHelper>

DWIDGET_USE_NAMESPACE

FileChooserWidget::FileChooserWidget(ConvertWorker *worker, QWidget *parent)
    : QWidget(parent)
    , m_worker(worker)
{
    initUI();

    connect(m_worker, &ConvertWorker::checkFinished, this, [this](bool ok){
        qInfo() << "check deb finished:" << ok;
        if (ok) {
            m_stackedWidget->setCurrentIndex(CHOOSER_PAGE_SELECTED_FILE);
        } else {
            m_stackedWidget->setCurrentIndex(CHOOSER_PAGE_CHECK_ERROR);
        }
        emit checkStatusChanged(ok);
    });

    setAcceptDrops(true);
}

FileChooserWidget::~FileChooserWidget()
{

}

void FileChooserWidget::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_stackedWidget = new QStackedWidget(this);
    mainLayout->addWidget(m_stackedWidget);

    // choose file page
    m_chooseFilePage = new QWidget();
    QVBoxLayout *chooseFileLayout = new QVBoxLayout(m_chooseFilePage);
    chooseFileLayout->setContentsMargins(10, 10, 10, 10);

    m_convertIcon = new QLabel();
    m_convertIcon->setPixmap(QIcon::fromTheme("convert").pixmap(64, 64));
    m_convertIcon->setAlignment(Qt::AlignCenter);

    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, [this]() {
        m_convertIcon->setPixmap(QIcon::fromTheme("convert").pixmap(64, 64));
    });

    m_chooseTitleLabel = new QLabel(tr("Drag or click to import theme file"));
    DFontSizeManager::instance()->bind(m_chooseTitleLabel, DFontSizeManager::T6);
    m_chooseTitleLabel->setAlignment(Qt::AlignCenter);

    m_chooseDescLabel = new QLabel(tr("Converts to DCI format (.deb only) "));
    DFontSizeManager::instance()->bind(m_chooseDescLabel, DFontSizeManager::T7);
    m_chooseDescLabel->setAlignment(Qt::AlignCenter);

    chooseFileLayout->addStretch();
    chooseFileLayout->addWidget(m_convertIcon);
    chooseFileLayout->addWidget(m_chooseTitleLabel);
    chooseFileLayout->addWidget(m_chooseDescLabel);
    chooseFileLayout->addStretch();
    m_stackedWidget->addWidget(m_chooseFilePage);

    // checking page
    m_checkingPage = new QWidget();
    QVBoxLayout *checkingLayout = new QVBoxLayout(m_checkingPage);
    checkingLayout->setContentsMargins(10, 10, 10, 10);
    checkingLayout->setSpacing(10);

    m_checkingSpinner = new DSpinner();
    m_checkingSpinner->setFixedSize(32, 32);
    m_checkingSpinner->start();
    m_checkingLabel = new QLabel(tr("Verifying file, please wait..."));
    m_checkingLabel->setAlignment(Qt::AlignCenter);

    checkingLayout->addStretch();
    checkingLayout->addWidget(m_checkingSpinner, 0, Qt::AlignHCenter);
    checkingLayout->addWidget(m_checkingLabel);
    checkingLayout->addStretch();
    m_stackedWidget->addWidget(m_checkingPage);

    // selected file page
    m_selectedFilePage = new QWidget();
    QVBoxLayout *selectedFileLayout = new QVBoxLayout(m_selectedFilePage);
    selectedFileLayout->setContentsMargins(10, 10, 10, 10);
    selectedFileLayout->setSpacing(10);

    QWidget *selectedFileIconWidget = new QWidget();
    selectedFileIconWidget->setFixedSize(72, 72);
    QVBoxLayout *selectedFileIconLayout = new QVBoxLayout(selectedFileIconWidget);
    selectedFileIconLayout->setContentsMargins(0, 0, 0, 0);
    m_debFileIcon = new QLabel();
    m_debFileIcon->setPixmap(QIcon::fromTheme("deb").pixmap(64, 64));
    selectedFileIconLayout->addWidget(m_debFileIcon, 0, Qt::AlignCenter);
    m_delButton = new QToolButton(selectedFileIconWidget);
    m_delButton->setFixedSize(18, 18);
    m_delButton->setIconSize(QSize(18, 18));
    m_delButton->setIcon(QIcon::fromTheme("close"));
    m_delButton->move(selectedFileIconWidget->width() - m_delButton->width(), 0);

    m_debFileNameLabel = new QLabel("example.deb");
    m_debFileNameLabel->setAlignment(Qt::AlignCenter);

    selectedFileLayout->addStretch();
    selectedFileLayout->addWidget(selectedFileIconWidget, 0, Qt::AlignHCenter);
    selectedFileLayout->addWidget(m_debFileNameLabel, 0, Qt::AlignHCenter);
    selectedFileLayout->addStretch();
    connect(m_delButton, &QToolButton::clicked, this, &FileChooserWidget::clearFile);
    m_stackedWidget->addWidget(m_selectedFilePage);

    // check error page
    m_checkErrorPage = new QWidget();
    QVBoxLayout *errorLayout = new QVBoxLayout(m_checkErrorPage);
    errorLayout->setContentsMargins(10, 10, 10, 10);
    errorLayout->setSpacing(10);
    m_errorIcon = new QLabel();
    m_errorIcon->setPixmap(QIcon::fromTheme("dialog-error").pixmap(40, 40));
    m_errorLabel = new QLabel(tr("Supports icon theme packages only."));
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_reselectButton = new DCommandLinkButton(tr("Re-import"));
    m_reselectButton->setFixedWidth(120);
    errorLayout->addStretch();
    errorLayout->addWidget(m_errorIcon, 0, Qt::AlignHCenter);
    errorLayout->addWidget(m_errorLabel);
    errorLayout->addWidget(m_reselectButton, 0, Qt::AlignHCenter);
    errorLayout->addStretch();

    m_stackedWidget->addWidget(m_checkErrorPage);
    connect(m_reselectButton, &DCommandLinkButton::clicked, this, [this]() {
        m_stackedWidget->setCurrentIndex(CHOOSER_PAGE_CHOOSE_FILE);
    });

    // main layout
    setLayout(mainLayout);
    m_stackedWidget->setCurrentIndex(CHOOSER_PAGE_CHOOSE_FILE);
    connect(m_stackedWidget, &QStackedWidget::currentChanged, this, [this](int){ this->update(); });
}

void FileChooserWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    static auto getDefaultStyle = []() -> std::pair<QColor, QBrush> {
        if (DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType) {
            return {QColor(255, 255, 255, 255 * 0.2), Qt::NoBrush};
        } else {
            return {QColor(0, 0, 0, 255 * 0.2), Qt::NoBrush};
        }
    };

    // draw border and background
    QPen pen;
    pen.setWidth(2);
    pen.setStyle(Qt::PenStyle::DotLine);

    if (m_stackedWidget->currentIndex() == CHOOSER_PAGE_CHOOSE_FILE) {
        if (m_isPressed || m_isDragOver) {
            pen.setColor(QColor(0, 91, 255, 255 * 0.2));
            painter.setBrush(QColor(9, 91, 255, 255 * 0.1));
        } else if (m_isHover) {
            pen.setColor(QColor(0, 91, 255, 255 * 0.2));
            painter.setBrush(QColor(9, 91, 255, 255 * 0.05));
        } else {
            pen.setColor(getDefaultStyle().first);
            painter.setBrush(getDefaultStyle().second);
        }
    } else if (m_stackedWidget->currentIndex() == CHOOSER_PAGE_CHECK_ERROR) {
        pen.setColor(QColor(255, 0, 0, 255 * 0.2));
        painter.setBrush(QColor(255, 0, 0, 255 * 0.05));
    } else if (m_stackedWidget->currentIndex() == CHOOSER_PAGE_SELECTED_FILE) {
        if (m_isDragOver) {
            pen.setColor(QColor(0, 91, 255, 255 * 0.2));
            painter.setBrush(QColor(9, 91, 255, 255 * 0.1));
        } else {
            pen.setColor(getDefaultStyle().first);
            painter.setBrush(getDefaultStyle().second);
        }
    } else {
        pen.setColor(getDefaultStyle().first);
        painter.setBrush(getDefaultStyle().second);
    }
    painter.setPen(pen);
    
    painter.drawRoundedRect(rect().marginsRemoved(QMargins(1, 1, 1, 1)), 6, 6);

    QWidget::paintEvent(event);
}

void FileChooserWidget::enterEvent(QEnterEvent *event)
{
    setCursor(Qt::PointingHandCursor);
    m_isHover = true;
    update();
    QWidget::enterEvent(event);
}

void FileChooserWidget::leaveEvent(QEvent *event)
{
    unsetCursor();
    m_isHover = false;
    update();
    QWidget::leaveEvent(event);
}

void FileChooserWidget::mousePressEvent(QMouseEvent *event)
{
    m_isPressed = true;
    update();
    QWidget::mousePressEvent(event);
}

void FileChooserWidget::mouseReleaseEvent(QMouseEvent *event)
{
    m_isPressed = false;
    update();
    if (m_stackedWidget->currentIndex() == CHOOSER_PAGE_CHOOSE_FILE) {
        QString filePath = QFileDialog::getOpenFileName(this, tr("Select theme file"), QString(), "theme deb (*.deb)");
        if (!filePath.isEmpty()) {
            if (filePath.endsWith(".deb", Qt::CaseInsensitive)) {
                selectFile(filePath);
            } else {
                m_stackedWidget->setCurrentIndex(CHOOSER_PAGE_CHECK_ERROR);
            }
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void FileChooserWidget::dragEnterEvent(QDragEnterEvent *event)
{
    // 只能拖入单个deb文件，其他情况不接受
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (urls.size() == 1) {
            const QString filePath = urls.first().toLocalFile();
            if (filePath.endsWith(".deb", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                m_isDragOver = true;
                update();
                return;
            }
        }
    }
    QWidget::dragEnterEvent(event);
}

void FileChooserWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_isDragOver = false;
    update();
    QWidget::dragLeaveEvent(event);
}

void FileChooserWidget::dropEvent(QDropEvent *event)
{
    m_isDragOver = false;
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (urls.size() == 1) {
            const QString filePath = urls.first().toLocalFile();
            if (filePath.endsWith(".deb", Qt::CaseInsensitive)) {
                selectFile(filePath);
                event->acceptProposedAction();
            } else {
                m_stackedWidget->setCurrentIndex(CHOOSER_PAGE_CHECK_ERROR);
            }
        } else {
            m_stackedWidget->setCurrentIndex(CHOOSER_PAGE_CHECK_ERROR);
        }
    }
    update();
    QWidget::dropEvent(event);
}

void FileChooserWidget::selectFile(const QString &filePath)
{
    if (filePath.isEmpty())
        return;

    this->m_filePath = filePath;
    m_debFileNameLabel->setText(filePath.split("/").last());
    m_stackedWidget->setCurrentIndex(CHOOSER_PAGE_CHECKING);
    m_worker->clear();
    m_worker->setDebFilePath(filePath);
    m_worker->requestCheckDebValid();
    emit fileChanged(filePath);
}

void FileChooserWidget::clearFile()
{
    m_filePath.clear();
    m_debFileNameLabel->setText("");
    m_stackedWidget->setCurrentIndex(CHOOSER_PAGE_CHOOSE_FILE);
    m_worker->clear();
    emit fileChanged("");
}
