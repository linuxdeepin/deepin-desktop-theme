// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QGuiApplication>
#include <QDebug>
#include <QProcess>
#include <QDateTime>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFileInfo>
#include <QMap>
#include <QTemporaryDir>
#include <DLog>
#include <QLoggingCategory>
#include <QElapsedTimer>

#include <private/qiconloader_p.h>

Q_LOGGING_CATEGORY(xdgIcon2DciLog, "xdgicon2dci")

#define DEFAULT_RECORD_FILE "/var/lib/deepin-desktop-theme/xdgicon2dci-record"
#define DCI_TOOL_PATH_DTK6 "/usr/libexec/dtk6/DGui/bin/dci-icon-theme"
#define DCI_COMPRESSION_LEVEL 95
#define DEFAULT_SEARCH_PATH "/usr/share/icons"
#define RECORD_SPLIT '|'

static const QStringList SUPPORTED_ICON_FILES {"*.svg", "*.png"};
static const QList<QIconDirInfo::Context> SUPPORTED_CONTEXT {QIconDirInfo::Applications};

class XDGIcon2Dci {
public:
    XDGIcon2Dci(const QString &themeName, const QString &targetDir);

    bool initialize();
    int run();

private:
    QString m_sourceDir;
    QString m_themeName;
    QString m_targetDir;

    std::vector<QThemeIconInfo> m_needHandleIcon;

    QMap<QString, QString> loadRecordCache();
    void flushRecordCache(const QMap<QString, QString> &recordCache);

    static bool doConvert(const QString &sourceFile, const QString &destFile);
    static QString getFileHash(const QString &filePath);
    static bool ensureDirectoryExists(const QString &dirPath);

    void convertMultiSizeIconBatch(const QString &tempDirSrc, const QString &tempDirDst);
    void copyDciFiles(const QString &fromDir, QMap<QString, QString> &recordCache);

    void collectIconTasks();
    void handleIconTasks();
};

XDGIcon2Dci::XDGIcon2Dci(const QString &themeName, const QString &targetDir)
    : m_themeName(themeName)
    , m_targetDir(targetDir)
{
    m_sourceDir = QString(DEFAULT_SEARCH_PATH) + "/" + themeName;
}

bool XDGIcon2Dci::initialize()
{
    if (!QFile::exists(DCI_TOOL_PATH_DTK6)) {
        qCritical() << "Error: DCI tool not found:" << DCI_TOOL_PATH_DTK6;
        return false;
    }

    if (!ensureDirectoryExists(m_targetDir)) {
        qCritical() << "Cannot create target directory:" << m_targetDir;
        return false;
    }

    return true;
}

QString XDGIcon2Dci::getFileHash(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    return hash.result().toHex();
}

QMap<QString, QString> XDGIcon2Dci::loadRecordCache()
{
    QMap<QString, QString> recordCache;
    QFile recordFile(DEFAULT_RECORD_FILE);
    if (recordFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&recordFile);
        QString line;
        while (stream.readLineInto(&line)) {
            QStringList parts = line.split(RECORD_SPLIT);
            if (parts.size() >= 2) {
                QString iconName = parts[0];
                QString hash = parts[1];
                recordCache[iconName] = hash;
            }
        }
    }
    return recordCache;
}

void XDGIcon2Dci::flushRecordCache(const QMap<QString, QString> &recordCache)
{
    QFileInfo recordFileInfo(DEFAULT_RECORD_FILE);
    if (!ensureDirectoryExists(recordFileInfo.dir().path())) {
        qCWarning(xdgIcon2DciLog) << "Cannot create record file directory:" << recordFileInfo.dir().path();
        return;
    }

    QFile recordFile(DEFAULT_RECORD_FILE);
    if (!recordFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(xdgIcon2DciLog) << "Cannot write record file:" << DEFAULT_RECORD_FILE;
        return;
    }

    QTextStream stream(&recordFile);
    for (auto it = recordCache.begin(); it != recordCache.end(); ++it) {
        stream << it.key() << RECORD_SPLIT << it.value() << Qt::endl;
    }
}

bool XDGIcon2Dci::ensureDirectoryExists(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists() && !dir.mkpath(".")) {
        qCWarning(xdgIcon2DciLog) << "Cannot create directory:" << dirPath;
        return false;
    }
    return true;
}

void XDGIcon2Dci::collectIconTasks()
{
    // 扫描源目录中所有的图标名
    QSet<QString> allIconNames;
    QStringList supportedExtensions = SUPPORTED_ICON_FILES;
    QDirIterator it(m_sourceDir, 
                   supportedExtensions,
                   QDir::Files,
                   QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QString iconFile = it.next();
        QFileInfo fileInfo(iconFile);
        QString iconName = fileInfo.completeBaseName();
        allIconNames.insert(iconName);
    }

    QIconLoader *loader = QIconLoader::instance();
    loader->setThemeSearchPath({DEFAULT_SEARCH_PATH});
    loader->setThemeName(m_themeName);

    for (const QString &iconName : allIconNames) {
        auto iconInfo = loader->loadIcon(iconName);
        m_needHandleIcon.push_back(std::move(iconInfo));
    }
}

void XDGIcon2Dci::handleIconTasks()
{
    QTemporaryDir tempDir(QDir::tempPath());
    QTemporaryDir srcDir(tempDir.path() + "/src");

    const QString tmpSrcDir = srcDir.path();
    const QString tmpDstDir = tempDir.path() + "/dst";

    // 多尺寸图标转换
    if (!m_needHandleIcon.empty()) {
        convertMultiSizeIconBatch(tmpSrcDir, tmpDstDir);
    }

    QMap<QString, QString> recordCache = loadRecordCache();
    copyDciFiles(tmpDstDir, recordCache);
    flushRecordCache(recordCache);
}

void XDGIcon2Dci::convertMultiSizeIconBatch(const QString &tempDirSrc, const QString &tempDirDst)
{
    QSet<QString> createdSizeDirs;
    // 准备目录结构 16/ 24/ 32/ 48/ 64/ 96/ 128/ 256/
    for (const auto &iconInfo : m_needHandleIcon) {
        const auto &entries = iconInfo.entries;
        // size, <isScalable, suffix>
        QMap<short, QPair<bool, QString>> handled;

        for (const auto &entry : entries) {
            if (!SUPPORTED_CONTEXT.contains(entry->dir.context)) {
                continue;
            }

            QString sourceFile = entry->filename;
            QFileInfo sourceFileInfo(sourceFile);

            if (handled.keys().contains(entry->dir.size)) {
                // 优先svg, 非scalable的图标
                if (handled.value(entry->dir.size).second == "svg" || !handled.value(entry->dir.size).first) {
                    continue;
                } else {
                    QString rmFile = tempDirSrc + "/" + QString::number(entry->dir.size) + "/" + handled.value(entry->dir.size).second;
                    if (QFile::exists(rmFile)) {
                        QFile::remove(rmFile);
                    }
                    handled[entry->dir.size] = QPair<bool, QString>(handled.value(entry->dir.size).first, sourceFileInfo.suffix());
                }
            }

            bool isScalable = entry->dir.path.contains("scalable");
            handled.insert(entry->dir.size, QPair<bool, QString>(isScalable, sourceFileInfo.suffix()));

            QString tmpSizeDir = tempDirSrc + "/" + QString::number(entry->dir.size);
            QString tmpSizeFile = tmpSizeDir + "/" + QFileInfo(entry->filename).fileName();
            ensureDirectoryExists(tmpSizeDir);

            // 删除已存在的目标文件
            if (QFile::exists(tmpSizeFile)) {
                QFile::remove(tmpSizeFile);
            }

            if (!QFile::copy(sourceFile, tmpSizeFile)) {
                qCWarning(xdgIcon2DciLog) << "Copy failed:" << sourceFile << "->" << tmpSizeFile;
            }
        }
    }

    if (!doConvert(tempDirSrc, tempDirDst)) {
        qCWarning(xdgIcon2DciLog) << "Multisize convert failed:" << tempDirSrc << "->" << tempDirDst;
    }
}

void XDGIcon2Dci::copyDciFiles(const QString &fromDir, QMap<QString, QString> &recordCache)
{    
    int copiedCount = 0;
    int totalFoundCount = 0;

    QStringList needDeleteIcons = recordCache.keys();

    QDirIterator it(fromDir, QStringList() << "*.dci", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString sourcePath = it.next();
        QFileInfo fileInfo(sourcePath);
        QString iconName = fileInfo.completeBaseName();

        totalFoundCount++;

        QString targetPath = m_targetDir + "/" + fileInfo.fileName();
        QString newFileHash = getFileHash(sourcePath);

        // 通过Hash是否变更检查是否需要复制
        bool needCopy = false;
        if (!QFile::exists(targetPath)) {
            needCopy = true;
        } else if (recordCache.contains(iconName)) {
            QString targetFileHash = getFileHash(targetPath);
            if (targetFileHash != recordCache.value(iconName)) { // 记录的hash和目前已经存在文件hash不一致，说明目标文件已经被其他操作修改，不再跟踪
                recordCache.remove(iconName);
            } else if (recordCache.value(iconName) != newFileHash) {
                needCopy = true;
            }
        }

        if (needCopy) {
            if (QFile::exists(targetPath)) {
                QFile::remove(targetPath);
            }

            if (QFile::copy(sourcePath, targetPath)) {
                qCDebug(xdgIcon2DciLog) << "Installed:" << targetPath;
                copiedCount++;
                recordCache[iconName] = newFileHash;
                
            } else {
                qCWarning(xdgIcon2DciLog) << "Copy failed:" << sourcePath << "->" << targetPath;
            }
        }
        needDeleteIcons.removeAll(iconName);
    }

    for (const QString &iconName : needDeleteIcons) {
        QString targetFile = m_targetDir + "/" + iconName + ".dci";
        if (QFile::exists(targetFile)) {
            QFile::remove(targetFile);
            recordCache.remove(iconName);
            qCDebug(xdgIcon2DciLog) << "Removed:" << targetFile;
        }
    }
    qCInfo(xdgIcon2DciLog) << "Copy stats: found" << totalFoundCount << ", installed" << copiedCount << ", removed" << needDeleteIcons.size();
}

bool XDGIcon2Dci::doConvert(const QString &sourceFile, const QString &destFile)
{
    QStringList arguments;
    arguments << sourceFile;
    arguments << "-o" << destFile;
    arguments << "-O" << QString("3=%1").arg(DCI_COMPRESSION_LEVEL);

    QElapsedTimer timer;
    timer.start();

    QProcess process;
    process.start(DCI_TOOL_PATH_DTK6, arguments);
    process.waitForFinished(60 * 1000);

    if (process.exitCode() != 0) {
        qCWarning(xdgIcon2DciLog) << "convert err: " << process.readAllStandardError() << "exitcode: " << process.exitCode();
    }

    qCInfo(xdgIcon2DciLog) << "convert time:" << timer.elapsed() << "ms";

    return process.exitCode() == 0;
}

int XDGIcon2Dci::run()
{
    collectIconTasks();
    handleIconTasks();
    return 0;
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("DSG_APP_ID", QByteArrayView("xdgicon2dci"));
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption nameOption(QStringList() << "n" << "name", 
                                   "name", "name");
    QCommandLineOption targetOption(QStringList() << "t" << "target", 
                                   "target path", "path");

    parser.addOption(nameOption);
    parser.addOption(targetOption);
    parser.process(app);

    Dtk::Core::DLogManager::registerJournalAppender();

    XDGIcon2Dci xdgIcon2Dci(parser.value(nameOption), parser.value(targetOption));

    if (!xdgIcon2Dci.initialize()) {
        return 1;
    }

    return xdgIcon2Dci.run();
}
