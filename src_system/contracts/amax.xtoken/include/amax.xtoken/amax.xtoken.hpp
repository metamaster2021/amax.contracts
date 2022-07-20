#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

#include <string>

namespace amax_xtoken
{

    using std::string;
    using namespace eosio;

    /**
     * The `amax.xtoken` sample system contract defines the structures and actions that allow users to create, issue, and manage tokens for AMAX based blockchains. It demonstrates one way to implement a smart contract which allows for creation and management of tokens. It is possible for one to create a similar contract which suits different needs. However, it is recommended that if one only needs a token with the below listed actions, that one uses the `amax.xtoken` contract instead of developing their own.
     *
     * The `amax.xtoken` contract class also implements two useful public static methods: `get_supply` and `get_balance`. The first allows one to check the total supply of a specified token, created by an account and the second allows one to check the balance of a token for a specified account (the token creator account has to be specified as well).
     *
     * The `amax.xtoken` contract manages the set of tokens, accounts and their corresponding balances, by using two internal multi-index structures: the `accounts` and `stats`. The `accounts` multi-index table holds, for each row, instances of `account` object and the `account` object holds information about the balance of one token. The `accounts` table is scoped to an eosio account, and it keeps the rows indexed based on the token's symbol.  This means that when one queries the `accounts` multi-index table for an account name the result is all the tokens that account holds at the moment.
     *
     * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each row, which contains information about current supply, maximum supply, and the creator account for a symbol token. The `stats` table is scoped to the token symbol.  Therefore, when one queries the `stats` table for a token symbol the result is one single entry/row corresponding to the queried symbol token if it was previously created, or nothing, otherwise.
     * The `amax.xtoken` is base on `amax.token`, support fee of transfer
     */
    class [[eosio::contract("amax.xtoken")]] xtoken : public contract
    {
    public:
        using contract::contract;

        static constexpr uint64_t RATIO_BOOST = 10000;

         static constexpr eosio::name active_permission{"active"_n};

        /**
         * Allows `issuer` account to create a token in supply of `maximum_supply`. If validation is successful a new entry in statstable for token symbol scope gets created.
         *
         * @param issuer - the account that creates the token,
         * @param maximum_supply - the maximum supply set for the token created.
         *
         * @pre Token symbol has to be valid,
         * @pre Token symbol must not be already created,
         * @pre maximum_supply has to be smaller than the maximum supply allowed by the system: 1^62 - 1.
         * @pre Maximum supply must be positive;
         */
        [[eosio::action]] void create(const name &issuer,
                                      const asset &maximum_supply);
        /**
         *  This action issues to `to` account a `quantity` of tokens.
         *
         * @param to - the account to issue tokens to, it must be the same as the issuer,
         * @param quntity - the amount of tokens to be issued,
         * @memo - the memo string that accompanies the token issue transaction.
         */
        [[eosio::action]] void issue(const name &to, const asset &quantity, const string &memo);

        /**
         * The opposite for create action, if all validations succeed,
         * it debits the statstable.supply amount.
         *
         * @param quantity - the quantity of tokens to retire,
         * @param memo - the memo string to accompany the transaction.
         */
        [[eosio::action]] void retire(const asset &quantity, const string &memo);

        /**
         * Allows `from` account to transfer to `to` account the `quantity` tokens.
         * One account is debited and the other is credited with quantity tokens.
         *
         * @param from - the account to transfer from,
         * @param to - the account to be transferred to,
         * @param quantity - the quantity of tokens to be transferred,
         * @param memo - the memo string to accompany the transaction.
         */
        [[eosio::action]] void transfer(const name &from,
                                        const name &to,
                                        const asset &quantity,
                                        const string &memo);

        /**
         * Notify pay fee.
         * Must be Triggered as inline action by transfer()
         *
         * @param from - the from account of transfer(),
         * @param to - the to account of transfer, fee payer,
         * @param fee_receiver - fee receiver,
         * @param fee - the fee of transfer to be payed,
         * @param memo - the memo of the transfer().
         * Require contract auth
         */
        [[eosio::action]] void notifypayfee(const name &from, const name &to, const name& fee_receiver, const asset &fee, const string &memo);

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

        /**
         * Set token fee ratio
         *
         * @param symbol - the symbol of the token.
         * @param fee_ratio - fee ratio, boost 10000.
         */
        [[eosio::action]] void feeratio(const symbol &symbol, uint64_t fee_ratio);


        /**
         * Set token fee receiver
         *
         * @param symbol - the symbol of the token.
         * @param fee_receiver - fee receiver.
         */
        [[eosio::action]] void feereceiver(const symbol &symbol, const name &fee_receiver);

        /**
         * Set token min fee quantity
         *
         * @param symbol - the symbol of the token.
         * @param min_fee_quantity - min fee quantity.
         */
        [[eosio::action]] void minfee(const symbol &symbol, const asset &min_fee_quantity);

        /**
         * set account `is fee exempt`
         * @param symbol - the symbol of the token.
         * @param account - account name.
         * @param is_fee_exempt - is account fee exempt. if true, there is no fee for the account
         */
        [[eosio::action]] void feeexempt(const symbol &symbol, const name &account, bool is_fee_exempt);

        /**
         * Pause token
         * If token is paused, users can not do actions: transfer(), open(), close(),
         * @param symbol - the symbol of the token.
         * @param is_paused - is paused.
         */
        [[eosio::action]] void pause(const symbol &symbol, bool is_paused);

        /**
         * freeze account
         * If account of token is frozen, it can not do actions: transfer(), close(),
         * @param symbol - the symbol of the token.
         * @param account - account name.
         * @param is_frozen - is account frozen.
         */
        [[eosio::action]] void freezeacct(const symbol &symbol, const name &account, bool is_frozen);

        static asset get_supply(const name &token_contract_account, const symbol_code &sym_code)
        {
            stats statstable(token_contract_account, sym_code.raw());
            const auto &st = statstable.get(sym_code.raw());
            return st.supply;
        }

        static asset get_balance(const name &token_contract_account, const name &owner, const symbol_code &sym_code)
        {
            accounts accountstable(token_contract_account, owner.value);
            const auto &ac = accountstable.get(sym_code.raw());
            return ac.balance;
        }

        using create_action = eosio::action_wrapper<"create"_n, &xtoken::create>;
        using issue_action = eosio::action_wrapper<"issue"_n, &xtoken::issue>;
        using retire_action = eosio::action_wrapper<"retire"_n, &xtoken::retire>;
        using transfer_action = eosio::action_wrapper<"transfer"_n, &xtoken::transfer>;
        using notifypayfee_action = eosio::action_wrapper<"notifypayfee"_n, &xtoken::notifypayfee>;
        using open_action = eosio::action_wrapper<"open"_n, &xtoken::open>;
        using close_action = eosio::action_wrapper<"close"_n, &xtoken::close>;
        using feeratio_action = eosio::action_wrapper<"feeratio"_n, &xtoken::feeratio>;
        using feereceiver_action = eosio::action_wrapper<"feereceiver"_n, &xtoken::feereceiver>;
        using minfee_action = eosio::action_wrapper<"minfee"_n, &xtoken::minfee>;
        using feewhitelist_action = eosio::action_wrapper<"feeexempt"_n, &xtoken::feeexempt>;
        using pause_action = eosio::action_wrapper<"pause"_n, &xtoken::pause>;
        using freezeacct_action = eosio::action_wrapper<"freezeacct"_n, &xtoken::freezeacct>;

    private:
        struct [[eosio::table]] account
        {
            asset balance;
            bool  is_frozen = false;
            bool  is_fee_exempt = false;

            uint64_t primary_key() const { return balance.symbol.code().raw(); }
        };

        struct [[eosio::table]] currency_stats
        {
            asset supply;
            asset max_supply;
            name issuer;
            bool is_paused = false;
            name fee_receiver;              // fee receiver
            uint64_t fee_ratio = 0;         // fee ratio, boost 10000
            asset min_fee_quantity;         // min fee quantity

            uint64_t primary_key() const { return supply.symbol.code().raw(); }
        };

        typedef eosio::multi_index<"accounts"_n, account> accounts;
        typedef eosio::multi_index<"stat"_n, currency_stats> stats;

        template <typename Field, typename Value>
        void update_currency_field(const symbol &symbol, const Value &v, Field currency_stats::*field,
                                   currency_stats *st_out = nullptr);

        void sub_balance(const currency_stats &st, const name &owner, const asset &value,
                         bool is_check_frozen = false);
        void add_balance(const currency_stats &st, const name &owner, const asset &value,
                         const name &ram_payer, bool is_check_frozen = false);

        inline bool is_account_frozen(const currency_stats &st, const name &owner, const account &acct) const {
            return acct.is_frozen && owner != st.issuer;
        }

        bool open_account(const name &owner, const symbol &symbol, const name &ram_payer);
    };

}
