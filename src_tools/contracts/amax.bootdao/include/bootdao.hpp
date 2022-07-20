#include "bootdao_db.hpp"

using namespace std;
using namespace wasm::db;

class [[eosio::contract("amax.bootdao")]] bootdao: public eosio::contract {
private:
    global_singleton    _global;
    global_t            _gstate;

public:
    using contract::contract;

    bootdao(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds),
        _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~bootdao() { _global.set( _gstate, get_self() ); }

public:
    ACTION init();
    ACTION recycle(const vector<name>& accounts);

private:
    void recycle_account(const name& account);
    asset get_balance(const name& bank, const symbol& symb, const name& account);

   struct accounts {
      asset balance;
      uint64_t primary_key() const {return balance.symbol.code().raw();}
   };
   typedef eosio::multi_index< name("accounts"), accounts > tbl_accounts;

}; //contract bootdao