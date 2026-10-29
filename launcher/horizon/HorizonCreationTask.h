// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "InstanceCreationTask.h"
#include "HorizonVersion.h"

class HorizonCreationTask : public InstanceCreationTask {
    Q_OBJECT
   public:
    HorizonCreationTask(BaseVersion::Ptr version) : InstanceCreationTask(), m_version(std::move(version)) {}

    std::unique_ptr<MinecraftInstance> createInstance() override;

   private:
    BaseVersion::Ptr m_version;
};
