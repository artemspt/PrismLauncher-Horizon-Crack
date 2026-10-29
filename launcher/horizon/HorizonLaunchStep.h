// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <launch/LaunchStep.h>
#include <minecraft/auth/AuthSession.h>
#include <minecraft/MinecraftInstance.h>

class HorizonLaunchStep : public LaunchStep {
    Q_OBJECT
   public:
    explicit HorizonLaunchStep(LaunchTask* parent, AuthSessionPtr session);
    void executeTask() override;

   private:
    AuthSessionPtr m_session;
};
