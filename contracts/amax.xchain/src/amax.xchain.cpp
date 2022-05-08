#include <amax.xchain/amax.xchain.hpp>

namespace amax {

ACTION xchain::reqxintoaddr( const name& account, const name& base_chain )
{
   require_auth( account );

   auto type = _check_base_chain(base_chain.symbol_name);

   auto idx = (uint128_t)account.value << 64 || (uint128_t)base_chain.value;
   auto xchaddr_idx = account_xchain_address_t:::idx_t(_self, _self.value);
   check( xchaddr_idx.find(idx) == xchaddr_idx.end(),  "the record already exists" );

   auto xchaddr = account_xchain_address_t();
   xchaddr.id = xchaddr_idx::available_primary_key();
   xchaddr.account = account;
   xchaddr.base_chain = base_chain;
   xchaddr.created_at = time_point_sec(current_time_point());

   if( type != 0 ) {
      xchaddr.status = (uint8_t)address_status::PENDING;
   } else {
      xchaddr.status = (uint8_t)address_status::CONFIGURED;
      xchaddr.xin_to = str(xchaddr.id);
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
   auto xchaddr_idx = account_xchain_address_t:::idx_t(_self, _self.value);
   auto xchaddr_ptr = xchaddr_idx.find(idx)
   check( xchaddr_ptr != xchaddr_idx.end(),  "the record not exists" );
   check( xchaddr_ptr->status != (uint8_t)address_status::CONFIGURED, "address already existed");

   xchaddr->status = (uint8_t)address_status::CONFIGURED;
   xchaddr->xin_to = xin_to;
   xchaddr->updated_at = time_point_sec(current_time_point());
   _db.set(xchaddr);
}

ACTION xchain::mkxinorder( const name& to, const name& chain, const name& coin_name, 
                           const string& txid, const string& xin_from, const string& xin_to,
                           const asset& quantity)
{
   require_auth( _gstate.maker );

   _check_chain_coin(chain, coin_name);
   
   xin_order_t::idx_t xin_orders(_self, _self.value);
    auto txid_index 			   = xin_orders.get_index<"xintxids"_n>();
    auto lower_itr 				= ordersn_index.lower_bound(hash(txid));
    auto upper_itr 				= ordersn_index.upper_bound(hash(txid));

   check( xin_orders.find(txid_index) == ordersn_index.end() , "txid already existing!" );

   auto created_at = time_point_sec(current_time_point());
   auto xin_order_id = xin_orders.available_primary_key();

   xin_orders.emplace( _self, [&]( auto& row ) {
      row.id 					= xin_order_id;
      row.txid 			   = txid;
      row.user_amacct      = to;
      row.xin_from         = xin_from;
      row.xin_to           = xin_to;
      row.chain 			   = chain;
      row.coin_name  	   = coin_name;
      row.quantity		   = quantity;
      row.status   			= (uint8_t)order_status::CREATED;
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
   require_auth( _gstate.taker );

   xin_order_t::idx_t xin_orders(_self, _self.value);
   auto xin_order_itr = xin_orders.find(id);
   check( xin_order_itr != xin_orders.end(), "xin order not found: " + to_string(id) );
   auto status = (deal_status_t)xin_order_itr->status;
   check( (uint8_t)status != (uint8_t)order_status::CREATED, "xin order already closed: " + to_string(id) );

   xin_order_itr->status         = (uint8_t)order_status::FUFILLED;
   xin_order_itr->closed_at     = time_point_sec(current_time_point());
   xin_order_itr->updated_at     = time_point_sec(current_time_point());
   _db.set(xin_order_itr);
}

ACTION xchain::cancelorder( const name& account, const uint_64& id, const string& cancel_reason )
{
   require_auth( _gstate.taker );

   xin_order_t::idx_t xin_orders(_self, _self.value);
   auto xin_order_itr = xin_orders.find(id);
   check( xin_order_itr != xin_orders.end(), "xin order not found: " + to_string(id) );
   auto status = (deal_status_t)xin_order_itr->status;
   check( (uint8_t)status != (uint8_t)order_status::CREATED, "xin order already closed: " + to_string(id) );

   xin_order_itr->status         = (uint8_t)order_status::CANCELED;
   xin_order_itr->cancel_reason  = cancel_reason;
   xin_order_itr->closed_at      = time_point_sec(current_time_point());;
   xin_order_itr->updated_at     = time_point_sec(current_time_point());;
   _db.set(xin_order_itr);
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
   auto chain = parts[1];
   auto coin_name = part[2];
   auto order_no = part[3];
   if(parts.size() == 5) {
      memo_detail =  part[4];
   }
   
   if (get_first_receiver() == SYS_BANK) return;

    auto created_at = time_point_sec(current_time_point());
    xout_order_t::table_t xout_orders(_self, _self.value);
    auto id = stake_log_tbl.available_primary_key();
    xout_orders.emplace( _self, [&]( auto& row ) {
      row.id 					   = id;
      row.xout_to 			   = xout_to;
      row.chain               = chain;
      row.coin_name           = coin_name;
      row.apply_amount		   = quantity;
      row.amount		         = quantity;
      //   row.fee			         = ;  
      row.status			      = (uint8_t)order_status::CREATED;
      row.memo			         = memo_detail;
      row.maker               = from;
      row.created_at          = time_point_sec(current_time_point());
      row.updated_at          = time_point_sec(current_time_point());;

    });
}

ACTION xchain::onpaying( const name& account, const uint64_t& id, const string& txid, const string& payno, const string& xout_from )
{
   require_auth( _gstate.taker );

   xout_order_t::idx_t xout_orders(_self, _self.value);
   auto xout_order_itr = xout_orders.find(id);
   check( xout_order_itr != xout_orders.end(), "xout order not found: " + to_string(id) );
   check(xout_order_itr.status == (unit8_t)xout_order_status::CREATED,  "xout order status is not created: " + to_string(id));

   xout_orders.modify( *xout_order_itr, _self, [&]( auto& row ) {
      row.status     = (uint8_t)order_status::PAYING;
      row.txid       = txid;
      row.xout_from  = xout_from;
      row.maker      = _gstate.taker;
      row.updated_at = time_point_sec(current_time_point());
   });
}

ACTION xchain::onpaysucc( const name& account, const uint64_t& id )
{
   require_auth( _gstate.taker );

   xout_order_t::idx_t xout_orders(_self, _self.value);
   auto xout_order_itr = xout_orders.find(id);
   check( xout_order_itr != xout_orders.end(), "xout order not found: " + to_string(id) );
   check(xout_order_itr.status == (unit8_t)xout_order_status::PAYING,  "xout order status is not paying");

   //check status
   xout_orders.modify( *xout_order_itr, _self, [&]( auto& row ) {
      row.status     = (uint8_t)order_status::PAY_SUCCESS;
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
   check(xout_order_itr.status == (unit8_t)xout_order_status::PAY_SUCCESS,  "xout order status is not pay_success");

   xout_orders.modify( *xout_order_itr, _self, [&]( auto& row ) {
      row.status     = (uint8_t)order_status::CHECKED;
      row.closed_at  = time_point_sec(current_time_point());
      row.updated_at = time_point_sec(current_time_point());
   });
}

ACTION xchain::cancelxout( const name& account, const uint64_t& id )
{
   require_auth(account);
   check(account == _gstate.checker || account == _gstate.taker, "account is not checker or taker" )

   xout_order_t::idx_t xout_orders(_self, _self.value);
   auto xout_order_itr = xout_orders.find(id);
   check( xout_order_itr != xout_orders.end(), "xout order not found: " + to_string(id) );
   check(xout_order_itr.status == (unit8_t)xout_order_status::PAY_SUCCESS ||
               xout_order_itr.status == (unit8_t)xout_order_status::PAYING 
               ,  "xout order status is not pay_success");

   xout_orders.modify( *xout_order_itr, _self, [&]( auto& row ) {
      row.status     = (uint8_t)order_status::CHECKED;
      row.closed_at  = time_point_sec(current_time_point());
      row.updated_at = time_point_sec(current_time_point());   
   });
}

uint8_t xchain::_check_base_chain(const name& chain) {
   chain::idx_t chains(_self, _self.value);
   auto chain_itr = chains.find(chain.value);

   check( chain_itr != chains.end(), "chain not found: " + to_string(chain) );
   check( chain_itr->base_chain,  to_string(chain) + "is not base chain" );
   
   if( chain_itr->xin_account == null )
      return 0;
   else 
      return 1;

}

uint8_t xchain::_check_chain_coin(const name& chain, const name& coin) {
   chain_coin_t::idx_t chain_coins(_self, _self.value);
   auto chain_coin_itr = chain_coins.find(chain.value);
   check( chain_coin_itr != chain_coins.end(), "chain_coin not found: " + to_string(chain) + "_" +  to_string(coin));
}

} /// namespace apollo
