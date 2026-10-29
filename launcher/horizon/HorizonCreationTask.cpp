// SPDX-License-Identifier: GPL-3.0-only
#include "HorizonCreationTask.h"

#include "FileSystem.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "settings/INISettingsObject.h"

std::unique_ptr<MinecraftInstance> HorizonCreationTask::createInstance()
{
    setStatus(tr("Creating Horizon instance from version %1").arg(m_version ? m_version->name() : QString()));

    auto inst = std::make_unique<MinecraftInstance>(m_globalSettings, std::make_unique<INISettingsObject>(FS::PathCombine(m_stagingPath, "instance.cfg")),
                           m_stagingPath);
    SettingsObject::Lock lock(inst->settings());

    auto components = inst->getPackProfile();
    components->buildingFromScratch();
    components->saveNow();

    // Keep Prism's instance type stable across restarts.
    inst->settings()->registerSetting("InstanceType", "");
    inst->settings()->set("InstanceType", "Horizon");
    if (m_version) {
        inst->settings()->registerSetting("HorizonVersion", "");
        inst->settings()->set("HorizonVersion", m_version->descriptor());
        inst->settings()->registerSetting("HorizonDownloadUrl", "");
        inst->settings()->set("HorizonDownloadUrl", std::dynamic_pointer_cast<HorizonVersion>(m_version) ? std::dynamic_pointer_cast<HorizonVersion>(m_version)->downloadUrl() : QString());
    }

    inst->setName(name());
    inst->setIconKey(m_instIcon);

    return inst;
}
