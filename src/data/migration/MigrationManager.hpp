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
#include "data/migration/TempMigrator.hpp"

#include <boost/asio/spawn.hpp>

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

template <typename... Ts>
class MigrationManager {
    std::unordered_map<std::string, std::shared_ptr<BaseMigrator>> registeredMigrators_;
    std::shared_ptr<data::BackendInterface> backend_;

public:
    MigrationManager(std::shared_ptr<data::BackendInterface>& backend) : backend_(backend)
    {
        registerMigrator<TempMigrator>();
    }

    std::vector<std::tuple<std::string, bool>>
    allMigratorsStatus(boost::asio::yield_context ctx)
    {
        auto const migratedFromDB = backend_->fetchMigratedFeatures(ctx);
        if (not migratedFromDB.has_value())
            return {};

        std::vector<std::tuple<std::string, bool>> result;
        for (auto const& migrator : registeredMigrators_) {
            if (migratedFromDB->contains(migrator.first)) {
                result.emplace_back(migrator.first, true);
            } else {
                result.emplace_back(migrator.first, false);
            }
        }

        return result;
    }

private:
    template <typename T>
    void
    registerMigrator()
    {
        auto migrator = std::make_shared<T>(backend_);
        registeredMigrators_.emplace(migrator->name(), migrator);
    }
};
