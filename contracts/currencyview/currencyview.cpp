#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

#include <string>

namespace amax {

using std::string;
using namespace eosio;

static constexpr name AMAX_BANK = "amax.token"_n;
static constexpr name APL_BANK = "aplink.token"_n;
static constexpr name CNYD_BANK = "cnyd.token"_n;

static constexpr symbol_code AMAX = symbol_code("AMAX");
static constexpr symbol_code APL = symbol_code("APL");
static constexpr symbol_code CNYD = symbol_code("CNYD");

class [[eosio::contract("currencyview")]] currencyview : public contract {
public:
   using contract::contract;

   ACTION view( const name& account ) {
      auto amax_bal = get_balance(AMAX_BANK, AMAX, account);
      auto apl_bal = get_balance(APL_BANK, APL, account);
      auto cnyd_bal = get_balance(CNYD_BANK, CNYD, account);

      check(false, amax_bal.to_string() + "," + apl_bal.to_string() + "," + cnyd_bal.to_string());
   }

   struct accounts {
      asset balance;
      uint64_t primary_key() const {return balance.symbol.code().raw();}
   };
   typedef eosio::multi_index< name("accounts"), accounts > tbl_accounts;

private:
   asset get_balance(const name& bank, const symbol_code& symbcode, const name& account) {
      tbl_accounts tmp(bank, account.value);
      auto itr = tmp.find(symbcode.raw());

      eosio::check(itr != tmp.end(), "The token doesn't exist in the token contract, or the account doesn't own any of these tokens");
      auto balance = itr->balance;
      return balance;
   }
};
}
