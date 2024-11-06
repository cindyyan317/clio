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
#include "data/cassandra/Concepts.hpp"
#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/Statement.hpp"
#include "util/log/Logger.hpp"

#pragma once

/**
 * @brief Manages the DB schema and provides access to prepared statements.
 */
template <data::cassandra::SomeSettingsProvider SettingsProviderType, typename Statements>
class MigrationSchema {
    util::Logger log_{"Backend"};
    std::reference_wrapper<SettingsProviderType const> settingsProvider_;
    std::unique_ptr<Statements> statements_{nullptr};

public:
    /**
     * @brief Construct a new Schema object
     *
     * @param settingsProvider The settings provider
     */
    explicit MigrationSchema(SettingsProviderType const& settingsProvider)
        : settingsProvider_{std::cref(settingsProvider)}
    {
    }

    std::unique_ptr<Statements> const&
    operator->() const
    {
        return statements_;
    }

    void
    prepareStatements(data::cassandra::Handle const& handle)
    {
        LOG(log_.info()) << "Preparing cassandra statements";
        statements_ = std::make_unique<Statements>(settingsProvider_, handle);
        LOG(log_.info()) << "Finished preparing statements";
    }
};

template <data::cassandra::SomeSettingsProvider SettingsProviderType>
class MigationStatements {
    std::reference_wrapper<SettingsProviderType const> settingsProvider_;
    std::reference_wrapper<data::cassandra::Handle const> handle_;

public:
    /**
     * @brief Construct a new Statements object
     *
     * @param settingsProvider The settings provider
     * @param handle The handle to the DB
     */
    MigationStatements(SettingsProviderType const& settingsProvider, data::cassandra::Handle const& handle)
        : settingsProvider_{settingsProvider}, handle_{std::cref(handle)}
    {
    }

    data::cassandra::PreparedStatement
    getPreparedStatement(std::string const& table, std::string const& key)
    {
        return handle_.get().prepare(fmt::format(
            R"(
                SELECT * FROM {} 
                         WHERE TOKEN({}) >= ? 
                           AND TOKEN({}) <= ?
                )",
            qualifiedTableName(settingsProvider_.get(), table, key, key)
        ));
    }

    // the statements only work in migration process
    data::cassandra::PreparedStatement objectsTraverse = [this]() {
        return handle_.get().prepare(fmt::format(
            R"(
                SELECT * FROM {} 
                         WHERE TOKEN(key) >= ? 
                           AND TOKEN(key) <= ?
                )",
            qualifiedTableName(settingsProvider_.get(), "objects")
        ));
    }();

    data::cassandra::PreparedStatement transactionsTraverse = [this]() {
        return handle_.get().prepare(fmt::format(
            R"(
                SELECT transaction, metadata FROM {} 
                         WHERE TOKEN(hash) >= ? 
                           AND TOKEN(hash) <= ?
                )",
            qualifiedTableName(settingsProvider_.get(), "transactions")
        ));
    }();
};
