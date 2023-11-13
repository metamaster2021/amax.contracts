
#include <amax.token.hpp>
#include "safemath.hpp"
#include "tokensplit.hpp"
#include "utils.hpp"

#include <chrono>

namespace amax {

using namespace wasm;
using namespace wasm::safemath;
// using namespace eosiosystem;
static constexpr name ACTIVE_PERM       = "active"_n;

#define PLANTRACE(plan_trace_t)                             \
            tokensplit::plantrace_action(get_self(), {{get_self(), ACTIVE_PERM}}) \
               .send(plan_trace_t);

#define CLAIMALL(owner,plan_id)                             \
            tokensplit::claimall_action(get_self(), {{get_self(), ACTIVE_PERM}}) \
               .send(plan_id,owner);
namespace db {

    template<typename table, typename Lambda>
    inline void set(table &tbl,  typename table::const_iterator& itr, const eosio::name& emplaced_payer,
            const eosio::name& modified_payer, Lambda&& setter )
   {
        if (itr == tbl.end()) {
            tbl.emplace(emplaced_payer, [&]( auto& p ) {
               setter(p, true);
            });
        } else {
            tbl.modify(itr, modified_payer, [&]( auto& p ) {
               setter(p, false);
            });
        }
    }

    template<typename table, typename Lambda>
    inline void set(table &tbl,  typename table::const_iterator& itr, const eosio::name& emplaced_payer,
               Lambda&& setter )
   {
      set(tbl, itr, emplaced_payer, eosio::same_payer, setter);
   }

}
inline int64_t get_precision(const symbol &s) {
    int64_t digit = s.precision();
    check(digit >= 0 && digit <= 18, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
    return calc_precision(digit);
}

inline int64_t get_precision(const asset &a) {
    return get_precision(a.symbol);
}


inline void check_rate(const vector<split_unit_s>& confs){
    uint64_t init_rate = 0;
    set<name> s;
    for ( auto c : confs){
        CHECKC( is_account(c.token_receiver),err::ACCOUNT_INVALID,"account invalid")
        CHECKC( c.token_split_amount > 0 && c.token_split_amount <= PCT_BOOST,err::OVERSIZED,"1 - 10000")
        init_rate += c.token_split_amount;
        if (s.find(c.token_receiver) != s.end()) {
            // The element is already in the collection and there are duplicates
            CHECKC(false,err::PARAM_ERROR,"There are duplicate elements")
        }
        s.insert(c.token_receiver);
        // CHECKC( init_rate <= PCT_BOOST,err::OVERSIZED,"The total number should be less than 100%");
    }   

    CHECKC( init_rate == PCT_BOOST,err::OVERSIZED,"The total number should be 100%");
}

void tokensplit::init(const name& admin,const asset& fee,const uint64_t& min_count, const uint64_t& max_count) {
    require_auth( _self );

    CHECKC( is_account(admin),err::RECORD_NOT_FOUND,"account invalid" )
    CHECKC( fee.symbol.raw() == SYS_SYMB.raw(), err::DATA_MISMATCH,"fee musdt be AMAX")
    CHECKC( min_count >= 1 && max_count <= 50, err::PARAM_ERROR,"The number of split accounts should be 1-50")
    _gstate.admin = admin;
    _gstate2.fee = fee;
    _gstate2.max_split_count = max_count;
    _gstate2.min_split_count = min_count;
}

void tokensplit::paused( const bool& paused){

    CHECKC( has_auth(_self) || has_auth(_gstate.admin),err::NO_AUTH,"no auth");
    _gstate2.running = !paused;
}
void tokensplit::addplan(const name& owner, const string& title, const vector<split_unit_s>& conf, const bool& is_auto) {
    
    CHECKC( has_auth(owner) || has_auth(_gstate.admin),err::NO_AUTH,"no_auth")

    CHECKC( _gstate2.running, err::NO_AUTH,"paused")
    uint64_t size = conf.size();
    CHECKC( size >= _gstate2.min_split_count && size <= _gstate2.max_split_count,err::OVERSIZED,"Number of users is "+ to_string(_gstate2.min_split_count) + "-" + to_string(_gstate2.max_split_count))
    CHECKC( title.size() >= 4 && title.size()<= 255,err::OVERSIZED, "Title length should be 4-255" );
    
    check_rate(conf);
    auto fee = _gstate2.fee;
    if ( owner != _gstate.admin){
        account_t::tbl_t accounts( _self, _self.value);
        auto account_itr = accounts.find(owner.value);
        CHECKC( account_itr != accounts.end() , err::PARAM_ERROR,"account not found!")
        CHECKC( account_itr -> balance >= _gstate2.fee, err::PARAM_ERROR,"Unpaid")

        accounts.modify(account_itr, same_payer,[&]( auto& row){
            row.balance -= _gstate2.fee;
        });
    }
    
    token_split_plan_t::tbl_t plans( _self, _self.value);
    plans.emplace( _self, [&]( auto& row ) {
        // row.split_by_rate   = true;
        row.id              = ++_gstate.last_plan_id;
        row.title           = title;
        row.creator         = owner;
        row.split_conf      = conf;
        row.split_type      = is_auto ? split_type::AUTO : split_type::MANUAL;
        row.paid_fee        = fee;
        row.create_at       = current_time_point();
        row.status          = plan_status::RUNNING;
    });
}

void tokensplit::editplan(const name& owner,const uint64_t& plan_id,const vector<split_unit_s>& conf, const bool& is_auto) {
    require_auth( owner );

    CHECKC( _gstate2.running, err::NO_AUTH,"paused")
    uint64_t size = conf.size();
    CHECKC( size >= _gstate2.min_split_count && size <= _gstate2.max_split_count,err::OVERSIZED,"Number of users is "+ to_string(_gstate2.min_split_count) + "-" + to_string(_gstate2.max_split_count))
    check_rate(conf);
    token_split_plan_t::tbl_t plans( _self, _self.value);
    auto plan_itr = plans.find(plan_id);
    CHECKC( plan_itr != plans.end(), err::RECORD_NOT_FOUND,"plan not found!")
    CHECKC( plan_itr -> creator == owner,err::NO_AUTH,"no auth")
    CHECKC( plan_itr-> status == plan_status::RUNNING,err::DATA_MISMATCH,"Plan closed")

    plans.modify( plan_itr,same_payer ,[&]( auto& row ) {
        row.split_conf      = conf;
        row.split_type      = is_auto ? split_type::AUTO : split_type::MANUAL;
    });

    if (is_auto){
        wallet_t::tbl_t wallets(_self,plan_id);
        auto itr = wallets.begin();
        if (itr != wallets.end()) CLAIMALL(owner,plan_id);
    }
}

void tokensplit::closeplan(const name& creator, const uint64_t& plan_id){
    require_auth( creator );
    CHECKC( _gstate2.running, err::NO_AUTH,"paused")
    token_split_plan_t::tbl_t plans( _self, _self.value);
    auto plan_itr = plans.find(plan_id);
    CHECKC( plan_itr != plans.end(), err::RECORD_NOT_FOUND,"plan not found!")
    CHECKC( plan_itr -> creator == creator,err::NO_AUTH,"no auth")
    CHECKC( plan_itr-> status == plan_status::RUNNING,err::DATA_MISMATCH,"Plan closed")

    plans.modify( plan_itr,same_payer ,[&]( auto& row ) {
        
        row.status          = plan_status::CLOSED;
    });

    // _empty_wallets(creator,plan_id);
}

void tokensplit::claim( const uint64_t& plan_id, const name& owner){

    CHECKC( _gstate2.running, err::NO_AUTH,"paused")

    token_split_plan_t::tbl_t plans( _self, _self.value);
    auto plan_itr = plans.find( plan_id);
    CHECKC( plan_itr != plans.end(), err::RECORD_NOT_FOUND,"plan not found!")

    CHECKC( has_auth(owner) || has_auth(plan_itr -> creator),err::NO_AUTH,"no auth")
    
    wallet_t::tbl_t wallets(_self,plan_id);
    auto wallet_itr = wallets.find(owner.value);

    CHECKC( wallet_itr != wallets.end(), err::RECORD_NOT_FOUND,"wallet not found!")
    CHECKC( !wallet_itr -> balances.empty(), err::RECORD_NOT_FOUND,"no rewards to claim")

    auto balances = wallet_itr -> balances;
    for ( auto w : balances){
        if ( w.second.amount > 0)
            TRANSFER( w.first.get_contract(), wallet_itr -> owner, w.second, "claim rewards" )        
    }
    
    balances.clear();
    wallets.erase(wallet_itr);
}

void tokensplit::claimall( const uint64_t& plan_id, const name& creator){

    // CHECKC( has_auth(_self) || has_auth(creator),err::NO_AUTH,"no auth");

    CHECKC( _gstate2.running, err::NO_AUTH,"paused")
    token_split_plan_t::tbl_t plans( _self, _self.value);
    auto plan_itr = plans.find( plan_id);
    CHECKC( plan_itr != plans.end(), err::RECORD_NOT_FOUND,"plan not found!")
    CHECKC( plan_itr -> creator == creator, err::NO_AUTH,"no auth")

    _empty_wallets(plan_id);
    
}

void tokensplit::plantrace(const plan_trace_t& trace){
    require_auth(get_self());
}


/**
 * @brief send nasset tokens into nftone marketplace
 *
 * @param from
 * @param to
 * @param quantity
 * @param memo: 
 *  1) $plan_id
 *  2) plan:$plan_id
 *  3) plan:$plan_id:$boost
 * 
 *   - plan_id: split plan ID
 *   - boost: boost for absolute split quantity 
 *
 */
void tokensplit::ontransfer(const name& from, const name& to, const asset& quant, const string& memo) {
    if (from == get_self() || to != get_self()) return;

    CHECKC( _gstate2.running, err::NO_AUTH,"paused")
    CHECKC( from != to, err::PARAM_ERROR, "cannot send to self" )
    vector<string_view> memo_params = split(memo, ":");
    CHECKC( memo_params.size() >= 1, err::MEMO_FORMAT_ERROR, "memo not prefixed" )
    CHECK( quant.amount > 0, "must transfer positive quantity: " + quant.to_string() )

    auto action_name = name(memo_params[0]) ;

    switch (action_name.value){
        case action_name::FEE:
            _recharge(from,quant);
            break;
        case action_name::PLAN:{
            auto plan_id = stoi( string( memo_params[1] ));
            auto boost = 1;
            if (memo_params.size() == 3){
                boost = stoi( string( memo_params[2] ));
            }
            _split(from,plan_id,quant,boost);
            break;
        }   
        default:
            CHECKC( false, err::PARAM_ERROR, "memo not Supported");
            break;
    }

}


void tokensplit::_recharge( const name& owner, const asset& quantity){

    CHECKC( get_first_receiver() == SYS_BANK, err::RECORD_NOT_FOUND, "not amax.token")
    CHECKC( quantity.symbol == SYS_SYMB, err::RECORD_NOT_FOUND, "not AMAX")
    CHECKC( quantity >= _gstate2.fee, err::PARAM_ERROR,"Unpaid")

    account_t::tbl_t accounts(_self,_self.value);
    auto account_itr = accounts.find(owner.value);

    db::set( accounts, account_itr, _self, _self, [&]( auto& p, bool is_new ){
        if ( is_new){
            p.owner = owner;
        }
        p.balance += quantity;
    });

    if ( is_account(_gstate2.fee_receiver) )
        TRANSFER( SYS_BANK, _gstate2.fee_receiver, quantity, "" ) 
}

void tokensplit::_split( const name& from, const uint64_t& plan_id, const asset& quant,const uint64_t& boost){
    token_split_plan_t::tbl_t plans( _self, _self.value);
    auto plan_itr = plans.find( plan_id);
    CHECKC( plan_itr != plans.end(), err::RECORD_NOT_FOUND,"plan not found")
    CHECKC( plan_itr-> status == plan_status::RUNNING,err::DATA_MISMATCH,"Plan closed")

    auto token_bank = get_first_receiver();
    auto current_quant = quant;

    for ( auto p : plan_itr-> split_conf){

        auto to = p.token_receiver;
        auto amount = p.token_split_amount;
        auto tokens = asset( 0, quant.symbol );
        // tokens.amount = ( plan_itr->split_by_rate ) ? multiply_decimal( quant.amount, amount, PCT_BOOST ) : multiply_decimal( amount, get_precision(quant.symbol), PCT_BOOST );
        tokens.amount = quant.amount * amount / PCT_BOOST;
        CHECKC( tokens<= current_quant, err::RATE_OVERLOAD, "Insufficient distribution balance")

        if (tokens.amount > 0) {

            plan_trace_t trace;
            trace.issuer = from;
            trace.plan_id = plan_id;
            trace.receiver = to;
            trace.contract = token_bank;
            trace.base_quantity = quant;
            trace.divide_quantity = tokens;
            trace.rate = amount;
            PLANTRACE(trace);

            if ( plan_itr -> split_type == split_type::AUTO){
                TRANSFER( token_bank, to, tokens, "" )
            }else {
                _add_wallet(to,plan_id,token_bank, tokens);
            }

            current_quant -= tokens;
        }else {
            break;
        }
        
    }

    auto last_to = plan_itr-> split_conf[0].token_receiver;
    if (current_quant.amount > 0){
        if ( plan_itr -> split_type == split_type::AUTO){
            TRANSFER( token_bank, last_to, current_quant, "" )
        }else {
            _add_wallet(last_to,plan_id,token_bank, current_quant);
        }
    }
}

void tokensplit::_add_wallet( const name& owner, const uint64_t& plan_id, const name& contract, const asset& quantity){

    extended_symbol sym = {quantity.symbol,contract};

    wallet_t::tbl_t wallets(_self,plan_id);
    auto wallet_itr = wallets.find(owner.value);

    db::set( wallets, wallet_itr, _self, _self, [&]( auto& p, bool is_new ){
        if ( is_new){
            // p.plan_id = plan_id;
            p.owner = owner;
            p.create_at = current_time_point();
        }
        
        if (p.balances.count(sym)){
            p.balances[sym] += quantity;
        }else {
            p.balances[sym] = quantity;
        }

        p.update_at = current_time_point();
    });
}

bool tokensplit::_empty_wallets(const uint64_t& plan_id){

    wallet_t::tbl_t wallets(_self,plan_id);
    auto itr = wallets.begin();
    if (itr == wallets.end()) return false;

    int curr = 0;
    while( itr != wallets.end()){

        auto balances = itr -> balances;
        for ( auto w : balances){
            if ( w.second.amount > 0)
                TRANSFER( w.first.get_contract(), itr->owner, w.second, "claim" )        
        }
        itr = wallets.erase(itr);
        curr ++;
        if ( curr >= 20)
            break;
    }
    return true;
}

}  //namespace amax