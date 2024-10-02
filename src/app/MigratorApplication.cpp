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

#include "app/MigratorApplication.hpp"

#include "data/BackendFactory.hpp"
#include "data/BackendInterface.hpp"
#include "data/migration/MigrationManager.hpp"
#include "rpc/common/impl/HandlerProvider.hpp"
#include "util/build/Build.hpp"
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/log/attributes/scoped_attribute.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace app {

namespace {

/**
 * @brief Start context threads
 *
 * @param ioc Context
 * @param numThreads Number of worker threads to start
 */
// void
// start(boost::asio::io_context& ioc, std::uint32_t numThreads)
// {
//     std::vector<std::thread> v;
//     v.reserve(numThreads - 1);
//     for (auto i = numThreads - 1; i > 0; --i)
//         v.emplace_back([&ioc] { ioc.run(); });

//     ioc.run();
//     for (auto& t : v)
//         t.join();
// }

}  // namespace

MigratorApplication::MigratorApplication(util::Config const& config, std::string subCmd)
    : config_(config), subCmd_(std::move(subCmd))
{
    LOG(util::LogService::info()) << "Clio version: " << util::build::getClioFullVersionString();
    PrometheusService::init(config);
}

int
MigratorApplication::run()
{
    // auto const threads = config_.valueOr("io_threads", 2);
    // if (threads <= 0) {
    //     LOG(util::LogService::fatal()) << "io_threads is less than 1";
    //     return EXIT_FAILURE;
    // }
    // LOG(util::LogService::info()) << "Number of io threads = " << threads;

    // // IO context to handle all incoming requests, as well as other things.
    // // This is not the only io context in the application.
    // boost::asio::io_context ioc{threads};

    // // Interface to the database

    auto backend = data::make_Backend(config_);
    std::shared_ptr<MigrationManager> migrationManager = std::make_shared<MigrationManager>(backend);

    if (subCmd_ == "status") {
        auto const migratedFeatures =
            data::synchronous([&](auto yield) { return backend->fetchMigratedFeatures(yield); });
        if (not migratedFeatures) {
            LOG(util::LogService::fatal()) << "Could not fetch migrated features";
            return EXIT_FAILURE;
        }

        auto const allMigratorsStatus =
            data::synchronous([&](auto yield) { return migrationManager->allMigratorsStatus(yield); });

        for (auto const& [migrator, status] : allMigratorsStatus) {
            std::cout << "Migrator: " << migrator << " - " << (status ? "migrated" : "not migrated") << std::endl;
        }

    } else {
        LOG(util::LogService::fatal()) << "Unknown subcommand for migrator helper: " << subCmd_;
        return EXIT_FAILURE;
    }

    // std::this_thread::sleep_for(std::chrono::seconds(5));
    //  boost::asio::spawn(ioc, [&backend](boost::asio::yield_context yield) {
    //      auto range = backend->hardFetchLedgerRange(yield);
    //      if (not range) {
    //          LOG(util::LogService::fatal()) << "Could not fetch ledger range";
    //          return;
    //      }
    //      LOG(util::LogService::info()) << "Ledger range: " << range->minSequence << " - " << range->maxSequence;
    //  });

    // Blocks until stopped.
    // When stopped, shared_ptrs fall out of scope
    // Calls destructors on all resources, and destructs in order
    // start(ioc, threads);

    return EXIT_SUCCESS;
}

}  // namespace app
