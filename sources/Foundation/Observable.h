
#pragma once

#include <vector>
#include "System/Process/SysMutex.h"

// Data to be passed from the observable to the observer

class I_ObservableData {
} ;

// The observer: Simply allows to be notified with data

class Observable ;

class I_Observer {
public:
	virtual ~I_Observer() {} ;
    virtual void Update(Observable &o,I_ObservableData *d)=0 ;
} ;

// The observable

class Observable {
public:
	Observable() ;
	virtual ~Observable() ;
	void AddObserver(I_Observer &o) ;
	void RemoveObserver(I_Observer &o) ;
	void RemoveAllObservers() ;
	int  CountObservers() ;

	inline void NotifyObservers() { NotifyObservers(0) ; } ;

	void NotifyObservers(I_ObservableData *d) ;

	void SetChanged() ;
	inline void ClearChanged() { _hasChanged=false ; } ;
	bool HasChanged() ;
private:
	// _mutex owns an SDL_mutex* it creates lazily: never copy an Observable
	Observable(const Observable &) ;
	Observable &operator=(const Observable &) ;

	std::vector<I_Observer *> _list ;
	bool _hasChanged ;
	// AudioDriver (and anything it notifies, e.g. AudioOutDriver) is
	// notified from the real-time audio callback thread while the main
	// thread can Add/RemoveObserver at the same time (a view opening,
	// closing or a project reload). Without this, NotifyObservers walking
	// _list while the main thread mutates it is a use-after-free / bad
	// iterator crash (seen as a SIGSEGV inside NotifyObservers under load).
	SysMutex _mutex ;
};


