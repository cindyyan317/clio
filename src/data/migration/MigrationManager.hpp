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
#include "data/BackendInterface.hpp"
#include "data/migration/BaseMigrator.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

enum class MigrationStatus { Migrated, NotMigrated, UnknownMigrator };

template <typename... MigratorType>
class MigrationManager {
    std::unordered_map<std::string, std::shared_ptr<BaseMigrator>> registeredMigrators_;
    std::shared_ptr<data::BackendInterface> backend_;
    util::Logger log_{"Migration"};

public:
    MigrationManager(std::shared_ptr<data::BackendInterface>& backend) : backend_(backend)
    {
        (registerMigrator<MigratorType>(), ...);
    }

    std::vector<std::string>
    getBlockedMigrations(boost::asio::yield_context ctx)
    {
        auto migratedFromDB = backend_->fetchMigratedFeatures(ctx);
        if (not migratedFromDB.has_value())
            return {};

        std::vector<std::string> blockedMigrations;
        for (auto const& migrator : registeredMigrators_) {
            if (!migratedFromDB->contains(migrator.first)) {
                LOG(log_.warn()) << "Migrator " << migrator.first << " has not been run";
                if (migrator.second->blockIfNotMigrated()) {
                    blockedMigrations.push_back(migrator.first);
                }
            }
        }
        return blockedMigrations;
    }

    std::vector<std::tuple<std::string, MigrationStatus>>
    allMigratorsStatus(boost::asio::yield_context ctx)
    {
        auto migratedFromDB = backend_->fetchMigratedFeatures(ctx);
        if (not migratedFromDB.has_value())
            return {};

        std::vector<std::tuple<std::string, MigrationStatus>> result;
        for (auto const& migrator : registeredMigrators_) {
            if (migratedFromDB->contains(migrator.first)) {
                result.emplace_back(migrator.first, MigrationStatus::Migrated);
            } else {
                result.emplace_back(migrator.first, MigrationStatus::NotMigrated);
            }
            migratedFromDB->erase(migrator.first);
        }

        for (auto const& migrated : migratedFromDB.value()) {
            result.emplace_back(migrated, MigrationStatus::UnknownMigrator);
        }

        return result;
    }

    void
    runMigration(std::string name)
    {
        if (registeredMigrators_.find(name) == registeredMigrators_.end()) {
            LOG(log_.error()) << "Migrator " << name << " not found";
            return;
        }
        registeredMigrators_[name]->runMigration(backend_);
    }

private:
    template <typename T>
    void
    registerMigrator()
    {
        auto migrator = std::make_shared<T>();
        registeredMigrators_.emplace(migrator->name(), migrator);
    }
};
