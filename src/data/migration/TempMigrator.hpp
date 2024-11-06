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
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include "data/migration/BaseMigrator.hpp"
#include "data/migration/FullTableScaner.hpp"
#include "data/migration/MigrationCassandraBackend.hpp"
#include "data/migration/ObjectsReader.hpp"
// #include "data/migration/FullTableScaner.hpp"

class TempMigrator : public BaseMigrator {
public:
    std::string
    toString() const override
    {
        return "TempMigrator";
    }

    std::string
    name() const override
    {
        return "TempMigrator";
    }

    bool
    blockIfNotMigrated() const override
    {
        return true;
    }

    void
    runMigration(std::shared_ptr<MigrationCassandraBackend> backend) override
    {
        ObjectsScaner scanner(
            2,
            4,
            TokenRangesProvider(1000).getRanges(),
            ObjectsReader(
                backend,
                [](data::Blob const& object, std::uint32_t sequence) {
                    std::cout << "sequence: " << sequence << " object: " << object.size() << std::endl;
                }
            )
        );

        scanner.wait();
    }
};
