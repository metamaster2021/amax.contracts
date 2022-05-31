#include <amax.fixswap/amax.fixswap.hpp>

namespace amax {


 /**
    * @brief create wallet or lock amount into mulsign wallet
    *
    * @param from
    * @param to
    * @param quantity
    * @param memo: 1) create:$m:$n:$title; 2) lock:$wallet_id
    */
   [[eosio::on_notify("*::transfer")]]
   void ontransfer(const name& from, const name& to, const asset& quantity, const string& memo) {
      CHECKC( from != to, err::ACCOUNT_INVALID,"cannot transfer to self" );
      CHECKC( quantity.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      CHECKC( memo != "", err::PARAM_ERROR, "empty memo!" )

      auto bank_contract = get_first_receiver();
   }


} //namespace amax