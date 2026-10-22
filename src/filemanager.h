#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QDir>
#include <QFileInfo>

class FileManager : public QObject
{
    Q_OBJECT

public:
    explicit FileManager(QObject *parent = nullptr);
    ~FileManager();

    // 文件操作
    bool readFile(const QString &filePath, QString &content);
    bool writeFile(const QString &filePath, const QString &content);
    bool createFile(const QString &filePath);
    bool deleteFile(const QString &filePath);
    bool renameFile(const QString &oldPath, const QString &newPath);
    
    // 目录操作
    bool createDirectory(const QString &dirPath);
    bool deleteDirectory(const QString &dirPath);
    QStringList listFiles(const QString &dirPath, const QStringList &filters = QStringList());
    
    // 文件信息
    bool exists(const QString &path) const;
    QFileInfo getFileInfo(const QString &path) const;
    bool isFile(const QString &path) const;
    bool isDirectory(const QString &path) const;
    
    // 监视文件变化
    void watchFile(const QString &filePath);
    void watchDirectory(const QString &dirPath);
    void stopWatching(const QString &path);
    
signals:
    void fileChanged(const QString &path);
    void directoryChanged(const QString &path);

private slots:
    void onFileChanged(const QString &path);
    void onDirectoryChanged(const QString &path);

private:
    QFileSystemWatcher *m_watcher;
};

#endif // FILEMANAGER_H