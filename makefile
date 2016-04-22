all : reference actor-model-I actor-model-II premier-pgm bidouille
#~ all : reference actor-model-I premier-pgm bidouille

reference : reference.cpp
	g++ -g -std=c++11 reference.cpp --output reference

actor-model-I : actor-model-I.cpp
	g++ -g -std=c++11 actor-model-I.cpp -lcaf_core -lcaf_io --output actor-model-I

actor-model-II : actor-model-II.cpp
	g++ -g -std=c++11 actor-model-II.cpp -lcaf_core -lcaf_io --output actor-model-II

premier-pgm : premier-pgm.cpp
	g++ -g -std=c++11 premier-pgm.cpp --output premier-pgm

bidouille : bidouille.cpp
	g++ -g -std=c++11 bidouille.cpp --output bidouille
