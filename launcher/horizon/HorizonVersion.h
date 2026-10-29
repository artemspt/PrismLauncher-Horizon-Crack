// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>
#include "BaseVersion.h"

class HorizonVersion : public BaseVersion {
   public:
    HorizonVersion() = default;
    HorizonVersion(const QString& tag,
                   const QString& publishedAt,
                   const QString& downloadUrl,
                   bool prerelease);

    QString descriptor() const override { return m_tag; }
    QString name() const override { return m_tag; }
    QString typeString() const override { return m_prerelease ? "snapshot" : "release"; }

    QString tag() const { return m_tag; }
    QString publishedAt() const { return m_publishedAt; }
    QString downloadUrl() const { return m_downloadUrl; }
    bool isPrerelease() const { return m_prerelease; }

    bool operator<(BaseVersion& other) const override;

   private:
    QString m_tag;
    QString m_publishedAt;
    QString m_downloadUrl;
    bool m_prerelease = false;
};
