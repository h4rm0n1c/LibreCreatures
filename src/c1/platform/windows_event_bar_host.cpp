#include "windows_shell.hpp"
#include "windows_embedded_kit_host.hpp"

namespace creatures1::platform {


void C1EventBar::bind_document(C1WindowsDocument* document) {
    document_ = document;
}

void C1EventBar::update_status_panes( creatures1::ui::EventBarStatusApi& status) {
    event_bar_.update_status_panes(status);
}

void C1EventBar::refresh_object_display_panes( creatures1::ui::EventBarStatusApi& status, const creatures1::ui::EventBarObjectPolicyApi& policy) {
    event_bar_.refresh_object_display_panes(status, policy);
}

void C1EventBar::add_object(creatures1::objects::Object* object, creatures1::ui::EventBarObjectPolicyApi& policy) {
    event_bar_.add_object(object, policy);
}

void C1EventBar::remove_object(creatures1::objects::Object* object, bool record_auxiliary_state, creatures1::ui::EventBarObjectPolicyApi& policy) {
    event_bar_.remove_object(object, record_auxiliary_state, policy);
}

creatures1::ui::EventBar& C1EventBar::semantic_event_bar() { return event_bar_; }


creatures1::ui::StatusBarPaneInfo C1EventBar::pane_info( std::uint32_t pane_index) const {
    UINT command_id = 0;
    UINT style = 0;
    int width = 0;
    GetPaneInfo(static_cast<int>(pane_index), command_id, style, width);
    return {command_id, style, width};
}

creatures1::ui::StatusBarClientRect C1EventBar::client_rect() const {
    CRect rect;
    GetClientRect(&rect);
    return {rect.right};
}

bool C1EventBar::has_parent_window() const {
    return GetParent() != nullptr;
}

bool C1EventBar::parent_is_zoomed() const {
    const CWnd* parent = GetParent();
    return parent != nullptr && parent->IsZoomed() != FALSE;
}

std::uint32_t C1EventBar::window_style() const {
    return static_cast<std::uint32_t>(GetStyle());
}

std::int32_t C1EventBar::system_metric(std::int32_t metric) const {
    return GetSystemMetrics(metric);
}

void C1EventBar::def_window_proc(std::uint32_t message, std::uintptr_t pane_count, std::intptr_t pane_right_edges) {
    deferred_result_ = CStatusBar::DefWindowProc(
        static_cast<UINT>(message),
        static_cast<WPARAM>(pane_count),
        static_cast<LPARAM>(pane_right_edges));
}

LRESULT C1EventBar::WindowProc(UINT message, WPARAM w_param, LPARAM l_param) {
    // CEventBar overrides SB_SETPARTS (0x0404) to redistribute disabled
    // and spring panes before USER32 lays them out.  Keep that policy in
    // the clean EventBar class while the adapter supplies CStatusBar's
    // actual panes and default procedure.
    if (message == 0x0404 && l_param != 0) {
        deferred_result_ = 0;
        event_bar_.window_proc(
            *this, message, static_cast<std::uint32_t>(w_param),
            reinterpret_cast<std::int32_t*>(l_param));
        return deferred_result_;
    }
    return CStatusBar::WindowProc(message, w_param, l_param);
}

C1EventBarStatusAdapter::C1EventBarStatusAdapter(C1EventBar& event_bar, const C1WindowsDocument& document) : event_bar_(event_bar), document_(document) {}


std::string C1EventBarStatusAdapter::load_string(std::uint32_t resource_id) const {
    CStringA value;
    value.LoadStringA(static_cast<UINT>(resource_id));
    return value.GetString();
}

std::string C1EventBarStatusAdapter::object_pane_text( const creatures1::objects::Object& /*object*/, std::uint32_t resource_id) const {
    // The recovered CEventBar does not format an object name here.  It
    // selects 0xef1b/0xef1c/0xef1d and loads that localized caption;
    // object identity and the ten-entry display list are a separate
    // EventBar ownership boundary.
    return load_string(resource_id);
}


creatures1::ui::StatusBarTextMetrics C1EventBarStatusAdapter::measure_text( std::string_view text) const {
    C1EventBar& event_bar = const_cast<C1EventBar&>(event_bar_);
    CDC* device_context = event_bar.GetDC();
    if (device_context == nullptr) {
        return {};
    }
    const CStringA value(std::string(text).c_str());
    const CSize extent = device_context->GetTextExtent(value);
    event_bar.ReleaseDC(device_context);
    return {extent.cx};
}

void C1EventBarStatusAdapter::set_pane_text(std::uint32_t pane_index, std::string_view text) {
    event_bar_.SetPaneText(static_cast<int>(pane_index),
                           CStringA(std::string(text).c_str()));
}

void C1EventBarStatusAdapter::set_pane_width(std::uint32_t pane_index, std::int32_t width) {
    UINT command_id = 0;
    UINT style = 0;
    int current_width = 0;
    const int index = static_cast<int>(pane_index);
    event_bar_.GetPaneInfo(index, command_id, style, current_width);
    event_bar_.SetPaneInfo(index, command_id, style, width);
}

void C1EventBarStatusAdapter::set_pane_disabled(std::uint32_t pane_index, bool disabled) {
    UINT command_id = 0;
    UINT style = 0;
    int width = 0;
    const int index = static_cast<int>(pane_index);
    event_bar_.GetPaneInfo(index, command_id, style, width);
    if (disabled) {
        style |= 0x04000000;
        width = 0;
    } else {
        style &= ~static_cast<UINT>(0x04000000);
    }
    event_bar_.SetPaneInfo(index, command_id, style, width);
}

void C1EventBarStatusAdapter::invalidate_status_bar() {
    event_bar_.Invalidate(FALSE);
}

bool C1EventBarStatusAdapter::selected_creature_exists() const {
    return document_.selected_creature() != nullptr;
}

bool C1EventBarStatusAdapter::selected_creature_is_dead() const {
    const creatures1::creatures::Creature* creature =
        document_.selected_creature();
    return creature != nullptr && creature->life_state() !=
                                       creatures1::creatures::CreatureLifeState::alive;
}

std::uint8_t C1EventBarStatusAdapter::selected_creature_glycogen() const {
    // The native EventBar reads CBiochemistry::chemical_states[0x3b]
    // when formatting the selected creature's Life Force pane.  This is
    // Glycogen in the recovered C1 chemical table; it is not a cached UI
    // field and must be read from the selected Creature's biochemistry.
    const creatures1::creatures::Creature* creature =
        document_.selected_creature();
    if (creature == nullptr) {
        return 0;
    }
    return static_cast<std::uint8_t>(
        creature->chemical_concentration(0x3b));
}

creatures1::ui::EventBarScoreSnapshot C1EventBarStatusAdapter::score_snapshot() const {
    creatures1::ui::EventBarScoreSnapshot snapshot{};
    const creatures1::application::Document* document =
        document_.semantic_document();
    if (document != nullptr) {
        snapshot.natural_eggs_laid = static_cast<std::int32_t>(
            document->score.natural_eggs_laid);
        snapshot.population_time_accumulator = static_cast<std::int32_t>(
            document->score.population_time_accumulator);
    }
    return snapshot;
}

std::uint32_t C1EventBarStatusAdapter::world_tick_count() const {
    return document_.world_tick_count();
}

C1EventBarObjectAdapter::C1EventBarObjectAdapter(C1EventBar& event_bar, C1WindowsDocument& document) : event_bar_(event_bar), document_(document) {}


bool C1EventBarObjectAdapter::is_creature(const creatures1::objects::Object& object) const {
    // This is the exact native classifier discriminator used by
    // CEventBar.  The document then resolves the Object identity through
    // Creature::Skeleton rather than treating Creature as an Object.
    return (object.classifier_base() & 0xff000000U) == 0x04000000U;
}

bool C1EventBarObjectAdapter::creature_is_dead( const creatures1::objects::Object& object) const {
    const auto* creature = document_.creature_for_object(object);
    return creature != nullptr &&
           creature->life_state() !=
               creatures1::creatures::CreatureLifeState::alive;
}

std::uint32_t C1EventBarObjectAdapter::genome_filename_id( const creatures1::objects::Object& object) const {
    const auto* creature = document_.creature_for_object(object);
    return creature == nullptr ? 0
                               : creature->skeleton().genome_source_filename;
}

std::size_t C1EventBarObjectAdapter::funeral_state_word_count() const {
    return document_.funeral_state_word_count();
}

void C1EventBarObjectAdapter::append_funeral_state_word(std::uint32_t value) {
    document_.append_funeral_state_word(value);
}

void C1EventBarObjectAdapter::flush_funeral_kit_state() {
    // The native flush is an embedded-kit document-state notification.
    // The state itself is now retained by the C1 document; the COM/DDE
    // notification remains in the explicit kit boundary.
    document_.flush_funeral_state();
}

void C1EventBarObjectAdapter::disable_viewport_navigation() {
    document_.disable_viewport_navigation();
}

void C1EventBarObjectAdapter::refresh_display_panes() {
    document_.refresh_event_bar();
}

bool C1EventBarObjectAdapter::point_is_inside_pane(std::uint32_t pane_index, long x, long y) const {
    CRect pane_rect;
    C1EventBar& event_bar = const_cast<C1EventBar&>(event_bar_);
    event_bar.GetItemRect(static_cast<int>(pane_index), &pane_rect);
    return pane_rect.PtInRect(CPoint(static_cast<int>(x),
                                     static_cast<int>(y))) != FALSE;
}

int C1EventBarObjectAdapter::viewport_width() const {
    return document_.renderer_viewport_width();
}

int C1EventBarObjectAdapter::viewport_height() const {
    return document_.renderer_viewport_height();
}

void C1EventBarObjectAdapter::request_viewport_origin(int x, int y) {

    document_.request_event_bar_viewport_origin(x, y);
}

bool C1EventBarObjectAdapter::viewport_navigation_is_disabled() const {
    return document_.viewport_navigation_is_disabled();
}

void C1EventBarObjectAdapter::select_creature(creatures1::objects::Object& object) {
    auto* creature = document_.mutable_creature_for_object(object);
    if (creature != nullptr) {
        creatures1::application::apply_selected_creature(
            document_, creature, true);
    }
}

void C1EventBarObjectAdapter::notify_embedded_kit_of_death(
    creatures1::objects::Object& object) {
    // NotifyEmbeddedKit9OfCreatureDeath @ 0040e2d0: launch the Funeral Kit if
    // its slot is not connected, then send it the deceased creature's genome
    // filename.  The launch-then-send decision is application policy; this
    // adapter supplies the slot state and the dispatch.
    creatures1::application::notify_funeral_kit_of_creature_death(
        *this, genome_filename_id(object));
}

bool C1EventBarObjectAdapter::send_kit_message(
    std::size_t tool_index,
    const creatures1::application::EmbeddedKitMessage& message) {
    C1MainFrame* frame = DYNAMIC_DOWNCAST(C1MainFrame, AfxGetMainWnd());
    if (frame == nullptr) {
        return false;
    }
    COleDispatchDriver& driver = frame->embedded_kit_dispatch(tool_index);
    if (driver.m_lpDispatch == nullptr) {
        return false;
    }
    return invoke_kit_communicate(driver, message);
}


void C1EventBarObjectAdapter::flush_funeral_kit_document_state() {
    document_.flush_funeral_state();
}

std::string C1EventBarObjectAdapter::object_display_name( const creatures1::objects::Object& object) const {
    const auto* creature = document_.creature_for_object(object);
    return creature == nullptr ? std::string() : creature->display_name();
}

std::string C1EventBarObjectAdapter::unnamed_creature_label() const {
    CStringA label;
    label.LoadStringA(0xef1f);
    return label.GetString();
}

bool C1EventBarObjectAdapter::embedded_kit_is_connected(
    std::size_t tool_index) const {
    C1MainFrame* frame = DYNAMIC_DOWNCAST(C1MainFrame, AfxGetMainWnd());
    return frame != nullptr &&
           frame->embedded_kit_dispatch(tool_index).m_lpDispatch != nullptr;
}

void C1EventBarObjectAdapter::execute_embedded_kit_tool(std::size_t tool_index) {
    // Launch ordering is application policy; COM activation and process
    // handling belong to the kit host.
    C1MainFrame* frame = DYNAMIC_DOWNCAST(C1MainFrame, AfxGetMainWnd());
    if (frame == nullptr) {
        return;
    }
    WindowsEmbeddedKitHost host(*frame);
    creatures1::application::execute_embedded_kit_tool(host, tool_index);
}


BEGIN_MESSAGE_MAP(C1EventBar, CStatusBar)
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

void C1EventBar::OnLButtonDown(UINT flags, CPoint point) {
    if (document_ == nullptr) {
        CStatusBar::OnLButtonDown(flags, point);
        return;
    }

    C1EventBarObjectAdapter adapter(*this, *document_);
    event_bar_.on_left_button_down(static_cast<std::uint32_t>(flags),
                                   point.x, point.y, adapter, adapter);
}

} // namespace creatures1::platform
