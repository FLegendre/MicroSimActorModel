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
#define D(arg) arg
//~ #define D(arg)
#define RANGE(cont) cont.begin(), cont.end()

using namespace std ;

typedef unsigned int UInt ; 

// Message received by a market to start the process or to continue.
using go_a = caf::atom_constant<caf::atom("GO")>;
// Message about price : sent by a market and received by households.
using price_a = caf::atom_constant<caf::atom("PRICE")>;
// Message about quantity : sent by an household and received by a market.
using quant_a = caf::atom_constant<caf::atom("QUANT")>;
// Message about relative excess demande : sent by a market and received by the supervisor.
using red_a = caf::atom_constant<caf::atom("RED")> ;
// Message received by an household or by a market to stop.
using stop_a = caf::atom_constant<caf::atom("STOP")>;

// A market receives
//  * a message to start the process or to continue ;
//  * a message from an household with a quantity ;
//  * a message from the supervisor to stop.
using Market_t = caf::typed_actor<
     caf::replies_to<go_a>::with<void>
   , caf::replies_to<quant_a, UInt, double>::with<void>
   , caf::replies_to<stop_a>::with<void>
> ;

// An household receives
//  * a message from a market with a price ;
//  * a message from the supervisor to stop.
using Household_t = caf::typed_actor<
     caf::replies_to<price_a, UInt, double>::with<void>
   , caf::replies_to<stop_a>::with<void>
> ;

// The supervisor receives a message from a market with the relative excess demand.
using Supervisor_t = caf::typed_actor<caf::replies_to<red_a, UInt, double>::with<void>> ;

// The supervisor.
Supervisor_t supervisor ;
// All the markets in this economy.
vector<Market_t> markets ;
// All the households in this economy.
vector<Household_t> households ;

class Supervisor : public Supervisor_t::base {
public :
	Supervisor(UInt M)
	   : M_(M)
	   , check_(0)
	   , nr_received_reds_(0)
	   , crit_(0.)
	   { D(caf::aout(this) << "Constructing supervisor" << endl ;) }
protected :
	behavior_type make_behavior() override {
		return { [&](red_a, UInt m, double red) { do_receive_red(m, red) ; } } ;
	}
private:
	const UInt M_ ;
	size_t check_ ;
	UInt nr_received_reds_ ;
	double crit_ ;
	void do_receive_red(UInt m, double red) {
		D(caf::aout(this) << "Supervisor receives relative excess demande " << red <<
		   " from market #" << m << " -- nr_received_reds " << nr_received_reds_ << endl ;)
		check_ += m ;
		crit_ += red*red ;
		if ( ++ nr_received_reds_ == M_ )
			do_cont() ;
	}
	void do_cont() {
		caf::aout(this) << "Supervisor evaluates crit " << crit_ << endl ;
		// A simple way to partially check that each market sent a relative excess demand.
		assert ( check_ == ((M_-1)*M_/2) ) ;
		// Convergence achieved: send the stop signal to each market and to each household.
		if ( crit_ < .0001 ) {
			for ( const auto & m : markets )
				send(m, stop_a::value) ;
			for ( const auto & h : households )
				send(h, stop_a::value) ;
			quit() ;
		}
		else {
			check_ = nr_received_reds_ = 0, crit_ = 0. ;
			for ( const auto & m : markets )
				send(m, go_a::value) ;
		}
	}
} ;

class Market : public Market_t::base {
public:
	static UInt serial_number_ ;
	Market(size_t H) : id_(serial_number_++), H_(H), p_(1.)
	   { D(caf::aout(this) << "Constructing market #" << id_ << endl ;) }
protected:
	behavior_type make_behavior() override {
		return { 
			  [&](go_a) { do_go() ; }
			, [&](quant_a, UInt h, double q) { do_receive_quantity(h, q) ; }
			, [&](stop_a) { do_stop() ; }
		} ;
	}
private:
	const UInt id_ ;
	const UInt H_ ;
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
		send(supervisor, red_a::value, id_, red) ;
	}
	void do_go() {
		D(caf::aout(this) << "Market #" << id_ << " receives the go signal..." << endl ;)
		check_ = nr_received_quantities_ = 0 ;
		supply_ = demand_ = 0 ;
		for ( const auto & h : households )
			send(h, price_a::value, id_, p_) ;
	}
	void do_stop() {
		caf::aout(this) << "Market #" << id_ << " receives the stop signal, equilibrium price = " << p_ << endl ;
		quit() ;
	}
} ;
UInt Market::serial_number_ = 0 ;

class Household : public Household_t::base {
public :
	static UInt serial_number_ ;
	Household(const vector<float> & alphas, const vector<float> & endowments)
	   : id_(serial_number_++)
	   , alphas_(alphas)
	   , endowments_(endowments)
	   , check_(0)
	   , nr_received_prices_(0)
	   , prices_(alphas.size())
	   {
		assert( alphas.size() == endowments.size() ) ;
		D(caf::aout(this) << "Constructing household #" << id_ << endl ;) }
protected :
	behavior_type make_behavior() override {
		return { 
			  [&](price_a, UInt m, double p) { do_receive_price(m, p) ; }
			, [&](stop_a) { do_stop() ; }
		} ;
	}
private:
	const UInt id_ ;
	const vector<float> alphas_ ;
	const vector<float> endowments_ ;
	size_t check_ ;
	UInt nr_received_prices_ ;
	vector<double> prices_ ;

	void do_receive_price(UInt m, double p) {
		D(caf::aout(this) << "Household #" << id_ << " receives price " << p <<
		   " from market #" << m << " -- nr_received_prices " << nr_received_prices_ << endl ;)
		prices_.at(m) = p ;
		check_ += m ;
		if ( ++ nr_received_prices_ == prices_.size() )
			do_optimisation() ;
	}
	void do_optimisation() {
		D(caf::aout(this) << "Household #" << id_ << " doing optimisation..." << endl ;)
		// A simple way to partially check that each market sent a price.
		assert ( check_ == ((prices_.size()-1)*prices_.size()/2) ) ;
		const auto M = prices_.size() ;
		constexpr auto sig = 2. ;
		// First term to compute the general level of prices for this household.
		const auto sum = inner_product( RANGE(prices_), alphas_.begin(), 0.,
			 plus<double>(), [=] (double p, double alpha) { return pow(alpha, sig) * pow(p, 1.-sig) ; } ) ;
		// General level of prices for this household.
		const auto P = pow(sum, 1./(1.-sig)) ;
		// Value of initial endowment, i.e. revenu of consummer.
		const auto R = inner_product(RANGE(prices_), endowments_.begin(), 0.) ;
		for ( UInt m = 0 ; m <= M ; ++ m ) {
			const auto q = pow(alphas_[m], sig) * pow(prices_[m]/P, -sig) * R/P - endowments_[m] ;
			send(markets[m], quant_a::value, id_, q) ;
		}
		check_ = nr_received_prices_ = 0 ;
	}
	void do_stop() {
		D(caf::aout(this) << "Household #" << id_ << " receives the stop signal..." << endl ;)
		quit() ;
	}
} ;
UInt Household::serial_number_ = 0 ;

// This actor sends the start signal to each market and deads.
void start(caf::event_based_actor * self) {
	for ( const auto & m : markets )
		self->send(m, go_a::value) ;
	self->quit() ;
}

int main() {

	//~ constexpr UInt M = 100 ;
	//~ constexpr UInt H = 10*1000 ;
	constexpr UInt M = 2 ;
	constexpr UInt H = 3 ;

	default_random_engine rng ;
	uniform_real_distribution<double> ran_uni ;

	// Spawn the supervisor.
	supervisor = caf::spawn_typed<Supervisor>(M) ;

	households.reserve(H) ;
	markets.reserve(M) ;

	for ( size_t h = 0 ; h < H ; ++ h ) {

		// Set up the 𝛼 parameter for each good.
		vector<float> alphas ; alphas.reserve(M) ;
		for ( UInt m = 0 ; m < M ; ++ m )
			alphas.emplace_back(ran_uni(rng)) ;

		// Set up the initial endowment for each good.
		vector<float> endowments ; endowments.reserve(M) ;
		for ( UInt m = 0 ; m < M ; ++ m )
			endowments.emplace_back(100*ran_uni(rng)) ;

		households.emplace_back(caf::spawn_typed<Household>(alphas, endowments)) ;
	}

	for ( size_t m = 0 ; m < M ; ++ m )
		markets.emplace_back(caf::spawn_typed<Market>(H)) ;

	// Spawn the actor who sends the start signal to each market.
	caf::spawn(start) ;

	caf::await_all_actors_done() ;
	caf::shutdown() ;

	return 0 ;
}
