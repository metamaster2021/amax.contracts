#include <amax.xchain/amax.xchain.hpp>

namespace amax {

ACTION xchain::reqxintoaddr( const name& account, const name& base_chain )
{
   require_auth( account );

   check( _gstate.base_chains.count(quantity.symbol) != 0, "base chain is not allowed for xchain in" );
   
   auto idx = (uint128_t)account.value << 64 || (uint128_t)base_chain.value;
   auto xchaddr_idx = account_xchain_address_t:::idx_t(_self, _self.value);
   check( xchaddr_idx.find(idx) == xchaddr_idx.end(),  "the record already exists" );

   auto xchaddr = account_xchain_address_t();
   xchaddr.id = xchaddr_idx::available_primary_key();
   xchaddr.account = account;
   xchaddr.base_chain = base_chain;
   xchaddr.created_at = time_point_sec(current_time_point());

   if( _gstate.account_chains.count(quantity.symbol) != 0 ) {
      xchaddr.status = "pending"_n;
   } else {
      xchaddr.status = "initialized"_n;
      xchaddr.xin_to = str(xchaddr.id);
      xchaddr.updated_at = time_point_sec(current_time_point());
   }
   _db.set(account, xchaddr);
}

ACTION xchain::setaddress( const name& account, const name& base_chain, const string& xin_to ) 
{
   require_auth( _gstate.maker );

   check( xin_to.length() < max_addr_len, "illegal address" );
   check( _gstate.base_chains.count(quantity.symbol) != 0, "base chain is not allowed for xchain in" );

   auto idx = (uint128_t)account.value << 64 || (uint128_t)base_chain.value;
   auto xchaddr_idx = account_xchain_address_t:::idx_t(_self, _self.value);
   auto xchaddr_ptr = xchaddr_idx.find(idx)
   check( xchaddr_ptr != xchaddr_idx.end(),  "the record not exists" );
   check( xchaddr_ptr->status == "pending"_n, "address already existed");

   xchaddr.xin_to = xin_to;
   _db.set(xchaddr);
}

ACTION xchain::mkxinorder( const name& to, const name& chain, const string& txid, const string& order_no, const asset& quantity)
{

}

/**
 * checker to confirm xin order
 */
ACTION xchain::chkxinorder( const name& account, const uint64_t& id, const string& txid, const asset& quantity )
{
   
}

ACTION xchain::cancelorder( const name& account, const uint_64& id, const string& xin_to, const string& cancel_reason )
{

}

/**
 * ontransfer, trigger by recipient of transfer()
 * @param quantity - mirrored asset on AMC
 * @param memo - memo format: $addr@$chain@coin_name&order_no
 *               
 */
[[eosio::on_notify("*::transfer")]] 
void xchain::ontransfer(name from, name to, asset quantity, string memo) 
{

}

ACTION xchain::onpaying( const name& account, const uint64_t& id, const string& txid, const asset& quantity )
{

}

ACTION xchain::onpaysucc( const name& account, const uint64_t& id, const string& payno, const asset& quantity )
{

}

/**
 * checker to confirm out order
 */
ACTION xchain::chkxoutorder( const name& account, const uint64_t& id, const string& txid, const asset& quantity )
{

}

ACTION xchain::cancelxout( const name& account, const uint64_t& id, const string& payno, const asset& quantity )
{

}

void xchain::_check_base_chain(const asset& quantity) {

}

} /// namespace apollo
