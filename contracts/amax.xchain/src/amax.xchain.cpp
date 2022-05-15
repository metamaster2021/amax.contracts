#include <amax.xchain/amax.xchain.hpp>

#include<utils.hpp>
#include<string>
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

ACTION xchain::reqxintoaddr( const name& applicant, const name& base_chain )
{
   require_auth( applicant );

   auto chain_info  = chain_t(base_chain); 
   check( _db.get(chain_info), "chain does not exist: " + base_chain.to_string() );

   auto idx = make128key(applicant.value, base_chain.value);
   // check(false ,  idx);
   account_xchain_address_t::idx_t xchaddrs( _self, _self.value );
   check( xchaddrs.find(idx) == xchaddrs.end(),  "the record already exists" );

   auto acct_xchain_addr            = account_xchain_address_t( applicant, base_chain );
   acct_xchain_addr.id              = xchaddrs.available_primary_key();
   acct_xchain_addr.created_at      = time_point_sec( current_time_point() );
   acct_xchain_addr.updated_at      = acct_xchain_addr.created_at;   

   if( chain_info.common_xin_account != "" ) { //for chain type like eos, amax
      acct_xchain_addr.status       = (uint8_t)address_status::CONFIGURED;
      acct_xchain_addr.xin_to       = to_string(acct_xchain_addr.id);
   }
   _db.set( acct_xchain_addr );
}

ACTION xchain::setaddress( const name& applicant, const name& base_chain, const string& xin_to ) 
{
   require_auth( _gstate.maker );

   check( xin_to.length() < max_addr_len, "illegal address" );

   auto chain_info =  chain_t( base_chain );
   check( _db.get(chain_info), "chain does not exist: " + base_chain.to_string() );

   auto acct_xchain_addr = account_xchain_address_t( applicant, base_chain );
   auto idx = acct_xchain_addr.by_accout_base_chain();

   account_xchain_address_t::idx_t  xchaddrs( _self, _self.value );
   auto xchaddr_ptr = xchaddrs.find( idx );
   check( xchaddr_ptr != xchaddrs.end(),  "the record does not exist" );
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
ACTION xchain::mkxinorder( const name& to, const name& chain_name, const symbol& coin_name, 
                           const string& txid, const string& xin_from, const string& xin_to,
                           const asset& quantity)
{
   require_auth( _gstate.maker );

   auto chain_coin = chain_coin_t(chain_name, coin_name);
   check( _db.get(chain_coin), "chain_coin does not exist: " + chain_coin.to_string());

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
 * tranfer amtoken(AMBTC/AMETH) event trigger
 * ontransfer, trigger by recipient of transfer()
 * @param quantity - mirrored asset on AMC
 * @param memo - memo format: $addr:$chain:coin_name:order_no:memo
 *                            "$eth_addr:eth:ETH,8:123:xchain's memo
 *               
 */
[[eosio::on_notify("amax.token::transfer")]] 
void xchain::ontransfer( name from, name to, asset quantity, string memo ) 
{
   eosio::print( "from: ", from, ", to:", to, ", quantity:" , quantity, ", memo:" , memo );

   if( _self == from ) return;
   if( to != _self ) return;
   if( get_first_receiver() == SYS_BANK ) return;

   auto parts = split( memo, ":" );
   check( parts.size() >= 4, "Expected format 'address@chain@coin_name@order_no@memo'" );
   auto xout_to      = parts[0];
   auto chain_name   = name( parts[1] );
   auto coin_name    = to_symbol((string)parts[2]);
   auto order_no     = parts[3];

   auto chain_coin = chain_coin_t( chain_name, coin_name );
   check( _db.get(chain_coin), "chain_coin does not exist: " + chain_coin.to_string() );

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
      row.fee			         = chain_coin.fee;  
      row.status			      = (uint8_t)xin_order_status::CREATED;
      row.maker               = from;
      row.created_at          = time_point_sec( current_time_point() );
      row.updated_at          = time_point_sec( current_time_point() );
      if( parts.size() == 5 ) row.memo = parts[4];
   });

   TRANSFER( XCHAIN_BANK, _gstate.fee_collector, chain_coin.fee,  to_string(id) );
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

void xchain::addchain( const name& chain, const bool is_basechain, const string& common_xin_account ) {
   require_auth( _self );

   auto chain_info = chain_t(chain);
   check( !_db.get(chain_info), "chain already exists: " + chain.to_string() );

   chain_info.is_basechain       = is_basechain;
   chain_info.common_xin_account = common_xin_account;
   _db.set( chain_info );
}

void xchain::delchain( const name& chain ) {
   require_auth( _self );
   auto chain_info = chain_t(chain);
   check( _db.get(chain_info), "chain does not exists: " + chain.to_string() );

   _db.del( chain_info );
}

void xchain::addcoin( const symbol& coin ) {
   require_auth( _self );

   auto coin_info = coin_t(coin);
   check( !_db.get(coin_info), "coin already exists: " + coin.code().to_string() );

   coin_info.coin = coin;
   _db.set( coin_info );
}

void xchain::delcoin( const symbol& coin ) {
   require_auth( _self );

   auto coin_info = coin_t(coin);
   check( _db.get(coin_info), "coin already exists: " + coin.code().to_string() );

   _db.del( coin_info );
}

void xchain::addchaincoin( const name& chain, const symbol& coin, const asset& fee ) {
   require_auth( _self );

   auto chain_coin = chain_coin_t(chain, coin);
   check( !_db.get(chain_coin), "chain_coin already exists: " + chain_coin.to_string());

   chain_coin.fee = fee;
   _db.set( chain_coin );
}

void xchain::delchaincoin( const name& chain, const symbol& coin ) {
   require_auth( _self );

   auto chain_coin = chain_coin_t(chain, coin);
   check( _db.get(chain_coin), "chain_coin does not exists: " + chain_coin.to_string());

   _db.del( chain_coin );
}

} /// namespace xchain
