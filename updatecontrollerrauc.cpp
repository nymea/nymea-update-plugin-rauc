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

#include "updatecontrollerrauc.h"
#include "raucdbusinterface.h"
#include "selfhostedrepository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>

#include <loggingcategories.h>
#include <nymeasettings.h>

UpdateControllerRauc::UpdateControllerRauc(QObject *parent)
    : PlatformUpdateController{parent}
    , m_networkManager{new QNetworkAccessManager(this)}
{
    QTimer::singleShot(0, this, &UpdateControllerRauc::init);
}

PlatformUpdateController::UpdateType UpdateControllerRauc::updateType() const
{
    return PlatformUpdateController::UpdateTypeSystem;
}

bool UpdateControllerRauc::updateManagementAvailable() const
{
    return m_raucAvailable && !m_repositories.isEmpty();
}

bool UpdateControllerRauc::checkForUpdates()
{
    qCDebug(dcPlatformUpdate()) << "Checking for updates...";

    if (!updateManagementAvailable()) {
        qCDebug(dcPlatformUpdate()) << "Cannot check for updates. The update manager is not available";
        return false;
    }

    // Only check for updates if there is not an update running currently
    if (updateRunning()) {
        qCWarning(dcPlatformUpdate()) << "Cannot check for updates. There is currently an update running.";
        return false;
    }

    // Only check for updates if the updater is not busy. If busy, there is already a check or an update in progress
    if (busy()) {
        qCWarning(dcPlatformUpdate()) << "Cannot check for updates. There update manager is currently busy.";
        return false;
    }

    setBusyState(true);
    setUpdateProgress(-1);

    QString error;
    bool ok = true;

    m_slotStatus = m_raucInterface->getSlotStatus(&error);
    if (!error.isEmpty()) {
        qCWarning(dcPlatformUpdate()) << "Failed to read RAUC slot status:" << error;
        ok = false;
    }

    m_primarySlot = m_raucInterface->getPrimary(&error);
    if (!error.isEmpty()) {
        qCWarning(dcPlatformUpdate()) << "Failed to read RAUC primary slot:" << error;
        ok = false;
    }

    // Note: once we support more then one repository type, we need to distinguish here.
    SelfHostedRepository repository = m_repositories.value(m_currentRepositoryId);
    QUrl releaseUrl = repository.url();
    releaseUrl.setPath(releaseUrl.path() + "/release.json");

    qCDebug(dcPlatformUpdate()) << "Fetching information from" << releaseUrl.toString();

    QNetworkRequest request(releaseUrl);
    if (repository.authentication() == SelfHostedRepository::AuthenticationBasic) {
        qCDebug(dcPlatformUpdate()) << "Using basic authentication:" << repository.userName()
                                    << QString(repository.password()).left(3) + QString(repository.password().size() - 3, '*');
        QString concatenated = QString("%1:%2").arg(repository.userName(), repository.password());
        QString headerData = "Basic " + concatenated.toLocal8Bit().toBase64();
        request.setRawHeader("Authorization", headerData.toLocal8Bit());
    }

    const QVariantMap currentSlot = m_slotStatus.value(m_primarySlot).toMap();
    m_currentHash = currentSlot.value("sha256").toString();
    const QString currentVersion = currentSlot.value("bundle.version").toString();

    // Set package information
    qCDebug(dcPlatformUpdate()) << "RAUC slot status:" << qUtf8Printable(QJsonDocument::fromVariant(m_slotStatus).toJson(QJsonDocument::Indented));
    qCDebug(dcPlatformUpdate()) << "RAUC available:" << m_raucAvailable;
    qCDebug(dcPlatformUpdate()) << "RAUC primary slot:" << m_primarySlot;
    qCDebug(dcPlatformUpdate()) << "RAUC current bundle version:" << currentVersion;
    qCDebug(dcPlatformUpdate()) << "RAUC current rootfs hash:" << m_currentHash;

    m_package.setInstalledVersion(currentVersion);

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
    connect(reply, &QNetworkReply::finished, this, [this, reply, repository]() {
        if (reply->error()) {
            qCWarning(dcPlatformUpdate()) << "Fetch repository reply finished with error: " << reply->errorString();
            setBusyState(false);
            return;
        }

        QByteArray data = reply->readAll();
        QJsonParseError error;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError) {
            qCWarning(dcPlatformUpdate()) << "Repository reply data contains invalid json data" << data << ":" << error.errorString();
            setBusyState(false);
            return;
        }

        // Note: the cache is optional, an error should not prevent updating
        QFileInfo cacheFileInfo(m_cacheFileName);
        QDir cacheDir = cacheFileInfo.dir();
        if (!cacheDir.exists() && !cacheDir.mkpath(QStringLiteral("."))) {
            qCWarning(dcPlatformUpdate()) << "Failed to create cache directory for" << m_cacheFileName;
        } else {
            QFile cacheFile(m_cacheFileName);
            if (!cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                qCWarning(dcPlatformUpdate()) << "Failed to write repository cache file" << m_cacheFileName << cacheFile.errorString();
            } else {
                cacheFile.write(data);
                cacheFile.close();
            }
        }

        qCDebug(dcPlatformUpdate()) << "Repository refreshed successfully:" << qUtf8Printable(jsonDoc.toJson(QJsonDocument::Indented));
        m_releaseInfo = jsonDoc.toVariant().toMap();
        m_candidateHash = m_releaseInfo.value("rootfs_sha256").toString();

        Package package = m_package;
        package.setCandidateVersion(m_releaseInfo.value("version").toString());
        package.setChangelog(m_releaseInfo.value("changelog").toString());
        package.setUpdateAvailable(verifyUpdateAvailable());

        if (m_package != package) {
            m_package = package;
            emit packageChanged(m_package);
        }

        setBusyState(false);
        setUpdateProgress(-1);

        if (m_package.updateAvailable()) {
            qCDebug(dcPlatformUpdate()) << "System update available from repository" << repository.id();
            qCDebug(dcPlatformUpdate()) << "Installed:" << m_currentHash << m_package.installedVersion();
            qCDebug(dcPlatformUpdate()) << "Candidate:" << m_candidateHash << m_package.candidateVersion();
        } else {
            qCDebug(dcPlatformUpdate()) << "The system is up to date with repository" << repository.id();
            qCDebug(dcPlatformUpdate()) << "Installed:" << m_currentHash << m_package.installedVersion();
        }
    });

    return ok;
}

bool UpdateControllerRauc::busy() const
{
    return m_busy;
}

bool UpdateControllerRauc::updateRunning() const
{
    return m_updateRunning;
}

int UpdateControllerRauc::updateProgress() const
{
    return m_updateProgress;
}

QList<Package> UpdateControllerRauc::packages() const
{
    return {m_package};
}

QList<Repository> UpdateControllerRauc::repositories() const
{
    QList<Repository> repositories;
    repositories.reserve(m_repositories.size());
    for (const SelfHostedRepository &repository : m_repositories) {
        repositories.append(repository);
    }
    return repositories;
}

bool UpdateControllerRauc::startUpdate(const QStringList &packageIds)
{
    qCDebug(dcPlatformUpdate()) << "Starting to update" << packageIds;

    if (!updateManagementAvailable()) {
        qCWarning(dcPlatformUpdate()) << "Cannot start update. The update manager is not available.";
        return false;
    }

    if (updateRunning()) {
        qCWarning(dcPlatformUpdate()) << "Cannot start update. There is currently an update running.";
        return false;
    }

    if (busy()) {
        qCWarning(dcPlatformUpdate()) << "Cannot start update. The update manager is currently busy.";
        return false;
    }

    if (m_releaseInfo.isEmpty()) {
        qCWarning(dcPlatformUpdate()) << "Cannot start update. No release info available.";
        return false;
    }

    // Write the reboot confirmation file so the RAUC hook understands that a reboot is desired
    QString rebootRequestFileName("/run/firmware-update-reboot-requested");
    QFileInfo rebootRequestFileInfo(rebootRequestFileName);
    if (!QDir().mkpath(rebootRequestFileInfo.absolutePath())) {
        qCWarning(dcPlatformUpdate()) << "Cannot start update. Unable to create folder" << rebootRequestFileInfo.absolutePath();
        return false;
    }

    QFile rebootRequestFile(rebootRequestFileName);
    if (!rebootRequestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(dcPlatformUpdate()) << "Cannot start update. Unable to create reboot request file" << rebootRequestFileName;
        return false;
    }

    rebootRequestFile.close();

    // Everything checked and set up. We can tell RAUC to start the installation
    QString bundleSource = m_repositories.value(m_currentRepositoryId).url().toString() + "/" + m_releaseInfo.value("image").toString();
    if (m_repositories.value(m_currentRepositoryId).authentication() == SelfHostedRepository::AuthenticationBasic) {
        QUrl bundleUrl(bundleSource);
        bundleUrl.setUserName(m_repositories.value(m_currentRepositoryId).userName());
        bundleUrl.setPassword(m_repositories.value(m_currentRepositoryId).password());
        bundleSource = bundleUrl.toString(QUrl::FullyEncoded);
    }

    QString errorMessage;
    if (!m_raucInterface->installBundle(bundleSource, &errorMessage)) {
        qCWarning(dcPlatformUpdate()) << "Failed to start RAUC install:" << errorMessage;
        return false;
    }

    setUpdateRunningState(true);
    return true;
}

bool UpdateControllerRauc::rollback(const QStringList &packageIds)
{
    Q_UNUSED(packageIds)
    qCWarning(dcPlatformUpdate()) << "Manual rollback is not supported.";
    return false;
}

bool UpdateControllerRauc::removePackages(const QStringList &packageIds)
{
    Q_UNUSED(packageIds)
    qCWarning(dcPlatformUpdate()) << "Removing is not supported for this update plugin.";
    return false;
}

bool UpdateControllerRauc::enableRepository(const QString &repositoryId, bool enabled)
{
    if (!enabled) {
        qCWarning(dcPlatformUpdate()) << "Disable the repository is not supported for this plugin.";
        return false;
    }

    const QString previousRepository = m_currentRepositoryId;

    if (!m_repositories.contains(repositoryId)) {
        qCWarning(dcPlatformUpdate()) << "Could not enable repository" << repositoryId << " because there is no such repository.";
        return false;
    }

    NymeaSettings settings(NymeaSettings::SettingsRoleGlobal);
    settings.beginGroup("rauc-update");
    settings.setValue("repository", repositoryId);
    settings.endGroup();

    m_currentRepositoryId = repositoryId;

    if (!previousRepository.isEmpty() && previousRepository != repositoryId) {
        if (QFile::exists(m_cacheFileName) && !QFile::remove(m_cacheFileName)) {
            qCWarning(dcPlatformUpdate()) << "Failed to remove repository cache file" << m_cacheFileName;
        }
        m_releaseInfo.clear();
    }

    // Make sure only one repository is enabled
    bool repositoryHasChanged = false;
    for (const QString &id : m_repositories.keys()) {
        bool changed = false;
        if (id == repositoryId && !m_repositories.value(id).enabled()) {
            m_repositories[id].setEnabled(true);
            qCDebug(dcPlatformUpdate()) << "Enable repository" << id;
            changed = true;
        } else {
            if (m_repositories.value(id).enabled()) {
                m_repositories[id].setEnabled(false);
                qCDebug(dcPlatformUpdate()) << "Disable repository" << id;
                changed = true;
            }
        }

        if (changed)
            emit repositoryChanged(static_cast<Repository>(m_repositories.value(id)));

        repositoryHasChanged |= changed;
    }

    if (repositoryHasChanged)
        checkForUpdates();

    return true;
}

void UpdateControllerRauc::init()
{
    // Load repositories and system info json file
    // First try to load an existing user configuration, if there is no user config, try to load the platform config, otherwise the update plugin is not available.
    QString configurationFileName = NymeaSettings::settingsPath() + "/rauc-update.json";
    if (!QFileInfo::exists(configurationFileName)) {
        configurationFileName = NymeaSettings::defaultSettingsPath() + "/rauc-update.json";
    }

    if (!QFileInfo::exists(configurationFileName)) {
        qCWarning(dcPlatformUpdate()) << "Platform update not available. Could not find any rauc-update.json configuration in" << NymeaSettings::settingsPath() << "and"
                                      << NymeaSettings::defaultSettingsPath();
        setRaucAvailable(false);
        setUpdateRunningState(false);
        setBusyState(false);
        setUpdateProgress(-1);
        return;
    }

    QString systemName;
    QString systemDescription;
    QString systemVendor;

    QJsonDocument doc = loadJsonFile(configurationFileName);
    if (!doc.isNull()) {
        QJsonObject obj = doc.object();
        systemName = obj.value("systemName").toString();
        systemDescription = obj.value("systemDescription").toString();
        systemVendor = obj.value("systemVendor").toString();

        QJsonArray repositoriesArray = obj.value("repositories").toArray();
        for (const QJsonValue &value : repositoriesArray) {
            QJsonObject repositoryObject = value.toObject();
            // Note: once we support more then one repository type, we need to distinguish here.
            if (repositoryObject.value("repositoryType").toString() == "selfhosted") {
                // Note: load all with enabled false, depending on the settings the current enabled repository will be enabled afterwards. Only one can be active.
                SelfHostedRepository repository(repositoryObject.value("name").toString(), repositoryObject.value("name").toString(), false);
                repository.setUrl(QUrl(repositoryObject.value("url").toString()));
                repository.setAuthentication(repositoryObject.value("authenticationType").toString() == "basic" ? SelfHostedRepository::AuthenticationBasic
                                                                                                                : SelfHostedRepository::AuthenticationNone);
                repository.setUserName(repositoryObject.value("userName").toString());
                repository.setPassword(repositoryObject.value("password").toString());
                m_repositories.insert(repository.id(), repository);
                qCDebug(dcPlatformUpdate()) << "Loaded repository" << repository.id() << repository.displayName();
            } else {
                qCWarning(dcPlatformUpdate()) << "Unrecognized repository type. Skipp loading repository object" << repositoryObject;
            }
        }

        // Loaded successfully. Now enable the currently selected repository...
        NymeaSettings settings(NymeaSettings::SettingsRoleGlobal);
        settings.beginGroup("rauc-update");
        QString activeRepository = settings.value("repository").toString();
        settings.endGroup();

        if (activeRepository.isEmpty() || !m_repositories.contains(activeRepository)) {
            if (m_repositories.isEmpty()) {
                qCWarning(dcPlatformUpdate()) << "The active repository could not be found. There is no repository available. The platform update will not be availbale.";
                return;
            } else {
                qCWarning(dcPlatformUpdate()) << "No active repository found. Selecting the first one available.";
                enableRepository(m_repositories.keys().first(), true);
            }
        } else {
            UpdateControllerRauc::enableRepository(activeRepository, true);
        }
    } else {
        systemName = "system";
    }

    qCDebug(dcPlatformUpdate()) << "System name" << systemName << systemDescription << systemVendor;
    qCDebug(dcPlatformUpdate()) << "System description" << systemDescription;
    qCDebug(dcPlatformUpdate()) << "System vendor" << systemVendor;

    // Load repository cache
    m_cacheFileName = NymeaSettings::cachePath() + "/rauc-update.json";
    if (QFileInfo::exists(m_cacheFileName))
        m_releaseInfo = loadJsonFile(m_cacheFileName).toVariant().toMap();

    // Create package  for current system
    m_package = Package("system", systemName);
    m_package.setSummary(systemDescription);
    m_package.setCanRemove(false);
    m_package.setRollbackAvailable(false);
    m_package.setUpdateAvailable(verifyUpdateAvailable());

    m_raucInterface = new RaucDBusInterface(this);
    connect(m_raucInterface, &RaucDBusInterface::availabilityChanged, this, [this](bool available) {
        setRaucAvailable(available);
        if (available)
            checkForUpdates();
    });

    connect(m_raucInterface, &RaucDBusInterface::progressChanged, this, [this](int percentage, const QString &message) {
        qCDebug(dcPlatformUpdate()) << "RAUC Update progress changed:" << message << percentage << "%";
        setUpdateProgress(percentage);
    });

    connect(m_raucInterface, &RaucDBusInterface::operationChanged, this, [](const QString &operation) {
        qCDebug(dcPlatformUpdate()) << "RAUC update operation changed:" << operation;
    });

    connect(m_raucInterface, &RaucDBusInterface::completed, this, [this](int result) {
        if (result != 0) {
            qCWarning(dcPlatformUpdate()) << "RAUC update process finished with error" << result;
        } else {
            qCInfo(dcPlatformUpdate()) << "RAUC update process finished successfully";
        }

        setUpdateRunningState(false);
        setBusyState(false);
        setUpdateProgress(-1);

        checkForUpdates();
    });

    setRaucAvailable(m_raucInterface->isAvailable());

    if (updateManagementAvailable())
        checkForUpdates();

    // Refresh timer
    m_checkUpdateTimer.setInterval(10 * 60 * 1000); // 10 min
    m_checkUpdateTimer.setSingleShot(false);
    connect(&m_checkUpdateTimer, &QTimer::timeout, this, &UpdateControllerRauc::checkForUpdates);
    m_checkUpdateTimer.start();
}

bool UpdateControllerRauc::verifyUpdateAvailable()
{
    if (m_candidateHash.isEmpty() || m_currentHash.isEmpty() || m_releaseInfo.isEmpty())
        return false;

    return m_currentHash != m_candidateHash;
}

void UpdateControllerRauc::setRaucAvailable(bool available)
{
    if (m_raucAvailable == available)
        return;

    m_raucAvailable = available;
    emit availableChanged();
}

void UpdateControllerRauc::setUpdateRunningState(bool running)
{
    if (m_updateRunning == running)
        return;

    m_updateRunning = running;
    emit updateRunningChanged();
}

void UpdateControllerRauc::setBusyState(bool busy)
{
    if (m_busy == busy)
        return;

    m_busy = busy;
    emit busyChanged();
}

void UpdateControllerRauc::setUpdateProgress(int updateProgress)
{
    if (m_updateProgress == updateProgress)
        return;

    m_updateProgress = updateProgress;
    emit updateProgressChanged();
}

QJsonDocument UpdateControllerRauc::loadJsonFile(const QString &fileName)
{
    QFile configurationFile(fileName);
    if (!configurationFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(dcPlatformUpdate()) << "Failed to open JSON file:" << fileName << configurationFile.errorString();
        return QJsonDocument();
    }

    const QByteArray data = configurationFile.readAll();
    configurationFile.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error in" << fileName << ":" << err.errorString() << "at offset" << err.offset;
        return QJsonDocument();
    }

    return doc;
}
