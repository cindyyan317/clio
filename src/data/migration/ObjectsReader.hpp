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

#include "data/migration/FullTableScaner.hpp"
#include "data/migration/MigrationCassandraBackend.hpp"

class ObjectsReader {
    using OnObjectRead = std::function<void(data::Blob const&, std::uint32_t)>;
    std::shared_ptr<MigrationCassandraBackend> backend_;
    OnObjectRead onObjectRead_;

    class TableObjectsDesc {
    public:
        using row = std::tuple<ripple::uint256, uint32_t, data::Blob>;
        using callback = std::function<void(row const&)>;
        static constexpr char const* partitionKey = "key";
        static constexpr char const* tableName = "objects";
    };

public:
    ObjectsReader(std::shared_ptr<MigrationCassandraBackend> backend, OnObjectRead onObjectRead)
        : backend_{std::move(backend)}, onObjectRead_{std::move(onObjectRead)}
    {
    }

    void
    onReadComplete(std::uint32_t sequence, data::Blob const& object)
    {
        // deserialize object
        onObjectRead_(object, sequence);
    }

    void
    fromTokenRange(TokenRange const& token, boost::asio::yield_context yield)
    {
        std::cout << "start: " << token.start << " end: " << token.end << std::endl;
        backend_->migrateInTokenRange<TableObjectsDesc>(
            token.start,
            token.end,
            [this](TableObjectsDesc::row const& row) {
                auto [key, seq, object] = row;
                this->onReadComplete(seq, object);
            },
            yield
        );
    }
};

class TransactionsReader {
    using OnTransactionRead = std::function<void(data::Blob const&, std::uint32_t)>;
    std::shared_ptr<MigrationCassandraBackend> backend_;
    OnTransactionRead onTransactionRead_;

public:
    TransactionsReader(std::shared_ptr<MigrationCassandraBackend> backend, OnTransactionRead onTransactionRead)
        : backend_{std::move(backend)}, onTransactionRead_{std::move(onTransactionRead)}
    {
    }

    void
    fromTokenRange(TokenRange const& token, boost::asio::yield_context)
    {
        std::cout << "start: " << token.start << " end: " << token.end << std::endl;
        // backend_->migrateObjectsInTokenRange(token.start, token.end, onReadComplete, yield);
    }

    void
    onReadComplete(std::uint32_t sequence, data::Blob const& object)
    {
        // deserialize object
        onTransactionRead_(object, sequence);
    }
};

using ObjectsScaner = FullTableScaner<ObjectsReader>;
using TransactionsScaner = FullTableScaner<TransactionsReader>;
