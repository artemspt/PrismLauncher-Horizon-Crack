// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "ui/pages/BasePage.h"
#include "HorizonVersionList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "ui/widgets/VersionSelectWidget.h"

class NewInstanceDialog;
class HorizonPage : public QWidget, public BasePage {
    Q_OBJECT
   public:
    explicit HorizonPage(NewInstanceDialog* dialog, QWidget* parent = nullptr);
    ~HorizonPage();

    QString id() const override { return "horizon"; }
    QString displayName() const override { return tr("Horizon"); }
    QIcon icon() const override { return QIcon(); }
    bool shouldDisplay() const override { return true; }

    void openedImpl() override {
        if (!m_initialized) {
            m_vlist = new HorizonVersionList(this);
            m_versionSelect->initialize(m_vlist);
            m_initialized = true;
        } else {
            suggestCurrent();
        }
    }

   private slots:
    void setSelectedVersion(BaseVersion::Ptr version);

   private:
    void suggestCurrent();

   private:
    NewInstanceDialog* m_dialog = nullptr;
    HorizonVersionList* m_vlist = nullptr;
    VersionSelectWidget* m_versionSelect = nullptr;
    BaseVersion::Ptr m_selectedVersion;
    bool m_initialized = false;
};
