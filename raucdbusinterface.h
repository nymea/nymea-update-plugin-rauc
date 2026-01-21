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

#ifndef RAUCDBUSINTERFACE_H
#define RAUCDBUSINTERFACE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

class QDBusArgument;
class QDBusServiceWatcher;
class RaucInstallerInterface;

class RaucDBusInterface : public QObject
{
    Q_OBJECT

public:
    explicit RaucDBusInterface(QObject *parent = nullptr);

    bool isAvailable() const;

    QVariantMap getSlotStatus(QString *errorMessage = nullptr) const;
    QString getPrimary(QString *errorMessage = nullptr) const;
    QString operation() const;
    int progressPercent() const;
    QString progressMessage() const;
    bool installBundle(const QString &bundleUrl, QString *errorMessage = nullptr) const;

signals:
    void availabilityChanged(bool available);
    void operationChanged(const QString &operation);
    void progressChanged(int percentage, const QString &message);
    void completed(int result);

private slots:
    void onServiceRegistered(const QString &service);
    void onServiceUnregistered(const QString &service);
    void onPropertiesChanged(const QString &interface, const QVariantMap &changedProperties, const QStringList &invalidatedProperties);
    void onCompleted(int result);

private:
    void setAvailable(bool available);
    void setOperation(const QString &operation);
    void setProgress(int percentage, const QString &message);
    void refreshProperties();
    void updateFromProperties(const QVariantMap &properties);
    bool parseProgress(const QVariant &value, int *percentage, QString *message) const;
    QVariantMap parseSlotStatus(const QVariant &value) const;
    QVariant normalizeVariant(const QVariant &value) const;
    QVariant normalizeDBusArgument(const QDBusArgument &argument) const;

    bool m_available = false;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    RaucInstallerInterface *m_installerInterface = nullptr;
    QString m_operation;
    int m_progressPercent = -1;
    QString m_progressMessage;
};

#endif // RAUCDBUSINTERFACE_H
