#include <amax.xchain/amax.xchain.hpp>

namespace amax {

ACTION xchain::init() {
   // _gstate.initialized = true;

}

ACTION xchain::setaddress( const name& account, const name& base_chain, const string& address )
{
   require_auth( get_self() );
   check( address.length() < 100, "illegal address" );

   auto xchaddr = account_xchain_address_t(base_chain);
   _db.get(xchaddr);
   xchaddr.xin_to = address;
   _db.set(xchaddr);
}

ACTION xchain::createxin( const name& to, const name& chain, const string& txid, const asset& quantity )
{
  
}

ACTION xchain::ontransfer(name from, name to, asset quantity, string memo) {

}


} /// namespace apollo
