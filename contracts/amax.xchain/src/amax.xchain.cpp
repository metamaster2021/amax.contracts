#include <amax.xchain/amax.xchain.hpp>

namespace amax {

ACTION xchain::reqxintoaddr( const name& account, const name& base_chain )
{

}

ACTION xchain::setaddress( const name& account, const name& base_chain, const string& xin_to ) 
{
   require_auth( get_self() );
   check( address.length() < 100, "illegal address" );

   auto xchaddr = account_xchain_address_t(base_chain);
   _db.get(xchaddr);
   xchaddr.xin_to = address;
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

} /// namespace apollo
