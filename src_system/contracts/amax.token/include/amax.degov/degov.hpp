#include <eosio/eosio.hpp>

#include <eosio/system.hpp>
#include <eosio/time.hpp>

using namespace std;
using namespace eosio;

static constexpr name degov_contract     {"amax.degov"_n};

class [[eosio::contract("amax.degov")]] degov: public eosio::contract {

public:
  using contract::contract;

  degov(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds) {}

public:
  static bool is_blacklisted( const name& account, const name& contract )
  {
    blacklist_t::tbl_t blacklist( contract, contract.value );
    return (blacklist.find(account.value) != blacklist.end());
  }  

  /**
  * blacklist table.
  * scope: contract self
  */
  struct blacklist_t {
      name            account;
      string          reason;
      name            degover;
      time_point_sec  created_at;
      
      blacklist_t(){}
      blacklist_t(const name& account): account(account) {}
      
      uint64_t primary_key() const { return account.value; }

      typedef eosio::multi_index<"blacklist"_n, blacklist_t> tbl_t;
      EOSLIB_SERIALIZE( blacklist_t, (account)(reason)(degover)(created_at) )

  };
};