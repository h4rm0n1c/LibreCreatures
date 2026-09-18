# C1 UI surface audit

Audit only: semantic policy is not counted as executable wiring until the maintained MFC boundary routes it.

- Menu commands in canonical resource: 51
- Concrete frame message-map IDs: 156
- Status HWND/indicators/layout: wired
- Status score/world-time adapter: periodic native refresh route wired; values advance when world-update host is bound
- Status selected-creature owner: wired; object-pane formatter: concrete localized-caption route; live object-list owner: missing; EventBar click route: missing; life-force/glycogen owner: wired to chemical 0x3b
- Toolbar HWND/layout: wired; creature selector population/selection route: wired
- Document lifecycle: New wired; Open wired, Save wired, Serialize wired, DeleteContents wired, Close wired
- Frame WM_TIMER route: wired; pipe-command route: missing
- Menu-popup policy: native override delegates to CFrameWnd then rebuilds the creature menu
- Dynamic creature-selection menu: wired plumbing; native registry population still pending; Favourite Places: wired; embedded kits: registry-backed menu/toolbar population wired; command execution/update boundary missing

## Recovered view hook coverage

| Method | Recovered route | Native wiring |
|---|---|---|
| PreCreateWindow | virtual override | wired |
| OnInitialUpdate | virtual override | wired |
| OnDraw | virtual override | wired |
| OnSize | ON_WM_SIZE | wired |
| OnMouseButtonRelease | button-release route | wired |
| OnMouseMove | ON_WM_MOUSEMOVE | wired |
| OnLButtonDown | ON_WM_LBUTTONDOWN | wired |
| OnRButtonDown | ON_WM_RBUTTONDOWN | wired |
| OnHScroll | ON_WM_HSCROLL | wired |
| OnVScroll | ON_WM_VSCROLL | wired |
| OnKillFocus | ON_WM_KILLFOCUS | wired |
| OnKeyDown | ON_WM_KEYDOWN | wired |

## Menu command coverage

| Popup | ID | Caption | Status | Evidence |
|---|---:|---|---|---|
| &File | 32832 | &Web Connect | concrete_frame | maintained C1MainFrame message map |
| &File | 32863 | &Export current creature | concrete_frame | maintained C1MainFrame message map |
| &File | 32864 | &Import creature | concrete_frame | maintained C1MainFrame message map |
| &File | 57665 | E&xit | framework_default | ID_APP_EXIT |
| &World | 57600 | &New\tCtrl+N | concrete_document | ID_FILE_NEW -> C1WindowsDocument::OnNewDocument |
| &World | 57601 | &Open...\tCtrl+O | framework_shell_only | ID_FILE_OPEN -> CDocument default; C1 open override absent |
| &World | 57603 | &Save the world\tCtrl+S | framework_shell_only | ID_FILE_SAVE -> CDocument default; C1 serialize/save override absent |
| &World | 57604 | Save &As... | framework_shell_only | ID_FILE_SAVE_AS -> CDocument default; C1 serialize/save override absent |
| Testing | 32774 | &Infinite scroll | concrete_frame | maintained C1MainFrame message map |
| Testing | 32804 | Create a male norn | concrete_frame | maintained C1MainFrame message map |
| Testing | 32805 | Create a female norn | concrete_frame | maintained C1MainFrame message map |
| Testing | 32865 | Force Ageing | concrete_frame | maintained C1MainFrame message map |
| Testing | 32777 | Instant &verb vocabulary | concrete_frame | maintained C1MainFrame message map |
| Testing | 32786 | Creatures burble | concrete_frame | maintained C1MainFrame message map |
| Testing | 32807 | Infect current norn | concrete_frame | maintained C1MainFrame message map |
| Testing | 32840 | Euthanasia | concrete_frame | maintained C1MainFrame message map |
| &Log | 32808 | Sound | concrete_frame | maintained C1MainFrame message map |
| &Log | 32809 | Macros | concrete_frame | maintained C1MainFrame message map |
| &Log | 32810 | Serialisation | concrete_frame | maintained C1MainFrame message map |
| &Log | 32811 | Graphics | concrete_frame | maintained C1MainFrame message map |
| &Log | 32812 | Chemistry | concrete_frame | maintained C1MainFrame message map |
| &Log | 32813 | Attention | concrete_frame | maintained C1MainFrame message map |
| &Log | 32814 | Actions | concrete_frame | maintained C1MainFrame message map |
| &Log | 32815 | Senses | concrete_frame | maintained C1MainFrame message map |
| &Log | 32816 | Stimuli | concrete_frame | maintained C1MainFrame message map |
| &Log | 32817 | Genetics | concrete_frame | maintained C1MainFrame message map |
| &Log | 32818 | Life changes | concrete_frame | maintained C1MainFrame message map |
| &Log | 32819 | Environment | concrete_frame | maintained C1MainFrame message map |
| &Log | 32820 | User | concrete_frame | maintained C1MainFrame message map |
| &Log | 32821 | OLE events | concrete_frame | maintained C1MainFrame message map |
| &Log | 32822 | Objects | concrete_frame | maintained C1MainFrame message map |
| &Log | 32823 | Brain | concrete_frame | maintained C1MainFrame message map |
| &Log | 32869 | Log Window | concrete_frame | maintained C1MainFrame message map |
| &Log | 32870 | Verbose log | concrete_frame | maintained C1MainFrame message map |
| &View | 59392 | &Toolbar | concrete_frame | maintained C1MainFrame message map |
| &View | 59393 | &Status Bar | concrete_frame | maintained C1MainFrame message map |
| &View | 32896 | CAOS Console\tCtrl+Shift+C | concrete_frame | maintained C1MainFrame message map |
| _IDS_CAMERA_MENU_ | 32783 | &Track Creature | concrete_frame | maintained C1MainFrame message map |
| _IDS_CAMERA_MENU_ | 32866 | &Smooth Scrolling | concrete_frame | maintained C1MainFrame message map |
| _IDS_CAMERA_MENU_ | 113 | &Which is my creature? | concrete_frame | maintained C1MainFrame message map |
| _IDS_CAMERA_MENU_ | 32839 | Add to favourite places... | concrete_frame | maintained C1MainFrame message map |
| _IDS_CAMERA_MENU_ | 32867 | Remove from favourite places... | concrete_frame | maintained C1MainFrame message map |
| _IDS_TOOLS_MENU_ | 32771 | Creature's View | concrete_frame | maintained C1MainFrame message map |
| _IDS_NORNS_MENU_ | 32803 | None available | placeholder | creature-selection empty placeholder |
| Options | 32894 | Volume... | concrete_frame | maintained C1MainFrame message map |
| Options | 32891 | Mute Sounds | concrete_frame | maintained C1MainFrame message map |
| Options | 32897 | Mute Creature Voices | concrete_frame | maintained C1MainFrame message map |
| Options | 32892 | Informative Creatures Menu | concrete_frame | maintained C1MainFrame message map |
| &Help | 57667 | &Help Topics | framework_default | ID_HELP_FINDER |
| &Help | 107 | Tip of the Day | concrete_frame | maintained C1MainFrame message map |
| &Help | 57664 | &About Creatures... | concrete_frame | maintained C1MainFrame message map |
