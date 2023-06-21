#include <amax.applybp/amax.applybp.hpp>

#include <math.hpp>
#include <utils.hpp>
#include "mdao.info/mdao.info.db.hpp"

#define ALLOT_APPLE(farm_contract, lease_id, to, quantity, memo) \
    {   aplink::farm::allot_action(farm_contract, { {_self, active_perm} }).send( \
            lease_id, to, quantity, memo );}

namespace amax {


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

}// namespace db


using namespace std;
using namespace amax;
using namespace mdao;

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ")  \
                                    + string("[[") + _self.to_string() + string("]] ") + msg); }


   void amax_applybp::init( const name& admin){
      require_auth( _self );

      CHECKC( is_account(admin),err::ACCOUNT_INVALID,"admin invalid:" + admin.to_string())
      // CHECKC( is_account(dao_contract),err::ACCOUNT_INVALID,"dao_contract invalid:" + dao_contract.to_string())

      _gstate.admin = admin;
      // _gstate.dao_contract = dao_contract;

   }
   

   void amax_applybp::applybp(const name& owner,
                              const string& logo_uri,
                              const string& org_name,
                              const string& org_info,
                              const name& dao_code,
                              const string& reward_shared_plan,
                              const string& manifesto,
                              const string& issuance_plan){
      require_auth( owner );

      auto prod_itr = _producer_tbl.find(owner.value);
      CHECKC( prod_itr == _producer_tbl.end(),err::RECORD_EXISTING,"Application submitted:" + owner.to_string())

      _set_producer(owner,logo_uri,org_name,org_info,dao_code,reward_shared_plan,manifesto,issuance_plan);
   }

   void amax_applybp::updatebp(const name& owner,
                              const string& logo_uri,
                              const string& org_name,
                              const string& org_info,
                              const name& dao_code,
                              const string& reward_shared_plan,
                              const string& manifesto,
                              const string& issuance_plan){

      require_auth( owner );
      auto prod_itr = _producer_tbl.find(owner.value);
      CHECKC( prod_itr != _producer_tbl.end(),err::RECORD_EXISTING,"Application submitted:" + owner.to_string())

      _set_producer(owner,logo_uri,org_name,org_info,dao_code,reward_shared_plan,manifesto,issuance_plan);
   }

   void amax_applybp::addproducer(const name& submiter,
                              const name& owner,
                              const string& logo_uri,
                              const string& org_name,
                              const string& org_info,
                              const name& dao_code,
                              const string& reward_shared_plan,
                              const string& manifesto,
                              const string& issuance_plan){
      require_auth( submiter );
      CHECKC( submiter == _gstate.admin,err::NO_AUTH,"Missing required authority of admin" )

      _set_producer(owner,logo_uri,org_name,org_info,dao_code,reward_shared_plan,manifesto,issuance_plan);
   }


   void amax_applybp::setstatus( const name& submiter, const name& owner, const name& status){

      require_auth( submiter );
      CHECKC( submiter == _gstate.admin,err::NO_AUTH,"Missing required authority of admin" )

      auto prod_itr = _producer_tbl.find(owner.value);
      CHECKC( prod_itr != _producer_tbl.end(),err::RECORD_NOT_FOUND,"producer not found:" + owner.to_string())
      CHECKC( prod_itr-> status != status, err::STATUS_ERROR,"No changes" )
      CHECKC( status == ProducerStatus::ENABLE ||  status == ProducerStatus::DISABLE,err::PARAM_ERROR,"Unsupported state")
      
      db::set(_producer_tbl, prod_itr, _self , [&]( auto& p, bool is_new ) {
         if (is_new) {
            p.owner        =  owner;
            p.created_at   = current_time_point();
         }
         p.status          = status;
         p.updated_at      = current_time_point();
      });
   
   }

   void amax_applybp::_set_producer(const name& owner,
                              const string& logo_uri,
                              const string& org_name,
                              const string& org_info,
                              const name& dao_code,
                              const string& reward_shared_plan,
                              const string& manifesto,
                              const string& issuance_plan){
      CHECKC( logo_uri.size() <= MAX_LOGO_SIZE ,err::OVERSIZED ,"logo size must be <= " + to_string(MAX_LOGO_SIZE))
      CHECKC( org_name.size() <= MAX_TITLE_SIZE ,err::OVERSIZED ,"org_name size must be <= " + to_string(MAX_TITLE_SIZE))
      CHECKC( org_info.size() <= MAX_TITLE_SIZE ,err::OVERSIZED ,"org_info size must be <= " + to_string(MAX_TITLE_SIZE))
      CHECKC( manifesto.size() <= MAX_TITLE_SIZE ,err::OVERSIZED ,"manifesto size must be <= " + to_string(MAX_TITLE_SIZE))
      CHECKC( issuance_plan.size() <= MAX_TITLE_SIZE ,err::OVERSIZED ,"issuance_plan size must be <= " + to_string(MAX_TITLE_SIZE))
      CHECKC( reward_shared_plan.size() <= MAX_TITLE_SIZE, err::OVERSIZED, "reward_shared_ratio is too large than 10000");

      // dao_info_t::idx_t dao_info( _gstate.dao_contract, _gstate.dao_contract.value);

      // auto dao_itr = dao_info.find( dao_code.value);
      // CHECKC( dao_itr != dao_info.end(),err::RECORD_NOT_FOUND,"dao_code does not exist:" + dao_code.to_string())

      auto prod_itr = _producer_tbl.find(owner.value);
      // CHECKC( prod_itr != _producer_tbl.end(),err::RECORD_EXISTING,"Application submitted:" + owner.to_string())

      db::set(_producer_tbl, prod_itr, _self, [&]( auto& p, bool is_new ) {
         if (is_new) {
            p.owner =  owner;
            p.created_at = current_time_point();
            p.status = ProducerStatus::DISABLE;
         }

         p.logo_uri              = logo_uri;
         p.org_name              = org_name;
         p.org_info              = org_info;
         p.dao_code              = dao_code;
         p.reward_shared_plan    = reward_shared_plan;
         p.manifesto            = manifesto;
         p.issuance_plan         = issuance_plan;
         // p.last_edited_at        = current_time_point();
      });
   }

}//namespace amax