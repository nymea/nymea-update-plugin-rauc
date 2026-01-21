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

#include "selfhostedrepository.h"

SelfHostedRepository::SelfHostedRepository()
    : Repository{}
{}

SelfHostedRepository::SelfHostedRepository(const QString &id, const QString &displayName, bool enabled)
    : Repository{id, displayName, enabled}
{}

QUrl SelfHostedRepository::url() const
{
    return m_url;
}

void SelfHostedRepository::setUrl(const QUrl &url)
{
    m_url = url;
}

void SelfHostedRepository::setAuthentication(Authentication authentication)
{
    m_authentication = authentication;
}

QString SelfHostedRepository::userName() const
{
    return m_userName;
}

void SelfHostedRepository::setUserName(const QString &userName)
{
    m_userName = userName;
}

QString SelfHostedRepository::password() const
{
    return m_password;
}

void SelfHostedRepository::setPassword(const QString &password)
{
    m_password = password;
}

SelfHostedRepository::Authentication SelfHostedRepository::authentication() const
{
    return m_authentication;
}
