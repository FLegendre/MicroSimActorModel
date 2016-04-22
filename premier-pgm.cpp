// coding: utf-8

#include <iostream>
#include <string>
#include <cassert>
#include <vector>

using namespace std ;

class Sal {
public:
Sal(const string & n, const string & p, double s) : nom_(n), pnom_(p), salaire_(s) {
	assert ( salaire_ > 0 ) ;
	}
string get_nom() const { return nom_ ; }
private:
	string nom_ ;
	string pnom_ ;
	double salaire_ ;
} ;



int main() {

	const auto sal1 = Sal("Harakat", "Kawtar", 1000) ;
	const auto sal2 = Sal("Legendre", "François", 2000) ;

	cout << "Le nom du premier salarié est " << sal1.get_nom() << endl ;
	cout << "Le nom du second salarié est " << sal2.get_nom() << endl ;

	vector<Sal> sals ;
	sals.push_back(sal1) ;
	sals.push_back(sal2) ;

	cout << "Le nom du premier salarié est " << sals[0].get_nom() << endl ;
	cout << "Le nom du second salarié est " << sals[1].get_nom() << endl ;



	return 0 ;
}
