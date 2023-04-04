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

namespace RPCng {

template <typename SubscriptionManagerType>
class BaseUnsubscribeHandler
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<SubscriptionManagerType> subscriptions_;

public:
    struct OrderBook
    {
        ripple::Book book;
        bool both = false;
    };

    using Output = VoidOutput;

    struct Input
    {
        std::optional<std::vector<std::string>> accounts;
        std::optional<std::vector<std::string>> streams;
        std::optional<std::vector<std::string>> accountsProposed;
        std::optional<std::vector<OrderBook>> books;
    };

    using Result = RPCng::HandlerReturnType<Output>;

    BaseUnsubscribeHandler(
        std::shared_ptr<BackendInterface> const& sharedPtrBackend,
        std::shared_ptr<SubscriptionManagerType> const& subscriptions)
        : sharedPtrBackend_(sharedPtrBackend), subscriptions_(subscriptions)
    {
    }

    RpcSpecConstRef
    spec() const
    {
        static auto const booksValidator =
            validation::CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
                if (!value.is_array())
                    return Error{RPC::Status{RPC::RippledError::rpcINVALID_PARAMS, std::string(key) + "NotArray"}};
                for (auto const& book : value.as_array())
                {
                    if (!book.is_object())
                        return Error{
                            RPC::Status{RPC::RippledError::rpcINVALID_PARAMS, std::string(key) + "ItemNotObject"}};
                    if (book.as_object().contains("both") && !book.as_object().at("both").is_bool())
                    {
                        return Error{RPC::Status{RPC::RippledError::rpcINVALID_PARAMS, "bothNotBool"}};
                    }
                    auto const parsedBook = RPC::parseBook(book.as_object());
                    if (auto const status = std::get_if<RPC::Status>(&parsedBook))
                        return Error(*status);
                }
                return MaybeError{};
            }};

        static auto const rpcSpec = RpcSpec{
            {JS(streams), validation::SubscibeStreamValidator},
            {JS(accounts), validation::SubscibeAccountsValidator},
            {JS(accounts_proposed), validation::SubscibeAccountsValidator},
            {JS(books), booksValidator}};

        return rpcSpec;
    }

    Result
    process(Input input, Context const& ctx) const
    {
        if (input.streams)
        {
            unsubscribeToStreams(*(input.streams), ctx.session);
        }
        if (input.accounts)
        {
            unsubscribeToAccounts(*(input.accounts), ctx.session);
        }
        if (input.accountsProposed)
        {
            unsubscribeToProposedAccounts(*(input.accountsProposed), ctx.session);
        }
        if (input.books)
        {
            unsubscribeToBooks(*(input.books), ctx.session);
        }
        return Output{};
    }

private:
    void
    unsubscribeToStreams(std::vector<std::string> const& streams, std::shared_ptr<WsBase> const& session) const
    {
        for (auto const& s : streams)
        {
            if (s == "ledger")
                subscriptions_->unsubLedger(session);
            else if (s == "transactions")
                subscriptions_->unsubTransactions(session);
            else if (s == "transactions_proposed")
                subscriptions_->unsubProposedTransactions(session);
            else if (s == "validations")
                subscriptions_->unsubValidation(session);
            else if (s == "manifests")
                subscriptions_->unsubManifest(session);
            else if (s == "book_changes")
                subscriptions_->unsubBookChanges(session);
            else
                assert(false);
        }
    }

    void
    unsubscribeToAccounts(std::vector<std::string> accounts, std::shared_ptr<WsBase> const& session) const
    {
        for (auto const& account : accounts)
        {
            auto accountID = RPC::accountFromStringStrict(account);
            subscriptions_->unsubAccount(*accountID, session);
        }
    }

    void
    unsubscribeToProposedAccounts(std::vector<std::string> accountsProposed, std::shared_ptr<WsBase> const& session)
        const
    {
        for (auto const& account : accountsProposed)
        {
            auto accountID = RPC::accountFromStringStrict(account);
            subscriptions_->unsubProposedAccount(*accountID, session);
        }
    }

    void
    unsubscribeToBooks(std::vector<OrderBook> const& books, std::shared_ptr<WsBase> const& session) const
    {
        for (auto const& orderBook : books)
        {
            subscriptions_->unsubBook(orderBook.book, session);
            if (orderBook.both)
                subscriptions_->unsubBook(ripple::reversed(orderBook.book), session);
        }
    }

    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv)
    {
        auto const& jsonObject = jv.as_object();
        Input input;
        if (auto const& streams = jsonObject.find(JS(streams)); streams != jsonObject.end())
        {
            input.streams = std::vector<std::string>();
            for (auto const& stream : streams->value().as_array())
                input.streams->push_back(stream.as_string().c_str());
        }
        if (auto const& accounts = jsonObject.find(JS(accounts)); accounts != jsonObject.end())
        {
            input.accounts = std::vector<std::string>();
            for (auto const& account : accounts->value().as_array())
                input.accounts->push_back(account.as_string().c_str());
        }
        if (auto const& accountsProposed = jsonObject.find(JS(accounts_proposed)); accountsProposed != jsonObject.end())
        {
            input.accountsProposed = std::vector<std::string>();
            for (auto const& account : accountsProposed->value().as_array())
                input.accountsProposed->push_back(account.as_string().c_str());
        }
        if (auto const& books = jsonObject.find(JS(books)); books != jsonObject.end())
        {
            input.books = std::vector<OrderBook>();
            for (auto const& book : books->value().as_array())
            {
                auto const& bookObject = book.as_object();
                OrderBook internalBook;
                if (auto const& both = bookObject.find(JS(both)); both != bookObject.end())
                    internalBook.both = both->value().as_bool();
                auto const parsedBookMaybe = RPC::parseBook(book.as_object());
                internalBook.book = std::get<ripple::Book>(parsedBookMaybe);
                input.books->push_back(internalBook);
            }
        }
        return input;
    }
};

using UnsubscribeHandler = BaseUnsubscribeHandler<SubscriptionManager>;

}  // namespace RPCng
