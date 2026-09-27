
#ifndef _MODAL_VIEW_H_
#define _MODAL_VIEW_H_

#include "View.h"

class ModalView : public View {
  public:
    ModalView(View &);
    virtual ~ModalView();

    bool IsFinished();
    virtual bool IsModal() { return true; }
    // B backs out one step, on press, with no side effects: by default it
    // closes the dialog (return code 0). A dialog with steps inside (a
    // folder, a recording take, typed letters) overrides this to go back
    // one of them first. Return false to handle B in ProcessButtonMask.
    virtual bool Back();
    int GetReturnCode();

  protected:
    void SetWindow(int width, int height);
    virtual void ClearRect(int x, int y, int w, int h);
    virtual void DrawString(int x, int y, const char *txt,
                            GUITextProperties &props);
    void EndModal(int returnCode);
    // Window position in characters, for pixel graphics
    int windowLeft() const { return left_; }
    int windowTop() const { return top_; }

  private:
    bool finished_;
    int returnCode_;
    int left_;
    int top_;
};
#endif