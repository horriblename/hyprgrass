#include "globals.hpp"
#include "wayfire/touch/touch.hpp"
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/helpers/Vector2D.hpp>
class WorkspaceSwipeManager {
  public:
    void beginSwipe(uint32_t time, const wf::touch::point_t& pos_in_percent);
    void endSwipe(uint32_t time, bool cancelled);
    void moveSwipe(uint32_t time, const wf::touch::point_t& pos_in_percent);

    bool isActive() const;

  private:
    wf::touch::point_t m_lastPosInPercent;
    bool m_active = false;
};
