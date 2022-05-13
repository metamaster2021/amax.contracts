#include <amax.xchain/amax.xchain.hpp>
#include<utils.hpp>

namespace amax {

static constexpr eosio::name SYS_BANK{"eosio.token"_n};


ACTION xchain::reqxintoaddr( const name& account, const name& base_chain )
{
   require_auth( account );

   auto type = _check_base_chain(base_chain);

   auto idx = (uint128_t)account.value << 64 || (uint128_t)base_chain.value;
   account_xchain_address_t::idx_t xchaddrs(_self, _self.value);
   check( xchaddrs.find(idx) == xchaddrs.end(),  "the record already exists" );

   auto xchaddr = account_xchain_address_t();
   xchaddr.id = xchaddrs.available_primary_key();
   xchaddr.account = account;
   xchaddr.base_chain = base_chain;
   xchaddr.created_at = time_point_sec(current_time_point());

   if( type != 0 ) {
      xchaddr.status = (uint8_t)address_status::PENDING;
   } else {
      xchaddr.status = (uint8_t)address_status::CONFIGURED;
      xchaddr.xin_to = to_string(xchaddr.id);
      xchaddr.updated_at = time_point_sec(current_time_point());
   }
   _db.set(account, xchaddr);
}

ACTION xchain::setaddress( const name& account, const name& base_chain, const string& xin_to ) 
{
   require_auth( _gstate.maker );

   check( xin_to.length() < max_addr_len, "illegal address" );
   _check_base_chain(base_chain);

   auto idx = (uint128_t)account.value << 64 || (uint128_t)base_chain.value;

   account_xchain_address_t::idx_t  xchaddrs(_self, _self.value);
   auto xchaddr_ptr = xchaddrs.find(idx);
   
   check( xchaddr_ptr != xchaddrs.end(),  "the record not exists" );
   check( xchaddr_ptr->status != (uint8_t)address_status::CONFIGURED, "address already existed");

   xchaddrs.modify( *xchaddr_ptr, _self, [&]( auto& row ) {
      row.status = (uint8_t)address_status::CONFIGURED;
      row.xin_to = xin_to;
      row.updated_at = time_point_sec(current_time_point());
   });
}

ACTION xchain::mkxinorder( const name& to, const name& chain_name, const name& coin_name, 
                           const string& txid, const string& xin_from, const string& xin_to,
                           const asset& quantity)
{
   require_auth( _gstate.maker );

   _check_chain_coin(chain_name, coin_name);
   
   xin_order_t::idx_t xin_orders(_self, _self.value);
   auto txid_index 			   = xin_orders.get_index<"xintxids"_n>();
   check( txid_index.find(hash(txid)) == txid_index.end() , "txid already existing!" );

   auto created_at = time_point_sec(current_time_point());
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
ACTION xchain::chkxinorder( const name& account, const uint64_t& id )
{
   require_auth( _gstate.maker );

   xin_order_t::idx_t xin_orders(_self, _self.value);
   auto xin_order_itr = xin_orders.find(id);
   check( xin_order_itr != xin_orders.end(), "xin order not found: " + to_string(id) );
   auto status = xin_order_itr->status;
   check(status != (uint8_t)xin_order_status::CREATED, "xin order already closed: " + to_string(id) );

   xin_orders.modify(xin_order_itr, _self, [&]( auto& row ) {
      row.status         = (uint8_t)xin_order_status::FUFILLED;
      row.closed_at      = time_point_sec(current_time_point());
      row.updated_at     = time_point_sec(current_time_point());
   });
}

ACTION xchain::cancelorder( const name& account, const uint64_t& id, const string& cancel_reason )
{
   require_auth( _gstate.maker);

   xin_order_t::idx_t xin_orders(_self, _self.value);
   auto xin_order_itr = xin_orders.find(id);
   check( xin_order_itr != xin_orders.end(), "xin order not found: " + to_string(id) );
   auto status = xin_order_itr->status;
   check( (uint8_t)status != (uint8_t)xin_order_status::CREATED, "xin order already closed: " + to_string(id) );
   
   xin_orders.modify(xin_order_itr, _self, [&]( auto& row ) {
      row.status           = (uint8_t)xin_order_status::CANCELED;
      row.close_reason     = cancel_reason;
      row.closed_at        = time_point_sec(current_time_point());
      row.updated_at       = time_point_sec(current_time_point());
   });
}

/**
 * ontransfer, trigger by recipient of transfer()
 * @param quantity - mirrored asset on AMC
 * @param memo - memo format: $addr@$chain@coin_name@order_no@memo
 *               
 */
[[eosio::on_notify("*::transfer")]] 
void xchain::ontransfer(name from, name to, asset quantity, string memo) 
{
   eosio::print("from: ", from, ", to:", to, ", quantity:" , quantity, ", memo:" , memo);

   if( _self == from ) return;
   if( to != _self ) return;

   auto parts = split(memo, "@");
   auto memo_detail =  "";
   check(parts.size() >= 4, "Expected format 'address@chain@coin_name@order_no'");
   //check chain, coin_name
   auto xout_to = parts[0];
   auto chain_name = name(parts[1]);
   auto coin_name = name(parts[2]);
   auto order_no = parts[3];

   if(parts.size() == 5) {
      // memo_detail = parts[4];
   }
   
   if (get_first_receiver() == SYS_BANK) return;

   auto created_at = time_point_sec(current_time_point());
   xout_order_t::idx_t xout_orders(_self, _self.value);
   auto id = xout_orders.available_primary_key();
   xout_orders.emplace( _self, [&]( auto& row ) {
      row.id 					   = id;
      row.xout_to 			   = xout_to;
      row.chain               = chain_name;
      row.coin_name           = coin_name;
      row.apply_amount		   = quantity;
      row.amount		         = quantity;
      //   row.fee			         = ;  
      row.status			      = (uint8_t)xin_order_status::CREATED;
      row.memo			         = memo_detail;
      row.maker               = from;
      row.created_at          = time_point_sec(current_time_point());
      row.updated_at          = time_point_sec(current_time_point());
   });
}

ACTION xchain::onpaying( const name& account, const uint64_t& id, const string& txid, const string& payno, const string& xout_from )
{
   require_auth( _gstate.maker );

   xout_order_t::idx_t xout_orders(_self, _self.value);
   auto xout_order_itr = xout_orders.find(id);
   check( xout_order_itr != xout_orders.end(), "xout order not found: " + to_string(id) );
   auto status = xout_order_itr->status;
   check(status == (uint8_t)xout_order_status::CREATED,  "xout order status is not created: " + to_string(id));

   xout_orders.modify( *xout_order_itr, _self, [&]( auto& row ) {
      row.status     = (uint8_t)xout_order_status::PAYING;
      row.txid       = txid;
      row.xout_from  = xout_from;
      row.maker      = _gstate.maker;
      row.updated_at = time_point_sec(current_time_point());
   });
}

ACTION xchain::onpaysucc( const name& account, const uint64_t& id )
{
   require_auth( _gstate.maker );

   xout_order_t::idx_t xout_orders(_self, _self.value);
   auto xout_order_itr = xout_orders.find(id);
   check( xout_order_itr != xout_orders.end(), "xout order not found: " + to_string(id) );
   check( xout_order_itr->status == (uint8_t)xout_order_status::PAYING,  "xout order status is not paying");

   //check status
   xout_orders.modify( *xout_order_itr, _self, [&]( auto& row ) {
      row.status     = (uint8_t)xin_order_status::FUFILLED;
      row.updated_at = time_point_sec(current_time_point());
   });
}

/**
 * checker to confirm out order
 */
ACTION xchain::chkxoutorder( const name& account, const uint64_t& id )
{
   require_auth( _gstate.checker );

   xout_order_t::idx_t xout_orders(_self, _self.value);
   auto xout_order_itr = xout_orders.find(id);
   check( xout_order_itr != xout_orders.end(), "xout order not found: " + to_string(id) );

   //check status
   check(xout_order_itr->status == (uint8_t)xout_order_status::PAY_SUCCESS,  "xout order status is not pay_success");

   xout_orders.modify( *xout_order_itr, _self, [&]( auto& row ) {
      row.status     = (uint8_t)xin_order_status::FUFILLED;
      row.closed_at  = time_point_sec(current_time_point());
      row.updated_at = time_point_sec(current_time_point());
   });
}

ACTION xchain::cancelxout( const name& account, const uint64_t& id )
{
   require_auth(account);
   check(account == _gstate.checker || account == _gstate.maker, "account is not checker or taker" );

   xout_order_t::idx_t xout_orders(_self, _self.value);
   auto xout_order_itr = xout_orders.find(id);
   check( xout_order_itr != xout_orders.end(), "xout order not found: " + to_string(id) );
   check( xout_order_itr->status == (uint8_t)xout_order_status::PAY_SUCCESS ||
               xout_order_itr->status == (uint8_t)xout_order_status::PAYING 
               ,  "xout order status is not ready for cancel");

   xout_orders.modify( *xout_order_itr, _self, [&]( auto& row ) {
      row.status     = (uint8_t)xin_order_status::FUFILLED;
      row.closed_at  = time_point_sec(current_time_point());
      row.updated_at = time_point_sec(current_time_point());   
   });
}

uint8_t xchain::_check_base_chain(const name& chain) {
   chain_t::idx_t chains(_self, _self.value);
   auto chain_itr = chains.find(chain.value);

   check( chain_itr != chains.end(), "chain not found: " + chain.to_string() );
   check( chain_itr->base_chain,  chain.to_string() + "is not base chain" );
   
   if( chain_itr->xin_account.empty() )
      return 0;
   else 
      return 1;
}

void xchain::_check_chain_coin(const name& chain, const name& coin) {
   chain_coin_t::idx_t chain_coins(_self, _self.value);
   auto chain_coin_itr = chain_coins.find(chain.value);

   check( chain_coin_itr != chain_coins.end(), "chain_coin not found: " + chain.to_string() + "_" + coin.to_string());
   while (chain_coin_itr != chain_coins.end()) {
      if (chain_coin_itr->coin == coin )
         return;
      chain_coin_itr++;
   }

   check(false , "not find any valid pair chain and coin");
}

} /// namespace apollo
