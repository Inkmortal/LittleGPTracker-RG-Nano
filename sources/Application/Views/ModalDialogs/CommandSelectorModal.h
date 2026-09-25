#ifndef _COMMAND_SELECTOR_MODAL_H_
#define _COMMAND_SELECTOR_MODAL_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include "Application/Views/BaseClasses/View.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Views/CommandSelectorCommon.h"

class CommandSelectorModal : public ModalView {
  public:
    CommandSelectorModal(View &parentView, FourCC *liveTarget,
                        ModalViewCallback previewCb = 0);
    virtual ~CommandSelectorModal();

    virtual void ProcessButtonMask(unsigned short mask, bool pressed);
    virtual void GetGuideTopic(const char *&page, const char *&section) {
        page = "commands";
        section = "";
    }
    virtual void DrawView();
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int tick = 0);
    virtual void OnFocus();
    virtual void CustomizeContextOverlay(const char *&name, const char *&where,
                                         const char *&edit, const char *&field,
                                         const char *&cmd1, const char *&cmd2,
                                         const char *&cmd3, const char *&cmd4,
                                         const char *&cmd5, const char *&cmd6,
                                         const char *&cmd7);

  private:
    void navigateGrid(int deltaRow, int deltaCol);
    void moveToCommand(FourCC command);
    int commandToRow(FourCC command) const;
    int commandToCol(FourCC command) const;
    FourCC cellAtGridPos(int row, int col) const;

    int selectedRow_;
    int selectedCol_;
    FourCC selectedCommand_;
    View &parentView_;
    FourCC *liveTarget_;
    FourCC savedCmd_;
    ModalViewCallback previewCb_;

    static const int GRID_COLUMNS = CommandSelectorCommon::kColumns;
};

#endif
