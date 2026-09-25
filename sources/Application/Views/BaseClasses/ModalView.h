
#ifndef _MODAL_VIEW_H_
#define _MODAL_VIEW_H_

#include "View.h"

class ModalView : public View {
  public:
    ModalView(View &);
    virtual ~ModalView();

    bool IsFinished();
    virtual bool IsModal() { return true; }
    int GetReturnCode();

  protected:
    void SetWindow(int width, int height);
    virtual void ClearRect(int x, int y, int w, int h);
    virtual void DrawString(int x, int y, const char *txt,
                            GUITextProperties &props);
    void EndModal(int returnCode);
    // B tapped on its own (pressed and let go with nothing else): back /
    // close. Waiting for the release keeps B + Up/Down free for paging.
    // Call with every press and release.
    bool backTapped(unsigned short mask, bool pressed);
    // Window position in characters, for pixel graphics
    int windowLeft() const { return left_; }
    int windowTop() const { return top_; }

  private:
    bool finished_;
    int returnCode_;
    bool bTap_;
    int left_;
    int top_;
};
#endif