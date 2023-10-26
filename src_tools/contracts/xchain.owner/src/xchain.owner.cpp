#include <xchain.owner/xchain.owner.hpp>

namespace amax {
   using namespace std;

   #define CHECKC(exp, code, msg) \
      { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ")  \
                                    + string("[[") + _self.to_string() + string("]] ") + msg); }

   void xchain_owner::_newaccount( const name& creator, const name& account, const authority& active) {
      require_auth(_gstate.admin);

      auto perm = creator != get_self()? OWNER_PERM : ACTIVE_PERM;
      amax_system::newaccount_action  act(SYS_CONTRACT, { {creator, perm} }) ;
      authority owner_auth  = { 1, {}, {{{get_self(), ACTIVE_PERM}, 1}}, {} }; 
      act.send( creator, account, owner_auth, active);

      amax_system::buyrambytes_action buy_ram_act(SYS_CONTRACT, { {get_self(), ACTIVE_PERM} });
      buy_ram_act.send( get_self(), account, _gstate.ram_bytes );

      amax_system::delegatebw_action delegatebw_act(SYS_CONTRACT, { {get_self(), ACTIVE_PERM} });
      delegatebw_act.send( get_self(), account,  _gstate.stake_net_quantity,  _gstate.stake_cpu_quantity, false );
   }

   void xchain_owner::_updateauth( const name& account, const eosio::public_key& pubkey ) {
      require_auth(_gstate.admin);

      authority auth = { 1, {{pubkey, 1}}, {}, {} };
      amax_system::updateauth_action act(SYS_CONTRACT, { {account, OWNER_PERM} });
      act.send( account, ACTIVE_PERM, OWNER_PERM, auth);
   }


   //如果xchain_pubkey 存在就替换pubkey

   void xchain_owner::proposebind( 
            const name& oracle_maker, 
            const name& xchain_name,        //eth,bsc,btc,trx
            const string& xchain_txid, 
            const string& xchain_pubkey, 
            const name& owner,
            const name& creator,
            const eosio::public_key& amc_pubkey
            ){     //如果pubkey变了,account name 会变不？
      require_auth( oracle_maker );
      bool found = ( _gstate.oracle_makers.count( oracle_maker ) > 0 );
      CHECKC(found, err::NO_AUTH, "no auth to operate" )

      xchain_account_t::idx_t xchain_accts (get_self(), xchain_name.value );
      auto xchain_accts_idx = xchain_accts.get_index<"xchainpubkey"_n>();
      auto xchain_acct_ptr = xchain_accts_idx.find(hash(xchain_pubkey));
      checksum256 amc_txid;
      _txid(amc_txid);
      
      if( xchain_acct_ptr == xchain_accts_idx.end()) {
         //检查account是否存在
         // CHECKC(!is_account(owner), err::ACCOUNT_INVALID, "account account already exist:" + owner.to_string() );
         //无法判断 pubkey 是否存在
         authority auth = { 1, {{amc_pubkey, 1}}, {}, {} };
         _newaccount(creator, owner, auth);

         xchain_accts.emplace( _self, [&]( auto& a ){
            a.account         = owner;
            a.xchain_txid     = xchain_txid;
            a.xchain_pubkey   = xchain_pubkey;
            a.amc_pubkey      = amc_pubkey;
            a.amc_txid        = amc_txid;
            a.bind_status     = BindStatus::REQUESTED;
            a.created_at      = time_point_sec( current_time_point() );
         });
         return;
      }
      CHECKC(false, err::STATUS_ERROR, "xchain_acct already exist xchain pubkey: " + xchain_pubkey );
   
   }  

   void xchain_owner::approvebind( 
            const name& oracle_checker, 
            const name& xchain, 
            const string& xchain_txid ){
      require_auth( oracle_checker );
      bool found = ( _gstate.oracle_checkers.find( oracle_checker ) != _gstate.oracle_checkers.end() );
      CHECKC(found, err::NO_AUTH, "no auth to operate" )

      xchain_account_t::idx_t xchain_accts (get_self(), xchain.value );
      auto xchain_accts_idx = xchain_accts.get_index<"xchaintxid"_n>();
      auto xchain_acct_ptr = xchain_accts_idx.find(hash(xchain_txid));
      CHECKC( xchain_acct_ptr != xchain_accts_idx.end(),
            err::RECORD_NOT_FOUND, "xchain_acct does not exist. ");
      _updateauth(xchain_acct_ptr->account, xchain_acct_ptr->amc_pubkey);
      
      xchain_accts_idx.modify(xchain_acct_ptr, _self, [&]( auto& a ){
         a.bind_status     = BindStatus::APPROVED;
      });
   }

   void xchain_owner::_txid(checksum256& txid) {
      size_t tx_size = transaction_size();
      char* buffer = (char*)malloc( tx_size );
      read_transaction( buffer, tx_size );
      txid = sha256( buffer, tx_size );
   }

}
