
#include "Observable.h"

#include "T_SimpleList.h"

Observable::Observable() {
	_hasChanged=false ;
}

Observable::~Observable() {
}

void Observable::AddObserver(I_Observer &o) {
	SysMutexLocker lock(_mutex) ;
	_list.push_back(&o) ;
}

void Observable::RemoveObserver(I_Observer &o) {
	SysMutexLocker lock(_mutex) ;
	std::vector<I_Observer *>::iterator it=_list.begin() ;
	while (it!=_list.end()) {
		if (*it==&o) {
			_list.erase(it) ;
			break ;
		}
		it++ ;
	}
}

void Observable::RemoveAllObservers() {
	SysMutexLocker lock(_mutex) ;
	std::vector<I_Observer *>::iterator it=_list.begin() ;
	while (it!=_list.end()) {
		it=_list.erase(it) ;
	}
}

// Same order and ClearChanged() timing as before the lock was added (some
// Update() chains rely on _hasChanged only flipping once every observer has
// run): held for the whole call, so the audio thread can never see _list
// mid-mutation. SysMutex wraps SDL's mutex, which (like a Windows critical
// section) the same thread can lock again without deadlocking, so an
// Update() that calls back into Add/RemoveObserver/NotifyObservers on this
// same Observable is still safe.
void Observable::NotifyObservers(I_ObservableData *d) {
	SysMutexLocker lock(_mutex) ;
	if (_hasChanged) {
		std::vector<I_Observer *>::iterator it=_list.begin() ;
		while (it!=_list.end()) {
			I_Observer *o=*it++ ;
			o->Update(*this,d) ;
		}
		ClearChanged() ;
	}
}

void Observable::SetChanged() {
	_hasChanged=true ;
}

bool Observable::HasChanged() {
	return _hasChanged ;
}
