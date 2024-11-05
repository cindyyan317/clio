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
#include "util/async/context/BasicExecutionContext.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

struct TokenRange {
    std::int64_t start;
    std::int64_t end;

    TokenRange(std::int64_t start, std::int64_t end) : start{start}, end{end}
    {
    }
};

struct TokenRangesProvider {
    uint32_t numRanges_;

    TokenRangesProvider(uint32_t numRanges) : numRanges_{numRanges}
    {
    }

    [[nodiscard]] std::vector<TokenRange>
    getRanges() const
    {
        auto const minValue = std::numeric_limits<std::int64_t>::min();
        auto const maxValue = std::numeric_limits<std::int64_t>::max();

        // Safely calculate the range size using uint64_t to avoid overflow
        uint64_t rangeSize = (static_cast<uint64_t>(maxValue) * 2) / numRanges_;

        std::vector<TokenRange> ranges;
        for (std::uint32_t i = 0; i < numRanges_; ++i) {
            int64_t start = minValue + i * static_cast<int64_t>(rangeSize);
            int64_t end = (i == numRanges_ - 1) ? maxValue : start + static_cast<int64_t>(rangeSize) - 1;
            ranges.emplace_back(start, end);
        }

        return ranges;
    }
};

template <typename TableReader>
class FullTableScaner {
private:
    util::async::AnyExecutionContext ctx_;
    etl::ThreadSafeQueue<TokenRange> queue_;
    std::vector<util::async::AnyOperation<void>> tasks_;
    TableReader reader_;

public:
    template <typename ExecutionContextType = util::async::CoroExecutionContext>
    FullTableScaner(
        std::uint32_t ctxThreadsNum,
        std::uint32_t workersNum,
        std::vector<TokenRange> const& cursors,
        TableReader&& reader
    )
        : ctx_(ExecutionContextType(ctxThreadsNum)), queue_{cursors.size()}, reader_{std::move(reader)}
    {
        std::ranges::for_each(cursors, [this](auto const& cursor) { queue_.push(cursor); });
        load(workersNum);
    }

    void
    wait()
    {
        for (auto& task : tasks_) {
            task.wait();
        }
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

                reader_.fromTokenRange(cursor.value(), token);
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
