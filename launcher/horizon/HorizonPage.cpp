// SPDX-License-Identifier: GPL-3.0-only
#include "HorizonPage.h"

#include "Application.h"
#include "HorizonCreationTask.h"
#include "ui/dialogs/NewInstanceDialog.h"

#include <QVBoxLayout>

HorizonPage::HorizonPage(NewInstanceDialog* dialog, QWidget* parent)
    : QWidget(parent), m_dialog(dialog)
{
    auto layout = new QVBoxLayout(this);
    m_versionSelect = new VersionSelectWidget(this);
    layout->addWidget(m_versionSelect);

    connect(m_versionSelect, &VersionSelectWidget::selectedVersionChanged, this, &HorizonPage::setSelectedVersion);
}

HorizonPage::~HorizonPage()
{
}

void HorizonPage::setSelectedVersion(BaseVersion::Ptr version)
{
    m_selectedVersion = version;
    suggestCurrent();
}

void HorizonPage::suggestCurrent()
{
    if (!m_dialog)
        return;
    if (!m_selectedVersion) {
        m_dialog->setSuggestedPack();
        return;
    }
    m_dialog->setSuggestedPack(m_selectedVersion->descriptor(), new HorizonCreationTask(m_selectedVersion));
    m_dialog->setSuggestedIcon("default");
}
