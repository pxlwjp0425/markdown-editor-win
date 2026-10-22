#include "filemanager.h"

FileManager::FileManager(QObject *parent)
    : QObject(parent)
{
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &FileManager::onFileChanged);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &FileManager::onDirectoryChanged);
}

FileManager::~FileManager()
{
}

bool FileManager::readFile(const QString &filePath, QString &content)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream in(&file);
    // 显式指定 UTF-8：否则读写结果会受区域设置影响（中文 Windows 上是 GBK），
    // 同一个文件在不同机器上解出来的内容不一样。
    in.setEncoding(QStringConverter::Utf8);
    content = in.readAll();
    file.close();
    
    return true;
}

bool FileManager::writeFile(const QString &filePath, const QString &content)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    file.close();
    
    return true;
}

bool FileManager::createFile(const QString &filePath)
{
    QFile file(filePath);
    return file.open(QIODevice::WriteOnly);
}

bool FileManager::deleteFile(const QString &filePath)
{
    return QFile::remove(filePath);
}

bool FileManager::renameFile(const QString &oldPath, const QString &newPath)
{
    return QFile::rename(oldPath, newPath);
}

bool FileManager::createDirectory(const QString &dirPath)
{
    QDir dir;
    return dir.mkpath(dirPath);
}

bool FileManager::deleteDirectory(const QString &dirPath)
{
    QDir dir;
    return dir.rmdir(dirPath);
}

QStringList FileManager::listFiles(const QString &dirPath, const QStringList &filters)
{
    QDir dir(dirPath);
    if (filters.isEmpty()) {
        return dir.entryList(QDir::Files);
    }
    return dir.entryList(filters, QDir::Files);
}

bool FileManager::exists(const QString &path) const
{
    return QFileInfo::exists(path);
}

QFileInfo FileManager::getFileInfo(const QString &path) const
{
    return QFileInfo(path);
}

bool FileManager::isFile(const QString &path) const
{
    return QFileInfo(path).isFile();
}

bool FileManager::isDirectory(const QString &path) const
{
    return QFileInfo(path).isDir();
}

void FileManager::watchFile(const QString &filePath)
{
    m_watcher->addPath(filePath);
}

void FileManager::watchDirectory(const QString &dirPath)
{
    m_watcher->addPath(dirPath);
}

void FileManager::stopWatching(const QString &path)
{
    m_watcher->removePath(path);
}

void FileManager::onFileChanged(const QString &path)
{
    emit fileChanged(path);
}

void FileManager::onDirectoryChanged(const QString &path)
{
    emit directoryChanged(path);
}