#include <amax.xchain/amax.xchain.hpp>

namespace amax {

ACTION token::init() {
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

ACTION token::ontransfer(name from, name to, asset quantity, string memo) {

}


} /// namespace apollo
