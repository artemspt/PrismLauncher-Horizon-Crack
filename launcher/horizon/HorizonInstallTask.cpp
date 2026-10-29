// SPDX-License-Identifier: GPL-3.0-only
#include "HorizonInstallTask.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <functional>
#include <QUrl>

#include "FileSystem.h"
#include "settings/SettingsObject.h"

HorizonInstallTask::HorizonInstallTask(MinecraftInstance* instance)
    : Task(), m_instance(instance)
{
    m_targetDir = FS::PathCombine(instance->instanceRoot(), "horizon");
}

void HorizonInstallTask::executeTask()
{
    setStatus(tr("Downloading Horizon..."));

    const QString url = m_instance->settings()->get("HorizonDownloadUrl").toString();
    if (url.isEmpty()) {
        emitFailed(tr("Horizon download URL is empty."));
        return;
    }

    m_archivePath = FS::PathCombine(m_instance->instanceRoot(), "horizon-archive.bin");
    QDir().mkpath(m_instance->instanceRoot());

    m_nam = new QNetworkAccessManager(this);
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("User-Agent", "PrismLauncher-Horizon");
    m_reply = m_nam->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &HorizonInstallTask::downloadFinished);
}

void HorizonInstallTask::downloadFinished()
{
    if (!m_reply) {
        emitFailed(tr("Horizon download failed."));
        return;
    }

    if (m_reply->error() != QNetworkReply::NoError) {
        QString reason = m_reply->errorString();
        m_reply->deleteLater();
        m_reply = nullptr;
        emitFailed(reason);
        return;
    }

    QByteArray data = m_reply->readAll();
    m_reply->deleteLater();
    m_reply = nullptr;

    QFile file(m_archivePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emitFailed(tr("Could not save Horizon archive."));
        return;
    }
    file.write(data);
    file.close();

    setStatus(tr("Extracting Horizon..."));
    QDir().mkpath(m_targetDir);

#if defined(Q_OS_WIN)
    QProcess proc;
    proc.setProgram("powershell");
    proc.setArguments({ "-NoProfile", "-Command", QString("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force").arg(m_archivePath, m_targetDir) });
    proc.start();
    if (!proc.waitForFinished(300000) || proc.exitCode() != 0) {
        emitFailed(tr("Failed to extract Horizon archive."));
        return;
    }
#elif defined(Q_OS_MACOS)
    QProcess proc;
    proc.setProgram("unzip");
    proc.setArguments({ "-o", m_archivePath, "-d", m_targetDir });
    proc.start();
    if (!proc.waitForFinished(300000) || proc.exitCode() != 0) {
        emitFailed(tr("Failed to extract Horizon archive."));
        return;
    }
#else
    QProcess proc;
    proc.setProgram("tar");
    proc.setArguments({ "-xzf", m_archivePath, "-C", m_targetDir });
    proc.start();
    if (!proc.waitForFinished(300000) || proc.exitCode() != 0) {
        emitFailed(tr("Failed to extract Horizon archive: %1").arg(proc.readAllStandardError()));
        return;
    }
#endif

    std::function<QString(const QDir&)> findBinary = [&](const QDir& dir) -> QString {
        for (const auto& entry : dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString candidate = dir.absoluteFilePath(entry);
            QFileInfo info(candidate);
            if (info.isFile() && info.fileName().startsWith("horizon-client")) {
                return candidate;
            }
            if (info.isDir()) {
                QString found = findBinary(QDir(candidate));
                if (!found.isEmpty()) {
                    return found;
                }
            }
        }
        return QString();
    };

    m_executablePath = findBinary(QDir(m_targetDir));
    if (m_executablePath.isEmpty()) {
#if defined(Q_OS_WIN)
        m_executablePath = FS::PathCombine(m_targetDir, "horizon-client.exe");
#else
        m_executablePath = FS::PathCombine(m_targetDir, "horizon-client");
#endif
    }

    m_instance->settings()->registerSetting("HorizonExecutablePath", "");
    m_instance->settings()->set("HorizonExecutablePath", m_executablePath);

    m_instance->settings()->registerSetting("HorizonResourcesPath", "");

    const QString prepareBin = FS::PathCombine(m_targetDir, "horizon-prepare");
    const QString resourcesPath = FS::PathCombine(m_targetDir, ".local/horizon-client/26.3");

    auto findMinecraftDir = []() -> QString {
        const QByteArray env = qgetenv("HORIZON_MINECRAFT_DIR");
        if (!env.isEmpty()) {
            QDir dir(QString::fromUtf8(env));
            if (QFile::exists(dir.absoluteFilePath("versions/26.3/26.3.json")))
                return dir.absolutePath();
        }
        QDir common(QDir::homePath() + "/.minecraft");
        if (QFile::exists(common.absoluteFilePath("versions/26.3/26.3.json")))
            return common.absolutePath();
        QDir instances(QDir::homePath() + "/.local/share/PrismLauncher/instances");
        if (instances.exists()) {
            for (const auto& entry : instances.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                QStringList candidates;
                candidates << instances.absoluteFilePath(entry) + "/minecraft";
                candidates << instances.absoluteFilePath(entry) + "/.minecraft";
                for (const auto& candidate : candidates) {
                    if (QFile::exists(candidate + "/versions/26.3/26.3.json"))
                        return candidate;
                }
            }
        }

        const QString linked = QDir::homePath() + "/.local/share/horizon-launcher/linked-minecraft";
        if (QFile::exists(linked + "/versions/26.3/26.3.json"))
            return linked;

        return QString();
    };

    auto downloadFile = [](const QUrl& url, const QString& path) -> bool {
        QNetworkAccessManager nam;
        QNetworkReply* reply = nam.get(QNetworkRequest(url));
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            return false;
        }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            reply->deleteLater();
            return false;
        }
        file.write(reply->readAll());
        file.close();
        reply->deleteLater();
        return true;
    };

    auto downloadServerJar = [&](const QString& minecraftDir) -> QString {
        QFile versionFile(minecraftDir + "/versions/26.3/26.3.json");
        if (!versionFile.open(QIODevice::ReadOnly))
            return QString();
        auto doc = QJsonDocument::fromJson(versionFile.readAll());
        versionFile.close();
        if (!doc.isObject())
            return QString();
        const QString url = doc.object().value("downloads").toObject().value("server").toObject().value("url").toString();
        if (url.isEmpty())
            return QString();
        const QString jarPath = minecraftDir + "/versions/26.3/26.3-server.jar";
        if (QFile::exists(jarPath))
            return jarPath;
        setStatus(tr("Downloading Minecraft server jar..."));
        if (downloadFile(QUrl(url), jarPath))
            return jarPath;
        return QString();
    };

    const QString minecraftDir = findMinecraftDir();
    if (QFile::exists(prepareBin) && !minecraftDir.isEmpty()) {
        const QString serverJar = downloadServerJar(minecraftDir);
        if (!serverJar.isEmpty()) {
            setStatus(tr("Preparing Horizon resources..."));
            QProcess prepareProc;
            prepareProc.setWorkingDirectory(m_targetDir);
            prepareProc.setProgram(prepareBin);
            prepareProc.setArguments({ "--prepare", "--minecraft-dir", minecraftDir, "--server-jar", serverJar });
            prepareProc.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
            prepareProc.start();
            if (prepareProc.waitForFinished(10 * 60 * 1000) && prepareProc.exitCode() == 0 && QDir(resourcesPath).exists()) {
                m_instance->settings()->set("HorizonResourcesPath", resourcesPath);
            } else {
                qWarning() << "Horizon prepare failed:" << prepareProc.readAllStandardError();
            }
        }
    }

    if (m_instance->settings()->get("HorizonResourcesPath").toString().isEmpty()) {
        if (QDir(resourcesPath).exists()) {
            m_instance->settings()->set("HorizonResourcesPath", resourcesPath);
        } else {
            QDirIterator it(QDir::homePath(), QStringList() << "26.3", QDir::Dirs, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                if (path.endsWith("/.local/horizon-client/26.3") || path.endsWith("/.local/horizon/.local/horizon-client/26.3")) {
                    m_instance->settings()->set("HorizonResourcesPath", path);
                    break;
                }
            }
        }
    }

    setStatus(tr("Horizon installed."));
    extractAndFinish();
}

void HorizonInstallTask::extractAndFinish()
{
    m_instance->saveNow();
    emitSucceeded();
}
