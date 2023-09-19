#include <xchain.owner/xchain.owner.hpp>

namespace amax {
    using namespace std;

   #define CHECKC(exp, code, msg) \
      { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ")  \
                                    + string("[[") + _self.to_string() + string("]] ") + msg); }

   void xchain_owner::_newaccount( const name& account, const authority& active) {
      require_auth(_gstate.admin);

      auto perm = OWNER_PERM;
      auto creator = _self;
      amax_system::newaccount_action  act(SYS_CONTRACT, { {creator, perm} }) ;
      authority owner_auth  = { 1, {}, {{{get_self(), ACTIVE_PERM}, 1}}, {} }; 
      act.send( creator, account, owner_auth, active );

      amax_system::buyrambytes_action buy_ram_act(SYS_CONTRACT, { {get_self(), ACTIVE_PERM} });
      buy_ram_act.send( get_self(), account, _gstate.ram_bytes );

      amax_system::delegatebw_action delegatebw_act(SYS_CONTRACT, { {get_self(), ACTIVE_PERM} });
      delegatebw_act.send( get_self(), account,  _gstate.stake_net_quantity,  _gstate.stake_cpu_quantity, false );
   }

   // void xchain_owner::updateauth( const name& account, const eosio::public_key& pubkey ) {
   //    require_auth(_gstate.admin);

   //    authority auth = { 1, {{pubkey, 1}}, {}, {} };
   //    amax_system::updateauth_action act(SYS_CONTRACT, { {account, OWNER_PERM} });
   //    act.send( account, ACTIVE_PERM, OWNER_PERM, auth);
   // }

   //如果xchain_pubkey 存在就替换pubkey

   void xchain_owner::proposebind( 
            const name& oracle_maker, 
            const name& xchain,        //eth,bsc,btc,trx
            const string& xchain_txid, 
            const string& xchain_pubkey, 
            const eosio::public_key& pubkey, 
            const name& account ){     //如果pubkey变了,account name 会变不？
      require_auth( oracle_maker );
      bool found = ( _gstate.oracle_makers.find( oracle_maker ) != _gstate.oracle_makers.end() );
      CHECKC(found, err::NO_AUTH, "no auth to operate" )
 

      xchain_account_t::idx_t xchain_coins (get_self(), xchain.value );
      auto chain_coins_idx = xchain_coins.get_index<"xchainpubkey"_n>();
      CHECKC( chain_coins_idx.find(hash(xchain_pubkey)) == chain_coins_idx.end(), err::RECORD_NOT_FOUND, "chain_coin already exist. ");
      //检查account是否存在
      CHECKC(!is_account(account), err::ACCOUNT_INVALID, "account account already exist");
      //无法判断 pubkey 是否存在

      checksum256 txid;
      // _txid(txid);
      auto xchain_account_itr = xchain_coins.find( account.value );
      authority auth = { 1, {{pubkey, 1}}, {}, {} };
      _newaccount(account, auth);
   }

   // void xchain_owner::changebind( 
   //          const name& oracle_maker, 
   //          const name& xchain,        //eth,bsc,btc,trx
   //          const string& xchain_txid, 
   //          const string& xchain_pubkey, 
   //          const eosio::public_key& pubkey, 
   //          const name& account ){
   //    require_auth( oracle_maker );
   //    bool found = ( _gstate.oracle_makers.find( oracle_maker ) != _gstate.oracle_makers.end() );
   //    CHECHC(found, err::NO_AUTH, "no auth to operate" )
   //    checksum256 txid;
   //    _txid(txid);

   //    xchain_account_t::idx_t xchain_coins (get_self(), xchain.value );
   //    auto chain_coins_idx = xchain_coins.get_index<"xchainpubkey"_n>();
   //    CHECKC( chain_coins_idx.find(hash(xchain_pubkey)) != chain_coins_idx.end(),
   //          err::RECORD_NOT_FOUND, "chain_coin does not exist. ");


   //    auto xchain_account_itr = xchain_account.find( account.value );

   // }
   

   void xchain_owner::approvebind( 
            const name& oracle_checker, 
            const name& xchain, 
            const string& xchain_txid ){
      require_auth( _gstate.admin );
      check( is_account( oracle_checker ), oracle_checker.to_string() + ": invalid account" );
      bool found = ( _gstate.oracle_checkers.find( oracle_checker ) != _gstate.oracle_checkers.end() );
      CHECKC(found, err::NO_AUTH, "no auth to operate" )

   }

   void xchain_owner::_txid(checksum256& txid) {
      size_t tx_size = transaction_size();
      char* buffer = (char*)malloc( tx_size );
      read_transaction( buffer, tx_size );
      txid = sha256( buffer, tx_size );
   }

}
