// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QList>
#include "BaseVersionList.h"
#include "HorizonVersion.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>

class HorizonVersionListLoadTask;

class HorizonVersionList : public BaseVersionList {
    Q_OBJECT
    enum class Status { NotDone, InProgress, Done };

   public:
    explicit HorizonVersionList(QObject* parent = nullptr);

    Task::Ptr getLoadTask(bool forceReload = false) override;
    bool isLoaded() override;
    const BaseVersion::Ptr at(int i) const override;
    int count() const override;
    void sortVersions() override;

    QVariant data(const QModelIndex& index, int role) const override;

   public slots:
    void updateListData(QList<BaseVersion::Ptr> versions) override;

   protected:
    Task::Ptr getCurrentTask();
    void load();

   protected:
    Status m_status = Status::NotDone;
    shared_qobject_ptr<HorizonVersionListLoadTask> m_load_task;
    QList<BaseVersion::Ptr> m_versions;
};

class HorizonVersionListLoadTask : public Task {
    Q_OBJECT
   public:
    explicit HorizonVersionListLoadTask(HorizonVersionList* list);
    ~HorizonVersionListLoadTask() override = default;

   protected:
    void executeTask() override;

   private slots:
    void replyFinished();

   protected:
    HorizonVersionList* m_list = nullptr;
    QNetworkAccessManager* m_nam = nullptr;
    QNetworkReply* m_reply = nullptr;
};
