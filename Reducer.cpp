template<typename KO, typename VO, typename UR>
Reducer<KO, VO, UR>::Reducer(unsigned int numReducerThreads,
		vector<unordered_map<KO, vector<VO>>> &intermediaryMaps,
		UR userReducerObj)
	: numReducerThreads(numReducerThreads),
	  intermediaryMaps(intermediaryMaps),
	  userReducerObj(userReducerObj) {
	useMapper = false;
	mapperElems = NULL;

	pthread_mutex_init(&reducerInitLock, NULL);
	isReducerInit = true;
	threads.resize(numReducerThreads);

	findAvailableKeys();
	initKeyMapFromKeySet();
	
	useSharedBarrier = false;
	sharedBarrier = NULL;
}

template<typename KO, typename VO, typename UR>
Reducer<KO, VO, UR>::Reducer(const unsigned int numReducerThreads,
		UR userReducerObj)
	: numReducerThreads(numReducerThreads),
	  userReducerObj(userReducerObj) {
	// Desi valoarea este pusa pe fals, daca se foloseste
	// acest constructor va fi nevoie mai tarziu de un mapper
	// de la care sa fie preluate date
	useMapper = false;
	mapperElems = NULL;

	pthread_mutex_init(&reducerInitLock, NULL);
	isReducerInit = false;

	threads.resize(numReducerThreads);

	useSharedBarrier = false;
	sharedBarrier = NULL;
}

template<typename KO, typename VO, typename UR>
Reducer<KO, VO, UR>::~Reducer() {
	if (mapperElems != NULL)
		delete mapperElems;
	pthread_mutex_destroy(&reducerInitLock);

	for (struct ThreadArgs* &threadArgs : threadArgsVec)
		delete threadArgs;
}

template<typename KO, typename VO, typename UR>
struct Reducer<KO, VO, UR>::MapperElems {
public:
	// Pastrez referinta la hashmap-urile din mapper, astfel
	// atunci cand trebuie sa realizez initializarea,
	// acest lucru se poate face in interiorul unuia din thread-uri
	vector<unordered_map<KO, vector<VO>>> &intermediaryMaps;

	MapperElems(vector<unordered_map<KO, vector<VO>>> &intermediaryMaps)
		: intermediaryMaps(intermediaryMaps) {
	}
};


template<typename KO, typename VO, typename UR>
template<typename KI, typename UM, typename C>
void Reducer<KO, VO, UR>::setMapper(Mapper<KI, KO, VO, UM, C> &mapper) {
	if (mapperElems)
		delete mapperElems;

	// Se preiau din mapper structurile necesare
	mapperElems = new MapperElems(mapper.intermediaryMaps);
	useMapper = true;
}

template<typename KO, typename VO, typename UR>
void Reducer<KO, VO, UR>::groupIntermediaryByKey(KO &key) {
	for (unordered_map<KO, vector<VO>> &interMap : intermediaryMaps) {
		if (interMap.find(key) != interMap.end()) {
			vector<VO> &valuesVec = groupedMaps.at(key);
			vector<VO> &interMapVec = interMap.at(key);

			valuesVec.insert(valuesVec.end(),
					interMapVec.begin(),
					interMapVec.end());
		}
	}
}

template<typename KO, typename VO, typename UR>
void Reducer<KO, VO, UR>::findAvailableKeys() {
	for (auto &interMap : intermediaryMaps)
		for (auto &keyValuesPair : interMap)
			keySet.insert(keyValuesPair.first);
}

template<typename KO, typename VO, typename UR>
void Reducer<KO, VO, UR>::initKeyMapFromKeySet() {
	if (keySet.size() != numReducerThreads) {
		// O limitare a acestei implementari este ca numarul
		// de chei trebuie sa fie egal cu numarul de thread-uri
		// specificat la inceput 
		return;
	}

	// Aloc fiecarui thread cate o cheie
	int i = 0;
	for (auto key : keySet)
		keyMap[i++] = key;
}

template<typename KO, typename VO, typename UR>
struct Reducer<KO, VO, UR>::ThreadArgs {
public:
	Reducer<KO, VO, UR> &reducer;
	const int id;

	ThreadArgs(Reducer<KO, VO, UR> &reducer,
			const int id)
		: reducer(reducer),
		id(id) {
	}

};

template<typename KO, typename VO, typename UR>
void* Reducer<KO, VO, UR>::threadFunc(void *args) {
	struct ThreadArgs threadArgs = *((struct ThreadArgs*)args);
	Reducer<KO, VO, UR> &reducer = threadArgs.reducer;

	// Se realizeaza sincronizarea cu mapper, daca se cere
	if (reducer.useSharedBarrier && reducer.sharedBarrier != NULL)
		pthread_barrier_wait(reducer.sharedBarrier);

	// Daca se cere initializarea datelor de la un mapper, atunci
	// doar unul din thread-uri va trebuie sa realizeze acest lucru
	if (reducer.useMapper) {
		pthread_mutex_lock(&reducer.reducerInitLock);

		if (!reducer.isReducerInit) {
			reducer.intermediaryMaps = vector<unordered_map<KO, vector<VO>>>(
					reducer.mapperElems->intermediaryMaps);

			// Intai se gasesc cheile care au fost generate de mapper si apoi
			// se aloca la thread-uri
			reducer.findAvailableKeys();
			reducer.initKeyMapFromKeySet();

			// Se insereaza de la inceput toate cheile in hashmap-ul cu
			// valorile grupate si in cel final
			//
			// O alta varianta ar fi ca fiecare thread sa faca insertia
			// in aceste hashmap-uri la momentul potrivit, dar
			// nu ar reprezenta un castig de viteza pentru ca doar
			// un thread ar putea insera in hashmap la un moment dat
			// si ar mai fi nevoie de semafoare in plus
			for (auto &key : reducer.keySet)
				reducer.groupedMaps[key] = vector<VO>();
			for (auto &key : reducer.keySet)
				reducer.finalMap[key] = vector<VO>();

			reducer.isReducerInit = true;
		}

		pthread_mutex_unlock(&reducer.reducerInitLock);
	}

	// Fiecare thread realizeaza gruparea valorilor intermediare
	reducer.groupIntermediaryByKey(reducer.keyMap.at(threadArgs.id));

	// Se ruleaza functia data de utilizator, aceasta va avea acces doar la ce are nevoie
	KO &threadKey = reducer.keyMap.at(threadArgs.id);
	vector<VO> reducedValues = reducer.userReducerObj(threadKey, reducer.groupedMaps[threadKey]);

	// Se stocheaza rezultatul reducerii
	reducer.finalMap[reducer.keyMap.at(threadArgs.id)] = reducedValues;

	pthread_exit(0);
	return NULL;
}

template<typename KO, typename VO, typename UR>
void Reducer<KO, VO, UR>::run() {
	for (unsigned int i = 0; i < numReducerThreads; i++) {
		threadArgsVec.push_back(new ThreadArgs(*this, i));
		pthread_create(&threads[i], NULL, threadFunc, ((void*)threadArgsVec[i]));
	}
}

template<typename KO, typename VO, typename UR>
void Reducer<KO, VO, UR>::join() {
	for (unsigned int i = 0; i < numReducerThreads; i++)
		pthread_join(threads[i], NULL);
}

template<typename KO, typename VO, typename UR>
void Reducer<KO, VO, UR>::setSharedBarrier(pthread_barrier_t *barrier) {
	this->sharedBarrier = barrier;
	this->useSharedBarrier = true;
}

template<typename KO, typename VO, typename UR>
bool& Reducer<KO, VO, UR>::UseSharedBarrier() {
	return useSharedBarrier;
}
