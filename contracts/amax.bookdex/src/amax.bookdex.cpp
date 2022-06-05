#include <amax.bookdex/amax.bookdex.hpp>
#include <amax.token.hpp>
#include "utils.hpp"

namespace amax {

using namespace std;

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("$$$") + to_string((int)code) + string("$$$ ") + msg); }

   /**
    * @brief create wallet or lock amount into mulsign wallet
    *
    * @param from
    * @param to
    * @param quantity
    * @param memo: format: b|q:$targetToken:$targetPrice:$slippage
    *              Examples:
    *                   b:AMAX:0:10.5     - to buy:   market price buy order, 10.5% slippage
    *                   q:CNYD:200.88     - to sell:  limit  price sell order 
    *                   q:MUSDT:0:12.55   - to sell:  market price sell order, 12.55% slippage
    *                   q:MUSDT:100       - to sell:  limit price sell order
    */
   [[eosio::on_notify("*::transfer")]]
   void bookdex::ontransfer(const name& from, const name& to, const asset& quantity, const string& memo) {
      CHECKC( from != to, err::ACCOUNT_INVALID,"cannot transfer to self" );
      CHECKC( quantity.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      CHECKC( memo != "", err::MEMO_FORMAT_ERROR, "empty memo!" )

      auto from_bank = get_first_receiver();
      auto symbol = quantity.symbol;

      vector<string_view> params = split(memo, ":");
      auto param_size = params.size();
      auto is_limit_order = ( param_size == 3 );
      auto is_market_order = ( param_size == 4 );
      CHECKC( is_limit_order || is_market_order, err::MEMO_FORMAT_ERROR, "memo format incorrect" )

      auto is_to_buy = ( params[0] == "b" );
      auto is_to_sell = ( params[0] == "q" );
      auto target_symbol = str_tolower( string(params[1]) );
      auto price = std::stof( string(params[2]) );
      float slippage = 0;

      if (is_market_order) {
         CHECKC( price == 0, err::MEMO_FORMAT_ERROR, "market order price must be zero")
         slippage = stof( string(params[3]) );

      } else if (is_limit_order)
         CHECKC( price > 0, err::MEMO_FORMAT_ERROR, "limit order price must be positive" )

      auto process_quantity = quantity;
      if( is_to_buy ){
         auto sym_pair = price_s::sym_pair( target_symbol, symbol.code().to_string() );
         auto tradepairs = trade_pair_t::idx_t(_self, _self.value);
         auto itr = tradepairs.find(sym_pair.value);
         CHECKC( itr != tradepairs.end(), err::PARAM_ERROR, "trade pair not found: " + sym_pair.to_string() )
         auto trade_pair = *itr;
         auto offers = baseoffer_idx( _self, sym_pair.value );
         auto price_info = price_s( target_symbol, symbol.code().to_string(), price );
         if (is_limit_order)
            process_limit_buy( trade_pair, offers, price_info, from, process_quantity );
         else 
            process_market_buy( trade_pair, offers, slippage, from, process_quantity );

      } else if( is_to_sell ){
         auto sym_pair = price_s::sym_pair( symbol.code().to_string() , target_symbol );
         auto tradepairs = trade_pair_t::idx_t(_self, _self.value);
         auto itr = tradepairs.find(sym_pair.value);
         CHECKC( itr != tradepairs.end(), err::PARAM_ERROR, "trade pair not found: " + sym_pair.to_string() )
         auto trade_pair = *itr;
         auto offers = quoteoffer_idx( _self, sym_pair.value );
         auto price_info = price_s( symbol.code().to_string(), target_symbol, price );

         if (is_limit_order)
            process_limit_sell( trade_pair, offers, price_info, from, process_quantity );
         else
            process_market_sell( trade_pair, offers, slippage, from, process_quantity );

      } else {
         CHECKC( false, err::MEMO_FORMAT_ERROR, "memo header field must be b or q" )
      }
   }

   void bookdex::addtradepair(const extended_symbol& base_symb, const extended_symbol& quote_symb, const float& maker_fee_rate, const float& taker_fee_rate) {
      auto tradepair = trade_pair_t::idx_t(_self, _self.value);
      tradepair.emplace(_self, [&]( auto& row ){
            row.base_symb      = base_symb;
            row.quote_symb     = quote_symb; 
            row.maker_fee_rate = maker_fee_rate;
            row.taker_fee_rate = taker_fee_rate;
      });
   }

   //TODO add cancel order

   // limit order buy
   //TODO add max step conrol
   void bookdex::process_limit_buy( const trade_pair_t& trade_pair, baseoffer_idx& offers, 
            const price_s& bid_price, const name& to, asset& quantity ){

      auto bank   = trade_pair.base_symb.get_contract();
      auto earned = asset(0, trade_pair.quote_symb.get_symbol());
      auto bought = asset(0, trade_pair.base_symb.get_symbol());

      auto idx = offers.get_index<"priceidx"_n>();
      for (auto itr = idx.begin(); itr != idx.end(); itr++) {
         auto price_diff = itr->price.amount - bid_price.amount;
         if (price_diff > 0) 
            break;   //offer or ask price > bid price
         
         auto offer_cost = itr->price.amount * itr->amount;
         if (offer_cost >= quantity.amount) {
            auto offer_buy_amount = quantity.amount / itr->price.amount;
            bought.amount += offer_buy_amount;

            idx.modify(itr, same_payer, [&]( auto& row ) {
               row.amount -= offer_buy_amount;
               row.updated_at = current_time_point();
            });
            
            //send to buyer for base tokens
            TRANSFER( bank, to, bought, "dex buy:" + to_string(itr->id) )

            //send to seller for quote tokens
            earned.amount = quantity.amount;
            TRANSFER( bank, itr->maker, earned, "dex sell:" + to_string(itr->id) )
            return;

         } else {// will buy the current offer wholely and continue
            bought.amount += itr->amount / itr->price.amount;
            quantity.amount -= offer_cost;

            idx.erase( itr );

            earned.amount = itr->amount;
            TRANSFER( bank, itr->maker, earned, "dex earn:" + to_string(itr->id) )
         }
      }

      if (bought.amount > 0)
         TRANSFER( bank, to, bought, "dex buy" )

      if (quantity.amount > 0) { //unsatisified remaining quantity will be placed as limit buy order
         auto quoteoffers = quoteoffer_idx( _self, trade_pair.primary_key() );
         quoteoffers.emplace(_self, [&]( auto& row ){
            row.id         = quoteoffers.available_primary_key();
            row.price      = bid_price;
            row.amount     = quantity.amount; 
            row.maker      = to;
            row.created_at = current_time_point();
         });
      }
   }

   // market order buy
   void bookdex::process_market_buy( const trade_pair_t& trade_pair, baseoffer_idx& offers, 
            const float& slippage, const name& to, asset& quantity ){

      auto bank   = trade_pair.base_symb.get_contract();
      auto earned = asset(0, trade_pair.quote_symb.get_symbol());
      auto bought = asset(0, trade_pair.base_symb.get_symbol());
      
      auto idx = offers.get_index<"priceidx"_n>();
      auto itr = idx.begin();
      auto init_price = itr->price.amount;

      for (; itr != idx.end(); itr++) {
         auto price_diff = itr->price.amount - init_price;
         if (price_diff > 0 && price_diff * 100 / init_price > slippage )
            break; //no more valid offer from this onwards

         auto offer_cost = itr->price.amount * itr->amount;
         if (offer_cost >= quantity.amount) {
            auto offer_buy_amount = quantity.amount / itr->price.amount;
            bought.amount += offer_buy_amount;

            idx.modify(itr, same_payer, [&]( auto& row ) {
               row.amount -= offer_buy_amount;
            });
            
            //send to buyer for base tokens
            TRANSFER( bank, to, bought, "dex buy:" + to_string(itr->id) )

            //send to seller for quote tokens
            earned.amount = quantity.amount;
            TRANSFER( bank, itr->maker, earned, "dex sell:" + to_string(itr->id) )
            return;

         } else {// will buy the current offer wholely
            bought.amount += itr->amount;
            quantity.amount -= offer_cost;

            //send to seller for quote tokens
            earned.amount = itr->amount;
            TRANSFER( bank, itr->maker, earned, "dex earn:" + to_string(itr->id) )

            idx.erase( itr );
         }
      }

      TRANSFER( bank, to, bought, "buy partial" )
      TRANSFER( bank, to, quantity, "market buy residual" )

   }


   //limit order sell
   void bookdex::process_limit_sell( const trade_pair_t& trade_pair, quoteoffer_idx& offers, 
            const price_s& ask_price, const name& to, asset& quantity ){
    
      auto bank   = trade_pair.quote_symb.get_contract();
      auto earned = asset(0, trade_pair.quote_symb.get_symbol());
      auto bought = asset(0, trade_pair.base_symb.get_symbol());
      
      auto idx = offers.get_index<"priceidx"_n>();
      for (auto itr = idx.begin(); itr != idx.end(); itr++) {
         if (itr->price.amount < ask_price.amount) 
            break;   //offer or bit price < ask price
         
         auto offer_amount = itr->amount;
         auto sell_amount = quantity.amount * itr->price.amount;
         if (offer_amount >= sell_amount) {
            earned.amount += sell_amount;
            idx.modify(itr, same_payer, [&]( auto& row ) {
               row.amount -= sell_amount;
            });
            
            //send to seller for quote tokens
            TRANSFER( bank, to, earned, "bookdex market buy:" + to_string(itr->id) )

            //send to buyer for base tokens
            bought.amount = quantity.amount;
            TRANSFER( bank, to, bought, "dex sell:" + to_string(itr->id) )
            return;

         } else {// will buy the current offer wholely
            earned.amount += itr->amount;
            quantity.amount -= offer_amount;

            //send to buyer for base tokens
            bought.amount = itr->amount;
            TRANSFER( bank, itr->maker, bought, "dex buy:" + to_string(itr->id) )

            idx.erase( itr );
         }
      }

      if (earned.amount > 0)
         TRANSFER( bank, to, earned, "bookdex market buy" )

      if (quantity.amount > 0) { //unsatisified remaining quantity will be placed as limit sell order
         auto baseoffers = baseoffer_idx( _self, trade_pair.primary_key() );
         baseoffers.emplace(_self, [&]( auto& row ){
            row.id         = baseoffers.available_primary_key();
            row.price      = ask_price;
            row.amount     = quantity.amount; 
            row.maker      = to;
            row.created_at = current_time_point();
         });
      }
   }

   //market order sell
   void bookdex::process_market_sell( const trade_pair_t& trade_pair, quoteoffer_idx& offers, 
            const float& slippage, const name& to, asset& quantity ){

      auto bank   = trade_pair.base_symb.get_contract();
      auto earned = asset( 0, trade_pair.quote_symb.get_symbol() );
      auto bought = asset( 0, trade_pair.base_symb.get_symbol() );
      
      auto idx = offers.get_index<"priceidx"_n>();
      auto itr = idx.begin();
      auto init_price = itr->price.amount;
      for (; itr != idx.end(); itr++) {
         auto price_diff = init_price - itr->price.amount;
         if( price_diff > 0 && price_diff * 100 / init_price > slippage )
            break; //no more valid offer from this onwards

         auto offer_amount = itr->amount;
         auto sell_amount = quantity.amount * itr->price.amount;
         if (offer_amount >= sell_amount) {
            earned.amount += sell_amount;

            idx.modify(itr, same_payer, [&]( auto& row ) {
               row.amount -= sell_amount;
            });
            
            //send to seller for quote tokens
            TRANSFER( bank, to, earned, "bookdex market sell:" + to_string(itr->id) )

            //send to buyer for base tokens
            bought.amount = quantity.amount;
            TRANSFER( bank, itr->maker, bought, "dex buy:" + to_string(itr->id) )

            return;

         } else {// will buy the current offer wholely
            earned.amount += itr->amount;
            quantity.amount -= offer_amount;

            //send to buyer for base tokens
            bought.amount = itr->amount;
            TRANSFER( bank, itr->maker, bought, "dex buy:" + to_string(itr->id) )

            idx.erase( itr );
         }
      }

      TRANSFER( bank, to, earned, "sell partial" )
      TRANSFER( bank, to, quantity, "market buy residual" )

   }

} //namespace amax