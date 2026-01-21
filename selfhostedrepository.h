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

#ifndef SELFHOSTEDREPOSITORY_H
#define SELFHOSTEDREPOSITORY_H

#include <QObject>
#include <QUrl>

#include <platform/repository.h>

class SelfHostedRepository : public Repository
{
    Q_GADGET
public:
    enum Authentication {
        AuthenticationNone,
        AuthenticationBasic
    };
    Q_ENUM(Authentication)

    explicit SelfHostedRepository();
    SelfHostedRepository(const QString &id, const QString &displayName, bool enabled);

    QUrl url() const;
    void setUrl(const QUrl &url);

    Authentication authentication() const;
    void setAuthentication(Authentication authentication);

    QString userName() const;
    void setUserName(const QString &userName);

    QString password() const;
    void setPassword(const QString &password);

private:
    QUrl m_url;
    Authentication m_authentication = AuthenticationNone;
    QString m_userName;
    QString m_password;
};

#endif // SELFHOSTEDREPOSITORY_H
