//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>

#include <boost/asio/spawn.hpp>

namespace RPCng {
class TransactionEntryHandler
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    struct Output
    {
        uint32_t ledgerIndex;
        std::string ledgerHash;
        // TODO: use a better type for this
        boost::json::object metadata;
        boost::json::object tx;
        // validated should be sent via framework
        bool validated = true;
    };

    struct Input
    {
        std::string txHash;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
    };

    using Result = RPCng::HandlerReturnType<Output>;

    TransactionEntryHandler(
        std::shared_ptr<BackendInterface> const& sharedPtrBackend)
        : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    RpcSpecConstRef
    spec() const
    {
        static auto const rpcSpec = RpcSpec{
            {JS(tx_hash),
             validation::Required{},
             validation::Uint256HexStringValidator},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator}};
        return rpcSpec;
    }

    Result
    process(Input input, boost::asio::yield_context& yield) const;
};

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    TransactionEntryHandler::Output const& output);

TransactionEntryHandler::Input
tag_invoke(
    boost::json::value_to_tag<TransactionEntryHandler::Input>,
    boost::json::value const& jv);
}  // namespace RPCng
