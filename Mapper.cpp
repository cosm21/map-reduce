template<typename KI, typename KO, typename VO, typename UM, typename C>
Mapper<KI, KO, VO, UM, C>::Mapper(unsigned int numMapperThreads,
			   vector<KI> inputElems,
			   UM userMapObj,
			   C compare)
	: numMapperThreads(numMapperThreads),
	  inputElems(priority_queue<KI, vector<KI>, C> (compare, inputElems)),
	  userMapObj(userMapObj) {
	threads.resize(numMapperThreads);

	pthread_mutex_init(&inputElemsLock, NULL);
	numInputElems = inputElems.size();

	intermediaryMaps.resize(numMapperThreads);

	sharedBarrier = NULL;
	useSharedBarrier = false;
}

template<typename KI, typename KO, typename VO, typename UM, typename C>
Mapper<KI, KO, VO, UM, C>::~Mapper() {
	pthread_mutex_destroy(&inputElemsLock);

	for (struct ThreadArgs* &threadArgs : threadArgsVec)
		delete threadArgs;
}

template<typename KI, typename KO, typename VO, typename UM, typename C>
struct Mapper<KI, KO, VO, UM, C>::ThreadArgs {
public:
	// Functiile de thread au acces la toti membrii clasei,
	// dar functia data de utilizator va primi acces doar la 
	// ce are nevoie
	Mapper<KI, KO, VO, UM, C> &mapper;
	const int id;

	ThreadArgs(Mapper<KI, KO, VO, UM, C> &mapper, const int id)
		: mapper(mapper),
		  id(id) {
	}
};

// Functia care este rulata de un thread
template<typename KI, typename KO, typename VO, typename UM, typename C>
void* Mapper<KI, KO, VO, UM, C>::threadFunc(void *args) {
	struct ThreadArgs threadArgs = *((struct ThreadArgs*)args);
	Mapper<KI, KO, VO, UM, C> &mapper = threadArgs.mapper;

	while (true) {
		// Incerc sa extrag un element de la intrare, dar pentru ca
		// trebuie sa il scot, operatia poate fi executata
		// doar de un singur thread la un moment dat
		pthread_mutex_lock(&mapper.inputElemsLock);

		// Daca nu au mai ramas elemente atunci pot opri executia
		// thread-ului
		if (mapper.numInputElems == 0) {
			pthread_mutex_unlock(&mapper.inputElemsLock);
			break;
		}

		// Daca mai sunt elemente de procesat atunci extrag unul
		KI inputElem = mapper.inputElems.top();
		mapper.inputElems.pop();
		mapper.numInputElems--;

		// Las si restul thread-urilor sa extraga elemente
		pthread_mutex_unlock(&mapper.inputElemsLock);

		// Aici se ruleaza functia data de utilizator, desi thread-ul
		// are access la toti membii clasei Mapper, functia utilizatorului
		// va primi access doar la cheia de la intrare si hashmap-ul pe care
		// trebuie sa-l completeze
		mapper.userMapObj(inputElem, mapper.intermediaryMaps[threadArgs.id]);
	}

	// Daca utilizatorul cere sincronizarea cu thread-urile reducer atunci
	// se asteapta bariera
	if (mapper.useSharedBarrier && mapper.sharedBarrier != NULL)
		pthread_barrier_wait(mapper.sharedBarrier);

	pthread_exit(0);
	return NULL;
}


template<typename KI, typename KO, typename VO, typename UM, typename C>
void Mapper<KI, KO, VO, UM, C>::run() {
	// Se creeaza argumentele pentru thread-uri si thread-urile
	// in sine
	for (unsigned int i = 0; i < numMapperThreads; i++) {
		threadArgsVec.push_back(new ThreadArgs(*this, i));
		pthread_create(&threads[i], NULL, threadFunc,
				((void*)threadArgsVec[i]));
	}
}

template<typename KI, typename KO, typename VO, typename UM, typename C>
void Mapper<KI, KO, VO, UM, C>::join() {
	for (unsigned int i = 0; i < numMapperThreads; i++)
		pthread_join(threads[i], NULL);
}

template<typename KI, typename KO, typename VO, typename UM, typename C>
void Mapper<KI, KO, VO, UM, C>::setSharedBarrier(pthread_barrier_t *barrier) {
	sharedBarrier = barrier;
	useSharedBarrier = true;
}

template<typename KI, typename KO, typename VO, typename UM, typename C>
bool& Mapper<KI, KO, VO, UM, C>::UseSharedBarrier() {
	return useSharedBarrier;
}
