// coding: utf-8
#include <iostream>

#define DEBUG(arg) std::cout << #arg "\t" << (arg) << std::endl ;

using namespace std ;

void imprimer(int i) {
	DEBUG(i)
}


int main() {

	const int i = 10 ;

	imprimer(i) ;

	*(const_cast<int *>(&i)) = 1000 ;

	imprimer(i) ;

	return 0 ;
}
