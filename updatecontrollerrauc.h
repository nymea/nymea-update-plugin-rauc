// SPDX-License-Identifier: GPL-3.0-or-later

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright (C) 2013 - 2024, nymea GmbH
* Copyright (C) 2024 - 2026, chargebyte austria GmbH
*
* This file is part of nymea-update-plugin-rauc.
*
* nymea-update-plugin-rauc is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* nymea-update-plugin-rauc is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with nymea-update-plugin-rauc. If not, see <https://www.gnu.org/licenses/>.
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef UPDATECONTROLLERRAUC_H
#define UPDATECONTROLLERRAUC_H

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVariantMap>

#include <platform/platformupdatecontroller.h>

#include "selfhostedrepository.h"

class RaucDBusInterface;

class UpdateControllerRauc : public PlatformUpdateController
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "io.nymea.PlatformUpdateController")
    Q_INTERFACES(PlatformUpdateController)

public:
    explicit UpdateControllerRauc(QObject *parent = nullptr);

    PlatformUpdateController::UpdateType updateType() const override;
    bool updateManagementAvailable() const override;

    bool checkForUpdates() override;
    bool busy() const override;
    bool updateRunning() const override;
    int updateProgress() const override;

    QList<Package> packages() const override;
    QList<Repository> repositories() const override;

    bool startUpdate(const QStringList &packageIds = QStringList()) override;
    bool rollback(const QStringList &packageIds) override;
    bool removePackages(const QStringList &packageIds) override;
    bool enableRepository(const QString &repositoryId, bool enabled) override;

private slots:
    void init();

private:
    QNetworkAccessManager *m_networkManager = nullptr;
    QTimer m_checkUpdateTimer;

    bool m_raucAvailable = false;
    RaucDBusInterface *m_raucInterface = nullptr;

    bool m_updateRunning = false;
    bool m_busy = false;
    int m_updateProgress = -1;

    QString m_primarySlot;
    QVariantMap m_slotStatus;

    QString m_cacheFileName;
    QVariantMap m_releaseInfo;

    QHash<QString, SelfHostedRepository> m_repositories;
    QString m_currentRepositoryId;

    Package m_package;

    QString m_currentHash;
    QString m_candidateHash;

    bool verifyUpdateAvailable();

    void setRaucAvailable(bool available);
    void setUpdateRunningState(bool running);
    void setBusyState(bool busy);
    void setUpdateProgress(int updateProgress);

    QJsonDocument loadJsonFile(const QString &fileName);
};

#endif // UPDATECONTROLLERRAUC_H
