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

#include "raucdbusinterface.h"

#include "raucinstallerinterface.h"

#include <QByteArray>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QDBusSignature>
#include <QDBusUnixFileDescriptor>
#include <QDBusVariant>
#include <QMetaType>
#include <QUrl>

#include <loggingcategories.h>

namespace {

const char kRaucService[] = "de.pengutronix.rauc";
const char kRaucPath[] = "/";
const char kDbusPropertiesInterface[] = "org.freedesktop.DBus.Properties";

} // namespace

RaucDBusInterface::RaucDBusInterface(QObject *parent)
    : QObject(parent)
{
    m_serviceWatcher = new QDBusServiceWatcher(QLatin1String(kRaucService),
                                               QDBusConnection::systemBus(),
                                               QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
                                               this);

    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceRegistered, this, &RaucDBusInterface::onServiceRegistered);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, &RaucDBusInterface::onServiceUnregistered);

    QDBusConnection::systemBus().connect(QLatin1String(kRaucService),
                                         QLatin1String(kRaucPath),
                                         QLatin1String(kDbusPropertiesInterface),
                                         QStringLiteral("PropertiesChanged"),
                                         this,
                                         SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    QDBusConnection::systemBus().connect(QLatin1String(kRaucService),
                                         QLatin1String(kRaucPath),
                                         QLatin1String(RaucInstallerInterface::staticInterfaceName()),
                                         QStringLiteral("Completed"),
                                         this,
                                         SLOT(onCompleted(int)));

    m_installerInterface = new RaucInstallerInterface(QLatin1String(kRaucService), QLatin1String(kRaucPath), QDBusConnection::systemBus(), this);

    QDBusConnectionInterface *busInterface = QDBusConnection::systemBus().interface();
    setAvailable(busInterface && busInterface->isServiceRegistered(QLatin1String(kRaucService)));
    if (m_available)
        refreshProperties();
}

bool RaucDBusInterface::isAvailable() const
{
    return m_available;
}

QVariantMap RaucDBusInterface::getSlotStatus(QString *errorMessage) const
{
    if (!m_available) {
        if (errorMessage)
            *errorMessage = QStringLiteral("RAUC service is not available.");

        return QVariantMap();
    }

    if (!m_installerInterface || !m_installerInterface->isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("RAUC installer interface is not available.");

        return QVariantMap();
    }

    QDBusMessage reply = m_installerInterface->call(QStringLiteral("GetSlotStatus"));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (errorMessage)
            *errorMessage = reply.errorMessage();

        return QVariantMap();
    }

    if (reply.arguments().isEmpty())
        return QVariantMap();

    return parseSlotStatus(reply.arguments().first());
}

QString RaucDBusInterface::getPrimary(QString *errorMessage) const
{
    if (!m_available) {
        if (errorMessage)
            *errorMessage = QStringLiteral("RAUC service is not available.");

        return QString();
    }

    if (!m_installerInterface || !m_installerInterface->isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("RAUC installer interface is not available.");

        return QString();
    }

    QDBusMessage reply = m_installerInterface->call(QStringLiteral("GetPrimary"));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (errorMessage)
            *errorMessage = reply.errorMessage();

        return QString();
    }

    if (reply.arguments().isEmpty())
        return QString();

    const QVariant normalized = normalizeVariant(reply.arguments().first());
    return normalized.toString();
}

QString RaucDBusInterface::operation() const
{
    return m_operation;
}

int RaucDBusInterface::progressPercent() const
{
    return m_progressPercent;
}

QString RaucDBusInterface::progressMessage() const
{
    return m_progressMessage;
}

bool RaucDBusInterface::installBundle(const QString &bundleUrl, QString *errorMessage) const
{
    if (!m_available) {
        if (errorMessage)
            *errorMessage = QStringLiteral("RAUC service is not available.");

        return false;
    }

    if (!m_installerInterface || !m_installerInterface->isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("RAUC installer interface is not available.");

        return false;
    }

    if (bundleUrl.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Bundle URL is empty.");

        return false;
    }

    QUrl url = QUrl::fromUserInput(bundleUrl);
    if (!url.isValid() || url.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Bundle URL is invalid.");

        return false;
    }

    QString sourceArgument;
    if (url.isLocalFile()) {
        sourceArgument = url.toLocalFile();
    } else {
        sourceArgument = url.toString(QUrl::FullyEncoded);
    }

    const QVariantMap args;
    QDBusPendingReply<> reply = m_installerInterface->InstallBundle(sourceArgument, args);
    reply.waitForFinished();
    if (reply.isError()) {
        if (errorMessage)
            *errorMessage = reply.error().message();
        return false;
    }

    return true;
}

void RaucDBusInterface::onServiceRegistered(const QString &service)
{
    Q_UNUSED(service)
    setAvailable(true);
    refreshProperties();
}

void RaucDBusInterface::onServiceUnregistered(const QString &service)
{
    Q_UNUSED(service)
    setAvailable(false);
    setOperation(QString());
    setProgress(-1, QString());
}

void RaucDBusInterface::setAvailable(bool available)
{
    if (m_available == available)
        return;

    m_available = available;
    emit availabilityChanged(available);
}

void RaucDBusInterface::setOperation(const QString &operation)
{
    if (m_operation == operation)
        return;

    m_operation = operation;
    emit operationChanged(m_operation);

    qCDebug(dcDebugServer()) << "Operation changed:" << m_operation;
}

void RaucDBusInterface::setProgress(int percentage, const QString &message)
{
    if (m_progressPercent == percentage && m_progressMessage == message)
        return;

    m_progressPercent = percentage;
    m_progressMessage = message;
    emit progressChanged(m_progressPercent, m_progressMessage);

    qCDebug(dcDebugServer()) << "Update progress changed:" << m_progressMessage << m_progressPercent << "%";
}

void RaucDBusInterface::refreshProperties()
{
    if (!m_available)
        return;

    QDBusInterface propertiesInterface(QLatin1String(kRaucService), QLatin1String(kRaucPath), QLatin1String(kDbusPropertiesInterface), QDBusConnection::systemBus());
    if (!propertiesInterface.isValid()) {
        qCWarning(dcPlatformUpdate()) << "RAUC properties interface is not available.";
        return;
    }

    QDBusMessage reply = propertiesInterface.call(QStringLiteral("GetAll"), QLatin1String(RaucInstallerInterface::staticInterfaceName()));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCWarning(dcPlatformUpdate()) << "Failed to read RAUC properties:" << reply.errorMessage();
        return;
    }

    if (reply.arguments().isEmpty())
        return;

    const QVariant normalized = normalizeVariant(reply.arguments().first());
    if (normalized.userType() != qMetaTypeId<QVariantMap>())
        return;

    updateFromProperties(normalized.toMap());
}

void RaucDBusInterface::updateFromProperties(const QVariantMap &properties)
{
    if (properties.contains(QStringLiteral("Operation")))
        setOperation(properties.value(QStringLiteral("Operation")).toString());

    if (properties.contains(QStringLiteral("Progress"))) {
        int percentage = m_progressPercent;
        QString message = m_progressMessage;

        if (parseProgress(properties.value(QStringLiteral("Progress")), &percentage, &message))
            setProgress(percentage, message);
    }
}

void RaucDBusInterface::onPropertiesChanged(const QString &interface, const QVariantMap &changedProperties, const QStringList &invalidatedProperties)
{
    if (interface != QLatin1String(RaucInstallerInterface::staticInterfaceName()))
        return;

    if (changedProperties.isEmpty() && invalidatedProperties.isEmpty())
        return;

    updateFromProperties(changedProperties);

    for (auto it = changedProperties.cbegin(); it != changedProperties.cend(); ++it) {
        const QVariant normalized = normalizeVariant(it.value());
        qCDebug(dcPlatformUpdate()) << "RAUC property changed:" << it.key() << normalized;
    }

    if (!invalidatedProperties.isEmpty())
        qCDebug(dcPlatformUpdate()) << "RAUC properties invalidated:" << invalidatedProperties;
}

void RaucDBusInterface::onCompleted(int result)
{
    emit completed(result);
}

bool RaucDBusInterface::parseProgress(const QVariant &value, int *percentage, QString *message) const
{
    if (!percentage || !message)
        return false;

    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument arg = value.value<QDBusArgument>();
        if (arg.currentSignature() == QLatin1String("(isi)")) {
            int percent = 0;
            QString msg;
            int depth = 0;

            arg.beginStructure();
            arg >> percent >> msg >> depth;
            arg.endStructure();
            *percentage = percent;
            *message = msg;
            Q_UNUSED(depth)
            return true;
        }
    }

    const QVariant normalized = normalizeVariant(value);
    if (!normalized.isValid())
        return false;

    if (normalized.userType() == qMetaTypeId<QVariantList>()) {
        const QVariantList list = normalized.toList();
        if (!list.isEmpty())
            *percentage = list.value(0).toInt();

        if (list.size() > 1)
            *message = list.value(1).toString();

        return true;
    }

    if (normalized.canConvert<int>()) {
        *percentage = normalized.toInt();
        return true;
    }

    if (normalized.canConvert<QString>()) {
        *message = normalized.toString();
        return true;
    }

    return false;
}

QVariantMap RaucDBusInterface::parseSlotStatus(const QVariant &value) const
{
    if (!value.isValid())
        return QVariantMap();

    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument arg = value.value<QDBusArgument>();
        if (arg.currentSignature() == QLatin1String("a(sa{sv})")) {
            QVariantMap slotMap;
            arg.beginArray();
            while (!arg.atEnd()) {
                arg.beginStructure();
                QString slotName;
                QVariantMap slotProps;
                arg >> slotName >> slotProps;
                arg.endStructure();

                const QVariant normalized = normalizeVariant(slotProps);
                if (normalized.userType() == qMetaTypeId<QVariantMap>()) {
                    slotMap.insert(slotName, normalized.toMap());
                } else {
                    slotMap.insert(slotName, normalized);
                }
            }

            arg.endArray();
            return slotMap;
        }
    }

    const QVariant normalized = normalizeVariant(value);
    if (normalized.userType() == qMetaTypeId<QVariantMap>())
        return normalized.toMap();

    if (normalized.userType() == qMetaTypeId<QVariantList>()) {
        QVariantMap wrapper;
        wrapper.insert(QStringLiteral("slots"), normalized.toList());
        return wrapper;
    }

    QVariantMap wrapper;
    wrapper.insert(QStringLiteral("value"), normalized);
    return wrapper;
}

QVariant RaucDBusInterface::normalizeVariant(const QVariant &value) const
{
    if (!value.isValid())
        return QVariant();

    if (value.userType() == qMetaTypeId<QDBusVariant>())
        return normalizeVariant(value.value<QDBusVariant>().variant());

    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        const QVariant normalized = normalizeDBusArgument(value.value<QDBusArgument>());
        if (normalized.isValid())
            return normalized;

        return value;
    }

    if (value.userType() == qMetaTypeId<QVariantMap>()) {
        const QVariantMap map = value.toMap();
        QVariantMap normalized;
        for (auto it = map.begin(); it != map.end(); ++it)
            normalized.insert(it.key(), normalizeVariant(it.value()));

        return normalized;
    }

    if (value.userType() == qMetaTypeId<QVariantList>()) {
        const QVariantList list = value.toList();
        QVariantList normalizedList;
        normalizedList.reserve(list.size());

        for (const QVariant &item : list)
            normalizedList.append(normalizeVariant(item));

        return normalizedList;
    }

    return value;
}

QVariant RaucDBusInterface::normalizeDBusArgument(const QDBusArgument &argument) const
{
    const QDBusArgument arg(argument);
    const QString signature = arg.currentSignature();

    if (arg.currentType() == QDBusArgument::VariantType) {
        QDBusVariant variant;
        arg >> variant;
        return normalizeVariant(variant.variant());
    }

    if (signature == QLatin1String("a{sv}")) {
        QVariantMap map;
        arg.beginMap();
        while (!arg.atEnd()) {
            arg.beginMapEntry();
            QString key;
            QDBusVariant entryValue;
            arg >> key >> entryValue;
            map.insert(key, normalizeVariant(entryValue.variant()));
            arg.endMapEntry();
        }
        arg.endMap();
        return map;
    }

    if (signature == QLatin1String("a{ss}")) {
        QVariantMap map;
        arg.beginMap();
        while (!arg.atEnd()) {
            arg.beginMapEntry();
            QString key;
            QString entryValue;
            arg >> key >> entryValue;
            map.insert(key, entryValue);
            arg.endMapEntry();
        }
        arg.endMap();
        return map;
    }

    if (signature == QLatin1String("av")) {
        QVariantList list;
        arg.beginArray();
        while (!arg.atEnd()) {
            QDBusVariant entryValue;
            arg >> entryValue;
            list.append(normalizeVariant(entryValue.variant()));
        }
        arg.endArray();
        return list;
    }

    if (signature == QLatin1String("as")) {
        QStringList list;
        arg >> list;
        return list;
    }

    if (signature == QLatin1String("ay")) {
        QByteArray bytes;
        arg >> bytes;
        return bytes;
    }

    if (signature == QLatin1String("(isi)")) {
        int percent = 0;
        QString message;
        int depth = 0;
        arg.beginStructure();
        arg >> percent >> message >> depth;
        arg.endStructure();
        QVariantList list;
        list << percent << message << depth;
        return list;
    }

    if (signature.size() == 1) {
        switch (signature.at(0).toLatin1()) {
        case 'y': {
            uchar value = 0;
            arg >> value;
            return value;
        }
        case 'b': {
            bool value = false;
            arg >> value;
            return value;
        }
        case 'n': {
            short value = 0;
            arg >> value;
            return value;
        }
        case 'q': {
            ushort value = 0;
            arg >> value;
            return value;
        }
        case 'i': {
            int value = 0;
            arg >> value;
            return value;
        }
        case 'u': {
            uint value = 0;
            arg >> value;
            return value;
        }
        case 'x': {
            qlonglong value = 0;
            arg >> value;
            return value;
        }
        case 't': {
            qulonglong value = 0;
            arg >> value;
            return value;
        }
        case 'd': {
            double value = 0.0;
            arg >> value;
            return value;
        }
        case 's': {
            QString value;
            arg >> value;
            return value;
        }
        case 'o': {
            QDBusObjectPath value;
            arg >> value;
            return value.path();
        }
        case 'g': {
            QDBusSignature value;
            arg >> value;
            return value.signature();
        }
        case 'h': {
            QDBusUnixFileDescriptor value;
            arg >> value;
            return value.fileDescriptor();
        }
        default:
            break;
        }
    }

    return QVariant();
}
