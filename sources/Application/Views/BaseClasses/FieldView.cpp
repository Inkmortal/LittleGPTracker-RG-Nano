#include "FieldView.h"
#include "System/Console/Trace.h"

FieldView::FieldView(GUIWindow &w,ViewData *data):View(w,data),T_SimpleList<UIField>(true) {
	focus_=0 ;	
	nearestColumn_=false ;
} ;

// Up/Down on a grid of fields: of the fields on the row we move to, the one
// closest to the cursor's column (off: the row's first field, as always)
UIField *FieldView::nearestInRow(UIField *target) {
	if (!nearestColumn_ || !target || !focus_) return target ;
	int y=target->GetPosition()._y ;
	int x=focus_->GetPosition()._x ;
	UIField *best=target ;
	int bestDistance=-1 ;
	IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
	for (it->Begin();!it->IsDone();it->Next()) {
		UIField &current=it->CurrentItem() ;
		if (current.IsStatic() || current.GetPosition()._y!=y) continue ;
		int d=current.GetPosition()._x-x ;
		if (d<0) d=-d ;
		if (bestDistance<0 || d<bestDistance) {
			best=&current ;
			bestDistance=d ;
		}
	}
	return best ;
}

void FieldView::SetFocus(UIField *field) {

	if (focus_) {
		focus_->ClearFocus() ;
	}
	focus_=field ;

//  Empty field view, we don't have anything to do

	if (focus_==0) return ;

	focus_->SetFocus() ;

} ;

void FieldView::ClearFocus() {
	if (focus_) {
		focus_->ClearFocus() ;
	} ;
	focus_=0 ;
} ;

UIField *FieldView::GetFocus() {
    return focus_ ;
} ;

void FieldView::Redraw() {

	if (focus_==0) {
		SetFocus(T_SimpleList<UIField>::GetFirst()) ;
	}

	IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;

	for (it->Begin();!it->IsDone();it->Next()) {
		UIField &current=it->CurrentItem() ;
		current.Draw(w_) ;
	} ;
};

void FieldView::ProcessButtonMask(unsigned short mask) {

	if (focus_==0) {
		focus_=T_SimpleList<UIField>::GetFirst() ;
	//  Empty field view, we don't have anything to do
		if (focus_==0) return ;
 		focus_->SetFocus() ;
	}

	
	if (mask&EPBM_A) {  // A or A+ARROW is sent to the field
		if (mask&EPBM_DOWN) {
			focus_->ProcessArrow(EPBM_DOWN) ;
			isDirty_=true ;
		}
		if (mask&EPBM_UP){
			focus_->ProcessArrow(EPBM_UP)  ;
			isDirty_=true ;
		}

		if (mask&EPBM_LEFT) {
			focus_->ProcessArrow(EPBM_LEFT) ;
			isDirty_=true ;
		}

		if (mask&EPBM_RIGHT){
			focus_->ProcessArrow(EPBM_RIGHT)  ;
			isDirty_=true ;
		}

		if (mask==EPBM_A) {
			focus_->OnClick() ;
		};

	} else {
		if (mask&EPBM_B) {  // B or B+ARROW is sent to the field

			if (mask==EPBM_B) {
				focus_->OnBClick() ;
				isDirty_=true ;
			};

			if (mask&EPBM_DOWN) {
				focus_->ProcessBArrow(EPBM_DOWN) ;
				isDirty_=true ;
			}
			if (mask&EPBM_UP){
				focus_->ProcessBArrow(EPBM_UP)  ;
				isDirty_=true ;
			}

			if (mask&EPBM_LEFT) {
				focus_->ProcessBArrow(EPBM_LEFT) ;
				isDirty_=true ;
			}

			if (mask&EPBM_RIGHT){
				focus_->ProcessBArrow(EPBM_RIGHT)  ;
				isDirty_=true ;
			}

		} else { // Nor B or A is pressed

			if (!(mask&(EPBM_A|EPBM_B|EPBM_L|EPBM_R|EPBM_SELECT|EPBM_START))) {

				if (mask&EPBM_DOWN) {
					UIField *next=0 ;
					UIField *first=0 ;

					GUIPoint focusPos=focus_->GetPosition() ;

					IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
					for (it->Begin();!it->IsDone();it->Next()) {
						UIField &current=it->CurrentItem() ;
						if (!current.IsStatic()) {
							if (first) {
								if (current.GetPosition()._y<first->GetPosition()._y) {
									first=&current ;
								} ;
							} else {
								first=&current ;
							}
							if (current.GetPosition()._y>focus_->GetPosition()._y) {
								if (next) {
									if (current.GetPosition()._y<next->GetPosition()._y) {
										next=&current ;
									} else {
										// if both target at same height
									} ;
								} else {
									next=&current ;
								};
							} ;

						}
					}
					if (next==0) {
						next=first ;
					}
					next=nearestInRow(next) ;

					focus_->ClearFocus() ;
					focus_=next ;
					focus_->SetFocus() ;
					isDirty_=true ;
				}


				if (mask&EPBM_UP){

					UIField *prev=0 ;
					UIField *last=0 ;

					IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
					for (it->Begin();!it->IsDone();it->Next()) {
						UIField &current=it->CurrentItem() ;
						if (!current.IsStatic()) {
							if (last) {
								if (current.GetPosition()._y>last->GetPosition()._y) {
									last=&current ;
								} ;
							} else {
								last=&current ;
							}
							if (current.GetPosition()._y<focus_->GetPosition()._y) {
								if (prev) {
									if (current.GetPosition()._y>prev->GetPosition()._y) {
										prev=&current ;
									} else {
										// if both target at same height
									} ;
								} else {
									prev=&current ;
								};
							} ;

						}
					}
					if (prev==0) {
						prev=last ;
					}
					prev=nearestInRow(prev) ;

					focus_->ClearFocus() ;
 					focus_=prev ;
					focus_->SetFocus() ;
					isDirty_=true ;
				}

				if (mask&EPBM_RIGHT) {
					UIField *next=0 ;
					UIField *first=0 ;

					GUIPoint focusPos=focus_->GetPosition() ;

					IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
					for (it->Begin();!it->IsDone();it->Next()) {
						UIField &current=it->CurrentItem() ;
						if (!current.IsStatic()&&(current.GetPosition()._y==focus_->GetPosition()._y)) {
							if (first) {
								if (current.GetPosition()._x<first->GetPosition()._x) {
									first=&current ;
								} ;
							} else {
								first=&current ;
							}
							if (current.GetPosition()._x>focus_->GetPosition()._x) {
								if (next) {
									if (current.GetPosition()._x<next->GetPosition()._x) {
										next=&current ;
									} else {
										// if both target at same height
									} ;
								} else {
									next=&current ;
								};
							} ;

						}
					}
					if (next==0) {
						next=first ;
					}

					focus_->ClearFocus() ;
					focus_=next ;
					focus_->SetFocus() ;
					isDirty_=true ;
				}

				if (mask&EPBM_LEFT){

					UIField *prev=0 ;
					UIField *last=0 ;

					IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
					for (it->Begin();!it->IsDone();it->Next()) {
						UIField &current=it->CurrentItem() ;
						if (!current.IsStatic()&&(current.GetPosition()._y==focus_->GetPosition()._y)) {
							if (last) {
								if (current.GetPosition()._x>last->GetPosition()._x) {
									last=&current ;
								} ;
							} else {
								last=&current ;
							}
							if (current.GetPosition()._x<focus_->GetPosition()._x) {
								if (prev) {
									if (current.GetPosition()._x>prev->GetPosition()._x) {
										prev=&current ;
									} else {
										// if both target at same height
									} ;
								} else {
									prev=&current ;
								};
							} ;

						}
					}
					if (prev==0) {
						prev=last ;
					}

					focus_->ClearFocus() ;
 					focus_=prev ;
					focus_->SetFocus() ;
					isDirty_=true ;
				}
			}
		}
	}
}

int FieldView::GetFocusIndex() {

	int focusIndex=0 ;
	IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
	for (it->Begin();!it->IsDone();it->Next()) {
		if (&(it->CurrentItem())==focus_) {
			break ;
		} ;
		focusIndex++ ;
	} ;
	return focusIndex ;
}
