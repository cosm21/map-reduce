#ifndef MAPPER_H_
#define MAPPER_H_

#include <pthread.h>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <iostream>

using std::vector;
using std::priority_queue;
using std::unordered_map;
using std::greater;
using std::unordered_set;

// Clasa generica care descrie o serie de thread-uri care executa operatia de map
// KI -> tipul cheilor date la intrare
// KO, VO -> tipul cheilor si a valorilor asociate cheilor pe care mapper-ul le
// scoate la iesire
// UM (User Mapper) -> functia de mapare data de utilizator
// C -> functia de comparare care este folosite pentru a ordona cheile
template<typename KI,
	 typename KO, typename VO,
	 typename UM, 
	 typename C = greater<KI>>
class Mapper {
private:
	// Numarul M din problema
	const unsigned int numMapperThreads;

	vector<pthread_t> threads;

	// Lock pe elementele de intrare pentru a evita race condition
	pthread_mutex_t inputElemsLock;
	// Elementele de intrare sunt tinute intr-un heap si ordonate
	// cu functia data de utilizator
	priority_queue<KI, vector<KI>, C> inputElems;
	unsigned int numInputElems;

	// Fiecare thread are access la cate un hashtable, functia data
	// de utilizator are rolul de a lua cheia de intrare si de a 
	// genera una sau mai multe chei si de a asocia valori acestora
	//
	// Nu exista data race-uri pentru ca fiecare thread acceseaza
	// un hastable diferit
	//
	// In final intermediaryMaps reprezinta output-ul tuturor thread-urilor
	// de mapare
	vector<unordered_map<KO, vector<VO>>> intermediaryMaps;

	bool useSharedBarrier;
	pthread_barrier_t *sharedBarrier;

	// Utilizatorul va da un obiect al unei clase care implementeaza operator()
	// Practic, semnatura functiei pe care trebuie sa o dea utilizatorul este
	// urmatoarea:
	// void (*userMapFunc)(KI&, unordered_map<KO, vector<KO>>&);
	//
	// Nu e neaparat nevoie ca utilizatorul sa creeze o clasa cu overload
	// pe operator(), se pot utiliza si functii lambda sau functii in stil C
	UM userMapObj;

	// O structura care contine argumentele necesare functiei care
	// se va rula in fiecare thread
	struct ThreadArgs;

	// Functia care va fi rulata in thread-uri
	static void* threadFunc(void *args);

	// Argumentele fiecarui thread
	vector<struct ThreadArgs*> threadArgsVec;

	// Clasa Reducer are nevoie de access la elementele din mapper ca sa
	// poata fi initializata corespunzator
	template<typename KOO, typename VOO, typename UR>
	friend class Reducer;

public:
	Mapper(unsigned int numMapperThreads,
			vector<KI> inputElems,
			UM userMapFuncObj = UM(),
			C compare = C());

	~Mapper();

	void run();

	void join();

	void setSharedBarrier(pthread_barrier_t *);

	bool& UseSharedBarrier();
};

#include "Mapper.cpp"

#endif
