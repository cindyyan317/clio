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
#include "data/cassandra/Handle.hpp"
#include "data/cassandra/Schema.hpp"
#include "data/migration/MigrationSchema.hpp"

template <
    data::cassandra::SomeSettingsProvider SettingsProviderType,
    data::cassandra::SomeExecutionStrategy ExecutionStrategyType>
class BasicMigrationCassandraBackend
    : public data::cassandra::BasicCassandraBackend<SettingsProviderType, ExecutionStrategyType> {
    MigrationSchema<SettingsProviderType, MigationStatements<SettingsProviderType>> migrationSchema_;

    util::Logger log_{"MigrationBackend"};

public:
    BasicMigrationCassandraBackend(SettingsProviderType settingsProvider)
        : data::cassandra::
              BasicCassandraBackend<SettingsProviderType, ExecutionStrategyType>{settingsProvider, false /* readOnly*/}
        , migrationSchema_{settingsProvider}
    {
        migrationSchema_.prepareStatements(this->handle_);
    }

    auto const&
    getMigrationSchema() const
    {
        return migrationSchema_;
    }

    template <typename TableDesc>
    void
    migrateInTokenRange(
        std::int64_t const& start,
        std::int64_t const& end,
        TableDesc::callback callback,
        boost::asio::yield_context yield
    )
    {
        static auto statementPrepared =
            migrationSchema_->getPreparedStatement(TableDesc::tableName, TableDesc::partitionKey);
        auto statement = statementPrepared.bind(start, end);

        auto const res = this->executor_.read(yield, statement);
        auto const& results = res.value();
        if (not results.hasRows()) {
            LOG(log_.debug()) << "No rows returned";
            return;
        }

        auto numRows = results.numRows();
        LOG(log_.info()) << "num_rows = " << numRows;

        for (auto row : std::apply(
                 [&](auto... args) { return data::cassandra::extract<decltype(args)...>(results); },
                 typename TableDesc::row{}
             )) {
            callback(row);
        }
    }

    void
    migrateObjectsInTokenRange(
        std::int64_t const& start,
        std::int64_t const& end,
        std::function<void(std::uint32_t, data::Blob const&)> const& onRead,
        boost::asio::yield_context yield
    )
    {
        auto statement = migrationSchema_->objectsTraverse.bind(start, end);
        auto const res = this->executor_.read(yield, statement);
        auto const& results = res.value();
        if (not results.hasRows()) {
            LOG(log_.debug()) << "No rows returned";
            return;
        }

        auto numRows = results.numRows();
        LOG(log_.info()) << "num_rows = " << numRows;

        for (auto [key, seq, object] : data::cassandra::extract<ripple::uint256, uint32_t, data::Blob>(results)) {
            onRead(seq, object);
        }
    }

    void
    migrateTransactionsInTokenRange(
        std::int64_t const& start,
        std::int64_t const& end,
        std::function<void(data::Blob const&, data::Blob const&)> const& onRead,
        boost::asio::yield_context yield
    )
    {
        auto statement = migrationSchema_->transactionsTraverse.bind(start, end);
        auto const res = this->executor_.read(yield, statement);
        auto const& results = res.value();
        if (not results.hasRows()) {
            LOG(log_.debug()) << "No rows returned";
            return;
        }

        auto numRows = results.numRows();
        LOG(log_.info()) << "num_rows = " << numRows;

        for (auto [_1, _2, _3, txBlob, metaBlob] :
             data::cassandra::extract<ripple::uint256, uint32_t, uint32_t, data::Blob, data::Blob>(results)) {
            onRead(txBlob, metaBlob);
        }
    }
};

using MigrationCassandraBackend = BasicMigrationCassandraBackend<
    data::cassandra::SettingsProvider,
    data::cassandra::impl::DefaultExecutionStrategy<>>;

inline std::shared_ptr<MigrationCassandraBackend>
make_MigrationBackend(util::Config const& config)
{
    static util::Logger const log{"MigrationBackend"};
    LOG(log.info()) << "Constructing MigrationBackend";

    auto const type = config.value<std::string>("database.type");
    std::shared_ptr<MigrationCassandraBackend> backend = nullptr;

    if (boost::iequals(type, "cassandra")) {
        auto cfg = config.section("database." + type);
        backend = std::make_shared<MigrationCassandraBackend>(data::cassandra::SettingsProvider{cfg});
    }

    if (!backend)
        throw std::runtime_error("Invalid database type");

    LOG(log.info()) << "Constructed MigrationBackend Successfully";
    return backend;
}
