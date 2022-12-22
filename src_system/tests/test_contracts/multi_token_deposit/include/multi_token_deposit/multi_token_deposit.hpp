#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

#include <string>

namespace amax_xtoken
{

    using std::string;
    using namespace eosio;

    #define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

    static constexpr name ACTIVE_PERM       = "active"_n;

    struct nsymbol {
        uint32_t id;
        uint32_t parent_id;

        nsymbol() {}
        nsymbol(const uint32_t& i): id(i),parent_id(0) {}
        nsymbol(const uint32_t& i, const uint32_t& pid): id(i), parent_id(pid) {}

        friend bool operator==(const nsymbol&, const nsymbol&);
        bool is_valid()const { return( id > parent_id ); }
        uint64_t raw()const { return( (uint64_t) parent_id << 32 | id ); }

        EOSLIB_SERIALIZE( nsymbol, (id)(parent_id) )
    };

    bool operator==(const nsymbol& symb1, const nsymbol& symb2) {
        return( symb1.id == symb2.id && symb1.parent_id == symb2.parent_id );
    }

    struct nasset {
        int64_t         amount;
        nsymbol         symbol;

        nasset() {}
        nasset(const uint32_t& id): symbol(id), amount(0) {}
        nasset(const uint32_t& id, const uint32_t& pid): symbol(id, pid), amount(0) {}
        nasset(const uint32_t& id, const uint32_t& pid, const int64_t& am): symbol(id, pid), amount(am) {}
        nasset(const int64_t& amt, const nsymbol& symb): amount(amt), symbol(symb) {}

        nasset& operator+=(const nasset& quantity) {
            check( quantity.symbol.raw() == this->symbol.raw(), "nsymbol mismatch");
            this->amount += quantity.amount; return *this;
        }
        nasset& operator-=(const nasset& quantity) {
            check( quantity.symbol.raw() == this->symbol.raw(), "nsymbol mismatch");
            this->amount -= quantity.amount; return *this;
        }

        bool is_valid()const { return symbol.is_valid(); }

        EOSLIB_SERIALIZE( nasset, (amount)(symbol) )
    };

    static const std::set<eosio::name> tokens = {
        "amax.token"_n
    };

    static const std::set<eosio::name> ntokens = {
        "amax.ntoken"_n
    };

    /**
     * multi_token_deposit example for xtoken
     */
    class [[eosio::contract("multi_token_deposit")]] multi_token_deposit : public contract
    {
    public:
        using contract::contract;

        /**
         * Notify by transfer() of token contract
         *
         */
        [[eosio::on_notify("*::transfer")]] void ontransfer();


    private:
        void on_token_transfer(     const name &from,
                                    const name &to,
                                    const asset &quantity,
                                    const string &memo);

        void on_ntoken_transfer(    const name& from,
                                    const name& to,
                                    const std::vector<nasset>& assets,
                                    const string& memo );
    };

}
