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
#include "data/migration/MigrationManager.hpp"
#include "data/migration/TempMigrator.hpp"
#include "util/config//Config.hpp"

#include <memory>
#include <string>

namespace app {

/**
 * @brief The migrate class
 */
class MigratorApplication {
    util::Config const& config_;
    std::string option_;

public:
    /**
     * @brief Construct a new MigratorApplication object
     *
     * @param config The configuration of the application
     */
    MigratorApplication(util::Config const& config, std::string option);

    /**
     * @brief Run the application
     *
     * @return exit code
     */
    int
    run();

private:
    using MigrationManager = MigrationManager<TempMigrator>;
    std::shared_ptr<MigrationManager> migrationManager_;
    std::shared_ptr<data::BackendInterface> backend_;
    int
    printStatus();
};

}  // namespace app
