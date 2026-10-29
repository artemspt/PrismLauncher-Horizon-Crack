// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "tasks/Task.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "minecraft/MinecraftInstance.h"

class HorizonInstallTask : public Task {
    Q_OBJECT
   public:
    explicit HorizonInstallTask(MinecraftInstance* instance);
    ~HorizonInstallTask() override = default;

   protected:
    void executeTask() override;

   private slots:
    void downloadFinished();

   private:
    void extractAndFinish();

   private:
    MinecraftInstance* m_instance = nullptr;
    QNetworkAccessManager* m_nam = nullptr;
    QNetworkReply* m_reply = nullptr;
    QString m_archivePath;
    QString m_targetDir;
    QString m_executablePath;
};
