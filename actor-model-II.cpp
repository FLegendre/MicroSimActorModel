// coding: utf-8
#include <iostream>
#include <cassert>
#include <random>
#include <vector>
#include <cmath>
#include <numeric>
#include <functional>
#include <caf/all.hpp>

#define DEBUG(arg) std::cout << #arg "\t" << (arg) << std::endl ;
//~ #define D(arg) arg
#define D(arg)
#define RANGE(cont) cont.begin(), cont.end()

using namespace std ;

typedef unsigned int UInt ; 

// Message about prices : sent by the supervisor and received by households.
using price_a = caf::atom_constant<caf::atom("PRICE")>;
// Message about quantity : sent by an household and received by a market.
using quant_a = caf::atom_constant<caf::atom("QUANT")>;
// Message about price and relative excess demande : sent by a market and received by the supervisor.
using pred_a = caf::atom_constant<caf::atom("PRED")> ;
// Message received by an household or by a market to stop.
using stop_a = caf::atom_constant<caf::atom("STOP")>;

// A market receives
//  * a message from an household with a quantity ;
//  * a message from the supervisor to stop.
using MarketAddr = caf::typed_actor<
     caf::replies_to<quant_a, UInt, double>::with<void>
   , caf::replies_to<stop_a>::with<void>
> ;

// An household receives
//  * a message from the supervisor with a vector of prices ;
//  * a message from the supervisor to stop.
using HouseholdAddr = caf::typed_actor<
     caf::replies_to<price_a, vector<double>>::with<void>
   , caf::replies_to<stop_a>::with<void>
> ;

// The supervisor receives
//  * a message from a market with the price and the relative excess demand.
using SupervisorAddr = caf::typed_actor<
   caf::replies_to<pred_a, UInt, double, double>::with<void>
> ;

class Market : public MarketAddr::base {
public:
	static UInt serial_number_ ;
	Market(UInt H, SupervisorAddr supervisor)
	   : id_(serial_number_++)
	   , H_(H)
	   , supervisor_(supervisor)
	   , p_(1.)
	   {
		D(caf::aout(this) << "Constructing market #" << id_ << endl ;)
		iteration_init() ;
	}
protected:
	behavior_type make_behavior() override {
		return { 
			  [&](quant_a, UInt h, double q) { do_receive_quantity(h, q) ; }
			, [&](stop_a) { do_stop() ; }
		} ;
	}
private:
	const UInt id_ ;
	const UInt H_ ;
	const SupervisorAddr supervisor_ ;
	double p_ ;
	size_t check_ ;
	UInt nr_received_quantities_ ;
	double supply_, demand_ ;
	void do_receive_quantity(UInt h, double q) {
		D(caf::aout(this) << "Market #" << id_ << " receives quantity " << q << " from household #" << h << endl ;)
		((q < 0) ? supply_ : demand_) += q ;
		if ( ++ nr_received_quantities_ == H_ )
			do_price_update() ;
	}
	void do_price_update() {
		D(caf::aout(this) << "Market #" << id_ << " doing price update..." << endl ;)
		// Supply is accounted negatively.
		const auto red = (demand_ + supply_) / ((-supply_+demand_)/2) ;
		p_ *= (1.+.25*red) ;
		send(supervisor_, pred_a::value, id_, p_, red) ;
		iteration_init() ;
	}
	void do_stop() {
		caf::aout(this) << "Market #" << id_ << " receives the stop signal, equilibrium price = " << p_ << endl ;
		quit() ;
	}
	void iteration_init() {
		nr_received_quantities_ = 0 ;
		supply_ = demand_ = 0. ;
	}
} ;
UInt Market::serial_number_ = 0 ;

class Household : public HouseholdAddr::base {
public :
	static UInt serial_number_ ;
	Household(
	     const vector<float> & alphas
	   , const vector<float> & endowments
	   , const vector<MarketAddr> & markets
	   )
	   : id_(serial_number_++)
	   , alphas_(alphas)
	   , endowments_(endowments)
	   , markets_(markets)
	   {
		assert( alphas.size() == endowments.size() ) ;
		assert( alphas.size() == markets.size() ) ;
		D(caf::aout(this) << "Constructing household #" << id_ << endl ;) }
protected :
	behavior_type make_behavior() override {
		return { 
			  [&](price_a, const vector<double> & prices) { do_receive_price(prices) ; }
			, [&](stop_a) { do_stop() ; }
		} ;
	}
private:
	const UInt id_ ;
	const vector<float> alphas_ ;
	const vector<float> endowments_ ;
	const vector<MarketAddr> markets_ ;

	void do_receive_price(const vector<double> & prices) {
		D(caf::aout(this) << "Household #" << id_ << " receives prices " << *prices.begin() <<
		   " ... " << *prices.rbegin() << endl ;)
		const auto M = prices.size() ;
		assert ( M == alphas_.size() ) ;
		constexpr auto sig = 2. ;
		// First term to compute the general level of prices for this household.
		const auto sum = inner_product( RANGE(prices), alphas_.begin(), 0.,
			 plus<double>(), [=] (double p, double alpha) { return pow(alpha, sig) * pow(p, 1.-sig) ; } ) ;
		// General level of prices for this household.
		const auto P = pow(sum, 1./(1.-sig)) ;
		// Value of initial endowment, i.e. revenu of consummer.
		const auto R = inner_product(RANGE(prices), endowments_.begin(), 0.) ;
		for ( UInt m = 0 ; m < M ; ++ m ) {
			const auto q = pow(alphas_[m], sig) * pow(prices[m]/P, -sig) * R/P - endowments_[m] ;
			D(caf::aout(this) << "Household #" << id_ << " sends quantity " << q <<
			   " to market # " << m << endl ;)
			send(markets_[m], quant_a::value, id_, q) ;
		}
	}
	void do_stop() {
		D(caf::aout(this) << "Household #" << id_ << " receives the stop signal..." << endl ;)
		quit() ;
	}
} ;
UInt Household::serial_number_ = 0 ;

class Supervisor : public SupervisorAddr::base {
public :
	Supervisor(UInt M, UInt H)
	   : M_(M)
	   , H_(H)
		{
		D(caf::aout(this) << "Constructing supervisor" << endl ;)

		default_random_engine rng ;
		uniform_real_distribution<double> ran_uni ;

		const auto supervisor_address = address() ;

		// Spawn all the markets in this economy.
		markets_.reserve(M_) ;
		for ( size_t m = 0 ; m < M_ ; ++ m )
			markets_.emplace_back(caf::spawn_typed<Market>(H, this)) ;

		// Spawn all the households in this economy. Each household needs the market addresses to
		// send their the QUANT message.
		households_.reserve(H) ;
		for ( size_t h = 0 ; h < H ; ++ h ) {

			// Set up the 𝛼 parameter for each good.
			vector<float> alphas ; alphas.reserve(M_) ;
			for ( UInt m = 0 ; m < M ; ++ m )
				alphas.emplace_back(ran_uni(rng)) ;

			// Set up the initial endowment for each good.
			vector<float> endowments ; endowments.reserve(M_) ;
			for ( UInt m = 0 ; m < M_ ; ++ m )
				endowments.emplace_back(100*ran_uni(rng)) ;

			households_.emplace_back(caf::spawn_typed<Household>(alphas, endowments, markets_)) ;
		}

		iteration_init(), prices_.assign(M_, 1.) ;

		// Send the initial prices to households.
		for ( const auto & h : households_ )
			send(h, price_a::value, prices_) ;
	}
protected :
	behavior_type make_behavior() override {
		return {
			[&](pred_a, UInt m, double price, double red) { do_receive_pred(m, price, red) ; }
			} ;
	}
private:
	UInt M_ ;
	UInt H_ ;
	vector<double> prices_ ;
	size_t check_ ;
	UInt nr_received_reds_ ;
	double crit_ ;
	vector<HouseholdAddr> households_ ;
	vector<MarketAddr> markets_ ;

	void do_receive_pred(UInt m, double price, double red) {
		D(caf::aout(this) << "Supervisor receives price " << price <<
		   " and relative excess demande " << red << " from market #" << m <<
		   " -- nr_received_reds " << nr_received_reds_ << endl ;)
		prices_[m] = price ;
		check_ += m ;
		crit_ += red*red ;
		if ( ++ nr_received_reds_ == M_ )
			do_cont() ;
	}
	void do_cont() {
		caf::aout(this) << "Supervisor evaluates crit " << crit_ << endl ;
		// A simple way to partially check that each market sent a relative excess demand.
		assert ( check_ == ((M_-1)*M_/2) ) ;
		// Convergence achieved: send the stop signal to each market and to each household and dies.
		if ( crit_ < .0001 ) {
			for ( const auto & m : markets_ )
				send(m, stop_a::value) ;
			for ( const auto & h : households_ )
				send(h, stop_a::value) ;
			quit() ;
		}
		else {
			for ( const auto & h : households_ )
				send(h, price_a::value, prices_) ;
			iteration_init() ;
		}
	}
	void iteration_init() {
		check_ = nr_received_reds_ = 0, crit_ = 0. ;
	}
} ;

int main() {

	constexpr UInt M = 100 ;
	constexpr UInt H = 25*1000 ;
	//~ constexpr UInt M = 2 ;
	//~ constexpr UInt H = 3 ;

	// Spawn the supervisor.
	(void) caf::spawn_typed<Supervisor>(M, H) ;

	caf::await_all_actors_done() ;
	caf::shutdown() ;

	return 0 ;
}
