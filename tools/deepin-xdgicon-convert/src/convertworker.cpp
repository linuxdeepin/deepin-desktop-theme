// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "convertworker.h"

#include <QProcess>
#include <QDebug>
#include <QDir>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QElapsedTimer>

#define DPKG_TOOL "/usr/bin/dpkg-deb"
#define DTK6_DCI_THEME_TOOL "/usr/libexec/dtk6/DGui/bin/dci-icon-theme"
#define DCI_COMPRESSION_LEVEL 95

#define TMP_DIR "/tmp/xdgiconconvert"
// 解压deb目标路径
#define UNPACK_DIR QString(TMP_DIR) + "/deb_unpack"
// 整理后的xdg图标路径
#define XDG_ICON_DIR QString(TMP_DIR) + "/xdgicon"
// 待打包deb路径
#define TAR_DEB_DIR QString(TMP_DIR) + "/tar_deb"
// dci图标输出路径
#define DCI_OUTPUT_DIR QString(UNPACK_DIR) + "/usr/share/dsg/icons"

ConvertHandler::ConvertHandler(QObject *parent)
    : QObject(parent)
{

}

ConvertHandler::~ConvertHandler()
{

}

bool ConvertHandler::checkDebValid(const QString &debFilePath)
{
    if (debFilePath.isEmpty()) {
        qWarning() << "debFilePath is empty";
        emit checkFinished(false);
        return false;
    }

    if (!QFile::exists(debFilePath)) {
        qWarning() << "debFilePath not exists:" << debFilePath;
        emit checkFinished(false);
        return false;
    }

    if (!unpackDeb(debFilePath)) {
        emit checkFinished(false);
        return false;
    }

    const QString iconDirPath = UNPACK_DIR + "/usr/share/icons";
    if (!QDir(iconDirPath).exists()) {
        qWarning() << "icon dir not exists:" << iconDirPath;
        emit checkFinished(false);
        return false;
    }

    const QStringList themeDirs = QDir(iconDirPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (themeDirs.size() == 1 && themeDirs.first() == "hicolor") {
        qWarning() << "only hicolor theme found, invalid icon theme package";
        emit checkFinished(false);
        return false;
    }

    emit checkFinished(true);
    return true;
}

bool ConvertHandler::xdgIcon2DciDeb(const QString &debFilePath, const QString &outDir)
{
    emit convertProgressChanged(0);
    // 1 解压
    if (debFilePath.isEmpty()) {
        emit convertFinished(false);
        qWarning() << "debFilePath is empty";
        return false;
    }

    if (!QDir(UNPACK_DIR).exists()) {
        qInfo() << "unpack dir not exists, unpacking first";
        if (!unpackDeb(debFilePath)) {
            emit convertFinished(false);
            return false;
        }
    }
    emit convertProgressChanged(20);

    const QString unpackXdgIconRootDir = UNPACK_DIR + "/usr/share/icons";

    // 2 复制出需要的xdgIcon目录
    // 获取themeId 也就是图标主题文件夹名称
    QDir dir(unpackXdgIconRootDir);
    if (!dir.exists()) {
        qWarning() << "icon theme dir not exists:" << unpackXdgIconRootDir;
    }
    const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.isEmpty()) {
        qWarning() << "no theme dir found in:" << unpackXdgIconRootDir;
    }
    if (entries.size() > 1) {
        qWarning() << "multiple theme dirs found";
    }
    QFileInfo xdgIconThemeDirInfo = entries.first();
    QString themeId = xdgIconThemeDirInfo.fileName();
    qInfo() << "theme ID:" << themeId;

    ensureConvertXdgIconDir(xdgIconThemeDirInfo.absoluteFilePath(), XDG_ICON_DIR);

    emit convertProgressChanged(30);

    // 3 转换dci
    if (!doConvert(XDG_ICON_DIR, DCI_OUTPUT_DIR + "/" + themeId)) {
        emit convertFinished(false);
        return false;
    }

    emit convertProgressChanged(60);

    // 4 准备打包前的目录结构
    if (!prepareDebDir(UNPACK_DIR)) {
        emit convertFinished(false);
        return false;
    }

    emit convertProgressChanged(80);

    // 5 打包
    if (!doPackageDeb(UNPACK_DIR, outDir)) {
        emit convertFinished(false);
        return false;
    }

    emit convertProgressChanged(100);

    emit convertFinished(true);
    return true;
}

bool ConvertHandler::unpackDeb(const QString &debFilePath)
{
    qInfo() << "unpack deb:" << debFilePath;

    QDir dir(UNPACK_DIR);
    if (dir.exists()) {
        dir.removeRecursively();
    }
    dir.mkpath(UNPACK_DIR);

    QProcess process;
    QStringList args;
    args << "-R" << debFilePath << UNPACK_DIR;
    process.start(DPKG_TOOL, args);
    process.waitForFinished(100 * 1000);
    
    if (process.exitCode() == 0) {
        qInfo() << "unpack deb success";
        return true;
    } else {
        qWarning() << "unpack deb failed:" << process.readAllStandardError();
        return false;
    }
}

bool ConvertHandler::prepareDebDir(const QString &srcDebUnpackDir)
{
    qInfo() << "prepare deb dir:" << srcDebUnpackDir << DCI_OUTPUT_DIR;

    const QString controlFile = srcDebUnpackDir + "/DEBIAN/control";
    const QString md5sumsFile = srcDebUnpackDir + "/DEBIAN/md5sums";

    // 1. 修补版本号
    if (!QFile::exists(controlFile)) {
        qWarning() << "control file not exists:" << controlFile;
        return false;
    }

    // 读取 control 文件内容
    QStringList controlLines;
    QFile controlRead(controlFile);
    if (!controlRead.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "failed to open control file for reading";
        return false;
    }

    bool versionUpdated = false;
    while (!controlRead.atEnd()) {
        QString line = controlRead.readLine();
        // 不要 trim，保留原始格式
        if (line.startsWith("Version:")) {
            // 提取版本号（保留冒号后的空格）
            int colonPos = line.indexOf(':');
            if (colonPos != -1) {
                QString prefix = line.left(colonPos + 1);  // "Version:"
                QString versionPart = line.mid(colonPos + 1).trimmed();  // 版本号部分
                QString newVersion = incrementVersion(versionPart);
                line = prefix + " " + newVersion + "\n";
                versionUpdated = true;
                qInfo() << "bump version:" << versionPart << "->" << newVersion;
            }
        }
        controlLines.append(line);
    }
    controlRead.close();
    
    // 写回 control 文件
    QFile controlWrite(controlFile);
    if (!controlWrite.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "failed to open control file for writing";
        return false;
    }
    for (const QString &line : controlLines) {
        controlWrite.write(line.toUtf8());
    }
    controlWrite.close();
    
    if (!versionUpdated) {
        qWarning() << "version not found or updated in control file";
    }

    // 2. 生成新的 md5sums 文件（递归计算所有文件）
    if (QFile::exists(md5sumsFile)) {
        QFile::remove(md5sumsFile);
    }

    QFile md5sums(md5sumsFile);
    if (!md5sums.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "failed to open md5sums file for writing";
        return false;
    }

    // 递归计算所有文件的 md5（排除 DEBIAN 目录）
    if (!generateMd5Sums(srcDebUnpackDir, srcDebUnpackDir, md5sums)) {
        md5sums.close();
        qWarning() << "failed to generate md5sums";
        return false;
    }

    md5sums.close();
    qInfo() << "prepare deb dir finished";
    return true;
}

void ConvertHandler::ensureConvertXdgIconDir(const QString &xdgIconThemeDir, const QString &outDir)
{
    qInfo() << "ensure convert xdg icon dir:" << xdgIconThemeDir << outDir;
    static const QStringList excludeList = {
        "cursors",
        "cursors.theme",
    };

    // 确保源目录存在
    QDir srcDir(xdgIconThemeDir);
    if (!srcDir.exists()) {
        qWarning() << "source directory not exists:" << xdgIconThemeDir;
        return;
    }

    // 当前仅复制目录内容
    copyDirectoryContents(xdgIconThemeDir, outDir, excludeList);

    qInfo() << "copy directory finished";
}

bool ConvertHandler::doConvert(const QString &xdgIconDir, const QString &outDir)
{
    qInfo() << "convert xdg icon to dci:" << xdgIconDir << outDir;

    QStringList arguments;
    arguments << xdgIconDir;
    arguments << "-o" << outDir;
    arguments << "-O" << QString("3=%1").arg(DCI_COMPRESSION_LEVEL);

    QElapsedTimer timer;
    timer.start();

    QProcess process;
    process.start(DTK6_DCI_THEME_TOOL, arguments);
    process.waitForFinished(60 * 1000);

    if (process.exitCode() != 0) {
        qWarning() << "convert xdg icon to dci failed:" << process.readAllStandardError();
        return false;
    }

    qInfo() << "convert finished, elapsed time:" << timer.elapsed() << "ms";

    return process.exitCode() == 0;
}

bool ConvertHandler::doPackageDeb(const QString &debDir, const QString &outDir)
{
    qInfo() << "package deb:" << debDir << outDir;

    QProcess process;
    QStringList args;
    args << "-Zxz" << "-b" << debDir << outDir;
    process.start(DPKG_TOOL, args);
    process.waitForFinished(100 * 1000);

    if (process.exitCode() == 0) {
        qInfo() << "package deb success";
        return true;
    } else {
        qWarning() << "package deb failed:" << process.readAllStandardError();
        return false;
    }
}

QString ConvertHandler::incrementVersion(const QString& version)
{
    if (version.trimmed().isEmpty()) {
        return version;
    }

    // 查找最后一个连续的数字序列
    QRegularExpression regex(R"(\d+)");
    QRegularExpressionMatchIterator it = regex.globalMatch(version);
    
    QRegularExpressionMatch lastMatch;
    while (it.hasNext()) {
        lastMatch = it.next();
    }

    // 如果找到数字，则递增
    if (lastMatch.hasMatch()) {
        QString numStr = lastMatch.captured(0);
        bool ok;
        int num = numStr.toInt(&ok);

        if (ok) {
            // 保持前导零（如果有）
            QString newNum = QString::number(num + 1);
            if (numStr.startsWith('0') && numStr.length() > 1) {
                newNum = newNum.rightJustified(numStr.length(), '0');
            }

            // 替换原数字
            QString result = version;
            result.replace(lastMatch.capturedStart(), 
                          lastMatch.capturedLength(), 
                          newNum);
            return result;
        }
    }

    // 未找到数字，返回原版本
    qWarning() << "no numeric part found in version:" << version << "falling back to original";
    return version;
}

bool ConvertHandler::copyDirectoryContents(const QString &src, const QString &dst, const QStringList &excludeList)
{
    QDir srcDir(src);
    if (!srcDir.exists()) return false;

    QDir().mkpath(dst);

    foreach (const QString &entry, srcDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString srcPath = src + "/" + entry;
        QString dstPath = dst + "/" + entry;

        QFileInfo info(srcPath);
        if (excludeList.contains(entry)) {
            qInfo() << "exclude entry:" << entry;
            continue;
        }
        if (info.isDir()) {
            if (!copyDirectoryContents(srcPath, dstPath, excludeList)) return false;
        } else {
            QFile::remove(dstPath);  // 如果目标文件已存在则删除
            if (!QFile::copy(srcPath, dstPath)) return false;
        }
    }
    return true;
}

bool ConvertHandler::generateMd5Sums(const QString &rootDir, const QString &currentDir, QFile &md5sumsFile)
{
    QDir dir(currentDir);
    if (!dir.exists()) {
        return false;
    }

    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QFileInfo &entry : entries) {
        QString absolutePath = entry.absoluteFilePath();
        QString relativePath = absolutePath.mid(rootDir.length() + 1);  // 去掉根目录前缀

        // 跳过 DEBIAN 目录
        if (relativePath.startsWith("DEBIAN")) {
            continue;
        }

        if (entry.isDir()) {
            // 递归处理子目录
            if (!generateMd5Sums(rootDir, absolutePath, md5sumsFile)) {
                return false;
            }
        } else if (entry.isFile()) {
            // 计算文件的 md5
            QFile file(absolutePath);
            if (!file.open(QIODevice::ReadOnly)) {
                qWarning() << "failed to open file for md5:" << absolutePath;
                continue;
            }

            QCryptographicHash hash(QCryptographicHash::Md5);
            if (!hash.addData(&file)) {
                qWarning() << "failed to calculate md5 for:" << absolutePath;
                file.close();
                continue;
            }
            file.close();

            QString md5Hex = hash.result().toHex();
            QString md5Line = md5Hex + "  " + relativePath + "\n";
            md5sumsFile.write(md5Line.toUtf8());
            
            qDebug() << "md5:" << md5Hex << relativePath;
        }
    }
    
    return true;
}

ConvertWorker::ConvertWorker(QObject *parent)
    : QObject(parent)
    , m_workerThread(new QThread(this))
    , m_handler(new ConvertHandler())
{
    m_handler->moveToThread(m_workerThread);
    m_workerThread->start();
    clear();

    connect(m_handler, &ConvertHandler::checkFinished, this, &ConvertWorker::checkFinished);
    connect(m_handler, &ConvertHandler::convertFinished, this, &ConvertWorker::convertFinished);
    connect(m_handler, &ConvertHandler::convertProgressChanged, this, &ConvertWorker::convertProgressChanged);
}

ConvertWorker::~ConvertWorker()
{
    qDebug() << "destroy ConvertWorker";
    m_workerThread->quit();
    m_workerThread->wait();
    clear();
}

void ConvertWorker::setDebFilePath(const QString &debFilePath)
{
    qInfo() << "set deb file path:" << m_debFilePath;
    m_debFilePath = debFilePath;
}

void ConvertWorker::requestCheckDebValid()
{
    qInfo() << "request check deb valid:" << m_debFilePath;
    QMetaObject::invokeMethod(m_handler, "checkDebValid", Qt::QueuedConnection,
                                              Q_ARG(QString, m_debFilePath));
}

void ConvertWorker::requestConvertDeb(const QString &outDir)
{
    qInfo() << "request convert deb:" << m_debFilePath << outDir;
    QMetaObject::invokeMethod(m_handler, "xdgIcon2DciDeb", Qt::QueuedConnection,
                                              Q_ARG(QString, m_debFilePath),
                                              Q_ARG(QString, outDir));
}

void ConvertWorker::clear()
{
    qInfo() << "clear status and temp files";
    m_debFilePath = "";
    m_themeID = "";

    QDir dir(TMP_DIR);
    if (dir.exists()) {
        dir.removeRecursively();
    }
}
