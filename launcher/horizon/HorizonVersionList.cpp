// SPDX-License-Identifier: GPL-3.0-only
#include "HorizonVersionList.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

HorizonVersionList::HorizonVersionList(QObject* parent) : BaseVersionList(parent) {}

Task::Ptr HorizonVersionList::getLoadTask(bool forceReload)
{
    Q_UNUSED(forceReload)
    load();
    return getCurrentTask();
}

Task::Ptr HorizonVersionList::getCurrentTask()
{
    if (m_status == Status::InProgress) {
        return m_load_task;
    }
    return nullptr;
}

void HorizonVersionList::load()
{
    if (m_status != Status::InProgress) {
        m_status = Status::InProgress;
        m_load_task.reset(new HorizonVersionListLoadTask(this));
        m_load_task->start();
    }
}

const BaseVersion::Ptr HorizonVersionList::at(int i) const
{
    return m_versions.at(i);
}

bool HorizonVersionList::isLoaded()
{
    return m_status == Status::Done;
}

int HorizonVersionList::count() const
{
    return m_versions.count();
}

QVariant HorizonVersionList::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();
    if (index.row() >= count())
        return QVariant();

    auto version = std::dynamic_pointer_cast<HorizonVersion>(m_versions[index.row()]);
    switch (role) {
        case VersionPointerRole:
            return QVariant::fromValue(m_versions[index.row()]);
        case VersionRole:
            return version ? version->tag() : QString();
        case VersionIdRole:
            return version ? version->tag() : QString();
        case TypeRole:
            return version ? version->typeString() : QString();
        default:
            return BaseVersionList::data(index, role);
    }
}

void HorizonVersionList::sortVersions()
{
    beginResetModel();
    std::sort(m_versions.begin(), m_versions.end(), [](const BaseVersion::Ptr& a, const BaseVersion::Ptr& b) {
        auto ha = std::dynamic_pointer_cast<HorizonVersion>(a);
        auto hb = std::dynamic_pointer_cast<HorizonVersion>(b);
        if (ha && hb) {
            return hb->operator<(*ha);
        }
        return a->descriptor() > b->descriptor();
    });
    endResetModel();
}

void HorizonVersionList::updateListData(QList<BaseVersion::Ptr> versions)
{
    beginResetModel();
    QSet<QString> seen;
    QList<BaseVersion::Ptr> unique;
    for (const auto& version : versions) {
        if (seen.contains(version->descriptor()))
            continue;
        seen.insert(version->descriptor());
        unique.append(version);
    }
    m_versions = unique;
    sortVersions();
    endResetModel();
    m_status = Status::Done;
    m_load_task.reset();
}

HorizonVersionListLoadTask::HorizonVersionListLoadTask(HorizonVersionList* list) : Task(), m_list(list) {}

void HorizonVersionListLoadTask::executeTask()
{
    setStatus(tr("Loading Horizon versions..."));

#if defined(Q_OS_WIN)
    const QString assetName = "horizon-windows-x64.zip";
#elif defined(Q_OS_MACOS)
    const QString assetName = "horizon-macos-arm64.zip";
#else
    const QString assetName = "horizon-linux-x86_64.tar.gz";
#endif

    QUrl url("https://api.github.com/repos/horizon-rust/horizon/releases?per_page=100");
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "PrismLauncher-Horizon");
    request.setRawHeader("Accept", "application/vnd.github+json");

    m_nam = new QNetworkAccessManager(this);
    m_reply = m_nam->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &HorizonVersionListLoadTask::replyFinished);
}

void HorizonVersionListLoadTask::replyFinished()
{
    if (!m_reply)
        return;

    if (m_reply->error() != QNetworkReply::NoError) {
        QString reason = m_reply->errorString();
        m_reply->deleteLater();
        m_reply = nullptr;
        m_list->updateListData({});
        emitFailed(reason);
        return;
    }

    const QByteArray data = m_reply->readAll();
    m_reply->deleteLater();
    m_reply = nullptr;

#if defined(Q_OS_WIN)
    const QString assetName = "horizon-windows-x64.zip";
#elif defined(Q_OS_MACOS)
    const QString assetName = "horizon-macos-arm64.zip";
#else
    const QString assetName = "horizon-linux-x86_64.tar.gz";
#endif

    QList<BaseVersion::Ptr> versions;
    QJsonParseError parseError;
    auto doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        m_list->updateListData({});
        emitFailed(tr("Invalid response from GitHub"));
        return;
    }

    const QJsonArray releases = doc.array();
    for (const QJsonValue& value : releases) {
        if (!value.isObject())
            continue;
        const QJsonObject release = value.toObject();
        const QString tagName = release.value("tag_name").toString();
        if (tagName.isEmpty() || tagName.startsWith("launcher@"))
            continue;

        QString downloadUrl;
        const QJsonArray assets = release.value("assets").toArray();
        for (const QJsonValue& assetValue : assets) {
            const QJsonObject asset = assetValue.toObject();
            if (asset.value("name").toString() == assetName) {
                downloadUrl = asset.value("browser_download_url").toString();
                break;
            }
        }
        if (downloadUrl.isEmpty())
            continue;

        const QString publishedAt = release.value("published_at").toString();
        const bool prerelease = release.value("prerelease").toBool();
        auto version = std::make_shared<HorizonVersion>(tagName, publishedAt, downloadUrl, prerelease);
        versions.append(std::dynamic_pointer_cast<BaseVersion>(version));
    }

    m_list->updateListData(versions);
    emitSucceeded();
}
