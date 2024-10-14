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
#include "etl/ETLHelpers.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>

class FullTableScaner {
private:
    util::async::AnyExecutionContext ctx_;
    std::string table_;
    std::shared_ptr<data::BackendInterface> backend_;
    std::function<void(std::int64_t, std::int64_t)> callback_;
    etl::ThreadSafeQueue<std::tuple<std::int64_t, std::int64_t>> queue_;
    std::vector<util::async::AnyOperation<void>> tasks_;

public:
    template <typename CtxType>
    FullTableScaner(
        CtxType& ctx,
        std::string table_name,
        std::shared_ptr<data::BackendInterface> backend,
        std::function<void(std::int64_t, std::int64_t)> callback,
        std::vector<std::tuple<std::int64_t, std::int64_t>> const& cursors

    )
        : ctx_(ctx)
        , table_(std::move(table_name))
        , backend_(std::move(backend))
        , callback_(std::move(callback))
        , queue_{cursors.size()}
    {
        std::ranges::for_each(cursors, [this](auto const& cursor) { queue_.push(cursor); });
        load(10);
    }

private:
    [[nodiscard]] auto
    spawnWorker()
    {
        return ctx_.execute([this](auto token) {
            while (not token.isStopRequested()) {
                auto cursor = queue_.tryPop();
                if (not cursor.has_value()) {
                    return;  // queue is empty
                }

                auto [start, end] = cursor.value();
                // access the db

                callback_(data_row);
            }
        });
    }

    void
    load(size_t workerNum)
    {
        namespace vs = std::views;

        tasks_.reserve(workerNum);

        for ([[maybe_unused]] auto taskId : vs::iota(0u, workerNum))
            tasks_.push_back(spawnWorker());
    }
};
