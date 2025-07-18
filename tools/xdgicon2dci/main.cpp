#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QDateTime>
#include <QTextStream>
#include <QCryptographicHash>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDirIterator>
#include <QtConcurrent>
#include <QMutex>
#include <QMap>

// 默认配置宏定义
#define DEFAULT_SOURCE_DIR "/usr/share/icons/hicolor"
#define DEFAULT_TARGET_DIR "/usr/share/dsg/icons/convert"
#define DEFAULT_RECORD_FILE "/var/lib/deepin-desktop-theme/xdgicon2dci-record"
#define DEFAULT_LOG_FILE "/var/log/xdgicon2dci.log"
#define DCI_TOOL_PATH_DTK6 "/usr/libexec/dtk6/DGui/bin/dci-icon-theme"
#define TEMP_DIR_MAIN "xdgicon2dci-temp"
#define TEMP_DIR_MULTISIZE "xdgicon2dci-temp-multisize"
#define TEMP_DIR_SINGLESIZE "xdgicon2dci-temp-singlesize"
#define SUPPORTED_CONTEXT "apps"
#define DCI_COMPRESSION_LEVEL "3=95"

struct ConvertTask {
    QString sourceFile;
    QString relativePath;
};

struct MultiSizeConvertTask {
    QString iconName;
    QStringList sourceFiles;
    QStringList sizes;
};

class HicolorConverter {
public:
    HicolorConverter();
    ~HicolorConverter();
    
    bool initialize();
    int run();
    
    void setSourceDir(const QString &sourceDir);
    void setTargetDir(const QString &targetDir);

private:
    QString m_sourceDir;
    QString m_targetDir;
    
    QFile m_logFileHandle;
    QTextStream m_logStream;
    
    int m_totalConverted;
    int m_totalSkipped;
    int m_totalFailed;
    
    QMutex m_logMutex;
    QMutex m_recordMutex;
    
    QMap<QString, QString> m_recordCache;
    bool m_recordCacheLoaded = false;
    bool m_recordCacheModified = false;

    // 目录扫描缓存
    struct DirectoryCache {
        QStringList sizeDirectories;  // 所有尺寸目录 (如 16x16/apps, 24x24/apps)
        QStringList appDirectories;   // 所有应用目录 (如 /apps, scalable/apps)
        QMap<QString, QStringList> iconFilesByDir;  // 每个目录下的图标文件
        QSet<QString> allIconNames;   // 所有图标名称
        bool isInitialized = false;
    };
    DirectoryCache m_dirCache;
    
    // 支持的上下文类型
    QStringList m_supportedContexts = {SUPPORTED_CONTEXT};
    
    // 图标查找优先级顺序 multisize: 多尺寸图标，singlesize: 单尺寸图标
    QStringList m_iconPriorities = {
        "multisize",
        "singlesize/scalable/apps",
        "singlesize/symbolic/apps", 
        "singlesize/apps"
    };

    bool checkDciTool();
    bool createDirectories();
    void initializeDirectoryCache();

    void loadRecordCache();
    void flushRecordCache();

    void logMessage(const QString &message, bool console = false);
    QString getFileHash(const QString &filePath);

    void scanAndConvert();
    void cleanupOrphanedDci();

    QString getRelativePath(const QString &basePath, const QString &fullPath);
    QStringList getSupportedIconFiles(const QString &directory);
    bool ensureDirectoryExists(const QString &dirPath);

    void convertMultiSizeIconBatch(const QList<MultiSizeConvertTask> &tasks, const QString &outputDir);
    void convertSingleSizeIconBatch(const QList<ConvertTask> &tasks, const QString &outputDir);
    void copyAllDciFiles(const QString &tempDir);
    
    // 处理同名文件冲突的辅助函数
    bool shouldCopyFile(const QString &sourceFile, const QString &destDir, QSet<QString> &copiedFileNames);
};

HicolorConverter::HicolorConverter()
    : m_totalConverted(0)
    , m_totalSkipped(0)
    , m_totalFailed(0)
{
    m_sourceDir = DEFAULT_SOURCE_DIR;
    m_targetDir = DEFAULT_TARGET_DIR;
}

HicolorConverter::~HicolorConverter()
{
    if (m_logFileHandle.isOpen()) {
        m_logFileHandle.close();
    }
}

bool HicolorConverter::initialize()
{
    if (!checkDciTool()) {
        return false;
    }
    
    if (!createDirectories()) {
        return false;
    }
    
    m_logFileHandle.setFileName(DEFAULT_LOG_FILE);
    if (!m_logFileHandle.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qCritical() << "Cannot open log file:" << DEFAULT_LOG_FILE;
        return false;
    }
    m_logStream.setDevice(&m_logFileHandle);
    
    initializeDirectoryCache();
    loadRecordCache();
    return true;
}

bool HicolorConverter::checkDciTool()
{
    QFileInfo toolInfo(DCI_TOOL_PATH_DTK6);
    if (!toolInfo.exists()) {
        qCritical() << "Error: DCI tool not found:" << DCI_TOOL_PATH_DTK6;
        return false;
    }
    
    if (!toolInfo.isExecutable()) {
        qCritical() << "Error: DCI tool not executable:" << DCI_TOOL_PATH_DTK6;
        return false;
    }
    
    return true;
}

bool HicolorConverter::createDirectories()
{
    if (!ensureDirectoryExists(m_targetDir)) {
        qCritical() << "Cannot create target directory:" << m_targetDir;
        return false;
    }
    
    QFileInfo recordFileInfo(DEFAULT_RECORD_FILE);
    if (!ensureDirectoryExists(recordFileInfo.dir().path())) {
        qCritical() << "Cannot create record file directory:" << recordFileInfo.dir().path();
        return false;
    }
    
    QFileInfo logFileInfo(DEFAULT_LOG_FILE);
    if (!ensureDirectoryExists(logFileInfo.dir().path())) {
        qCritical() << "Cannot create log file directory:" << logFileInfo.dir().path();
        return false;
    }
    
    return true;
}

void HicolorConverter::initializeDirectoryCache()
{
    if (m_dirCache.isInitialized) {
        return;
    }
    
    m_dirCache.sizeDirectories.clear();
    m_dirCache.appDirectories.clear();
    m_dirCache.iconFilesByDir.clear();
    m_dirCache.allIconNames.clear();
    
    QDir sourceDir(m_sourceDir);
    QStringList entries = sourceDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    // 扫描所有尺寸目录和特殊目录
    for (const QString &entry : entries) {
        for (const QString &context : m_supportedContexts) {
            QString contextDir = m_sourceDir + "/" + entry + "/" + context;
            
            if (QDir(contextDir).exists()) {
                // 检查是否是尺寸目录 (如 16x16, 24x24)
                if (QStringList parts = entry.split('x'); parts.size() == 2 && parts.first() == parts.last()) {
                    if (parts[0].toInt() && parts[1].toInt()) {
                        m_dirCache.sizeDirectories.append(contextDir);
                    }
                } else {
                    // 其他目录 (如 scalable/apps)
                    m_dirCache.appDirectories.append(contextDir);
                }
                
                QStringList iconFiles = getSupportedIconFiles(contextDir);
                m_dirCache.iconFilesByDir[contextDir] = iconFiles;
                
                // 收集所有图标名称
                for (const QString &iconFile : iconFiles) {
                    QFileInfo fileInfo(iconFile);
                    QString iconName = fileInfo.completeBaseName();
                    m_dirCache.allIconNames.insert(iconName);
                }
            }
        }
    }
    
    m_dirCache.isInitialized = true;
}

void HicolorConverter::logMessage(const QString &message, bool console)
{
    QMutexLocker locker(&m_logMutex);
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString logLine = QString("[%1] %2").arg(timestamp, message);
    if (console) {
        qDebug().noquote() << logLine;
    }
    
    if (m_logStream.device()) {
        m_logStream << logLine << Qt::endl;
        m_logStream.flush();
    }
}

QString HicolorConverter::getFileHash(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    return hash.result().toHex();
}

void HicolorConverter::loadRecordCache()
{
    if (m_recordCacheLoaded) {
        return;
    }
    
    QFile recordFile(DEFAULT_RECORD_FILE);
    if (recordFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&recordFile);
        QString line;
        int recordCount = 0;
        
        while (stream.readLineInto(&line)) {
            QStringList parts = line.split('|');
            if (parts.size() >= 2) {
                QString iconName = parts[0];
                QString hash = parts[1];
                m_recordCache[iconName] = hash;
                recordCount++;
            }
        }
        recordFile.close();        
    } else {
        logMessage("Record file not found or unreadable, using empty cache");
    }
    
    m_recordCacheLoaded = true;
    m_recordCacheModified = false;
}

void HicolorConverter::flushRecordCache()
{
    if (!m_recordCacheLoaded || !m_recordCacheModified) {
        return;
    }
    
    QMutexLocker locker(&m_recordMutex);
    
    QFile recordFile(DEFAULT_RECORD_FILE);
    if (!recordFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        logMessage(QString("Warning: Cannot write record file: %1").arg(DEFAULT_RECORD_FILE));
        return;
    }
    
    QTextStream stream(&recordFile);
    int recordCount = 0;
    
    for (auto it = m_recordCache.begin(); it != m_recordCache.end(); ++it) {
        stream << it.key() << "|" << it.value() << Qt::endl;
        recordCount++;
    }

    recordFile.close();
    m_recordCacheModified = false;
}

QString HicolorConverter::getRelativePath(const QString &basePath, const QString &fullPath)
{
    QDir baseDir(basePath);
    return baseDir.relativeFilePath(fullPath);
}

QStringList HicolorConverter::getSupportedIconFiles(const QString &directory)
{
    QStringList result;
    QDir dir(directory);

    if (!dir.exists()) {
        return result;
    }

    QStringList filters;
    filters << "*.svg" << "*.png";
    
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo &fileInfo : files) {
        result << fileInfo.absoluteFilePath();
    }
    
    return result;
}

bool HicolorConverter::ensureDirectoryExists(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists() && !dir.mkpath(".")) {
        logMessage(QString("Cannot create directory: %1").arg(dirPath));
        return false;
    }
    return true;
}

bool HicolorConverter::shouldCopyFile(const QString &sourceFile, const QString &destDir, QSet<QString> &copiedFileNames)
{
    // 处理同名但是不同格式的图标，dci只能取其一，优先为svg
    QFileInfo sourceInfo(sourceFile);
    QString baseName = sourceInfo.completeBaseName();
    QString extension = sourceInfo.suffix().toLower();
    
    if (!copiedFileNames.contains(baseName)) {
        copiedFileNames.insert(baseName);
        return true;
    }
    
    if (extension == "svg") {
        QDir dir(destDir);
        QStringList filters;
        filters << baseName + ".png" << baseName + ".jpg" << baseName + ".jpeg";
        QStringList existingFiles = dir.entryList(filters, QDir::Files);
        
        if (!existingFiles.isEmpty()) {
            // 删除已存在的非SVG文件，优先保留SVG
            for (const QString &existingFile : existingFiles) {
                QString fullPath = destDir + "/" + existingFile;
                if (QFile::exists(fullPath)) {
                    QFile::remove(fullPath);
                }
            }
            return true;
        }
        
        QString svgFile = destDir + "/" + baseName + ".svg";
        if (QFile::exists(svgFile)) {
            return false;
        }
        return true;
    } else {
        // 当前文件不是SVG，检查是否已有SVG版本
        QString svgFile = destDir + "/" + baseName + ".svg";
        if (QFile::exists(svgFile)) {
            return false;
        }
        return true;
    }
}

void HicolorConverter::setSourceDir(const QString &sourceDir)
{
    m_sourceDir = sourceDir;
}

void HicolorConverter::setTargetDir(const QString &targetDir)
{
    m_targetDir = targetDir;
}

void HicolorConverter::scanAndConvert()
{
    QMap<QString, MultiSizeConvertTask> multiSizeTasks;
    
    for (const QString &sizeDir : m_dirCache.sizeDirectories) {
        QStringList iconFiles = m_dirCache.iconFilesByDir.value(sizeDir);
        
        // 从路径中提取尺寸信息 (如 16x16/apps -> 16)
        QFileInfo dirInfo(sizeDir);
        QString sizeStr = dirInfo.dir().dirName();
        if (sizeStr.contains("x")) {
            sizeStr = sizeStr.split("x").first();
        }
        
        for (const QString &sourceFile : iconFiles) {
            QFileInfo sourceInfo(sourceFile);
            QString iconName = sourceInfo.completeBaseName();
            
            // 记录尺寸信息
            if (!multiSizeTasks.contains(iconName)) {
                multiSizeTasks[iconName] = MultiSizeConvertTask{iconName, {}, {}};
            }
            multiSizeTasks[iconName].sourceFiles.append(sourceFile);
            multiSizeTasks[iconName].sizes.append(sizeStr);
        }
    }

    QString mainTempDir = QDir::temp().absoluteFilePath(TEMP_DIR_MAIN);
    QString multiSizeTempDir = mainTempDir + "/multisize";
    QString singleSizeTempDir = mainTempDir + "/singlesize";
    
    ensureDirectoryExists(multiSizeTempDir);
    ensureDirectoryExists(singleSizeTempDir);

    // 多尺寸图标开始转换
    if (!multiSizeTasks.isEmpty()) {
        QList<MultiSizeConvertTask> tasks = multiSizeTasks.values();
        convertMultiSizeIconBatch(tasks, multiSizeTempDir);
    }
    
    QList<ConvertTask> singleSizeIconTasks;
    
    // 处理所有应用目录下的图标（不考虑优先级）
    for (const QString &appDir : m_dirCache.appDirectories) {
        QStringList iconFiles = m_dirCache.iconFilesByDir.value(appDir);
        
        for (const QString &sourceFile : iconFiles) {
            QFileInfo sourceInfo(sourceFile);
            QString iconName = sourceInfo.completeBaseName();

            QString relativePath = getRelativePath(m_sourceDir, sourceFile);
            singleSizeIconTasks.append({sourceFile, relativePath});
        }
    }
    
    // 执行单尺寸图标批量转换
    if (!singleSizeIconTasks.isEmpty()) {
        convertSingleSizeIconBatch(singleSizeIconTasks, singleSizeTempDir);
    }
    
    copyAllDciFiles(mainTempDir);
    
    // 清理临时目录
    QDir(mainTempDir).removeRecursively();
}

void HicolorConverter::cleanupOrphanedDci()
{
    int cleanedCount = 0;
    
    QStringList toRemove;
    for (auto it = m_recordCache.begin(); it != m_recordCache.end(); ++it) {
        QString iconName = it.key();
        
        bool shouldKeep = m_dirCache.allIconNames.contains(iconName);
        
        if (!shouldKeep) {
            // 源文件不存在，删除对应的 dci 文件
            QString targetFile = m_targetDir + "/" + iconName + ".dci";
            if (QFile::exists(targetFile)) {
                if (QFile::remove(targetFile)) {
                    cleanedCount++;
                }
            }
            // 标记从缓存中移除
            toRemove << iconName;
        }
    }
    
    // 从内存缓存中移除孤立的记录
    for (const QString &iconName : toRemove) {
        m_recordCache.remove(iconName);
        m_recordCacheModified = true;
    }
    
    if (cleanedCount > 0) {
        logMessage(QString("Cleaned %1 orphaned files").arg(cleanedCount), true);
    }
}

void HicolorConverter::convertMultiSizeIconBatch(const QList<MultiSizeConvertTask> &tasks, const QString &outputDir)
{
    if (tasks.isEmpty()) {
        return;
    }
    
    QString tempDir = QDir::temp().absoluteFilePath(TEMP_DIR_MULTISIZE);
    ensureDirectoryExists(tempDir);

    QSet<QString> createdSizeDirs;
    QMap<QString, QSet<QString>> sizeDirCopiedFiles;  // 每个尺寸目录的已复制文件名集合
    
    // 准备目录结构 16/ 24/ 32/ 48/ 64/ 96/ 128/ 256/
    for (const MultiSizeConvertTask &task : tasks) {
        for (int i = 0; i < task.sourceFiles.size(); ++i) {
            QString sourceFile = task.sourceFiles[i];
            QString size = task.sizes[i];
            
            // 创建尺寸目录 (如 16/, 24/)
            QString sizeDir = tempDir + "/" + size;
            if (!createdSizeDirs.contains(sizeDir)) {
                ensureDirectoryExists(sizeDir);
                createdSizeDirs.insert(sizeDir);
            }
            
            if (!shouldCopyFile(sourceFile, sizeDir, sizeDirCopiedFiles[sizeDir])) {
                continue;
            }
            
            QFileInfo sourceInfo(sourceFile);
            QString destFile = sizeDir + "/" + sourceInfo.fileName();
            
            if (QFile::exists(destFile)) {
                QFile::remove(destFile);
            }
            
            if (!QFile::copy(sourceFile, destFile)) {
                logMessage(QString("Copy failed: %1 -> %2").arg(sourceFile, destFile));
            }
        }
    }
    
    // 确保输出目录不存在
    if (QDir(outputDir).exists()) {
        QDir(outputDir).removeRecursively();
    }

    QStringList arguments;
    arguments << tempDir;
    arguments << "-o" << outputDir;
    arguments << "-O" << DCI_COMPRESSION_LEVEL;
    
    QProcess process;
    process.start(DCI_TOOL_PATH_DTK6, arguments);
    process.waitForFinished(-1);

    if (process.exitCode() != 0) {
        QString errorOutput = QString::fromLocal8Bit(process.readAllStandardError());
        logMessage(QString("Multisize convert failed: %1").arg(errorOutput));
        m_totalFailed += tasks.size();
    }
    
    // 清理临时源目录
    QDir(tempDir).removeRecursively();
}

void HicolorConverter::convertSingleSizeIconBatch(const QList<ConvertTask> &tasks, const QString &outputDir)
{
    if (tasks.isEmpty()) {
        return;
    }
    
    QString singleSizeTempSourceDir = QDir::temp().absoluteFilePath(TEMP_DIR_SINGLESIZE);
    ensureDirectoryExists(singleSizeTempSourceDir);
    
    QSet<QString> createdDirs;
    QMap<QString, QSet<QString>> dirCopiedFiles;
    
    for (const ConvertTask &task : tasks) {
        QFileInfo sourceInfo(task.sourceFile);
        
        QString relativePath = task.relativePath;
        QFileInfo relativeInfo(relativePath);
        QString dirPath = relativeInfo.path();  // 获取目录部分
        
        QString targetDir = singleSizeTempSourceDir + "/" + dirPath;
        if (!createdDirs.contains(targetDir)) {
            ensureDirectoryExists(targetDir);
            createdDirs.insert(targetDir);
        }
        
        if (!shouldCopyFile(task.sourceFile, targetDir, dirCopiedFiles[targetDir])) {
            continue;
        }
        
        QString destFile = targetDir + "/" + sourceInfo.fileName();
        
        if (QFile::exists(destFile)) {
            QFile::remove(destFile);
        }
        
        if (!QFile::copy(task.sourceFile, destFile)) {
            logMessage(QString("Copy failed: %1 -> %2").arg(task.sourceFile, destFile));
        }
    }
    
    // 确保输出目录不存在（让转换工具自己创建）
    if (QDir(outputDir).exists()) {
        QDir(outputDir).removeRecursively();
    }
    
    QDirIterator it(singleSizeTempSourceDir, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    int convertedCount = 0;
    int failedCount = 0;
    
    while (it.hasNext()) {
        QString subDir = it.next();
        QDir dir(subDir);
        
        QStringList svgFiles = dir.entryList(QStringList() << "*.svg", QDir::Files);
        if (svgFiles.isEmpty()) {
            continue;
        }
        
        QString relativeDirPath = QDir(singleSizeTempSourceDir).relativeFilePath(subDir);
        
        // 创建对应的输出子目录路径
        QString subOutputDir = outputDir + "/" + relativeDirPath;
        
        // 执行转换命令，输出到对应的子目录
        QStringList arguments;
        arguments << subDir;
        arguments << "-o" << subOutputDir;
        arguments << "-O" << DCI_COMPRESSION_LEVEL;
        
        QProcess process;
        process.setProcessChannelMode(QProcess::SeparateChannels);
        process.setStandardOutputFile(QProcess::nullDevice());  // 抑制标准输出
        process.start(DCI_TOOL_PATH_DTK6, arguments);
        process.waitForFinished(-1);
        
        if (process.exitCode() == 0) {
            convertedCount += svgFiles.size();
        } else {
            QString errorOutput = QString::fromLocal8Bit(process.readAllStandardError());
            logMessage(QString("Convert failed: %1 - %2").arg(relativeDirPath, errorOutput));
            failedCount += svgFiles.size();
        }
    }

    if (failedCount > 0) {
        m_totalFailed += failedCount;
    }
    
    QDir(singleSizeTempSourceDir).removeRecursively();
}

void HicolorConverter::copyAllDciFiles(const QString &tempDir)
{    
    int copiedCount = 0;
    int skippedCount = 0;
    int totalFoundCount = 0;
    QSet<QString> processedIcons;
    QMap<QString, int> priorityStats;  // 统计每个优先级的复制数量
    
    for (const QString &priority : m_iconPriorities) {
        QString priorityDir = tempDir + "/" + priority;
        int priorityCopied = 0;
        
        if (!QDir(priorityDir).exists()) {
            continue;
        }
        
        QDirIterator it(priorityDir, QStringList() << "*.dci", QDir::Files, QDirIterator::Subdirectories);
        
        while (it.hasNext()) {
            QString sourcePath = it.next();
            QFileInfo fileInfo(sourcePath);
            QString dciFileName = fileInfo.fileName();
            QString iconName = fileInfo.completeBaseName();
            
            totalFoundCount++;

            if (processedIcons.contains(iconName)) {
                continue;
            }
            
            QString targetPath = m_targetDir + "/" + dciFileName;
            
            QString newFileHash = getFileHash(sourcePath);
            
            bool needCopy = true;
            if (QFile::exists(targetPath)) {
                if (m_recordCache.contains(iconName)) {
                    QString recordedHash = m_recordCache.value(iconName);
                    if (recordedHash == newFileHash) {
                        needCopy = false;
                        skippedCount++;
                    }
                }
            }
            
            if (needCopy) {
                if (QFile::exists(targetPath)) {
                    QFile::remove(targetPath);
                }
                
                if (QFile::copy(sourcePath, targetPath)) {
                    copiedCount++;
                    priorityCopied++;
                    m_recordCache[iconName] = newFileHash;
                    m_recordCacheModified = true;
                    m_totalConverted++;
                } else {
                    logMessage(QString("Copy failed: %1 -> %2").arg(sourcePath, targetPath));
                    m_totalFailed++;
                }
            }
            processedIcons.insert(iconName);
        }
        
        if (priorityCopied > 0) {
            priorityStats[priority] = priorityCopied;
        }
    }
    
    logMessage(QString("Copy stats: found %1, copied %2, skipped %3").arg(totalFoundCount).arg(copiedCount).arg(skippedCount));
}

int HicolorConverter::run()
{
    scanAndConvert();
    cleanupOrphanedDci();
    flushRecordCache();

    return 0;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("xdgicon2dci");
    
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption sourceOption(QStringList() << "s" << "source", 
                                   "src path", "path", DEFAULT_SOURCE_DIR);
    QCommandLineOption targetOption(QStringList() << "t" << "target", 
                                   "target path", "path", DEFAULT_TARGET_DIR);
    
    parser.addOption(sourceOption);
    parser.addOption(targetOption);
    
    parser.process(app);
    
    HicolorConverter converter;
    
    // 设置源路径和目标路径（如果命令行参数提供了的话）
    if (parser.isSet(sourceOption)) {
        converter.setSourceDir(parser.value(sourceOption));
    }
    if (parser.isSet(targetOption)) {
        converter.setTargetDir(parser.value(targetOption));
    }
    
    if (!converter.initialize()) {
        return 1;
    }
    
    return converter.run();
}
