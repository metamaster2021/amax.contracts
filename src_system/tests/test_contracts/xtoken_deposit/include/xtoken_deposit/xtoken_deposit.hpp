#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

#include <string>

namespace amax_xtoken
{

    using std::string;
    using namespace eosio;

    #define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

    static constexpr symbol DEPOSIT_SYMBOL  = SYMBOL("CNY", 4);
    static constexpr name SYS_BANK          = "amax.xtoken"_n;
    static constexpr name ACTIVE_PERM       = "active"_n;

    /**
     * xtoken_deposit example for xtoken
     */
    class [[eosio::contract("xtoken_deposit")]] xtoken_deposit : public contract
    {
    public:
        using contract::contract;

        /**
         * Notify by transfer() of xtoken contract
         *
         * @param from - the account to transfer from,
         * @param to - the account to be transferred to,
         * @param quantity - the quantity of tokens to be transferred,
         * @param memo - the memo string to accompany the transaction.
         */
        [[eosio::on_notify("*::transfer")]] void ontransfer(const name &from,
                                        const name &to,
                                        const asset &quantity,
                                        const string &memo);

        /**
         * Notify by notifypayfee() of xtoken contact.
         *
         * @param from - the from account of transfer(),
         * @param to - the to account of transfer, fee payer,
         * @param fee_receiver - fee receiver,
         * @param fee - the fee of transfer to be payed,
         * @param memo - the memo of the transfer().
         */
        [[eosio::on_notify("*::notifypayfee")]] void onpayfee(const name &from, const name &to, const name& fee_receiver, const asset &fee, const string &memo);

        /**
         * Withdraw,
         *
         * @param owner - the account to be withdrawn from,
         * @param to - the account to withdraw to,
         * @param quantity - the quantity of tokens to withdraw,
         * @param memo - the memo string to accompany the transaction.
         */
        [[eosio::action]] void withdraw(const name& owner, const name& to, const asset &quantity, const string &memo);

        /**
         * Allows `ram_payer` to create an account `owner` with zero balance for
         * token `symbol` at the expense of `ram_payer`.
         *
         * @param owner - the account to be created,
         * @param symbol - the token to be payed with by `ram_payer`,
         * @param ram_payer - the account that supports the cost of this action.
         *
         */
        [[eosio::action]] void open(const name &owner, const symbol &symbol, const name &ram_payer);

        /**
         * This action is the opposite for open, it closes the account `owner`
         * for token `symbol`.
         *
         * @param owner - the owner account to execute the close action for,
         * @param symbol - the symbol of the token to execute the close action for.
         *
         * @pre The pair of owner plus symbol has to exist otherwise no action is executed,
         * @pre If the pair of owner plus symbol exists, the balance has to be zero.
         */
        [[eosio::action]] void close(const name &owner, const symbol &symbol);

        static asset get_balance(const name &token_contract_account, const name &owner, const symbol_code &sym_code)
        {
            accounts accountstable(token_contract_account, owner.value);
            const auto &ac = accountstable.get(sym_code.raw());
            return ac.balance;
        }

        using open_action = eosio::action_wrapper<"open"_n, &xtoken::open>;
        using close_action = eosio::action_wrapper<"close"_n, &xtoken::close>;
    private:
        struct [[eosio::table]] account
        {
            asset balance;

            uint64_t primary_key() const { return balance.symbol.code().raw(); }
        };

        typedef eosio::multi_index<"accounts"_n, account> accounts;

        void sub_balance(const name &owner, const asset &value, const name &ram_payer);
        void add_balance(const name &owner, const asset &value, const name &ram_payer);

        bool open_account(const name &owner, const symbol &symbol, const name &ram_payer);
    };

}
