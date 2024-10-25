//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================
#pragma once

#include "data/CassandraBackend.hpp"
#include "data/cassandra/Schema.hpp"
#include "data/cassandra/Types.hpp"
#include "data/migration/MigrationSchema.hpp"

template <
    data::cassandra::SomeSettingsProvider SettingsProviderType,
    data::cassandra::SomeExecutionStrategy ExecutionStrategyType>
class BasicMigrationCassandraBackend
    : public data::cassandra::BasicCassandraBackend<SettingsProviderType, ExecutionStrategyType> {
    MigrationSchema<SettingsProviderType> migrationSchema_;

public:
    BasicMigrationCassandraBackend(SettingsProviderType settingsProvider)
        : data::cassandra::
              BasicCassandraBackend<SettingsProviderType, ExecutionStrategyType>{settingsProvider, false /* readOnly*/}
        , migrationSchema_{settingsProvider}
    {
        migrationSchema_.prepareStatements(this->handle_);
    }

    MigrationSchema<SettingsProviderType> const&
    getMigrationSchema() const
    {
        return migrationSchema_;
    }
};

using MigrationCassandraBackend = BasicMigrationCassandraBackend<
    data::cassandra::SettingsProvider,
    data::cassandra::impl::DefaultExecutionStrategy<>>;

inline std::shared_ptr<MigrationCassandraBackend>
make_MigrationBackend(util::Config const& config)
{
    static util::Logger const log{"MigrationBackend"};
    LOG(log.info()) << "Constructing BackendInterface";

    auto const readOnly = config.valueOr("read_only", false);

    auto const type = config.value<std::string>("database.type");
    std::shared_ptr<MigrationCassandraBackend> backend = nullptr;

    if (boost::iequals(type, "cassandra")) {
        auto cfg = config.section("database." + type);
        backend = std::make_shared<data::cassandra::MigrationCassandraBackend>(
            data::cassandra::SettingsProvider{cfg}, readOnly
        );
    }

    if (!backend)
        throw std::runtime_error("Invalid database type");

    LOG(log.info()) << "Constructed BackendInterface Successfully";
    return backend;
}
