#ifndef REDUCER_H_
#define REDUCER_H_

#include "Mapper.h"
#include <vector>
#include <unordered_map>

#include <iostream>

using std::vector;
using std::unordered_map;

// Clasa generica care descrie o serie de thread-uri care executa operatia
// de reduce pe un set de date
// KO, VO -> tipul cheilor si a valorilor asociate acestor chei
// UR (User Reducer) -> un obiect dintr-o clasa care implementeaza operator(),
// mai exact functia de reducere data de utilizator
template<typename KO, typename VO,
	 typename UR>
class Reducer {
public:
	// Structura care descrie elementele rezultate in urma
	// rularii thread-urilor de mapper, este folosita o structura
	// in caz ca va fi nevoie de preluarea de mai multe elemente
	// de la mapper
	struct MapperElems;

	// Ca aceasta clasa sa fie generica, este posibila initializarea
	// fara utilizarea in prealabil a unui mapper
	bool useMapper;
	struct MapperElems *mapperElems;

	// Atunci cand se cere initializarea de la datele de iesire a
	// unui mapper, dar si pentru a respecta cerinta problemei,
	// mai exact pornirea simultana a thread-urilor mapper si reducer,
	// va fi nevoie ca exact unul din thread-urile reducer sa
	// initializeze membrii obiectului
	pthread_mutex_t reducerInitLock;
	bool isReducerInit;

	// R-ul din problema
	const unsigned int numReducerThreads;
	
	vector<pthread_t> threads;

	// Valorile sunt luate ca atare din mapper, va fi rolul
	// fiecarui thread sa grupeze toate valorile
	// care corespund aceleiasi chei
	vector<unordered_map<KO, vector<VO>>> intermediaryMaps;

	// Hashmap-ul in care se pun valorile grupate dupa cheie
	unordered_map<KO, vector<VO>> groupedMaps;

	// Functie care grupeaza toate valorile asociate unei
	// chei si le stocheaza in groupedMaps
	void groupIntermediaryByKey(KO &);

	// Avand o lista de hashmap-uri, trebuie aflat mai intai
	// ce chei trebuie procesate. E nevoie sa fie stiute cheile
	// pentru ca trebuie alocate la thread-uri
	unordered_set<KO> keySet;
	void findAvailableKeys();

	// Un hashmap care contine alocari thread <-> cheie procesata
	unordered_map<int, KO> keyMap;

	// Realizeaza alocarea cheilor la thread-uri pornind de la
	// setul de chei cunoscute
	void initKeyMapFromKeySet();

	// Hashmap-ul care contine rezultatele finale
	//
	// In cerinta temei se cere ca rezultatul sa fie scris in fisier
	// de catre thread, dar in alte situatii poate fi util ca rezultatele
	// sa fie pastrate ca sa poata fi folosite mai tarziu
	unordered_map<KO, vector<VO>> finalMap;

	bool useSharedBarrier;
	pthread_barrier_t *sharedBarrier;

	// Tot ca la mapper, utilizatorul va pasa o functie sau o clasa care implementeaza
	// operator(), aceasta fiind efectiv functia care va realiza operatia de mapare
	// Aceasta functie trebuie sa respecte urmatoarea semnatura:
	// vector<VO> (*userMapFunc)(KO &key, vector<VO> &values)
	//
	// Ca aceasta clasa sa fie mai generica, functia intoarce vectorul cu elementele
	// asociate cheii date ca argument, vectorul fiind stocat in finalMap
	UR userReducerObj;

	// Structura care contine argumentele necesare rularii unui thread
	struct ThreadArgs;

	// Functia care va fi rulata in interiorul unui thread
	static void* threadFunc(void *);

	// Argumentele care vor fi date fiecarui thread
	vector<struct ThreadArgs*> threadArgsVec;
public:
	// Reducer-ul poate fi initializat fara sa fie neaparat nevoie de un mapper
	Reducer(const unsigned int, vector<unordered_map<KO, vector<VO>>> &,
			UR userReducerObj = UR());

	Reducer(const unsigned int,
			UR userReducerObj = UR());

	~Reducer();

	// Seteaza mapper-ul de la care se vor prelua datele intermediare
	template<typename KI, typename UM, typename C>
	void setMapper(Mapper<KI, KO, VO, UM, C> &);

	void run();

	void join();

	void setSharedBarrier(pthread_barrier_t *barrier);

	bool& UseSharedBarrier();
};

#include "Reducer.cpp"

#endif
