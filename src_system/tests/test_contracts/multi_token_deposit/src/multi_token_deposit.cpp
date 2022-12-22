
#include <multi_token_deposit/multi_token_deposit.hpp>
#include <contract_function.hpp>

namespace amax_xtoken {

#define CHECK(exp, msg) { if (!(exp)) eosio::check(false, msg); }
#ifndef ASSERT
    #define ASSERT(exp) eosio::CHECK(exp, #exp)
#endif

    using namespace amax;

    inline string to_string( const symbol& s ) {
        return std::to_string(s.precision()) + "," + s.code().to_string();
    }

    inline string to_string( const nasset& a ) {
        return "{\"amount\":" + std::to_string(a.amount) + ",\"symbol\":" + std::to_string(a.symbol.raw()) + "}";

    }

    inline string to_string( const std::vector<nasset>& assets ) {
        string s = "";
        for (const auto a : assets) {
            s += to_string(a) + ",";
        }
        return "[" + s + "]";
    }

    void multi_token_deposit::ontransfer()
    {
        auto token_contract = get_first_receiver();
        if (tokens.count(token_contract)) {
            execute_function(&multi_token_deposit::on_token_transfer);

        } else if (ntokens.count(token_contract)) {
            execute_function(&multi_token_deposit::on_ntoken_transfer);
        }
    }


    void multi_token_deposit::on_token_transfer(    const name &from,
                                                    const name &to,
                                                    const asset &quantity,
                                                    const string &memo)
    {
        print("token transfer: \n",
            "from: ", from, "\n",
            "to: ", to, "\n",
            "quantity: ", quantity.to_string(), "\n",
            "memo: ", memo, "\n"
        );
    }

    void multi_token_deposit::on_ntoken_transfer(   const name& from,
                                                    const name& to,
                                                    const std::vector<nasset>& assets,
                                                    const string& memo )
    {
        print("ntoken transfer: \n",
            "from: ", from, "\n",
            "to: ", to, "\n",
            "quantity: ", to_string(assets), "\n",
            "memo: ", memo, "\n"
        );
    }


} /// namespace amax_xtoken
