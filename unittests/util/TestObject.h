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

#include <backend/Types.h>

#include <ripple/ledger/ReadView.h>

#include <string_view>

/*
 * Create AccountID object with string
 */
[[nodiscard]] ripple::AccountID
GetAccountIDWithString(std::string_view id);

/*
 * Create a simple ledgerInfo object with only hash and seq
 */
[[nodiscard]] ripple::LedgerInfo
CreateLedgerInfo(std::string_view ledgerHash, ripple::LedgerIndex seq);

/*
 * Create a FeeSetting ledger object
 */
[[nodiscard]] ripple::STObject
CreateFeeSettingLedgerObject(
    uint64_t base,
    uint32_t reserveInc,
    uint32_t reserveBase,
    uint32_t refFeeUnit,
    uint32_t flag);

/*
 * Create a FeeSetting ledger object and return its blob
 */
[[nodiscard]] ripple::Blob
CreateFeeSettingBlob(
    uint64_t base,
    uint32_t reserveInc,
    uint32_t reserveBase,
    uint32_t refFeeUnit,
    uint32_t flag);

/*
 * Create a payment transaction object
 */
[[nodiscard]] ripple::STObject
CreatePaymentTransactionObject(
    std::string_view accountId1,
    std::string_view accountId2,
    int amount,
    int fee,
    uint32_t seq);

[[nodiscard]] ripple::STObject
CreatePaymentTransactionMetaObject(
    std::string_view accountId1,
    std::string_view accountId2,
    int finalBalance1,
    int finalBalance2);

/*
 * Create an account root ledger object
 */
[[nodiscard]] ripple::STObject
CreateAccountRootObject(
    std::string_view accountId,
    uint32_t flag,
    uint32_t seq,
    int balance,
    uint32_t ownerCount,
    std::string_view previousTxnID,
    uint32_t previousTxnSeq,
    uint32_t transferRate = 0);

/*
 * Create a createoffer treansaction
 * Taker pay is XRP
 */
[[nodiscard]] ripple::STObject
CreateCreateOfferTransactionObject(
    std::string_view accountId,
    int fee,
    uint32_t seq,
    std::string_view currency,
    std::string_view issuer,
    int takerGets,
    int takerPays);

/*
 * Return an issue object with given currency and issue account
 */
[[nodiscard]] ripple::Issue
GetIssue(std::string_view currency, std::string_view issuerId);

/*
 * Create a offer change meta data
 */
[[nodiscard]] ripple::STObject
CreateMetaDataForBookChange(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int perviousTakerGets,
    int finalTakerPays,
    int perviousTakerPays);

/*
 * Meta data for adding a offer object
 */
[[nodiscard]] ripple::STObject
CreateMetaDataForCreateOffer(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int finalTakerPays);

/*
 * Meta data for removing a offer object
 */
[[nodiscard]] ripple::STObject
CreateMetaDataForCancelOffer(
    std::string_view currency,
    std::string_view issueId,
    uint32_t transactionIndex,
    int finalTakerGets,
    int finalTakerPays);

/*
 * Create a owner dir ledger object
 */
[[nodiscard]] ripple::STObject
CreateOwnerDirLedgerObject(
    std::vector<ripple::uint256> indexes,
    std::string_view rootIndex);

/*
 * Create a payment channel ledger object
 */
[[nodiscard]] ripple::STObject
CreatePaymentChannelLedgerObject(
    std::string_view accountId,
    std::string_view destId,
    int amount,
    int balance,
    uint32_t settleDelay,
    std::string_view previousTxnId,
    uint32_t previousTxnSeq);

[[nodiscard]] ripple::STObject
CreateRippleStateLedgerObject(
    std::string_view accountId,
    std::string_view currency,
    std::string_view issuerId,
    int balance,
    std::string_view lowNodeAccountId,
    int lowLimit,
    std::string_view highNodeAccountId,
    int highLimit,
    std::string_view previousTxnId,
    uint32_t previousTxnSeq,
    uint32_t flag = 0);

ripple::STObject
CreateOfferLedgerObject(
    std::string_view account,
    int takerGets,
    int takerPays,
    std::string_view getsCurrency,
    std::string_view payssCurrency,
    std::string_view getsIssueId,
    std::string_view paysIssueId,
    std::string_view bookDirId);

ripple::STObject
CreateTicketLedgerObject(std::string_view rootIndex, uint32_t sequence);

ripple::STObject
CreateEscrowLedgerObject(std::string_view account, std::string_view dest);

ripple::STObject
CreateCheckLedgerObject(std::string_view account, std::string_view dest);

ripple::STObject
CreateDepositPreauthLedgerObject(
    std::string_view account,
    std::string_view auth);

[[nodiscard]] Backend::NFT
CreateNFT(
    std::string_view tokenID,
    std::string_view account,
    ripple::LedgerIndex seq = 1234u,
    ripple::Blob uri = ripple::Blob{'u', 'r', 'i'},
    bool isBurned = false);
