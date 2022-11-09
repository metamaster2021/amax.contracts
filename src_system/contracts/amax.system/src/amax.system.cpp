#include <amax.system/amax.system.hpp>
#include <amax.token/amax.token.hpp>

#include <eosio/crypto.hpp>
#include <eosio/dispatcher.hpp>

#include <cmath>

namespace eosiosystem {

   using eosio::current_time_point;
   using eosio::token;

   system_contract::system_contract( name s, name code, datastream<const char*> ds )
   :native(s,code,ds),
    _voters(get_self(), get_self().value),
    _producers(get_self(), get_self().value),
    _global(get_self(), get_self().value),
    _rammarket(get_self(), get_self().value),
    _rexpool(get_self(), get_self().value),
    _rexretpool(get_self(), get_self().value),
    _rexretbuckets(get_self(), get_self().value),
    _rexfunds(get_self(), get_self().value),
    _rexbalance(get_self(), get_self().value),
    _rexorders(get_self(), get_self().value)
   {
      #ifndef SYSTEM_DATA_UPGRADING
      _gstate  = _global.exists() ? _global.get() : get_default_parameters();
      #endif//SYSTEM_DATA_UPGRADING
   }

   symbol system_contract::get_core_symbol(const name& self) {
      global_state_singleton   global(self, self.value);
      check(global.exists(), "global does not exist");
      const auto& sym = global.get().core_symbol;
      check(sym.raw() != 0, "system contract must first be initialized");
      return sym;
   }

   amax_global_state system_contract::get_default_parameters() {
      amax_global_state dp;
      get_blockchain_parameters(dp);
      return dp;
   }

   const symbol& system_contract::core_symbol()const {
      check(_gstate.core_symbol.raw() != 0, "system contract must first be initialized");
      return _gstate.core_symbol;
   }

   system_contract::~system_contract() {
      #ifndef SYSTEM_DATA_UPGRADING
      _global.set( _gstate, get_self() );
      #endif// SYSTEM_DATA_UPGRADING
   }

   void system_contract::setram( uint64_t max_ram_size ) {
      require_auth( get_self() );

      check( _gstate.max_ram_size < max_ram_size, "ram may only be increased" ); /// decreasing ram might result market maker issues
      check( max_ram_size < 1024ll*1024*1024*1024*1024, "ram size is unrealistic" );
      check( max_ram_size > _gstate.total_ram_bytes_reserved, "attempt to set max below reserved" );

      auto delta = int64_t(max_ram_size) - int64_t(_gstate.max_ram_size);
      auto itr = _rammarket.find(ramcore_symbol.raw());

      /**
       *  Increase the amount of ram for sale based upon the change in max ram size.
       */
      _rammarket.modify( itr, same_payer, [&]( auto& m ) {
         m.base.balance.amount += delta;
      });

      _gstate.max_ram_size = max_ram_size;
   }

   void system_contract::update_ram_supply() {
      auto cbt = eosio::current_block_time();

      if( cbt <= _gstate.last_ram_increase ) return;

      auto itr = _rammarket.find(ramcore_symbol.raw());
      auto new_ram = (cbt.slot - _gstate.last_ram_increase.slot)*_gstate.new_ram_per_block;
      _gstate.max_ram_size += new_ram;

      /**
       *  Increase the amount of ram for sale based upon the change in max ram size.
       */
      _rammarket.modify( itr, same_payer, [&]( auto& m ) {
         m.base.balance.amount += new_ram;
      });
      _gstate.last_ram_increase = cbt;
   }

   void system_contract::setramrate( uint16_t bytes_per_block ) {
      require_auth( get_self() );

      update_ram_supply();
      _gstate.new_ram_per_block = bytes_per_block;
   }

   void system_contract::setparams( const eosio::blockchain_parameters& params ) {
      require_auth( get_self() );
      (eosio::blockchain_parameters&)(_gstate) = params;
      check( 3 <= _gstate.max_authority_depth, "max_authority_depth should be at least 3" );
      set_blockchain_parameters( params );
   }

   void system_contract::setpriv( const name& account, uint8_t ispriv ) {
      require_auth( get_self() );
      set_privileged( account, ispriv );
   }

   void system_contract::setalimits( const name& account, int64_t ram, int64_t net, int64_t cpu ) {
      require_auth( get_self() );

      user_resources_table userres( get_self(), account.value );
      auto ritr = userres.find( account.value );
      check( ritr == userres.end(), "only supports unlimited accounts" );

      auto vitr = _voters.find( account.value );
      if( vitr != _voters.end() ) {
         bool ram_managed = has_field( vitr->flags1, voter_info::flags1_fields::ram_managed );
         bool net_managed = has_field( vitr->flags1, voter_info::flags1_fields::net_managed );
         bool cpu_managed = has_field( vitr->flags1, voter_info::flags1_fields::cpu_managed );
         check( !(ram_managed || net_managed || cpu_managed), "cannot use setalimits on an account with managed resources" );
      }

      set_resource_limits( account, ram, net, cpu );
   }

   void system_contract::setacctram( const name& account, const std::optional<int64_t>& ram_bytes ) {
      require_auth( get_self() );

      int64_t current_ram, current_net, current_cpu;
      get_resource_limits( account, current_ram, current_net, current_cpu );

      int64_t ram = 0;

      if( !ram_bytes ) {
         auto vitr = _voters.find( account.value );
         check( vitr != _voters.end() && has_field( vitr->flags1, voter_info::flags1_fields::ram_managed ),
                "RAM of account is already unmanaged" );

         user_resources_table userres( get_self(), account.value );
         auto ritr = userres.find( account.value );

         ram = ram_gift_bytes;
         if( ritr != userres.end() ) {
            ram += ritr->ram_bytes;
         }

         _voters.modify( vitr, same_payer, [&]( auto& v ) {
            v.flags1 = set_field( v.flags1, voter_info::flags1_fields::ram_managed, false );
         });
      } else {
         check( *ram_bytes >= 0, "not allowed to set RAM limit to unlimited" );

         auto vitr = _voters.find( account.value );
         if ( vitr != _voters.end() ) {
            _voters.modify( vitr, same_payer, [&]( auto& v ) {
               v.flags1 = set_field( v.flags1, voter_info::flags1_fields::ram_managed, true );
            });
         } else {
            _voters.emplace( account, [&]( auto& v ) {
               v.owner  = account;
               v.flags1 = set_field( v.flags1, voter_info::flags1_fields::ram_managed, true );
            });
         }

         ram = *ram_bytes;
      }

      set_resource_limits( account, ram, current_net, current_cpu );
   }

   void system_contract::setacctnet( const name& account, const std::optional<int64_t>& net_weight ) {
      require_auth( get_self() );

      int64_t current_ram, current_net, current_cpu;
      get_resource_limits( account, current_ram, current_net, current_cpu );

      int64_t net = 0;

      if( !net_weight ) {
         auto vitr = _voters.find( account.value );
         check( vitr != _voters.end() && has_field( vitr->flags1, voter_info::flags1_fields::net_managed ),
                "Network bandwidth of account is already unmanaged" );

         user_resources_table userres( get_self(), account.value );
         auto ritr = userres.find( account.value );

         if( ritr != userres.end() ) {
            net = ritr->net_weight.amount;
         }

         _voters.modify( vitr, same_payer, [&]( auto& v ) {
            v.flags1 = set_field( v.flags1, voter_info::flags1_fields::net_managed, false );
         });
      } else {
         check( *net_weight >= -1, "invalid value for net_weight" );

         auto vitr = _voters.find( account.value );
         if ( vitr != _voters.end() ) {
            _voters.modify( vitr, same_payer, [&]( auto& v ) {
               v.flags1 = set_field( v.flags1, voter_info::flags1_fields::net_managed, true );
            });
         } else {
            _voters.emplace( account, [&]( auto& v ) {
               v.owner  = account;
               v.flags1 = set_field( v.flags1, voter_info::flags1_fields::net_managed, true );
            });
         }

         net = *net_weight;
      }

      set_resource_limits( account, current_ram, net, current_cpu );
   }

   void system_contract::setacctcpu( const name& account, const std::optional<int64_t>& cpu_weight ) {
      require_auth( get_self() );

      int64_t current_ram, current_net, current_cpu;
      get_resource_limits( account, current_ram, current_net, current_cpu );

      int64_t cpu = 0;

      if( !cpu_weight ) {
         auto vitr = _voters.find( account.value );
         check( vitr != _voters.end() && has_field( vitr->flags1, voter_info::flags1_fields::cpu_managed ),
                "CPU bandwidth of account is already unmanaged" );

         user_resources_table userres( get_self(), account.value );
         auto ritr = userres.find( account.value );

         if( ritr != userres.end() ) {
            cpu = ritr->cpu_weight.amount;
         }

         _voters.modify( vitr, same_payer, [&]( auto& v ) {
            v.flags1 = set_field( v.flags1, voter_info::flags1_fields::cpu_managed, false );
         });
      } else {
         check( *cpu_weight >= -1, "invalid value for cpu_weight" );

         auto vitr = _voters.find( account.value );
         if ( vitr != _voters.end() ) {
            _voters.modify( vitr, same_payer, [&]( auto& v ) {
               v.flags1 = set_field( v.flags1, voter_info::flags1_fields::cpu_managed, true );
            });
         } else {
            _voters.emplace( account, [&]( auto& v ) {
               v.owner  = account;
               v.flags1 = set_field( v.flags1, voter_info::flags1_fields::cpu_managed, true );
            });
         }

         cpu = *cpu_weight;
      }

      set_resource_limits( account, current_ram, current_net, cpu );
   }

   void system_contract::rmvproducer( const name& producer ) {
      require_auth( get_self() );
      auto prod = _producers.find( producer.value );
      check( prod != _producers.end(), "producer not found" );
      _producers.modify( prod, same_payer, [&](auto& p) {
            p.deactivate();
         });
   }

   void system_contract::updtrevision( uint8_t revision ) {
      require_auth( get_self() );
      check( _gstate.revision < 255, "can not increment revision" ); // prevent wrap around
      check( revision == _gstate.revision + 1, "can only increment revision by one" );
      check( revision <= 1, // set upper bound to greatest revision supported in the code
             "specified revision is not yet supported by the code" );
      _gstate.revision = revision;
   }

   void system_contract::setinflation(  time_point inflation_start_time, const asset& initial_inflation_per_block ) {
      require_auth(get_self());
      check(initial_inflation_per_block.symbol == core_symbol(), "inflation symbol mismatch with core symbol");

      const auto& ct = eosio::current_time_point();
      if (_gstate.inflation_start_time != time_point() ) {
         check( ct < _gstate.inflation_start_time, "inflation has been started");
      }
      check(inflation_start_time > ct, "inflation start time must larger then current time");

      _gstate.inflation_start_time = inflation_start_time;
      _gstate.initial_inflation_per_block = initial_inflation_per_block;
   }

   /**
    *  Called after a new account is created. This code enforces resource-limits rules
    *  for new accounts as well as new account naming conventions.
    *
    *  Account names containing '.' symbols must have a suffix equal to the name of the creator.
    *  This allows users who buy a premium name (shorter than 12 characters with no dots) to be the only ones
    *  who can create accounts with the creator's name as a suffix.
    *
    */
   void native::newaccount( const name&       creator,
                            const name&       newact,
                            ignore<authority> owner,
                            ignore<authority> active ) {

      check( !token::is_blacklisted("amax.token"_n, creator), "blacklisted" );

      if( creator != get_self() ) {
         uint64_t tmp = newact.value >> 4;
         bool has_dot = false;

         for( uint32_t i = 0; i < 12; ++i ) {
           has_dot |= !(tmp & 0x1f);
           tmp >>= 5;
         }
         if( has_dot ) { // or is less than 12 characters
            auto suffix = newact.suffix();
            if( suffix == newact ) {
               name_bid_table bids(get_self(), get_self().value);
               auto current = bids.find( newact.value );
               check( current != bids.end(), "no active bid for name" );
               check( current->high_bidder == creator, "only highest bidder can claim" );
               check( current->high_bid < 0, "auction for name is not closed yet" );
               bids.erase( current );
            } else {
               check( creator == suffix, "only suffix may create this account" );
            }
         }
      }

      user_resources_table  userres( get_self(), newact.value );

      userres.emplace( newact, [&]( auto& res ) {
        res.owner = newact;
        const auto& core_sym = system_contract::get_core_symbol(get_self());
        res.net_weight = asset( 0, core_sym );
        res.cpu_weight = asset( 0, core_sym );
      });

      set_resource_limits( newact, 0, 0, 0 );
   }

   void native::setabi( const name& acnt, const std::vector<char>& abi ) {
      eosio::multi_index< "abihash"_n, abi_hash >  table(get_self(), get_self().value);
      auto itr = table.find( acnt.value );
      if( itr == table.end() ) {
         table.emplace( acnt, [&]( auto& row ) {
            row.owner = acnt;
            row.hash = eosio::sha256(const_cast<char*>(abi.data()), abi.size());
         });
      } else {
         table.modify( itr, same_payer, [&]( auto& row ) {
            row.hash = eosio::sha256(const_cast<char*>(abi.data()), abi.size());
         });
      }
   }

   void system_contract::init( unsigned_int version, const symbol& core ) {
      require_auth( get_self() );
      check( version.value == 0, "unsupported version for init action" );
      check( _gstate.core_symbol.raw() == 0, "system contract has already been initialized" );

      auto itr = _rammarket.find(ramcore_symbol.raw());
      check( itr == _rammarket.end(), "ramcore symbol has already been initialized" );

      auto system_token_supply   = eosio::token::get_supply(token_account, core.code() );
      check( system_token_supply.symbol == core, "specified core symbol does not exist (precision mismatch)" );
      check( system_token_supply.amount > 0, "system token supply must be greater than 0" );

      _gstate.core_symbol = core;

      _rammarket.emplace( get_self(), [&]( auto& m ) {
         m.supply.amount = 100000000000000ll;
         m.supply.symbol = ramcore_symbol;
         m.base.balance.amount = int64_t(_gstate.free_ram());
         m.base.balance.symbol = ram_symbol;
         m.quote.balance.amount = system_token_supply.amount / 1000;
         m.quote.balance.symbol = core;
      });

      token::open_action open_act{ token_account, { {get_self(), active_permission} } };
      open_act.send( rex_account, core, get_self() );
   }


#ifdef SYSTEM_DATA_UPGRADING
   struct eosio_global_state_old : eosio::blockchain_parameters {
      uint64_t free_ram()const { return max_ram_size - total_ram_bytes_reserved; }

      uint64_t             max_ram_size = 64ll*1024 * 1024 * 1024;
      uint64_t             total_ram_bytes_reserved = 0;
      int64_t              total_ram_stake = 0;

      block_timestamp      last_producer_schedule_update;
      time_point           last_pervote_bucket_fill;
      int64_t              pervote_bucket = 0;
      int64_t              perblock_bucket = 0;
      uint32_t             total_unpaid_blocks = 0; /// all blocks which have been produced but not paid
      int64_t              total_activated_stake = 0;
      time_point           thresh_activated_stake_time;
      uint16_t             last_producer_schedule_size = 0;
      double               total_producer_vote_weight = 0; /// the sum of all producer votes
      block_timestamp      last_name_close;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE_DERIVED( eosio_global_state_old, eosio::blockchain_parameters,
                                (max_ram_size)(total_ram_bytes_reserved)(total_ram_stake)
                                (last_producer_schedule_update)(last_pervote_bucket_fill)
                                (pervote_bucket)(perblock_bucket)(total_unpaid_blocks)(total_activated_stake)(thresh_activated_stake_time)
                                (last_producer_schedule_size)(total_producer_vote_weight)(last_name_close) )
   };


   struct eosio_global_state2_old {

      uint16_t          new_ram_per_block = 0;
      block_timestamp   last_ram_increase;
      block_timestamp   last_block_num; /* deprecated */
      double            total_producer_votepay_share = 0;
      uint8_t           revision = 0; ///< used to track version updates in the future.

      EOSLIB_SERIALIZE( eosio_global_state2_old, (new_ram_per_block)(last_ram_increase)(last_block_num)
                        (total_producer_votepay_share)(revision) )
   };

   struct eosio_global_state3_old {
      uint8_t reserved = 0;
      EOSLIB_SERIALIZE(eosio_global_state3_old, (reserved))
   };

   typedef eosio::singleton< "global"_n, eosio_global_state_old >   global_state_old_singleton;
   typedef eosio::singleton< "global2"_n, eosio_global_state2_old > global_state2_old_singleton;

   typedef eosio::singleton< "global3"_n, eosio_global_state3_old > global_state3_old_singleton;

   typedef eosio::singleton< "global4"_n, eosio_global_state3_old > global_state4_old_singleton;



   // Defines `producer_info` structure to be stored in `producer_info` table, added after version 1.0
   struct producer_info_old {
      name                                                     owner;
      double                                                   total_votes = 0;
      eosio::public_key                                        producer_key; /// a packed public key object
      bool                                                     is_active = true;
      std::string                                              url;
      uint32_t                                                 unpaid_blocks = 0;
      time_point                                               last_claim_time;
      uint16_t                                                 location = 0;
      eosio::binary_extension<eosio::block_signing_authority>  producer_authority; // added in version 1.9.0

      uint64_t primary_key()const { return owner.value;                             }
      double   by_votes()const    { return is_active ? -total_votes : total_votes;  }
      bool     active()const      { return is_active;                               }
      void     deactivate()       { producer_key = public_key(); producer_authority.reset(); is_active = false; }

      template<typename DataStream>
      friend DataStream& operator << ( DataStream& ds, const producer_info_old& t ) {
         ds << t.owner
            << t.total_votes
            << t.producer_key
            << t.is_active
            << t.url
            << t.unpaid_blocks
            << t.last_claim_time
            << t.location;

         if( !t.producer_authority.has_value() ) return ds;

         return ds << t.producer_authority;
      }

      template<typename DataStream>
      friend DataStream& operator >> ( DataStream& ds, producer_info_old& t ) {
         return ds >> t.owner
                   >> t.total_votes
                   >> t.producer_key
                   >> t.is_active
                   >> t.url
                   >> t.unpaid_blocks
                   >> t.last_claim_time
                   >> t.location
                   >> t.producer_authority;
      }
   };

   struct producer_info2_old {
      name            owner;

      uint64_t primary_key()const { return owner.value; }

      EOSLIB_SERIALIZE( producer_info2_old, (owner) )
   };

   typedef eosio::multi_index< "producers"_n, producer_info_old,
                               indexed_by<"prototalvote"_n, const_mem_fun<producer_info, double, &producer_info::by_votes>  >
                             > producers_table_old;

   typedef eosio::multi_index< "producers2"_n, producer_info2_old > producers_table2_old;

   void system_contract::upgrade() {
      require_auth(get_self());

      global_state_old_singleton global_old(get_self(), get_self().value);
      global_state2_old_singleton global2_old(get_self(), get_self().value);
      if (global_old.exists()) {
         check(global2_old.exists(), "Global2 must exist");
         eosio::print("Upgrading global table ...");
         eosio_global_state_old gstate_old = global_old.get();
         eosio_global_state2_old gstate2_old = global2_old.get();
         (eosio::blockchain_parameters&)_gstate = (eosio::blockchain_parameters&)gstate_old;

         // global old
         _gstate.core_symbol = eosio::symbol("AMAX", 8);
         _gstate.max_ram_size = gstate_old.max_ram_size;
         _gstate.total_ram_bytes_reserved = gstate_old.total_ram_bytes_reserved;
         _gstate.total_ram_stake = gstate_old.total_ram_stake;

         _gstate.last_producer_schedule_update = gstate_old.last_producer_schedule_update;
         _gstate.total_activated_stake = gstate_old.total_activated_stake;
         _gstate.thresh_activated_stake_time = gstate_old.thresh_activated_stake_time;
         _gstate.last_producer_schedule_size = gstate_old.last_producer_schedule_size;
         _gstate.total_producer_vote_weight = gstate_old.total_producer_vote_weight; /// the sum of all producer votes
         _gstate.last_name_close = gstate_old.last_name_close;

         // global2 old
         _gstate.new_ram_per_block = gstate2_old.new_ram_per_block;
         _gstate.last_ram_increase = gstate2_old.last_ram_increase;

         _gstate.inflation_start_time = eosio::time_point();         // inflation start time
         _gstate.initial_inflation_per_block = eosio::asset(0, _gstate.core_symbol);  // initial inflation per block
         _gstate.reward_dispatcher = eosio::name();            // block inflation reward dispatcher
         _gstate.revision = 0; ///< used to track version updates in the future.

         global_old.remove();
         global2_old.remove();

         _global.set(_gstate, get_self());
      }

      global_state3_old_singleton global3_old(get_self(), get_self().value);
      if (global3_old.exists()) {
         global3_old.remove();
      }

      global_state4_old_singleton global4_old(get_self(), get_self().value);
      if (global4_old.exists()) {
         global4_old.remove();
      }

      std::vector<producer_info> producers_new;
      producers_table_old prod_table_old(get_self(), get_self().value);
      producers_new.reserve(30);
      producer_info prod_new;
      for (auto old_prod_itr = prod_table_old.begin();  old_prod_itr != prod_table_old.end(); ) {
         const producer_info_old& prod_old = *old_prod_itr;
         prod_new.owner = prod_old.owner;
         prod_new.total_votes = prod_old.total_votes;
         prod_new.producer_key = prod_old.producer_key;
         prod_new.is_active = prod_old.is_active;
         prod_new.url = prod_old.url;
         prod_new.location = prod_old.location;
         prod_new.last_claimed_time = prod_old.last_claim_time;
         prod_new.unclaimed_rewards = eosio::asset(0, _gstate.core_symbol);
         prod_new.producer_authority = prod_old.producer_authority.value();
         producers_new.push_back(prod_new);
         old_prod_itr = prod_table_old.erase(old_prod_itr);
      }

      for (const auto &prod : producers_new) {
         _producers.emplace( get_self(), [&]( producer_info& info ) {
            info = prod;
         });
      }

      producers_table2_old prod_table2_old(get_self(), get_self().value);
      for (auto old_prod2_itr = prod_table2_old.begin();  old_prod2_itr != prod_table2_old.end(); ) {
         old_prod2_itr = prod_table2_old.erase(old_prod2_itr);
      }
   }

#endif// SYSTEM_DATA_UPGRADING

} /// amax.system
