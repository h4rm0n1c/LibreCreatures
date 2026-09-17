#include "windows_shell.hpp"

#include "../scripting/macro.hpp"

#include <map>
#include <tuple>

namespace creatures1::platform {

void C1WindowsView::toggle_camera_tracking() {
    creatures1::ui::toggle_follow_selected_creature_mode(
        view_state_, view_settings_, *this);
}

void C1WindowsView::toggle_infinite_world() {
    creatures1::ui::toggle_infinite_scroll_world_size(
        *this, view_settings_);
    if (document() != nullptr) {
        document()->invalidate_renderer_view();
    }
}

void C1WindowsView::toggle_smooth_scrolling() {
    view_settings_.smooth_scrolling_enabled =
        !view_settings_.smooth_scrolling_enabled;
    if (document() != nullptr) {
        document()->set_renderer_smooth_scrolling(
            view_settings_.smooth_scrolling_enabled);
        document()->write_view_setting(
            "SmoothScrolling",
            view_settings_.smooth_scrolling_enabled ? 1 : 0);
    }
}

bool C1WindowsView::camera_tracks_selected_creature() const {
    return view_state_.viewport_navigation_mode ==
           creatures1::ui::ViewportNavigationMode::follow_selected_creature;
}

bool C1WindowsView::selected_creature_exists_for_command() const {
    return selected_creature_exists();
}

bool C1WindowsView::world_update_timer_is_running_for_command() const {
    return view_settings_.world_update_timer_is_running;
}

bool C1WindowsView::infinite_world_enabled() const {
    return view_settings_.half_height == 0x4b0;
}

bool C1WindowsView::smooth_scrolling_enabled() const {
    return view_settings_.smooth_scrolling_enabled;
}

void C1WindowsView::initialize_view_base() {
    C1WindowsDocument* document =
        DYNAMIC_DOWNCAST(C1WindowsDocument, GetDocument());
    if (document != nullptr) {
        document->bind_renderer_view(*this);
        document->bind_world_view(this);
    }
}

void C1WindowsView::set_navigation_mode(
    creatures1::ui::ViewportNavigationMode mode) {
    creatures1::ui::set_viewport_navigation_mode(
        view_state_, *this,
        mode == creatures1::ui::ViewportNavigationMode::manual);
}

void C1WindowsView::apply_sound_policy(
    creatures1::ui::SfcViewSoundPolicy policy) {
    switch (policy) {
    case creatures1::ui::SfcViewSoundPolicy::foreground_only:
        creatures1::ui::set_sound_foreground_policy(view_state_, *this);
        return;
    case creatures1::ui::SfcViewSoundPolicy::plays_unfocused:
        creatures1::ui::set_sound_conservative_policy(view_state_, *this);
        return;
    case creatures1::ui::SfcViewSoundPolicy::enable:
        creatures1::ui::enable_sound_and_restore_if_ready(view_state_, *this);
        return;
    case creatures1::ui::SfcViewSoundPolicy::disable:
        creatures1::ui::disable_sound_and_suspend_if_ready(view_state_, *this);
        return;
    }
}

void C1WindowsView::update_keyboard_scroll_for_world_tick() {
    creatures1::ui::update_keyboard_scroll(
        view_state_, view_settings_, *this);
}

bool C1WindowsView::manual_navigation_for_world_tick() const {
    return view_state_.viewport_navigation_mode ==
           creatures1::ui::ViewportNavigationMode::manual;
}

std::uint32_t C1WindowsView::manual_navigation_safe_frame_count_for_world_tick() const {
    return view_state_.manual_navigation_safe_frame_count;
}

void C1WindowsView::set_manual_navigation_safe_frame_count_for_world_tick( std::uint32_t frame_count) {
    view_state_.manual_navigation_safe_frame_count = frame_count;
}

void C1WindowsView::reset_world_scrollbars_for_world_tick() {
    creatures1::ui::reset_world_scrollbars(
        *this, view_settings_.half_width, view_settings_.half_height);
}

void C1WindowsView::load_view_settings(creatures1::ui::WorldViewSettings& settings, creatures1::ui::SfcViewState& state) {
    std::uint32_t value = 0;
    settings.world_update_timer_is_running =
        document() != nullptr && document()->world_timer_is_armed();
    if (document() != nullptr &&
        document()->read_view_setting("SmoothScrolling", value, 0)) {
        settings.smooth_scrolling_enabled = value != 0;
    }
    if (document() != nullptr &&
        document()->read_view_setting("ShowCoordinates", value, 0)) {
        state.show_coordinates = value != 0;
    }
    if (document() != nullptr &&
        document()->read_view_setting("ShowClassifiers", value, 0)) {
        state.show_classifiers = value != 0;
    }
}

void C1WindowsView::persist_view_settings( const creatures1::ui::WorldViewSettings& settings, const creatures1::ui::SfcViewState& state) {
    if (document() == nullptr) {
        return;
    }
    document()->write_view_setting("SmoothScrolling",
                                   settings.smooth_scrolling_enabled ? 1 : 0);
    document()->write_view_setting("ShowCoordinates",
                                   state.show_coordinates ? 1 : 0);
    document()->write_view_setting("ShowClassifiers",
                                   state.show_classifiers ? 1 : 0);
}

bool C1WindowsView::read_dword_setting(std::string_view name, std::uint32_t& value, std::uint32_t default_value) {
    return document() != nullptr &&
           document()->read_view_setting(name, value, default_value);
}

void C1WindowsView::write_dword_setting(std::string_view name, std::uint32_t value) {
    if (document() != nullptr) {
        document()->write_view_setting(name, value);
    }
}

void C1WindowsView::create_world_renderer(int, int, bool smooth_scrolling_enabled) {
    if (document() != nullptr) {
        document()->create_world_renderer_for_view(smooth_scrolling_enabled);
    }
}

void C1WindowsView::destroy_world_renderer() {
    if (document() != nullptr) {
        document()->destroy_world_renderer();
    }
}

void C1WindowsView::initialize_classifier_tip() {
    create_classifier_tip_window();
}


void C1WindowsView::destroy_classifier_tip() {
    if (classifier_tip_.GetSafeHwnd() != nullptr) {
        classifier_tip_.DestroyWindow();
    }
}


void C1WindowsView::forward_set_focus_message() {
    // The recovered SFCView handlers hand the message to CWnd::Default before
    // applying their own focus/mixer policy.
    Default();
}


void C1WindowsView::forward_kill_focus_message() {
    Default();
}


void C1WindowsView::restore_sound_mixer() {
    // SoundManager owns the recovered suspend/restore policy; the view only
    // supplies the focus event that triggers it.
    if (g_active_sound_manager != nullptr) {
        g_active_sound_manager->restore_mixer();
    }
}


void C1WindowsView::suspend_sound_mixer() {
    if (g_active_sound_manager != nullptr) {
        g_active_sound_manager->suspend_mixer();
    }
}


int C1WindowsView::scroll_position(unsigned axis) const {
    return GetScrollPos(axis == 0 ? SB_HORZ : SB_VERT);
}

bool C1WindowsView::main_frame_is_active() const {
    const CWnd* main_window = AfxGetMainWnd();
    return main_window != nullptr &&
           main_window->GetSafeHwnd() != nullptr &&
           ::GetActiveWindow() == main_window->GetSafeHwnd();
}

bool C1WindowsView::key_is_down(std::uint32_t virtual_key) const {
    return (::GetKeyState(static_cast<int>(virtual_key)) & 0x8000) != 0;
}

void C1WindowsView::scroll_viewport(int& delta_x, int& delta_y) {
    if (document() != nullptr) {
        document()->scroll_renderer_viewport(delta_x, delta_y);
    }
}

void C1WindowsView::reset_renderer_navigation() {
    if (document() != nullptr) {
        document()->reset_renderer_navigation();
    }
}

bool C1WindowsView::selected_creature_exists() const {
    const C1WindowsDocument* current_document = document();
    return current_document != nullptr &&
           current_document->selected_creature() != nullptr;
}

void C1WindowsView::set_view_window_class() {
    if (pending_create_struct_ == nullptr) {
        return;
    }
    // Recovered SFCView::PreCreateWindow @ 004368a0.  Resource 128 is the C1
    // view icon and resource 132 the C1 cursor - the original numeric IDs
    // 0x80 and 0x84.  Class style 0x0b is CS_VREDRAW|CS_HREDRAW|CS_DBLCLKS
    // with no background brush, matching the native AfxRegisterWndClass call.
    const LPCSTR icon_resource = MAKEINTRESOURCEA(128);
    const HINSTANCE icon_module =
        AfxFindResourceHandle(icon_resource, RT_GROUP_ICON);
    const HICON icon = LoadIconA(icon_module, icon_resource);

    const LPCSTR cursor_resource = MAKEINTRESOURCEA(132);
    const HINSTANCE cursor_module =
        AfxFindResourceHandle(cursor_resource, RT_GROUP_CURSOR);
    const HCURSOR cursor = LoadCursorA(cursor_module, cursor_resource);

    pending_create_struct_->lpszClass =
        AfxRegisterWndClass(0x0b, cursor, nullptr, icon);
}


void C1WindowsView::create_classifier_tip_window() {
    if (classifier_tip_.GetSafeHwnd() == nullptr) {
        classifier_tip_.create_for(*this);
    }
}


void C1WindowsView::forward_default_mouse_message(std::uint32_t, int, int) {
    // Default() reads the current message, so the recovered handlers pass no
    // arguments through to it.
    Default();
}


void C1WindowsView::forward_default_size(unsigned, int, int) {
    Default();
}


void C1WindowsView::release_mouse_capture() { ::ReleaseCapture(); }


void C1WindowsView::capture_mouse_for_drag() {
    previous_capture_window_ = ::SetCapture(GetSafeHwnd());
}


void C1WindowsView::materialize_previous_capture_window() {
    // Recovered SFCView::OnBeginDragResize calls CWnd::FromHandle on the
    // previous capture window and discards the result: the call exists to
    // put that window into MFC's temporary map, not to use the pointer.
    if (previous_capture_window_ != nullptr) {
        CWnd::FromHandle(previous_capture_window_);
        previous_capture_window_ = nullptr;
    }
}


void C1WindowsView::set_scroll_range(unsigned axis, int minimum, int maximum, bool redraw) {
    SetScrollRange(axis == 0 ? SB_HORZ : SB_VERT, minimum, maximum,
                   redraw ? TRUE : FALSE);
}

void C1WindowsView::set_scroll_position(unsigned axis, int position, bool redraw) {
    SetScrollPos(axis == 0 ? SB_HORZ : SB_VERT, position,
                 redraw ? TRUE : FALSE);
}

void C1WindowsView::set_default_arrow_cursor() {
    ::SetCursor(::LoadCursorA(nullptr, IDC_ARROW));
}

int C1WindowsView::viewport_left() const {
    return document() == nullptr ? 0 : document()->renderer_viewport_left();
}

int C1WindowsView::viewport_top() const {
    return document() == nullptr ? 0 : document()->renderer_viewport_top();
}

std::size_t C1WindowsView::non_scenery_object_count() const {
    return document() == nullptr ? 0 : document()->non_scenery_object_count();
}

creatures1::objects::Object* C1WindowsView::non_scenery_object_at( std::size_t index) const {
    return document() == nullptr ? nullptr
                                  : document()->non_scenery_object_at(index);
}

void C1WindowsView::report_invalid_non_scenery_object_index() const {
    // SFCView::OnMouseMove @ 00436b30 reaches AfxThrowInvalidArgException on
    // this branch; the port already reports registry-index violations as
    // std::out_of_range, so this follows that convention.
    throw std::out_of_range("C1 non-scenery object registry index");
}


std::size_t C1WindowsView::scenery_object_count() const {
    return document() == nullptr ? 0 : document()->scenery_count();
}

creatures1::objects::Object* C1WindowsView::scenery_object_at( std::size_t index) const {
    if (document() == nullptr || index >= document()->scenery_count()) {
        return nullptr;
    }
    return document()->scenery_at(index);
}

void C1WindowsView::report_invalid_scenery_object_index() const {
    throw std::out_of_range("C1 scenery render registry index");
}

creatures1::objects::Object* C1WindowsView::pointer_tool() const {
    return document() == nullptr ? nullptr : document()->pointer_tool();
}

bool C1WindowsView::point_in_world_rect(const creatures1::world::WorldRect& bounds, int world_x, int world_y) const {
    return document() != nullptr &&
           document()->contains_point(bounds, world_x, world_y);
}

std::string C1WindowsView::classifier_name(
    const creatures1::brain::ClassifierId& classifier) const {
    // ResolveClassifierDisplayName @ 00439310 over the map the startup host
    // publishes: exact key, then family/genus wildcard, then family only.
    return g_active_classifier_names == nullptr
               ? std::string()
               : creatures1::brain::ResolveClassifierDisplayName(
                     *g_active_classifier_names, classifier);
}


void C1WindowsView::update_classifier_tip(std::string_view text,
                                          int client_x, int client_y) {
    if (classifier_tip_.GetSafeHwnd() == nullptr) {
        return;
    }
    CPoint screen_point(client_x, client_y);
    ClientToScreen(&screen_point);
    creatures1::ui::update_classifier_tip(*this, text, screen_point.x,
                                          screen_point.y);
}


void C1WindowsView::clear_classifier_tip_text() {
    classifier_tip_.set_cached_text(CString());
}


void C1WindowsView::update_pointer_tool_unbounded_position_and_redraw() {
    if (document() != nullptr) {
        document()->invalidate_renderer_view();
    }
}

void C1WindowsView::fill_client_background_black(void* device_context) {
    if (document() != nullptr) {
        document()->fill_view_background_black(device_context);
    }
}

void C1WindowsView::mark_renderer_full_redraw() {
    if (document() != nullptr) {
        document()->set_full_redraw_pending(true);
    }
}

void C1WindowsView::present_renderer_view(void* device_context) {
    if (document() != nullptr) {
        document()->present_renderer_view(device_context);
    }
}

void C1WindowsView::resize_renderer_for_viewport(int client_width, int client_height) {
    if (document() != nullptr) {
        document()->resize_renderer_for_view(*this, client_width,
                                             client_height);
    }
}

void C1WindowsView::request_renderer_origin_for_selected_creature() {
    // ReturnViewportNavigationToSelectedCreature @ 0x00437910 delegates to
    // CWorldRenderer::RequestViewportOriginForSelectedCreature @ 0x004132a0,
    // which centres the creature horizontally and applies the 5/8 viewport
    // height offset.  Passing the raw down-foot as the origin instead put the
    // creature hard against the left edge of the view.
    if (C1WindowsDocument* current_document = document()) {
        current_document->request_viewport_origin_for_selected_creature();
    }
}



void C1WindowsView::configure_world_update_timer(std::uint32_t command) {
    // Recovered ConfigureWorldUpdateTimerInterval @ 00417f80: the interval
    // policy is C1-owned and the USER32 SetTimer is this boundary.  The timer
    // belongs to the main frame, not the view, and uses timer id 1.
    class Scheduler final : public creatures1::world::TimerScheduler {
    public:
        void set_timer(std::uintptr_t window_handle, std::uint32_t timer_id,
                       std::uint32_t interval_ms) override {
            if (window_handle != 0) {
                ::SetTimer(reinterpret_cast<HWND>(window_handle), timer_id,
                           interval_ms, nullptr);
            }
        }
    } scheduler;

    const CWnd* frame = AfxGetMainWnd();
    creatures1::world::configure_update_timer_interval(
        update_timer_state_, command, &scheduler,
        frame == nullptr
            ? 0
            : reinterpret_cast<std::uintptr_t>(frame->GetSafeHwnd()));
}


bool C1WindowsView::has_favourite_place(std::size_t index) const {
    return document() != nullptr && document()->has_favourite_place(index);
}

void C1WindowsView::request_renderer_origin_for_favourite_place( std::size_t index) {
    if (document() != nullptr) {
        document()->request_favourite_place(index);
    }
}

void C1WindowsView::generate_profiler_report() {
    // GenerateMagicProfilerReport @ 00437ef0, reached from Ctrl+Shift+M in
    // OnKeyDown.  The report generator, its HTML layout and the XML escaping
    // are already translated; this collects the snapshot it consumes and
    // supplies the MessageBoxA boundary.
    C1WindowsDocument* document = this->document();
    if (document == nullptr) {
        return;
    }

    class ReportWindow final : public creatures1::ui::MagicProfilerWindowApi {
    public:
        explicit ReportWindow(CWnd& owner) : owner_(owner) {}
        void show_message(std::string_view title,
                          std::string_view message) override {
            const std::string title_text(title);
            const std::string message_text(message);
            owner_.MessageBoxA(message_text.c_str(), title_text.c_str(),
                               MB_OK);
        }

    private:
        CWnd& owner_;
    };

    creatures1::ui::MagicProfilerSnapshot snapshot;
    snapshot.world_save_path = g_active_world_save_path == nullptr
                                   ? std::string()
                                   : *g_active_world_save_path;
    snapshot.world_name = document->GetTitle().GetString();
    snapshot.tick_count = document->world_tick_count();
    snapshot.creature_count =
        static_cast<std::uint32_t>(document->creature_count());
    snapshot.object_count =
        static_cast<std::uint32_t>(document->object_count());
    snapshot.scenery_count =
        static_cast<std::uint32_t>(document->scenery_count());
    snapshot.entity_count =
        static_cast<std::uint32_t>(document->entity_count());
    snapshot.total_script_count =
        static_cast<std::uint32_t>(creatures1::scripting::g_script_definition_count);
    snapshot.active_script_count =
        static_cast<std::uint32_t>(document->running_macro_count());
    snapshot.stimulus_count =
        static_cast<std::uint32_t>(document->queued_stimulus_count());
    snapshot.message_queue_count =
        static_cast<std::uint32_t>(document->immediate_event_count());
    snapshot.delayed_message_count =
        static_cast<std::uint32_t>(document->delayed_event_count());
    // Native: g_world_object_registry's size (CanBeDestroyed/InitializeRuntimeState
    // both grow it, per its 0x00456f64-adjacent xrefs) -- objects disabled/
    // orphaned and awaiting cleanup, hence "Death Row".
    snapshot.death_row_count =
        static_cast<std::uint32_t>(document->world_object_count());
    // Native: g_SFCDoc+0x118, i.e. serialized_document_state_words.size() --
    // confirmed via CEventBar::RemoveObjectFromDisplayList's own 2026-08-22
    // resolution (the Funeral Kit death-notification queue, 16-slot cap).
    // "Stuffed Norns" is this project's inherited debug label for that
    // queue's depth, not a literal word-learning-overflow counter.
    if (const auto* semantic_document = document->semantic_document();
        semantic_document != nullptr) {
        snapshot.stuffed_norn_word_count = static_cast<std::uint32_t>(
            semantic_document->serialized_document_state_words.size());
    }
    // Native: g_active_app_state's smoothed idle-cycle index (the same
    // value already surfaced via the DDE system-info record).
    snapshot.smoothed_idle_cycle_time =
        g_active_app_state == nullptr
            ? 0.0
            : static_cast<double>(g_active_app_state->idle_cadence.smoothed_idle_cycle);
    if (g_active_classifier_names != nullptr) {
        snapshot.classifier_names = *g_active_classifier_names;
    }
    // Native (GenerateMagicProfilerReport, 0x00438253-0x004385xx): two
    // separate tree-aggregation passes, both keyed by (family, genus,
    // species) decoded from Object::classifier_base the same way the
    // Eggs count above does.  The first walks the non-scenery object
    // registry ([0x004704e4]/[0x004704e8], confirmed identical to
    // document->non_scenery_object_count()/non_scenery_object_at() via
    // xrefs shared with PointerTool::ProcessPendingInput/SFCDoc::Serialize)
    // counting population per classifier.  The second walks
    // g_running_macro_slots ([0x00467dc0], bounded by
    // g_running_macro_count at [0x00467dac] -- this project's own
    // g_running_macros) and, for each running macro, reads its owning
    // object's classifier at owner+4 (native offset+0x98 from the running
    // slot is the owning Macro*, then +4 is that object's classifier_base
    // -- here g_running_macros[i]->object_context.script_owner) to count
    // active scripts per classifier.  The Ghidra DB itself flags the
    // native tree's construction helper
    // (MsvcCppEhHandler-adjacent MsvcVector_ConstructFromClassifierProfilerTreeRange,
    // 0x004395f0) as compiler-generated STL machinery meant to be expressed
    // with normal standard-library code, so this uses std::map rather than
    // replicate the red-black tree.
    {
        using ClassifierKey = std::tuple<int, int, int>;
        std::map<ClassifierKey, creatures1::ui::MagicProfilerSnapshot::ClassifierRow>
            aggregation;

        const auto classifier_key = [](std::uint32_t classifier_base) {
            const int family = static_cast<int>((classifier_base >> 24) & 0xffu);
            const int genus = static_cast<int>((classifier_base >> 16) & 0xffu);
            const int species = static_cast<int>((classifier_base >> 8) & 0xffu);
            return ClassifierKey{family, genus, species};
        };

        for (std::size_t index = 0; index < document->non_scenery_object_count();
             ++index) {
            const creatures1::objects::Object* object =
                document->non_scenery_object_at(index);
            if (object == nullptr) {
                continue;
            }
            const ClassifierKey key = classifier_key(object->classifier_base());
            creatures1::ui::MagicProfilerSnapshot::ClassifierRow& row =
                aggregation[key];
            row.classifier.family = std::get<0>(key);
            row.classifier.genus = std::get<1>(key);
            row.classifier.species = std::get<2>(key);
            ++row.population;
        }

        for (const creatures1::scripting::Macro* macro :
             creatures1::scripting::g_running_macros) {
            if (macro == nullptr) {
                continue;
            }
            const creatures1::objects::Object* owner =
                macro->object_context.script_owner;
            if (owner == nullptr) {
                continue;
            }
            const ClassifierKey key = classifier_key(owner->classifier_base());
            creatures1::ui::MagicProfilerSnapshot::ClassifierRow& row =
                aggregation[key];
            row.classifier.family = std::get<0>(key);
            row.classifier.genus = std::get<1>(key);
            row.classifier.species = std::get<2>(key);
            ++row.active_script_count;
        }

        snapshot.classifier_rows.reserve(aggregation.size());
        for (auto& [key, row] : aggregation) {
            snapshot.classifier_rows.push_back(std::move(row));
        }
    }

    ReportWindow window(*this);
    creatures1::ui::generate_magic_profiler_report(window, snapshot);
}


void C1WindowsView::hide_classifier_tip() {
    creatures1::ui::hide_classifier_tip(*this);
}

bool C1WindowsView::classifier_tip_visible() const {
    return classifier_tip_.GetSafeHwnd() != nullptr &&
           const_cast<C1ClassifierTipWindow&>(classifier_tip_)
                   .IsWindowVisible() != FALSE;
}

void C1WindowsView::hide_classifier_tip_window() {
    classifier_tip_.ShowWindow(SW_HIDE);
}

bool C1WindowsView::classifier_tip_text_matches(std::string_view text) const {
    const CString candidate(text.data(), static_cast<int>(text.size()));
    return classifier_tip_.cached_text().Compare(candidate) == 0;
}

void C1WindowsView::set_classifier_tip_text(std::string_view text) {
    classifier_tip_.set_cached_text(
        CString(text.data(), static_cast<int>(text.size())));
}

creatures1::ui::ClassifierTipExtent
C1WindowsView::measure_classifier_tip_text() {
    CClientDC device_context(&classifier_tip_);
    // The native measures with the stock GUI font, not the window's own.
    CGdiObject* stock_font =
        CGdiObject::FromHandle(::GetStockObject(DEFAULT_GUI_FONT));
    CFont* previous_font = device_context.SelectObject(
        static_cast<CFont*>(stock_font));
    const CSize extent =
        device_context.GetTextExtent(classifier_tip_.cached_text());
    device_context.SelectObject(previous_font);
    return {extent.cx, extent.cy};
}

creatures1::ui::ClassifierTipExtent C1WindowsView::measure_classifier_tip(
    int content_width, int content_height) {
    CRect frame(0, 0, content_width, content_height);
    ::AdjustWindowRectEx(&frame, classifier_tip_.GetStyle(), FALSE,
                         classifier_tip_.GetExStyle());
    return {static_cast<int>(frame.Width()),
            static_cast<int>(frame.Height())};
}

void C1WindowsView::move_classifier_tip(int screen_x, int screen_y) {
    classifier_tip_.SetWindowPos(&wndTopMost, screen_x, screen_y, 0, 0,
                                 SWP_NOSIZE | SWP_NOACTIVATE);
}

void C1WindowsView::resize_classifier_tip(int screen_x, int screen_y,
                                          int width, int height) {
    classifier_tip_.SetWindowPos(&wndTopMost, screen_x, screen_y, width,
                                 height, SWP_NOACTIVATE);
    classifier_tip_.Invalidate(TRUE);
}

void C1WindowsView::show_classifier_tip_if_hidden() {
    if (classifier_tip_.IsWindowVisible() == FALSE) {
        classifier_tip_.ShowWindow(SW_SHOWNOACTIVATE);
    }
}

BEGIN_MESSAGE_MAP(C1ClassifierTipWindow, CWnd)
    ON_WM_PAINT()
END_MESSAGE_MAP()

bool C1ClassifierTipWindow::create_for(CWnd& parent) {
    // AfxRegisterWndClass(CS_SAVEBITS, no cursor, COLOR_INFOBK+1, no icon)
    // then CreateEx as a topmost tool-window popup with a border.
    LPCTSTR window_class = AfxRegisterWndClass(
        CS_SAVEBITS, nullptr,
        reinterpret_cast<HBRUSH>(COLOR_INFOBK + 1), nullptr);
    return CreateEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, window_class, "",
                    WS_POPUP | WS_BORDER, 0, 0, 10, 10,
                    parent.GetSafeHwnd(), nullptr, nullptr) != FALSE;
}

void C1ClassifierTipWindow::OnPaint() {
    CPaintDC device_context(this);
    CGdiObject* stock_font =
        CGdiObject::FromHandle(::GetStockObject(DEFAULT_GUI_FONT));
    CFont* previous_font = device_context.SelectObject(
        static_cast<CFont*>(stock_font));
    device_context.SetBkMode(TRANSPARENT);
    CRect client;
    GetClientRect(&client);
    device_context.DrawText(cached_text_, &client,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    device_context.SelectObject(previous_font);
}


void C1WindowsView::forward_default_key_down(std::uint32_t virtual_key, std::uint32_t repeat_count, std::uint32_t key_flags) {
    CView::OnKeyDown(static_cast<UINT>(virtual_key),
                     static_cast<UINT>(repeat_count),
                     static_cast<UINT>(key_flags));
}

BOOL C1WindowsView::PreCreateWindow(CREATESTRUCT& create_struct) {
    // The recovered order registers the class first, then forwards to CView.
    // Driving it through the semantic policy keeps this override a boundary
    // rather than a second copy of the registration.
    pending_create_struct_ = &create_struct;
    creatures1::ui::configure_pre_create_window(*this);
    pending_create_struct_ = nullptr;
    return CView::PreCreateWindow(create_struct);
}

void C1WindowsView::OnInitialUpdate() {
    CView::OnInitialUpdate();
    const std::uint32_t privilege =
        g_active_app_state == nullptr
            ? 0
            : static_cast<std::uint32_t>(
                  g_active_app_state->privilege_level);
    creatures1::ui::initialize_view(view_state_, view_settings_, *this,
                                    privilege);
    creatures1::ui::initialize_view_window(
        *this, view_settings_.half_width, view_settings_.half_height);
}

void C1WindowsView::OnDraw(CDC* device_context) {
    if (device_context != nullptr) {
        // The renderer's void* device context is an HDC: the dirty-rect path
        // supplies one from GetDC, and the GDI host blits with it directly.
        // Passing MFC's CDC* here instead made every world blit target a
        // bogus HDC, which BitBlt rejects silently -- the world view painted
        // black while the MFC-drawn chrome around it was fine.
        creatures1::ui::on_draw(*this, device_context->GetSafeHdc());
        return;
    }
    CRect client_rect;
    GetClientRect(&client_rect);
    if (device_context != nullptr) {
        device_context->FillSolidRect(&client_rect, RGB(0, 0, 0));
    }
}

afx_msg void C1WindowsView::OnSize(UINT resize_type, int client_width, int client_height) {
    CView::OnSize(resize_type, client_width, client_height);
    creatures1::ui::on_size(*this, resize_type, client_width,
                            client_height);
}

afx_msg void C1WindowsView::OnSetFocus(CWnd* old_focus) {
    CView::OnSetFocus(old_focus);
    creatures1::ui::on_set_focus(view_state_, *this);
}

afx_msg void C1WindowsView::OnKillFocus(CWnd* new_focus) {
    CView::OnKillFocus(new_focus);
    creatures1::ui::on_kill_focus(view_state_, *this);
}

afx_msg void C1WindowsView::OnMouseMove(UINT flags, CPoint point) {
    CView::OnMouseMove(flags, point);
    creatures1::ui::on_mouse_move(view_state_, view_settings_, *this,
                                  flags, point.x, point.y);
}

afx_msg void C1WindowsView::OnLButtonDown(UINT flags, CPoint point) {
    CView::OnLButtonDown(flags, point);
    const std::uint32_t privilege =
        g_active_app_state == nullptr
            ? 0
            : static_cast<std::uint32_t>(
                  g_active_app_state->privilege_level);
    creatures1::ui::on_left_button_down(
        view_state_, *this, flags, point.x, point.y, privilege);
}

afx_msg void C1WindowsView::OnRButtonDown(UINT flags, CPoint point) {
    CView::OnRButtonDown(flags, point);
    creatures1::ui::on_right_button_down(view_state_, *this, flags,
                                         point.x, point.y);
}

afx_msg void C1WindowsView::OnLButtonUp(UINT flags, CPoint point) {
    CView::OnLButtonUp(flags, point);
    creatures1::ui::on_mouse_button_release(view_state_, *this, flags,
                                            point.x, point.y);
}

afx_msg void C1WindowsView::OnRButtonUp(UINT flags, CPoint point) {
    CView::OnRButtonUp(flags, point);
    creatures1::ui::on_mouse_button_release(view_state_, *this, flags,
                                            point.x, point.y);
}

afx_msg void C1WindowsView::OnHScroll(UINT code, UINT position, CScrollBar*) {
    creatures1::ui::on_horizontal_scroll(view_state_, view_settings_,
                                         *this, code,
                                         static_cast<int>(position));
}

afx_msg void C1WindowsView::OnVScroll(UINT code, UINT position, CScrollBar*) {
    creatures1::ui::on_vertical_scroll(view_state_, view_settings_, *this,
                                       code, static_cast<int>(position));
}

afx_msg void C1WindowsView::OnChar(UINT character_code, UINT /*repeat_count*/,
                                   UINT /*flags*/) {
    // SFCView::OnCharQueueTextInput @ 0x00437270.  Typed characters feed the
    // pointer tool's speech text through the document's key ring.
    creatures1::ui::queue_text_input(*this, character_code);
}

bool C1WindowsView::control_key_is_down() const {
    return ::GetKeyState(VK_CONTROL) < 0;
}

void C1WindowsView::forward_default_character_message(
    std::uint32_t /*character_code*/) {
    Default();
}

bool C1WindowsView::try_enqueue(char character) {
    C1WindowsDocument* const owning_document = document();
    return owning_document != nullptr &&
           owning_document->enqueue_text_input(character);
}

afx_msg void C1WindowsView::OnKeyDown(UINT virtual_key, UINT repeat_count, UINT flags) {
    // NOT a native key binding -- CWorldStatisticsFrame's own real trigger
    // could not be found (see the class comment on C1WorldStatisticsFrame).
    // This one case is a port-only addition to make the otherwise-orphaned,
    // decompile-verified window reachable, kept entirely out of
    // ui::on_key_down so that function stays an exact match for
    // SFCView::OnKeyDown and nothing here is mistaken for recovered
    // native behavior.
    if (virtual_key == 'W' && (::GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
        (::GetKeyState(VK_SHIFT) & 0x8000) != 0) {
        open_or_activate_world_statistics();
        return;
    }
    creatures1::ui::on_key_down(view_state_, *this, virtual_key,
                                repeat_count, flags);
}

C1WindowsDocument* C1WindowsView::document() const {
    return DYNAMIC_DOWNCAST(C1WindowsDocument, GetDocument());
}

BEGIN_MESSAGE_MAP(C1WindowsView, CView)
    ON_WM_SIZE()
    ON_WM_SETFOCUS()
    ON_WM_KILLFOCUS()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_RBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONUP()
    ON_WM_HSCROLL()
    ON_WM_VSCROLL()
    ON_WM_KEYDOWN()
    ON_WM_CHAR()
END_MESSAGE_MAP()

void C1WindowsView::set_coordinate_status(std::string_view text) {
    C1MainFrame* frame =
        DYNAMIC_DOWNCAST(C1MainFrame, AfxGetMainWnd());
    if (frame != nullptr) {
        frame->set_coordinate_status(text);
    }
}

void C1WindowsView::invalidate_main_toolbar() {
    C1MainFrame* frame =
        DYNAMIC_DOWNCAST(C1MainFrame, AfxGetMainWnd());
    if (frame != nullptr) {
        frame->invalidate_main_toolbar();
    }
}

void C1WindowsView::clear_coordinate_status() {
    C1MainFrame* frame =
        DYNAMIC_DOWNCAST(C1MainFrame, AfxGetMainWnd());
    if (frame != nullptr) {
        frame->clear_coordinate_status();
    }
}

IMPLEMENT_DYNCREATE(C1WindowsView, CView)


// --- C1EyeViewWindow: the concrete CEyeView @ 00417440 -------------------

BEGIN_MESSAGE_MAP(C1EyeViewWindow, CWnd)
    ON_WM_SIZE()
    ON_WM_CLOSE()
    ON_WM_PAINT()
    ON_WM_PALETTECHANGED()
    ON_WM_QUERYNEWPALETTE()
END_MESSAGE_MAP()

C1EyeViewWindow::~C1EyeViewWindow() {
    renderer_.reset();
    gdi_host_.reset();
}

bool C1EyeViewWindow::create(
    std::string_view title,
    const creatures1::application::EyeViewCreationParameters& parameters,
    std::int32_t initial_viewport_left,
    std::int32_t initial_viewport_top) {
    initial_viewport_left_ = initial_viewport_left;
    initial_viewport_top_ = initial_viewport_top;
    viewport_width_ = parameters.viewport_width;
    viewport_height_ = parameters.viewport_height;
    smooth_scrolling_enabled_ = parameters.smooth_scrolling;
    overlay_gallery_identifier_ = parameters.overlay_gallery_identifier;
    return creatures1::ui::create_eye_view_window(*this, title);
}

bool C1EyeViewWindow::create_native_eye_window(std::string_view title,
                                               std::uint32_t style) {
    HICON icon = AfxGetApp() == nullptr
                     ? nullptr
                     : ::LoadIconA(AfxFindResourceHandle(
                                       MAKEINTRESOURCEA(128), RT_GROUP_ICON),
                                   MAKEINTRESOURCEA(128));
    HCURSOR cursor = ::LoadCursorA(nullptr, IDC_ARROW);
    LPCTSTR window_class = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW, cursor, nullptr, icon);

    CWnd* parent = AfxGetMainWnd();
    const std::string window_title(title);
    if (CreateEx(0, window_class, window_title.c_str(),
                 static_cast<DWORD>(style), 0, 0, 0, 0,
                 parent == nullptr ? nullptr : parent->GetSafeHwnd(), nullptr,
                 nullptr) == FALSE) {
        return false;
    }

    // The eye view owns its own renderer; the document supplies the world.
    gdi_host_ = std::make_unique<
        creatures1::platform::WindowsWorldRendererGdiHost>(GetSafeHwnd());
    CRect client;
    GetClientRect(&client);
    renderer_ = std::make_unique<creatures1::display::WorldRenderer>(
        document_, GetSafeHwnd(), initial_viewport_left_, initial_viewport_top_,
        viewport_width_ > 0 ? viewport_width_ : (std::max)(1, (int)client.Width()),
        viewport_height_ > 0 ? viewport_height_ : (std::max)(1, (int)client.Height()),
        nullptr, smooth_scrolling_enabled_, overlay_gallery_identifier_,
        false);
    renderer_->realize_palette();
    return true;
}

std::string C1EyeViewWindow::localized_eye_view_title() const {
    // CEyeView::UpdateWindowTitleForSelectedCreature @ 0x004172f0 loads
    // string id 0xef26 -- confirmed directly from the reference exe's own
    // STRINGTABLE (bundle 3827): it is literally "View" (the same word the
    // View menu itself uses), not a distinct "Eye View" string. Id 0x80 is
    // not a string resource at all (128 exists only as a BITMAP/ICON), so
    // this always returned an empty title before appending " - <name>".
    CStringA value;
    value.LoadStringA(static_cast<UINT>(0xef26));
    return value.GetString();
}

std::string C1EyeViewWindow::selected_creature_name() const {
    const creatures1::creatures::Creature* creature =
        document_.selected_creature();
    return creature == nullptr ? std::string() : creature->display_name();
}

void C1EyeViewWindow::set_window_title(std::string_view title) {
    const std::string text(title);
    SetWindowTextA(text.c_str());
}

creatures1::ui::EyeViewPosition C1EyeViewWindow::window_position() const {
    RECT rect{};
    const_cast<C1EyeViewWindow*>(this)->GetWindowRect(&rect);
    return {rect.left, rect.top};
}

void C1EyeViewWindow::persist_eye_view_position(
    const creatures1::ui::EyeViewPosition& position) {
    HKEY key = nullptr;
    if (creatures1::platform::open_c1_secondary_registry(key, KEY_SET_VALUE)) {
        const std::int32_t values[2] = {position.x, position.y};
        RegSetValueExA(key, "EyePosn", 0, REG_BINARY,
                       reinterpret_cast<const BYTE*>(values), sizeof(values));
        RegCloseKey(key);
    }
}

bool C1EyeViewWindow::read_saved_eye_view_position(
    creatures1::ui::EyeViewPosition& position) const {
    HKEY key = nullptr;
    if (!creatures1::platform::open_c1_secondary_registry(key, KEY_READ)) {
        return false;
    }
    std::int32_t values[2] = {0, 0};
    DWORD size = sizeof(values);
    const bool read =
        RegQueryValueExA(key, "EyePosn", nullptr, nullptr,
                         reinterpret_cast<BYTE*>(values), &size) ==
            ERROR_SUCCESS &&
        size == sizeof(values);
    RegCloseKey(key);
    if (read) {
        position = {values[0], values[1]};
    }
    return read;
}

creatures1::ui::EyeViewRect C1EyeViewWindow::default_eye_view_rect() const {
    // g_eye_default_rect in the native; the recovered window is 128x96 of
    // world pixels doubled, matching the follow viewport's 0x40/0x30 half
    // extents.
    return {0, 0, 0x80, 0x60};
}

creatures1::ui::EyeViewPosition
C1EyeViewWindow::eye_view_position_limits() const {
    // CEyeView::CreateWindow @ 00417440 bounds EyePosn by GetSystemMetrics
    // 0x10/0x11 (the full-screen client area), not the raw screen size.
    return {::GetSystemMetrics(SM_CXFULLSCREEN),
            ::GetSystemMetrics(SM_CYFULLSCREEN)};
}

void C1EyeViewWindow::move_eye_view_window(int x, int y, int width, int height,
                                           bool repaint) {
    // The recovered default rectangle is the eye renderer's client extent.
    // MoveWindow consumes an outer frame extent for this overlapped window;
    // without adding the non-client metrics the caption and borders reduce
    // the drawable area below bubb.spr's 128x96 overlay.
    RECT frame{0, 0, width, height};
    const DWORD style = static_cast<DWORD>(GetStyle());
    const DWORD ex_style = static_cast<DWORD>(GetExStyle());
    if (::AdjustWindowRectEx(&frame, style, FALSE, ex_style) != FALSE) {
        width = frame.right - frame.left;
        height = frame.bottom - frame.top;
    }
    MoveWindow(x, y, width, height, repaint ? TRUE : FALSE);
}

void C1EyeViewWindow::forward_default_window_operation() { Default(); }

void C1EyeViewWindow::forward_default_size(unsigned size_type,
                                           int client_width,
                                           int client_height) {
    // CEyeView::OnSize calls CWnd::Default(), not CWnd::OnSize().  The latter
    // bypasses the window procedure's default handling and can leave the
    // overlapped eye window with stale non-client/update state.
    static_cast<void>(size_type);
    static_cast<void>(client_width);
    static_cast<void>(client_height);
    Default();
}

bool C1EyeViewWindow::palette_focus_is_this_window(
    const void* palette_focus_window) const {
    return palette_focus_window == static_cast<const void*>(this);
}

bool C1EyeViewWindow::palette_focus_owns_renderer(
    const void* palette_focus_window) const {
    return palette_focus_window != nullptr && renderer_ != nullptr &&
           static_cast<const CWnd*>(palette_focus_window)->GetSafeHwnd() ==
               const_cast<C1EyeViewWindow*>(this)->GetSafeHwnd();
}

std::uint32_t C1EyeViewWindow::realize_world_renderer_palette() {
    return renderer_ == nullptr ? 0 : renderer_->realize_palette();
}

void C1EyeViewWindow::resize_world_renderer(int client_width,
                                            int client_height) {
    if (renderer_ != nullptr) {
        renderer_->resize_back_buffer_for_viewport(
            GetSafeHwnd(), (std::max)(1, client_width),
            (std::max)(1, client_height));
    }
}

bool C1EyeViewWindow::selected_creature(
    creatures1::ui::FollowViewportTarget& target) const {
    creatures1::creatures::Creature* creature = document_.selected_creature();
    if (creature == nullptr) {
        return false;
    }
    const creatures1::objects::Object& speaker =
        document_.object_for_creature(*creature);
    // CEyeView::UpdateSelectedCreatureFollowViewport @ 004176e0: an awake
    // creature with a motion link (+0x7f0, +0x11f clear) and an aimed point
    // above y 0x4b0 centres on that point (+0x7f4/+0x7f8), i.e. what it is
    // looking at; otherwise the view falls back to the creature itself.
    const creatures1::creatures::Skeleton& skeleton = creature->skeleton();
    target.has_motion_target = skeleton.motion_link != nullptr;
    target.sleep_indicator_active = skeleton.sleep_indicator_active;
    target.motion_target_x = skeleton.motion_target_x;
    target.motion_target_y = skeleton.motion_target_y;
    target.sound_source_x = speaker.sound_source_x();
    target.sound_source_y = speaker.sound_source_y();
    target.visual_width = speaker.current_visual_width();
    target.visual_height = speaker.current_visual_height();
    return true;
}

void C1EyeViewWindow::set_follow_position(int center_x, int center_y,
                                          bool valid) {
    follow_center_x_ = center_x;
    follow_center_y_ = center_y;
    follow_valid_ = valid;
}

void C1EyeViewWindow::publish_follow_viewport(
    const creatures1::world::ViewportBounds& bounds) {
    if (renderer_ != nullptr) {
        renderer_->set_viewport_origin_without_world_shift(bounds.left,
                                                            bounds.top);
    }
}

void C1EyeViewWindow::queue_dirty_world_rect(
    const creatures1::world::WorldRect& rect) {
    if (renderer_ != nullptr) {
        renderer_->queue_dirty_world_rect(rect.min_x, rect.min_y, rect.max_x,
                                          rect.max_y);
    }
}

void C1EyeViewWindow::OnSize(UINT size_type, int client_width,
                             int client_height) {
    creatures1::ui::resize_eye_view(*this, size_type, client_width,
                                    client_height);
}

void C1EyeViewWindow::OnClose() {
    creatures1::ui::persist_eye_view_window_position(*this);
}

void C1EyeViewWindow::OnPaint() {
    // Native RedrawWorldRendererFullViewOnPaint constructs a CPaintDC and
    // calls WorldRenderer::RedrawFullView.  Use this paint DC directly: a
    // GetDC-based dirty present here can ignore the invalidated region and
    // leave the eye buffer partially stale after uncover/resize.
    CPaintDC paint_dc(this);
    if (renderer_ != nullptr) {
        renderer_->redraw_full_view(&paint_dc);
    }
}

void C1EyeViewWindow::present_world_rect(
    const creatures1::world::WorldRect& rect) {
    // Mirrors C1WindowsDocument::present_renderer_rect exactly, but against
    // THIS window's own renderer_/gdi_host_ rather than the document's.
    // The eye view's WorldRenderer is constructed with the document as its
    // WorldRendererHost (the same interface the main view's renderer
    // shares), so without this, its own present_current_view callback
    // silently drew into the main view's DC instead of here -- this
    // window's client area never received a single real paint.
    if (renderer_ == nullptr || gdi_host_ == nullptr) {
        return;
    }
    void* device_context = gdi_host_->acquire_client_context();
    if (device_context != nullptr) {
        if (!renderer_->full_redraw_pending()) {
            renderer_->redraw_full_view(device_context);
        } else {
            renderer_->present_world_rect(device_context, rect);
        }
        gdi_host_->release_client_context(device_context);
    }
}

void C1EyeViewWindow::redraw_full_view() {
    if (renderer_ == nullptr || gdi_host_ == nullptr) {
        return;
    }
    void* device_context = gdi_host_->acquire_client_context();
    if (device_context != nullptr) {
        renderer_->redraw_full_view(device_context);
        gdi_host_->release_client_context(device_context);
    }
}

void C1EyeViewWindow::OnPaletteChanged(CWnd* palette_focus_window) {
    creatures1::ui::on_eye_view_palette_changed(*this, palette_focus_window);
}

BOOL C1EyeViewWindow::OnQueryNewPalette() {
    return creatures1::ui::on_eye_view_query_new_palette(*this) != 0;
}



// --- C1SystemInfoWindow ----------------------------------------------------

namespace {
C1SystemInfoWindow* g_system_info_window = nullptr;

// The fourteen values in CreateSystemInfoData's pipe-delimited order.  The
// native window's own labels are not recovered, so these name the fields the
// snapshot already carries rather than inventing a layout claim.
constexpr const char* kSystemInfoLabels[] = {
    "Non-scenery objects", "Entities",       "Creatures",
    "Script definitions",  "Running macros", "Galleries",
    "Image cache entries", "Image cache bytes", "Rooms",
    "Ambient environment", "Selected action", "Action boost",
    "Motion link",         "Idle cycle",
};
} // namespace

C1SystemInfoWindow* active_system_info_window() {
    return g_system_info_window;
}

BEGIN_MESSAGE_MAP(C1SystemInfoWindow, CFrameWnd)
    ON_WM_PAINT()
END_MESSAGE_MAP()

bool C1SystemInfoWindow::create_for(CWnd& parent) {
    const RECT rect{0, 0, 320, 260};
    if (Create(nullptr, "System Information", WS_OVERLAPPEDWINDOW, rect,
               &parent) == FALSE) {
        return false;
    }
    g_system_info_window = this;
    ShowWindow(SW_SHOW);
    return true;
}

void C1SystemInfoWindow::PostNcDestroy() {
    // ReleaseSystemInfoWindowSingleton @ 00449f10 clears the global
    // unconditionally (not guarded by identity), then destroys the window.
    g_system_info_window = nullptr;
    delete this;
}

void C1SystemInfoWindow::set_snapshot(
    const creatures1::scripting::DdeSystemInfoSnapshot& info) {
    info_ = info;
    if (GetSafeHwnd() != nullptr) {
        Invalidate(FALSE);
    }
}

void C1SystemInfoWindow::OnPaint() {
    CPaintDC device_context(this);
    CGdiObject* stock_font =
        CGdiObject::FromHandle(::GetStockObject(DEFAULT_GUI_FONT));
    CFont* previous_font =
        device_context.SelectObject(static_cast<CFont*>(stock_font));

    const std::int32_t values[] = {
        info_.non_scenery_object_count, info_.entity_count,
        info_.creature_count,           info_.script_definition_count,
        info_.running_macro_count,      info_.gallery_count,
        info_.image_cache_entry_count,  info_.image_cache_bytes,
        info_.room_count,               info_.ambient_environment_index,
        info_.selected_action_id,       info_.selected_action_activation_boost,
        info_.selected_creature_motion_link,
        info_.smoothed_idle_cycle_index,
    };
    int y = 8;
    for (std::size_t index = 0; index < std::size(values); ++index) {
        CString line;
        line.Format("%-22s %d", kSystemInfoLabels[index], values[index]);
        device_context.TextOutA(8, y, line);
        y += 16;
    }
    device_context.SelectObject(previous_font);
}

void C1WindowsView::open_or_activate_system_information() {
    // SFCView::OnKeyDown @ 00437b30 opens the window on Ctrl+period and
    // reuses the existing one rather than creating a second.
    C1WindowsDocument* owner = document();
    if (owner == nullptr) {
        return;
    }
    if (g_system_info_window == nullptr) {
        auto* window = new C1SystemInfoWindow();
        if (!window->create_for(*this)) {
            delete window;
            return;
        }
    } else {
        g_system_info_window->SetActiveWindow();
    }
    g_system_info_window->set_snapshot(owner->system_info_snapshot());
}


// --- C1WorldStatisticsFrame -------------------------------------------------

namespace {
C1WorldStatisticsFrame* g_world_statistics_frame = nullptr;

constexpr std::uint32_t kWorldStatisticsDisplayId = 100;
constexpr int kEggClassifierFamily = 2;
constexpr int kEggClassifierGenus = 5;
constexpr int kEggClassifierSpecies = 2;
} // namespace

C1WorldStatisticsFrame* active_world_statistics_frame() {
    return g_world_statistics_frame;
}

BEGIN_MESSAGE_MAP(C1WorldStatisticsFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_TIMER()
END_MESSAGE_MAP()

bool C1WorldStatisticsFrame::create_for(CWnd& parent) {
    const RECT rect{0, 0, 320, 260};
    if (Create(nullptr, "World Statistics", WS_OVERLAPPEDWINDOW, rect,
               &parent) == FALSE) {
        return false;
    }
    owning_view = DYNAMIC_DOWNCAST(C1WindowsView, &parent);
    g_world_statistics_frame = this;
    ShowWindow(SW_SHOW);
    return true;
}

int C1WorldStatisticsFrame::OnCreate(LPCREATESTRUCT create_struct) {
    host_.pending_create_struct = create_struct;
    const int result = creatures1::ui::create_world_statistics_frame(host_);
    host_.pending_create_struct = nullptr;
    return result;
}

void C1WorldStatisticsFrame::OnTimer(UINT_PTR timer_id) {
    creatures1::ui::on_world_statistics_timer(
        host_, static_cast<std::uint32_t>(timer_id));
}

BOOL C1WorldStatisticsFrame::PreTranslateMessage(MSG* message) {
    host_.pending_message = message;
    creatures1::ui::WorldStatisticsKeyMessage key_message{};
    key_message.message = message->message;
    key_message.key = static_cast<std::uint32_t>(message->wParam);
    key_message.control_down = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool handled =
        creatures1::ui::pretranslate_world_statistics(host_, key_message);
    host_.pending_message = nullptr;
    return handled ? TRUE : CFrameWnd::PreTranslateMessage(message);
}

void C1WorldStatisticsFrame::PostNcDestroy() {
    if (g_world_statistics_frame == this) {
        g_world_statistics_frame = nullptr;
    }
    delete this;
}

int C1WorldStatisticsFrame::ConcreteHost::initialise_base_frame() {
    return owner_.ForwardBaseOnCreate(pending_create_struct);
}

creatures1::ui::WorldStatisticsRect
C1WorldStatisticsFrame::ConcreteHost::client_rect() const {
    RECT rect{};
    owner_.GetClientRect(&rect);
    return {rect.left, rect.top, rect.right, rect.bottom};
}

void C1WorldStatisticsFrame::ConcreteHost::create_display(
    std::string_view initial_text,
    const creatures1::ui::WorldStatisticsRect& bounds,
    std::uint32_t control_id) {
    const RECT rect{bounds.left, bounds.top, bounds.right, bounds.bottom};
    owner_.statistics_display_.Create(
        std::string(initial_text).c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT,
        rect, &owner_, static_cast<UINT>(control_id));
}

void C1WorldStatisticsFrame::ConcreteHost::apply_statistics_font(
    int point_size) {
    // Native: CFont::CreatePointFont(0x55, "Consolas", nullptr); on failure
    // (or if the created font's handle is null), fall back to the stock
    // DEFAULT_GUI_FONT.
    HFONT font_handle = nullptr;
    if (owner_.statistics_font_.CreatePointFont(point_size, "Consolas",
                                                nullptr) &&
        owner_.statistics_font_.GetSafeHandle() != nullptr) {
        font_handle = static_cast<HFONT>(owner_.statistics_font_.GetSafeHandle());
    } else {
        font_handle = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
    }
    owner_.statistics_display_.SendMessage(
        WM_SETFONT, reinterpret_cast<WPARAM>(font_handle), TRUE);
}

creatures1::ui::WorldStatisticsSnapshot
C1WorldStatisticsFrame::ConcreteHost::read_snapshot() const {
    creatures1::ui::WorldStatisticsSnapshot snapshot{};
    // This frame is a plain top-level CFrameWnd with no doc/view template
    // of its own, so CFrameWnd::GetActiveDocument() on itself always
    // returns null -- the owning view captured at create_for time is used
    // instead (the same document open_or_activate_world_statistics()'s
    // own view already has).
    C1WindowsDocument* document =
        owner_.owning_view == nullptr ? nullptr : owner_.owning_view->document();
    if (document == nullptr) {
        return snapshot;
    }

    snapshot.creature_count =
        static_cast<int>(document->creature_count());
    snapshot.object_count = static_cast<int>(document->object_count());
    snapshot.entity_count = static_cast<int>(document->entity_count());
    snapshot.total_script_count =
        static_cast<int>(creatures1::scripting::g_script_definition_count);
    snapshot.active_script_count =
        static_cast<int>(document->running_macro_count());
    snapshot.death_row_count =
        static_cast<int>(document->world_object_count());
    if (const auto* semantic_document = document->semantic_document();
        semantic_document != nullptr) {
        snapshot.stuffed_norn_count = static_cast<int>(
            semantic_document->serialized_document_state_words.size());
    }
    snapshot.message_count = static_cast<int>(document->immediate_event_count());
    snapshot.delayed_message_count =
        static_cast<int>(document->delayed_event_count());
    snapshot.stimulus_count =
        static_cast<int>(document->queued_stimulus_count());
    snapshot.idle_time =
        g_active_app_state == nullptr
            ? 0
            : g_active_app_state->idle_cadence.smoothed_idle_cycle;

    // Native's live loop (RefreshStatistics @ 0x00449d40): non-scenery
    // objects whose classifier is family 2 / genus 5 / species 2 (egg),
    // with the byte at object offset 0x4c set.  Object's own recovered
    // layout puts offset 0x4c (76) exactly on tick_enabled -- so this is
    // simply "how many egg objects are still ticking" (a hatched or
    // otherwise-removed egg stops ticking and drops out of the count),
    // not a separate unhatched flag.
    std::uint32_t egg_count = 0;
    for (std::size_t index = 0; index < document->non_scenery_object_count();
         ++index) {
        const creatures1::objects::Object* object =
            document->non_scenery_object_at(index);
        if (object == nullptr) {
            continue;
        }
        const std::uint32_t classifier = object->classifier_base() & 0xffffff00u;
        constexpr std::uint32_t kEggClassifier =
            (static_cast<std::uint32_t>(kEggClassifierFamily) << 24) |
            (static_cast<std::uint32_t>(kEggClassifierGenus) << 16) |
            (static_cast<std::uint32_t>(kEggClassifierSpecies) << 8);
        if (classifier == kEggClassifier && object->tick_enabled()) {
            ++egg_count;
        }
    }
    snapshot.egg_count = static_cast<int>(egg_count);

    return snapshot;
}

void C1WorldStatisticsFrame::ConcreteHost::set_display_text(
    std::string_view text) {
    owner_.statistics_display_.SetWindowTextA(std::string(text).c_str());
}

void C1WorldStatisticsFrame::ConcreteHost::start_timer(
    std::uint32_t timer_id, std::uint32_t interval_ms) {
    owner_.SetTimer(static_cast<UINT_PTR>(timer_id), interval_ms, nullptr);
}

void C1WorldStatisticsFrame::ConcreteHost::kill_timer(
    std::uint32_t timer_id) {
    owner_.KillTimer(static_cast<UINT_PTR>(timer_id));
}

void C1WorldStatisticsFrame::ConcreteHost::close_frame() {
    owner_.DestroyWindow();
}

void C1WorldStatisticsFrame::ConcreteHost::default_window_message() {
    owner_.ForwardDefaultMessage();
}

bool C1WorldStatisticsFrame::ConcreteHost::base_pre_translate(
    const creatures1::ui::WorldStatisticsKeyMessage& /*message*/) {
    return pending_message != nullptr &&
           owner_.ForwardBasePreTranslateMessage(pending_message) != FALSE;
}

void C1WindowsView::open_or_activate_world_statistics() {
    if (g_world_statistics_frame == nullptr) {
        auto* window = new C1WorldStatisticsFrame();
        if (!window->create_for(*this)) {
            delete window;
            return;
        }
    } else {
        g_world_statistics_frame->SetActiveWindow();
    }
}


// --- favourite place dialogs -----------------------------------------------

void C1AddFavouritePlaceDialog::DoDataExchange(CDataExchange* exchange) {
    CDialog::DoDataExchange(exchange);
    DDX_Text(exchange, kNameEdit, place_name_);
}

BEGIN_MESSAGE_MAP(C1RemoveFavouritePlaceDialog, CDialog)
    ON_BN_CLICKED(C1RemoveFavouritePlaceDialog::kRemoveButton, OnRemove)
END_MESSAGE_MAP()

BOOL C1RemoveFavouritePlaceDialog::OnInitDialog() {
    CDialog::OnInitDialog();
    place_list_.SubclassDlgItem(kPlaceList, this);
    for (std::size_t index = 0; index < document_.favourite_place_count();
         ++index) {
        place_list_.AddString(
            document_.favourite_place_name(index).c_str());
    }
    return TRUE;
}

void C1RemoveFavouritePlaceDialog::OnRemove() {
    const int selection = place_list_.GetCurSel();
    if (selection != LB_ERR) {
        removed_index_ = selection;
    }
    EndDialog(IDOK);
}

} // namespace creatures1::platform
