#include <amax.xchain/amax.xchain.hpp>
#include<utils.hpp>

namespace amax {

static constexpr eosio::name SYS_BANK{"amax.token"_n};
static constexpr eosio::name XCHAIN_BANK{"amax.amtoken"_n};

ACTION xchain::init( const name& admin, const name& maker, const name& checker, const name& fee_collector ) {
   require_auth( _self );
   
   _gstate.admin           = admin;
   _gstate.maker           = maker;
   _gstate.checker         = checker;
   _gstate.fee_collector   = fee_collector;
}

ACTION xchain::reqxintoaddr( const name& account, const name& base_chain )
{
   require_auth( account );

   auto type = _check_base_chain( base_chain );

   auto idx = (uint128_t)account.value << 64 || (uint128_t)base_chain.value;
   account_xchain_address_t::idx_t xchaddrs( _self, _self.value );
   check( xchaddrs.find(idx) == xchaddrs.end(),  "the record already exists" );

   auto xchaddr = account_xchain_address_t();
   xchaddr.id = xchaddrs.available_primary_key();
   xchaddr.account      = account;
   xchaddr.base_chain   = base_chain;
   xchaddr.created_at   = time_point_sec( current_time_point() );

   if( type != 0 ) {
      xchaddr.status = (uint8_t)address_status::PENDING;
   } else {
      xchaddr.status = (uint8_t)address_status::CONFIGURED;
      xchaddr.xin_to = to_string(xchaddr.id);
      xchaddr.updated_at = time_point_sec( current_time_point() );
   }
   _db.set( xchaddr );
}

ACTION xchain::setaddress( const name& account, const name& base_chain, const string& xin_to ) 
{
   require_auth( _gstate.maker );

   check( xin_to.length() < max_addr_len, "illegal address" );
   _check_base_chain( base_chain );

   auto idx = (uint128_t)account.value << 64 || (uint128_t)base_chain.value;

   account_xchain_address_t::idx_t  xchaddrs( _self, _self.value );
   auto xchaddr_ptr = xchaddrs.find( idx );
   
   check( xchaddr_ptr != xchaddrs.end(),  "the record not exists" );
   check( xchaddr_ptr->status != (uint8_t)address_status::CONFIGURED, "address already existed" );

   xchaddrs.modify( *xchaddr_ptr, _self, [&]( auto& row ) {
      row.status     = (uint8_t)address_status::CONFIGURED;
      row.xin_to     = xin_to;
      row.updated_at = time_point_sec( current_time_point() );
   });
}

/**
 * maker create xin order
 * */
ACTION xchain::mkxinorder( const name& to, const name& chain_name, const name& coin_name, 
                           const string& txid, const string& xin_from, const string& xin_to,
                           const asset& quantity)
{
   require_auth( _gstate.maker );

   _check_chain_coin( chain_name, coin_name );
   
   xin_order_t::idx_t xin_orders( _self, _self.value );
   auto txid_index 			   = xin_orders.get_index<"xintxids"_n>();
   check( txid_index.find( hash(txid) ) == txid_index.end() , "txid already existing!" );

   auto created_at = time_point_sec( current_time_point() );
   auto xin_order_id = xin_orders.available_primary_key();

   xin_orders.emplace( _self, [&]( auto& row ) {
      row.id 					= xin_order_id;
      row.txid 			   = txid;
      row.user_amacct      = to;
      row.xin_from         = xin_from;
      row.xin_to           = xin_to;
      row.chain 			   = chain_name;
      row.coin_name  	   = coin_name;
      row.quantity		   = quantity;
      row.status   			= (uint8_t)xin_order_status::CREATED;
      row.maker			   = _gstate.maker;
      row.created_at       = created_at;
      row.updated_at       = created_at;
   });

}

/**
 * checker to confirm xin order
 */
ACTION xchain::chkxinorder( const uint64_t& id )
{
   require_auth( _gstate.checker );

   xin_order_t::idx_t xin_orders( _self, _self.value );
   auto xin_order_itr = xin_orders.find( id );
   check( xin_order_itr != xin_orders.end(), "xin order not found: " + to_string(id) );
   auto status = xin_order_itr->status;
   check( status != (uint8_t)xin_order_status::CREATED, "xin order already closed: " + to_string(id) );

   xin_orders.modify( xin_order_itr, _self, [&]( auto& row ) {
      row.status         = (uint8_t)xin_order_status::FUFILLED;
      row.checker        = _gstate.checker;
      row.closed_at      = time_point_sec( current_time_point() );
      row.updated_at     = time_point_sec( current_time_point() );
   });
}


/**
 * checker cancel the xin order 
 * */
ACTION xchain::cslxinorder( const uint64_t& id, const string& cancel_reason )
{
   require_auth( _gstate.checker );

   xin_order_t::idx_t xin_orders( _self, _self.value );
   auto xin_order_itr = xin_orders.find( id );
   check( xin_order_itr != xin_orders.end(), "xin order not found: " + to_string(id) );
   auto status = xin_order_itr->status;
   check( (uint8_t)status != (uint8_t)xin_order_status::CREATED, "xin order already closed: " + to_string(id) );
   
   xin_orders.modify( xin_order_itr, _self, [&]( auto& row ) {
      row.status           = (uint8_t)xin_order_status::CANCELED;
      row.close_reason     = cancel_reason;
      row.checker          = _gstate.checker;
      row.closed_at        = time_point_sec( current_time_point() );
      row.updated_at       = time_point_sec( current_time_point() );
   });
}

/**
 * ontransfer, trigger by recipient of transfer()
 * @param quantity - mirrored asset on AMC
 * @param memo - memo format: $addr@$chain@coin_name@order_no@memo
 *               
 */
[[eosio::on_notify("*::transfer")]] 
void xchain::ontransfer( name from, name to, asset quantity, string memo ) 
{
   eosio::print( "from: ", from, ", to:", to, ", quantity:" , quantity, ", memo:" , memo );

   if( _self == from ) return;
   if( to != _self ) return;

   auto parts = split( memo, "@" );
   auto memo_detail =  "";
   check( parts.size() >= 4, "Expected format 'address@chain@coin_name@order_no'" );
   //check chain, coin_name
   auto xout_to = parts[0];
   auto chain_name = name( parts[1] );
   auto coin_name = name( parts[2] );
   auto order_no = parts[3];

   if( parts.size() == 5 ) {
      // memo_detail  = parts[4];
   }
   asset fee = _check_chain_coin( chain_name, coin_name );

   if( get_first_receiver() == SYS_BANK ) return;

   auto created_at = time_point_sec( current_time_point() );
   xout_order_t::idx_t xout_orders( _self, _self.value );
   auto id = xout_orders.available_primary_key();
   xout_orders.emplace( _self, [&]( auto& row ) {
      row.id 					   = id;
      row.xout_to 			   = xout_to;
      row.chain               = chain_name;
      row.coin_name           = coin_name;
      row.apply_amount		   = quantity;
      row.amount		         = quantity;
      row.fee			         = fee;  
      row.status			      = (uint8_t)xin_order_status::CREATED;
      row.memo			         = memo_detail;
      row.maker               = from;
      row.created_at          = time_point_sec( current_time_point() );
      row.updated_at          = time_point_sec( current_time_point() );
   });

   TRANSFER( XCHAIN_BANK, _gstate.fee_collector, fee,  to_string(id) );
}
/**
 * maker onpay the order
 * */
ACTION xchain::onpay( const name& account, const uint64_t& id, const string& txid, const string& payno, const string& xout_from )
{
   require_auth( _gstate.maker );

   xout_order_t::idx_t xout_orders( _self, _self.value );
   auto xout_order_itr = xout_orders.find( id );
   check( xout_order_itr != xout_orders.end(), "xout order not found: " + to_string(id) );
   auto status = xout_order_itr->status;
   check( status == (uint8_t)xout_order_status::CREATED,  "xout order status is not created: " + to_string(id));

   xout_orders.modify( *xout_order_itr, _self, [&]( auto& row ) {
      row.status     = (uint8_t)xout_order_status::PAYING;
      row.txid       = txid;
      row.xout_from  = xout_from;
      row.maker      = _gstate.maker;
      row.updated_at = time_point_sec( current_time_point() );
   });
}

/**
 * maker onpay the order
 * */
ACTION xchain::onpaysucc( const name& account, const uint64_t& id )
{
   require_auth( _gstate.maker );

   xout_order_t::idx_t xout_orders( _self, _self.value );
   auto xout_order_itr = xout_orders.find(id);
   check( xout_order_itr != xout_orders.end(), "xout order not found: " + to_string(id) );
   check( xout_order_itr->status == (uint8_t)xout_order_status::PAYING,  "xout order status is not paying");

   //check status
   xout_orders.modify( *xout_order_itr, _self, [&]( auto& row ) {
      row.status     = (uint8_t)xin_order_status::FUFILLED;
      row.updated_at = time_point_sec( current_time_point() );
   });
}

/**
 * checker to confirm out order
 */
ACTION xchain::chkxoutorder( const uint64_t& id )
{
   require_auth( _gstate.checker );

   xout_order_t::idx_t xout_orders( _self, _self.value );
   auto xout_order_itr = xout_orders.find( id );
   check( xout_order_itr != xout_orders.end(), "xout order not found: " + to_string(id) );

   //check status
   check( xout_order_itr->status == (uint8_t)xout_order_status::PAY_SUCCESS,  "xout order status is not pay_success" );

   xout_orders.modify( *xout_order_itr, _self, [&]( auto& row ) {
      row.status     = (uint8_t)xin_order_status::FUFILLED;
      row.closed_at  = time_point_sec( current_time_point() );
      row.updated_at = time_point_sec( current_time_point() );
      row.checker    = _gstate.checker;
   });
}

/**
 * maker or checker can cancel xchain out order
 */
ACTION xchain::cancelxout( const name& account, const uint64_t& id )
{
   require_auth( account );
   check( account == _gstate.checker || account == _gstate.maker, "account is not checker or taker" );

   xout_order_t::idx_t xout_orders( _self, _self.value );
   auto xout_order_itr = xout_orders.find( id );
   check( xout_order_itr != xout_orders.end(), "xout order not found: " + to_string(id) );
   check( xout_order_itr->status == (uint8_t)xout_order_status::PAY_SUCCESS ||
               xout_order_itr->status == (uint8_t)xout_order_status::PAYING 
               ,  "xout order status is not ready for cancel");

   xout_orders.modify( *xout_order_itr, _self, [&]( auto& row ) {
      row.status     = ( uint8_t )xin_order_status::FUFILLED;
      row.closed_at  = time_point_sec( current_time_point() );
      row.updated_at = time_point_sec( current_time_point() );   
      row.checker    = account;   
   });
}

uint8_t xchain::_check_base_chain( const name& chain ) {
   chain_t::idx_t chains( _self, _self.value );
   auto chain_itr = chains.find( chain.value );

   check( chain_itr != chains.end(), "chain not found: " + chain.to_string() );
   check( chain_itr->base_chain,  chain.to_string() + "is not base chain" );
   
   if( chain_itr->xin_account.empty() )
      return 0;
   else 
      return 1;
}

asset xchain::_check_chain_coin(const name& chain, const name& coin) {
   chain_coin_t::idx_t chain_coins( _self, _self.value );

   auto chain_coin_itr = chain_coins.find( chain.value << 32 | coin.value );

   check( chain_coin_itr != chain_coins.end(), "chain_coin not found: " + chain.to_string() + "_" + coin.to_string() );
   return chain_coin_itr->fee;
}

void xchain::addchain( const name& chain, const bool& base_chain, const string& xin_account ) {
   require_auth( _self );

   chain_t::idx_t chains( _self, _self.value );
   auto chain_itr = chains.find( chain.value );

   check( chain_itr == chains.end(), "chain already found: " + chain.to_string() );

   auto chain_info = chain_t();
   chain_info.chain = chain;
   chain_info.base_chain = base_chain;
   chain_info.xin_account = xin_account;
   _db.set( chain_info );
}

void xchain::delchain( const name& chain ) {
   require_auth( _self );

   chain_t::idx_t chains( _self, _self.value );
   auto chain_itr = chains.find( chain.value );

   check( chain_itr != chains.end(), "chain not found: " + chain.to_string() );
   chains.erase( chain_itr );
}

void xchain::addcoin( const name& coin ) {
   require_auth( _self );

   coin_t::idx_t coins( _self, _self.value );
   auto itr = coins.find( coin.value );

   check( itr == coins.end(), "coin already found: " + coin.to_string() );

   auto coin_info = coin_t();
   coin_info.coin = coin;
   _db.set( coin_info );
}

void xchain::delcoin( const name& coin ) {
   require_auth( _self );

   coin_t::idx_t coins( _self, _self.value );
   auto itr = coins.find( coin.value );

   check( itr != coins.end(), "coin not found: " + coin.to_string() );
   coins.erase( itr );
}

void xchain::addchaincoin( const name& chain, const name& coin, const asset& fee ) {
   require_auth( _self );

   chain_coin_t::idx_t chain_coins( _self, _self.value );

   auto chain_coin_itr = chain_coins.find( chain.value << 32 | coin.value );

   check( chain_coin_itr == chain_coins.end(), "chain_coin already exist found: " + chain.to_string() + "_" + coin.to_string());

   auto chain_coin = chain_coin_t();
   chain_coin.chain = chain;
   chain_coin.coin = coin;
   chain_coin.fee = fee;

   _db.set( chain_coin );
}

void xchain::delchaincoin( const name& chain, const name& coin ) {
   require_auth( _self );

   chain_coin_t::idx_t chain_coins( _self, _self.value );

   auto chain_coin_itr = chain_coins.find( chain.value << 32 | coin.value );

   check( chain_coin_itr != chain_coins.end(), "chain_coin is not existed: " + chain.to_string() + "_" + coin.to_string() );
   chain_coins.erase( chain_coin_itr );
}


} /// namespace xchain
