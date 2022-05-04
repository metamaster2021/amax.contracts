#include <amax.xchain/amax.xchain.hpp>

namespace amax {

ACTION token::init() {
   auto tokenstats = tokenstats_t(0);
   _db.del( tokenstats );

   // _gstate.initialized = true;

}

ACTION token::setaddress( const name& account, const name& base_chain, const string& address )
{
   require_auth( get_self() );
   check( address.length < 100, "illegal address" );

   auto xchaddr = account_xchain_address_t(base_chain);
   _db.get(xchaddr);
   xchaddr.address = address;
   _db.set(xchaddr)
}

ACTION token::createxin( const name& to, const name& chain, const string& txid, const asset& quantity )
{
  
}

ACTION token::transfer( const name& from, const name& to, const token_asset& quantity, const string& memo ) {
   check( from != to, "cannot transfer to self" );
   require_auth( from );
   check( is_account( to ), "to account does not exist");
   auto symid = quantity.symbid;
   auto token = tokenstats_t(symid);
   check( _db.get(token), "token asset not found: " + to_string(symid) );

   require_recipient( from );
   require_recipient( to );

   check( quantity.amount > 0, "must transfer positive quantity" );
   check( quantity.symbid == token.symbid, "symbol mismatch" );
   check( memo.size() <= 256, "memo has more than 256 bytes" );

   sub_balance( from, quantity );
   add_balance( to, quantity );
}

void token::add_balance( const name& owner, const token_asset& value ) {
   auto to_acnt = account_t(value.symbid);
   if (_db.get(owner.value, to_acnt)) {
      to_acnt.balance += value;
   } else {
      to_acnt.balance = value;
   }

   _db.set( owner.value, to_acnt );
}

void token::sub_balance( const name& owner, const token_asset& value ) {
   auto from_acnt = account_t(value.symbid);
   check( _db.get(owner.value, from_acnt), "account balance not found" );
   check( from_acnt.balance.amount >= value.amount, "overdrawn balance" );

   from_acnt.balance -= value;
   _db.set( owner.value, from_acnt );
}

ACTION token::setpowasset( const name& issuer, const uint64_t symbid, const pow_asset_meta& asset_meta) {
   require_auth( issuer );
   check( issuer == _gstate.admin, "non-admin issuer not allowed" );

   auto stats = tokenstats_t(symbid);
   check( _db.get(stats), "asset token not found: " + to_string(symbid) );

   auto pow = pow_asset_t(symbid);
   pow.asset_meta    = asset_meta;

   _db.set( pow );

}

} /// namespace apollo
