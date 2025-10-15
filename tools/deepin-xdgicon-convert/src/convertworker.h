// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QThread>
#include <QFile>

class ConvertHandler : public QObject
{
    Q_OBJECT
public:
    explicit ConvertHandler(QObject *parent = nullptr);
    ~ConvertHandler();

public slots:
    bool checkDebValid(const QString &debFilePath);
    bool xdgIcon2DciDeb(const QString &debFilePath, const QString &outDir);

private:
    bool unpackDeb(const QString &debFilePath);
    bool prepareDebDir(const QString &srcDebUnpackDir);
    void ensureConvertXdgIconDir(const QString &xdgIconThemeDir, const QString &outDir);
    bool doConvert(const QString &xdgIconDir, const QString &outDir);
    bool doPackageDeb(const QString &debDir, const QString &outDir);

    // utils
    QString incrementVersion(const QString& version);
    bool copyDirectoryContents(const QString &src, const QString &dst, const QStringList &excludeList = QStringList());
    bool generateMd5Sums(const QString &rootDir, const QString &currentDir, QFile &md5sumsFile);

signals:
    void checkFinished(bool ok);
    void convertFinished(bool ok);
    void convertProgressChanged(int value);
};

class ConvertWorker : public QObject
{
    Q_OBJECT
public:
    explicit ConvertWorker(QObject *parent = nullptr);
    ~ConvertWorker();

    void setDebFilePath(const QString &debFilePath);
    void requestCheckDebValid();
    void requestConvertDeb(const QString &outDir);
    void clear();

signals:
    void checkFinished(bool ok);
    void convertFinished(bool ok);
    void convertProgressChanged(int value);

private:
    QThread *m_workerThread = nullptr;
    ConvertHandler *m_handler = nullptr;
    QString m_debFilePath;
    QString m_themeID;
};
