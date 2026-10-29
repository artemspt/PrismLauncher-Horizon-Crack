// SPDX-License-Identifier: GPL-3.0-only
#include "HorizonLaunchStep.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QDateTime>
#include <QRandomGenerator>
#include <QString>
#include <QTextStream>

#include "launch/LaunchTask.h"
#include "Application.h"
#include "FileSystem.h"
#include "MessageLevel.h"
#include "minecraft/MinecraftInstance.h"
#include "settings/SettingsObject.h"

HorizonLaunchStep::HorizonLaunchStep(LaunchTask* parent, AuthSessionPtr session)
    : LaunchStep(parent), m_session(std::move(session))
{}

void HorizonLaunchStep::executeTask()
{
    setStatus(tr("Launching Horizon..."));

    MinecraftInstance* inst = m_parent->instance();
    inst->setRunning(true);

    QString program = inst->settings()->get("HorizonExecutablePath").toString();
    if (program.isEmpty() || !QFile::exists(program)) {
        QStringList candidates;
        candidates << inst->settings()->get("HorizonExecutablePath").toString();
        candidates << inst->instanceRoot() + "/horizon/horizon-linux-x86_64/horizon-client";
        candidates << inst->instanceRoot() + "/horizon/horizon-client";
        candidates << QDir::homePath() + "/.local/share/PrismLauncher/instances/" + inst->name() + "/horizon/horizon-linux-x86_64/horizon-client";

        // Scan the instance and cache directories for the binary.
        QDirIterator it(inst->instanceRoot(), QStringList() << "horizon-client", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            candidates << it.next();
        }

        for (const auto& candidate : candidates) {
            if (!candidate.isEmpty() && QFile::exists(candidate)) {
                program = candidate;
                // Persist the corrected path for next launch.
                inst->settings()->set("HorizonExecutablePath", candidate);
                break;
            }
        }
    }
    if (program.isEmpty()) {
        program = "horizon-client";
    }

    QStringList args;
    QString resources = inst->settings()->get("HorizonResourcesPath").toString();
    if (resources.isEmpty() || !QDir(resources).exists()) {
        // Try well-known prepared resource locations.
        QStringList candidates;
        candidates << inst->settings()->get("HorizonResourcesPath").toString();
        candidates << inst->instanceRoot() + "/horizon/.local/horizon-client/26.3";
        // Generic scan below covers all versions.
        candidates << QDir::homePath() + "/.local/share/horizon/.local/horizon-client/26.3";

        // Scan for any prepared Horizon resource tree for Minecraft 26.3.
        QDir base(QDir::homePath() + "/.local/share");
        if (base.exists("horizon-launcher")) {
            QDirIterator it(QDir::homePath() + "/.local/share/horizon-launcher/versions", QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                if (it.fileName() == "26.3") {
                    candidates << it.filePath();
                }
            }
        }

        for (const auto& candidate : candidates) {
            if (!candidate.isEmpty() && QDir(candidate).exists()) {
                resources = candidate;
                break;
            }
        }
    }
    if (!resources.isEmpty() && QDir(resources).exists()) {
        args << "--resources" << resources;
    }

    bool wantsSession = m_session && !m_session->access_token.isEmpty() && m_session->access_token != "0";

    if (wantsSession) {
        QString sessionsDir = FS::PathCombine(inst->instanceRoot(), "sessions");
        QDir().mkpath(sessionsDir);

        int suffix = QRandomGenerator::global()->generate();
        QString sessionPath = FS::PathCombine(sessionsDir, QString("session-%1.json").arg(suffix, 8, 16, QChar('0')));

        QJsonObject body;
        body["name"] = m_session->player_name;
        body["uuid"] = m_session->uuid;
        body["access_token"] = m_session->access_token;
        body["user_type"] = "msa";
        body["xuid"] = QString();

        QFile file(sessionPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(QJsonDocument(body).toJson(QJsonDocument::Compact));
            file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            file.close();
            args << "--session-file" << sessionPath;
        } else {
            emitFailed(tr("Could not write Horizon session file."));
            inst->setRunning(false);
            return;
        }
    } else {
        QString name = m_session ? m_session->player_name : inst->settings()->get("HorizonPlayerName").toString();
        if (name.isEmpty()) {
            name = "Player";
        }
        args << "--name" << name;
    }

    emit logLine(tr("Starting %1 %2").arg(program, args.join(' ')), MessageLevel::Launcher);

    bool started = QProcess::startDetached(program, args, inst->gameRoot());
    if (!started) {
        inst->setRunning(false);
        emitFailed(tr("Could not start Horizon executable: %1").arg(program));
        return;
    }

    emitSucceeded();
}
