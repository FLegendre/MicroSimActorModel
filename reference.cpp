// coding: utf-8
#include <iostream>
#include <cassert>
#include <random>
#include <vector>
#include <cmath>
#include <numeric>
#include <functional>

#define DEBUG(arg) std::cout << #arg "\t" << (arg) << std::endl ;

typedef unsigned int UInt ; 

using namespace std ;

class Household {
public:
	// Constructor for an household.
	Household(UInt nr, const vector<float> & alphas, const vector<float> & endowments)
	   : nr_(nr), alphas_(alphas), endowments_(endowments) {
		assert( alphas_.size() == endowments_.size() ) ;
	}
	// This function returns the vector of supplies (if < 0) or demands (if >= 0) of this
	// houselhold for the prices given by the “prices” argument.
	vector<float> supplies_or_demands(const vector<double> & prices) const ;
	UInt nr() const { return nr_ ; }
private:
	UInt nr_ ;
	vector<float> alphas_ ;
	vector<float> endowments_ ;
} ;

vector<float> Household::supplies_or_demands(const vector<double> & prices) const {
	const auto I = prices.size() ;
	assert( I == alphas_.size() ) ;
	constexpr auto sig = 2. ;
	// First term to compute the general level of prices for this household.
	const auto sum = inner_product( prices.begin(), prices.end(), alphas_.begin(), 0.,
	   plus<double>(), [=] (double p, double alpha) { return pow(alpha, sig) * pow(p, 1.-sig) ; } ) ;
	// General level of prices for this household.
	const auto P = pow(sum, 1./(1.-sig)) ;
	// Value of initial endowment, i.e. revenu of consummer.
	const auto R = inner_product(prices.begin(), prices.end(), endowments_.begin(), 0.) ;
	vector<float> tmp ; tmp.reserve(I) ;
	for ( UInt i = 0 ; i <= I ; ++ i )
		tmp.emplace_back(pow(alphas_[i], sig) * pow(prices[i]/P, -sig) * R/P - endowments_[i]) ;
	return tmp ;
}

class Market {
public:
	Market(UInt nr, UInt H) : nr_(nr), quantities_(H) { } 
	void set_supply_or_demand(float q, UInt h) {
		assert( h < quantities_.size() ) ;
		quantities_[h] = q ;
	}
	double relative_excess_demand() const ;
private:
	UInt nr_ ;
	vector<float> quantities_ ;
} ;

double Market::relative_excess_demand() const {
	double supply = 0, demand = 0 ;
	for ( const auto & q : quantities_ )
		((q < 0) ? supply : demand) += q ;
	// Supply is accounted negatively.
	return (demand + supply) / ((-supply+demand)/2) ;
}

int main() {

	//~ constexpr UInt H = 10*1000 ;
	//~ constexpr UInt I = 100 ;

	constexpr UInt H = 100*1000 ;
	constexpr UInt I = 100 ;


	default_random_engine rng ;
	uniform_real_distribution<double> ran_uni ;

	// Populate the economy.
	vector<Household> houselholds ; houselholds.reserve(H) ;
	for ( UInt h = 0 ; h < H ; ++ h ) {

		// Set up the 𝛼 for each good.
		vector<float> alphas ; alphas.reserve(I) ;
		for ( UInt i = 0 ; i < I ; ++ i )
			alphas.emplace_back(ran_uni(rng)) ;

		// Set up the initial endowment for each good.
		vector<float> endowments ; endowments.reserve(I) ;
		for ( UInt i = 0 ; i < I ; ++ i )
			endowments.emplace_back(100*ran_uni(rng)) ;

		houselholds.emplace_back(h, alphas, endowments) ;
	}

	// Create the markets.
	vector<Market> markets ; markets.reserve(I) ;
	for ( UInt i = 0 ; i < I ; ++ i )
		markets.emplace_back(i, H) ;

	// Walrasian tâtonnement.

	vector<double> prices(I, 1.) ;

	for ( UInt s = 0 ; s < 100 ; ++ s ) {

		for ( const auto & household : houselholds ) {
			const auto q = household.supplies_or_demands(prices) ;
			for ( UInt i = 0 ; i < I ; ++ i )
				markets[i].set_supply_or_demand(q[i], household.nr()) ;
		}

		double crit = 0. ;
		for ( UInt i = 0 ; i < I ; ++ i ) {
			const auto red = markets[i].relative_excess_demand() ;
			// Price update with respect to relative excess demand.
			prices[i] = prices[i] * (1.+.25*red) ;
			crit += red*red ;
		}
		DEBUG(crit)
		if ( crit < .0001 )
			break ;

	}

	return 0 ;
}
