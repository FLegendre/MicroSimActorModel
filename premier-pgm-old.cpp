// coding: utf-8

#include <iostream>
#include <string>
#include <cassert>
#include <vector>

#define DEBUG(arg) std::cout << #arg "\t" << (arg) << std::endl ;

using namespace std ;

class Sal {
public:
Sal(const string & n, const string & p, double s) : nom_(n), pnom_(p), salaire_(s) {
	assert ( salaire_ > 0 ) ;
	}
string get_nom() const { return nom_ ; }
double get_salaire() const { return salaire_ ; }
void augmentation(double x) ; 
private:
	string nom_ ;
	string pnom_ ;
	double salaire_ ;
} ;

void Sal::augmentation(double x) {
	salaire_ = salaire_ + x ;
}


int main() {

	auto sal1 = Sal("Harakat", "Kawtar", 1000) ;
	auto sal2 = Sal("Legendre", "François", 2000) ;

	DEBUG(sal1.get_salaire())
	sal1.augmentation(2000) ;
	DEBUG(sal1.get_salaire())

	cout << "Le nom du premier salarié est " << sal1.get_nom() << endl ;
	cout << "Le nom du second salarié est " << sal2.get_nom() << endl ;

	{
		vector<Sal> sals ;
		sals.push_back(sal1) ;
		sals.push_back(sal2) ;

		cout << "Le nom du premier salarié est " << sals[0].get_nom() << endl ;
		cout << "Le nom du second salarié est " << sals[1].get_nom() << endl ;
	}
	{
		vector<Sal> sals ;
		sals.emplace_back("Harakat", "Kawtar", 1000) ;
		sals.emplace_back("Legendre", "François", 2000) ;

		cout << "Le nom du premier salarié est " << sals[0].get_nom() << endl ;
		cout << "Le nom du second salarié est " << sals[1].get_nom() << endl ;
	}


	vector<Sal> sals ;
	sals.push_back(sal1) ;
	sals.push_back(sal2) ;



	return 0 ;
}
