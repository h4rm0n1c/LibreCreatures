#pragma once
// This project's OWN scaffolding, not a reconstruction of any real
// original file -- Ghidra's own placeholder names for "N bytes, exact
// type not yet recovered". Every real class header includes this.

typedef unsigned char undefined;
typedef unsigned char undefined1;
typedef unsigned short undefined2;
// Same real Ghidra placeholder family as undefined1/2/4/8 above, just
// the 3-byte member -- confirmed via live decompile of real functions
// (e.g. Bubble::Bubble @ 0x00429da0, Creature's methods): Ghidra's own
// decompiler emits "undefined3" verbatim for register/stack byte-runs
// of known size 3 with no recovered scalar type (most commonly
// `extraout_var`-named leftover high bytes of a bool-returning call's
// EAX, or a CONCAT21/CONCAT31/CONCAT13 operand). Stored 4-byte-wide
// like Ghidra itself does; real width is enforced by whichever CONCAT/
// SUB expression consumes it.
typedef unsigned int undefined3;
typedef unsigned int undefined4;
typedef unsigned long long undefined8;
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int dword;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef unsigned char uchar;
typedef signed char sbyte;
typedef unsigned long long ulonglong;
typedef long long longlong;

// Exact 12-byte SECURITY_ATTRIBUTES-shaped application record recovered in
// live Ghidra at 0x00472ec8.  It is declared before the generated global
// declarations because the object is stored by value there.
#pragma pack(push, 1)
struct C1SecurityAttributes {
    unsigned int nLength;
    void *lpSecurityDescriptor;
    int bInheritHandle;
};
#pragma pack(pop)

// Ghidra's `uint5` is an unnamed five-byte temporary widened to the next
// native scalar by the exporter. Keep the storage width conservative at the
// source boundary; the live uses consume it through 32/64-bit arithmetic.
typedef unsigned long long uint5;
typedef void (*_func_void_void_ptr)(void *);  // Ghidra's own synthetic name for this
                                               // anonymous function-pointer shape.
// MSVC __ehvec_dtor passes each element in ECX to the destructor callback;
// this is distinct from the cdecl callback shape above used by __ArrayUnwind.
typedef void (__thiscall *C1MsvcThiscallVoidPtr)(void *);
// Persisted Ghidra type for the target's __eh vector constructor callback;
// unlike the generic cdecl `_func_void_void_ptr`, this callback receives its
// element address in ECX under MSVC x86 __thiscall.
typedef void (__thiscall *C1MsvcVectorThiscallCallback)(void *);

// Ghidra's own documented byte-concatenation intrinsics
// (CONCATxy(hi, lo): hi occupies the upper x bytes of an (x+y)-byte
// result, lo the lower y bytes) -- real, standard Ghidra decompiler
// notation, not invented here; confirmed real and widespread via a
// bulk-compile run (C3861, 38 files) and live decompile spot-checks
// (e.g. CGenome.cpp's CONCAT21(CONCAT11(...),...) byte-array-to-int
// reassembly, CArchiveRuntimeState.cpp's CONCAT31 flag-byte packing).
// Result widths that don't land on a native size (3, 5 bytes) are
// widened to the next real integer type up -- safe because every real
// call site immediately truncates the result further (via assignment
// to a narrower type or an explicit cast), so the extra zero bits
// never survive to be observed.
static inline unsigned short CONCAT11(unsigned char hi, unsigned char lo) {
    return (unsigned short)(((unsigned short)hi << 8) | lo);
}
static inline unsigned int CONCAT13(unsigned char hi, unsigned int lo) {
    return ((unsigned int)hi << 24) | (lo & 0xFFFFFFu);
}
static inline unsigned int CONCAT21(unsigned short hi, unsigned char lo) {
    return ((unsigned int)hi << 8) | lo;
}
static inline unsigned int CONCAT22(unsigned short hi, unsigned short lo) {
    return ((unsigned int)hi << 16) | lo;
}
static inline unsigned int CONCAT31(unsigned int hi, unsigned char lo) {
    return ((hi & 0xFFFFFFu) << 8) | lo;
}
// Exact x86 frame-pointer value used by MSVC's /GS cookie prologue.  The
// decompiler exposes the saved cookie as `cookie ^ &stack...`; retain the
// machine-level EBP operand so the generated source can reproduce it.
static __forceinline unsigned int C1FramePointer() {
    __asm mov eax, ebp
}

// CMacroHolder's dispatch table is a four-byte MSVC member-function-pointer
// array in the recovered object layout.  Ghidra correctly preserves the
// four-byte storage but cannot emit a portable C++ type for the mixed member
// signatures assigned to it.  Keep the storage ABI exact and provide the
// two narrowly-scoped views needed by the generated source: a union extraction
// for the assignment (MSVC x86's single-word PMF representation here), and a
// __thiscall invocation type for the indirect call.  The actual field size and
// all five targets are independently recorded in the live CMacroHolder facts.
struct CMacroHolder;
typedef unsigned int (__thiscall *C1MacroDispatchCallbackFn)(CMacroHolder *, void *);
template <typename MemberFunction>
static __forceinline unsigned int C1MemberFunctionAddress(MemberFunction member) {
    union {
        MemberFunction member_function;
        unsigned int address;
    } value;
    value.member_function = member;
    return value.address;
}

static inline unsigned long long CONCAT32(unsigned int hi, unsigned short lo) {
    return ((unsigned long long)(hi & 0xFFFFFFu) << 16) | lo;
}
static inline unsigned long long CONCAT44(unsigned int hi, unsigned int lo) {
    return ((unsigned long long)hi << 32) | lo;
}
static inline unsigned int CARRY4(unsigned int a, unsigned int b) {
    return (a + b < a) ? 1u : 0u;
}
// CONCAT72 (7-byte hi, 2-byte lo -> 9-byte result) is NOT declared
// here: its only 2 real call sites (Skeleton.cpp, ~line 1814/1825) feed
// it operands built from a 13-byte SIMD/x87-shift byte array
// (`auVar7 << 0x40` through a matching `SUB137` sub-piece extraction)
// that isn't understood yet -- needs its own real-evidence investigation
// rather than a guessed signature. Left as a known, tracked gap.

// Ghidra's own documented zero-extension intrinsic (ZEXTxy: widen an
// x-byte unsigned value to y bytes, filling the new high bytes with
// zero) -- same real, standard Ghidra decompiler notation family as
// CONCATxy above, not invented here. Only ZEXT48 (4-byte -> 8-byte)
// found anywhere in this codebase (confirmed via a whole-tree grep,
// CBrain.cpp's own pointer-to-64-bit-multiply idiom, e.g. `ZEXT48
// (pCVar29) * 0x10`) -- declared narrowly rather than the whole
// ZEXTxy family since no other real call site needs one yet.
static inline unsigned long long ZEXT48(unsigned int x) {
    return (unsigned long long)x;
}
template <typename T>
static inline unsigned long long ZEXT48(T *x) {
    return (unsigned long long)(unsigned int)x;
}

// x86 SEH's per-frame exception-registration chain head. Ghidra recovers
// the compiler's fs:[0] reads/writes as the name `ExceptionList`; it is not
// an application global. Model the actual x86 segment-relative lvalue so
// generated code neither allocates nor links a fake process-global object.
#include <intrin.h>
#define ExceptionList (*((void **)(unsigned int)__readfsdword(0)))

// MSVC TLS runtime symbols emitted by Ghidra for one-time initialization
// guards. They are runtime data symbols, not project objects.
    extern void *ThreadLocalStoragePointer;
    extern int _tls_index;
// Exact live Ghidra data item at 0x00466090, confirmed against the pinned
// MSVC CRT's __isa_enabled feature-mask symbol. This is an external runtime
// link boundary; the game build does not allocate replacement storage.
extern "C" unsigned int __isa_enabled;
// Exact live Ghidra data item at 0x00466088, paired with __isa_enabled.
// The live listing exposes the low dword because the recovered startup code
// consumes that portion, while the pinned MSVCRT ABI declares the adjacent
// runtime object as an 8-byte unsigned value.  Keep the CRT's full object
// type at the link boundary rather than allocating replacement storage.
extern "C" unsigned long long __isa_inverted;
// MSVC C++ EH dispatch remains a linked CRT entry; recovered funclets call
// it with either the default or one continuation argument.
extern "C" void __cdecl __CxxFrameHandler3(...);
// This startup helper is the pinned CRT's processor feature-level storage,
// not an application global.
extern "C" unsigned int __scrt_processor_feature_level;
// The 0x0044bc6a thunk tail-jumps through MFC's imported AfxWinMain entry.
// Keep the implementation in MFC; this declaration is only the recovered
// source-level ABI boundary used by the thunk.
extern int __stdcall AfxWinMain(HINSTANCE,HINSTANCE,LPSTR,int);
// Exact version strings promoted in live Ghidra from their byte contents.
extern char s_version_major_004564f4[4];
extern char s_version_minor_004564f8[2];
extern const char s_community_edition_version_0045c0d4[5];
// Exact live-Ghidra data item at 0x00454f1c, used by InitializeDdeService.
extern const char s_Vivarium_00454f1c[9];
extern const char s_SFC_OLE_00458254[8];

// These unnamed data items are real address-bearing constants/objects used
// by SFCApp and the OLE registration path.  Keep the original symbol names
// at the export boundary until the corresponding live Ghidra data items are
// promoted; the declarations do not allocate replacement storage.
extern GUID g_sfc_ole_clsid;
extern GUID g_sfc_document_template_clsid;
extern const char s_registry_separator_0045adb4[3];
extern const char s_patch_registry_value_name[4];
extern unsigned int g_std_stringbuf_vftable_terminator;
extern const char s_Macro_Directory_0045aec0[];
extern const char s_Palette_Directory_0045aed0[];
extern const char s_Image_Directory_0045aee4[];
extern const char s_Genetics_Directory_0045aef4[];
extern const char s_Body_Data_0045af08[];
extern const char s_Programs_0045af14[];
extern const char s_wav_file_extension[];
extern const char s_sound_filename_placeholder_xxxx[];

// Ghidra's DAT_80000002 is not image data: it is the predefined Win32
// HKEY_LOCAL_MACHINE handle value recovered as an absolute address-shaped
// constant.  Preserve that exact source-level address instead of creating a
// relocatable global object.
#define DAT_80000002 (*(unsigned char *)(unsigned int)0x80000002u)

// Exact live Ghidra data items used by the recovered source. These are
// declarations of the original addresses, not replacement storage.
extern const char s_Dummy_00458170[];
extern const char s_242212212011200_00466b70[];
// Live Ghidra identifies 0x00458f48 as the eight-byte string "ERROR".
// Keep the original address-bearing symbol; it is shared by the pipe and
// macro command error paths.
extern const char s_ERROR_00458f48[];
// Live Ghidra data at 0x00466bd4 is the five-byte NUL-terminated string
// "Tool", used as the registry value-name prefix by the embedded-kit menu.
// It is a named data object now; retain the address-bearing declaration at
// the source boundary rather than treating it as an integer DAT.
extern const char s_tool_prefix_00466bd4[5];
// Exact live counter used to bound the script-definition table.
extern int g_script_definition_count;

// Raw CRT/static-initialization storage referenced by the decompiler. The
// live Ghidra program exposes these addresses as undefined/raw data; keep
// their byte-address semantics at the export boundary instead of inventing
// reconstructed C++ objects or enums.
extern int g_crash_report_stream_init_guard;
    extern unsigned char g_crash_report_stream[80];
    extern unsigned int g_msvc_local_stdio_printf_options[2];
extern double g_unsigned_int_double_biases[2];
extern unsigned char DAT_0045d4f0;
extern unsigned char DAT_0045d4f4;
extern unsigned int g_debug_log_flush_timer_id;
extern unsigned char s_text_input_punctuation_characters[13];
// Exact 5x3 byte table used by the macro object-attribute decoder.
extern unsigned char g_macro_object_attribute_table_004579a4[15];
// Exact XML entity literals persisted in live Ghidra at 0x00459f9c,
// 0x00459fa4, 0x00459fac, and 0x00459fb4.  These are named data arrays,
// not guessed replacement objects.
extern const char g_xml_escape_ampersand[];
extern const char g_xml_escape_less_than[];
extern const char g_xml_escape_greater_than[];
extern const char g_xml_escape_quoted[];
// Classifier serialization mask words. Ghidra exposes these as raw dwords;
// their consumers perform bitwise tests, so they are storage values rather
// than enum members.
extern unsigned int g_classifier_event_masks[4];
extern unsigned int g_msvc_basic_filebuf_state_word_0;
extern unsigned int g_msvc_basic_filebuf_state_word_1;
extern unsigned int g_registry_key_disposition;
extern int g_pointer_tool_name_update_deadline;
// Win32BuildCurrentUserSecurityDescriptor's exact 12-byte SECURITY_ATTRIBUTES
// record, plus the cached descriptor pointer and one-time initialization
// state. These are application-owned data objects recovered in Ghidra.
extern C1SecurityAttributes g_current_user_security_attributes;
extern void *g_current_user_security_descriptor;
extern int g_current_user_security_descriptor_init_state;
// The pointed-to object is a real MSVC codecvt facet, but this declaration
// appears before the standard-library includes in the prelude. Keep the
// address opaque here; the recovered STL boundary casts it after <locale>
// is visible.
extern void *g_cached_msvc_codecvt_facet;
extern undefined *g_mfc_get_this_message_map_ptr_a;
extern undefined *g_mfc_get_this_message_map_ptr_b;

// The first entry in SFCApp::primary_resource_directories is the main
// resource directory used by CopyPrimaryResourceDirectoryPath.
static constexpr unsigned int C1_RESOURCE_DIRECTORY_MAIN = 0u;

// UpdateWorld indexes and calls a table of no-argument phase handlers.
// Keep that source-boundary ABI explicit rather than declaring an int array.
typedef void (*C1WorldTickPhaseDispatchFn)(void);
// Live Ghidra types this four-byte scheduler state as WorldTickPhase.  The
// complete symbolic domain is not recovered yet, so preserve the exact
// width/name without fabricating enum members.
typedef unsigned int WorldTickPhase;
static constexpr unsigned int WORLD_TICK_PHASE_EVENT_QUEUE_0 = 0u;
static constexpr unsigned int WORLD_TICK_PHASE_BRAIN_INPUTS_1 = 1u;

// Body-part indices recovered from Skeleton::LoadGenome's live table and
// sprite-loader call sites.  These names are the semantic values behind
// the decompiler's four-byte C1BodyPartIndex fallback.
static constexpr unsigned int PART_HEAD = 0u;
static constexpr unsigned int PART_BODY = 1u;
static constexpr unsigned int PART_LTHIGH = 2u;
static constexpr unsigned int PART_LSHIN = 3u;
static constexpr unsigned int PART_LFOOT = 4u;
static constexpr unsigned int PART_RTHIGH = 5u;
static constexpr unsigned int PART_RSHIN = 6u;
static constexpr unsigned int PART_RFOOT = 7u;
static constexpr unsigned int PART_LHUMERUS = 8u;
static constexpr unsigned int PART_LRADIUS = 9u;
static constexpr unsigned int PART_RHUMERUS = 10u;
static constexpr unsigned int PART_RRADIUS = 11u;
static constexpr unsigned int PART_TAILROOT = 12u;
static constexpr unsigned int PART_TAILTIP = 13u;

// Real MFC vtable addresses are target-image data items, not synthetic
// compile-only objects.  The source-boundary normalizer materializes the
// exact addresses recorded by the live Ghidra RTTI/vftable labels at each
// recovered assignment site.  No fake placeholder symbol is permitted.

// Ghidra's recovered source uses C1MfcCFile for stack objects whose live
// constructors/destructors are the real MFC CFile routines. This is an
// alias, not a second layout: placement-new and all member calls are
// explicitly CFile calls. The forward declaration is needed because this
// prelude is included before the real MFC headers in per-class headers;
// aggregate builds provide the complete CFile definition before use.
class CFile;
typedef CFile C1MfcCFile;

// Real-CWnd-member raw HWND read, used once this project started
// composing REAL MFC control classes (CEdit/CButton/CComboBox/...) as
// actual member fields instead of flat public-access mimic structs.
// Real CWnd's own `m_hWnd` is PROTECTED, so a sibling member object
// (e.g. a `CEdit output_edit;` field on a class that doesn't itself
// derive from CEdit) can't name it via `.`/`->` -- but Ghidra's own
// real decompile evidence for every one of these call sites is a
// direct raw memory read at a fixed byte offset (`*(HWND
// *)(pCVar2 + 0x20)`-shaped, confirmed real via GetDlgItem() return
// values elsewhere in the same functions), meaning the REAL compiled
// original did a raw read too, not a `GetSafeHwnd()` call (which would
// compile to different, extra machine code -- an actual/possibly-not-
// inlined function call plus a null check -- breaking byte-for-byte
// matching even though it would also compile). Real CWnd::m_hWnd sits
// at offset 32 (confirmed live via this exact toolchain: 4x4-byte
// vtable-adjacent fields, matching this project's own C1MfcCEdit/
// C1MfcCButton mimics' own already-evidence-derived offset exactly).
// Pointer arithmetic on an object's OWN address, with no `.`/`->`
// member name used at all, is unaffected by C++ access control --
// legal regardless of the real field's protection level.
#define C1RawHwnd(cwndMemberExpr) (*(HWND *)((char *)&(cwndMemberExpr) + 32))

// Real CFrameWnd-derived project objects are exported by Ghidra with a
// synthetic `cframewnd_base.vtable` member.  Once the header models the
// confirmed MFC base as actual inheritance, that spelling is no longer a
// C++ member.  These exact raw-object accessors preserve the recovered
// first-word ABI without inventing a second CFrameWnd layout.
#define C1RawVtable(cwndObjectPtrExpr) (*(void ***) (cwndObjectPtrExpr))

// CWnd::Default is protected.  Ghidra emits calls through a recovered
// CWnd* for objects whose CWnd subobject is still represented as an exact
// 128-byte blob, so the direct spelling is rejected by C++.  This
// dependent access shim enters through a derived context while retaining
// the same receiver and virtual dispatch boundary; it does not model or
// resize the recovered object.
template <typename T>
struct C1WndDefaultAccess : T {
    static LRESULT Call(T *window) {
        return static_cast<C1WndDefaultAccess *>(window)->T::Default();
    }
};
template <typename T>
static inline LRESULT C1WndDefault(T *window) {
    return C1WndDefaultAccess<T>::Call(window);
}

// CWnd::OnSetFocus is protected.  This recovered free-function wrapper
// calls the real base implementation on a CWnd*; keep that boundary legal
// without exposing the handler globally.
struct C1WndOnSetFocusAccess : CWnd {
    static void Call(CWnd *window, CWnd *old_focus_window) {
        static_cast<C1WndOnSetFocusAccess *>(window)->CWnd::OnSetFocus(old_focus_window);
    }
};

// CDialog::OnInitDialog is protected and has no explicit receiver in its
// source-level signature. Ghidra renders calls from recovered global
// wrappers as `CDialog::OnInitDialog(dialog)`, which is an ABI-shaped call,
// not valid C++. Enter through a derived access context without changing the
// MFC object or dispatch boundary.
struct C1DialogOnInitDialogAccess : CDialog {
    static BOOL Call(CDialog *dialog) {
        return static_cast<C1DialogOnInitDialogAccess *>(dialog)->CDialog::OnInitDialog();
    }
};

// Same real raw-offset-read idiom as `C1RawHwnd` above, for real
// `CGdiObject`-derived locals (CBrush, CBitmap, ...) placement-
// constructed into raw storage -- real CGdiObject's own `HGDIOBJ m_hObject` is PROTECTED and sits
// right after the vtable pointer at offset 4 (real, well-known MFC
// layout: CGdiObject derives from CObject, which itself has no data
// members beyond the vtable). Confirmed via live evidence at a real
// call site (CTipDlg::OnPaint @ 0x00443b70): Ghidra's own decompile
// shows the just-`Attach()`-ed handle being re-read from this exact
// offset for a later `GetObjectA`/`SelectObject` call rather than
// reusing the register/local that held the value right before the
// `Attach()` call -- the real original compiler spilled it there too
// (the natural place, since `Attach()` had just written it), not to a
// separate stack slot; matching that read exactly (instead of
// substituting the pre-`Attach()` value) is what keeps this byte-exact
// rather than just behaviorally equivalent.
#define C1RawGdiHandle(cgdiObjectMemberExpr) (*(HGDIOBJ *)((char *)&(cgdiObjectMemberExpr) + 4))

// Real MSVC CRT internal state (secchk.c's __report_gsfailure, pulled
// in automatically from the runtime library by /GS-enabled code) --
// confirmed via live Ghidra evidence: the function writing this global
// (MsvcCaptureContextAndInvokeFatalHandler, 0x0044a9e0) stores
// 0xc0000409 (STATUS_STACK_BUFFER_OVERRUN, the GS-cookie-failure NTSTATUS
// code) into the first field, matches the real CRT's static
// `EXCEPTION_RECORD ExceptionRecord; EXCEPTION_POINTERS
// ExceptionPointers = {&ExceptionRecord, &ContextRecord};` layout
// exactly (g_gsFailureExceptionPointers -> g_gsFailureExceptionRecord,
// both renamed from Ghidra's PTR_DAT_/DAT_ placeholders to reflect this).
extern _EXCEPTION_POINTERS g_gsFailureExceptionPointers;
extern EXCEPTION_RECORD g_gsFailureExceptionRecord;

// This project's own bare (unqualified, all-caps) alias for real MFC's
// `CArchive::load` bit flag (`enum Mode { store = 0, load = 1, ... }`,
// afx.h) -- 229 real call sites across 31 files check
// `archive->m_nMode & LOAD`, matching CArchive::IsLoading()'s own real
// implementation (`(m_nMode & load) != 0`). Bound to the real enum
// value rather than hardcoding 1, so it stays correct if that ever
// changes.
const UINT LOAD = CArchive::load;

// This project's own bare enum-shaped constant NAMES (a prior renaming
// pass gave these real, readable names, but never actually declared
// them as real C++ constants anywhere) -- confirmed real via bulk-
// compile errors surfacing as earlier blockers cleared (Blackboard.cpp:
// TEXT_INPUT_ALLOW_*; CGenome-family call sites: CREATURE/BIOCHEMISTRY/
// BRAIN/IGNORE_STAGE/MATCH_GENOME_LOAD_STAGE). Real bit/index VALUES
// are NOT independently verified here (would need per-constant Ghidra
// evidence) -- placeholder values chosen to be functionally consistent
// with real USAGE SHAPE (bitwise-OR'd together -> distinct power-of-2
// bits; used as a plain selector/index argument -> small sequential
// ints), enough to compile correctly, not yet confirmed byte-exact.
// Flagged here, not silently guessed and left undocumented.
const UINT TEXT_INPUT_ALLOW_DIGITS = 1;
const UINT TEXT_INPUT_ALLOW_LETTERS = 2;
const UINT TEXT_INPUT_ALLOW_SPACE = 4;
const UINT TEXT_INPUT_ALLOW_EXCLAMATION_MARK = 8;
const UINT TEXT_INPUT_ALLOW_QUESTION_MARK = 0x10;
const UINT TEXT_INPUT_ALLOW_PUNCTUATION = 0x20;

const int CREATURE = 0;
const int BIOCHEMISTRY = 1;
const int BRAIN = 2;
const int IGNORE_STAGE = -1;
const int MATCH_GENOME_LOAD_STAGE = 0;

const int C1_SOUND_AUDIBILITY_ACTIVE_VIEWPORT = 0;
const int C1_SOUND_AUDIBILITY_EXTENDED_RANGE = 1;
const int C1_SOUND_AUDIBILITY_OUT_OF_RANGE = 2;
// EnsureSoundCacheCapacity returns 2 when no eligible cache space can be
// made; success is the ordinary zero status. These are source-level names
// for a four-byte result, not an invented packed enum representation.
static const unsigned int C1_SOUND_CACHE_CAPACITY_OK = 0u;
static const unsigned int C1_SOUND_CACHE_CAPACITY_UNAVAILABLE = 2u;
static const unsigned int C1_SOUND_MIXER_SUSPENDED = 1u;
static const unsigned int C1_SOUND_NO_AVAILABLE_CHANNEL = 2u;
static const unsigned int C1_SOUND_INVALID_CHANNEL_HANDLE = 3u;

// SFCApp::SFCApp @ 0x0043df30 stores these exact four-byte privilege
// values after decoding the registry strings.  Keep them as constants until
// the complete named Ghidra enum is recovered; the values themselves are
// disassembly-confirmed.
static const unsigned int USER = 3u;
static const unsigned int PRIVILEGE_LEVEL_2 = 2u;
static const unsigned int GREEN_TEA = 1u;

// Exact values from the live Ghidra C1MacroExecutionMode enum.  The
// decompiler emits this recovered typedef as an unsigned scalar in the
// generated class header, so these are constants rather than a duplicate
// enum declaration.
const unsigned int START_EXECUTION = 0;
const unsigned int EXECUTE_TO_OUTPUT = 1;
const unsigned int FORMAT_BRAIN_ACTIVITY_REPORT = 2;
const unsigned int DEFAULT_SUCCESS_3 = 3;
const unsigned int DEFAULT_SUCCESS_4 = 4;

// Live C1BiochemistryLocusKind use and the Creatures reference constants
// identify these resolver selectors exactly.
const unsigned int RECEPTOR = 0;
const unsigned int EMITTER = 1;
// Live Ghidra disassembly of CBiochemistry::Update @ 0x0042ee10:
// emitter flags bit 0x4 inverts the source locus, bit 0x2 selects fixed
// emission, and bit 0x1 clears the source locus after emission.
const unsigned int INVERT_SOURCE_LOCUS = 4;
const unsigned int FIXED_EMISSION_AMOUNT = 2;
const unsigned int CLEAR_SOURCE_LOCUS = 1;
// Exact C1CreatureDeathState values from Creature::Die @ 0x0040dcb8:
// the ALIVE guard tests byte 0x00 and the transition stores byte 0xff.
// Ghidra documentation enum: /Creatures1/C1CreatureDeathStateValues.
const unsigned int ALIVE = 0u;
const unsigned int DEAD = 0xffu;

// Exact /Creatures1/ObjectBoundsMode enum recovered in Ghidra (1-byte
// storage).  SetBoundsMode receives the promoted 32-bit parameter, so the
// source-facing constants remain unsigned ints while object fields retain
// their byte ABI.
static const unsigned int DEFAULT_WORLD = 0u;
static const unsigned int UNBOUNDED_1 = 1u;
static const unsigned int UNBOUNDED_2 = 2u;
static const unsigned int VEHICLE_LOCAL_BOUNDS = 3u;
static const unsigned int EXPLICIT_RECT = 4u;

// Exact live InitializeRuntimeState writes: [Object+0x09] |= 0x02 and
// classifier.family = 4 in the packed ScriptClassifier word.
static const unsigned int ALLOW_POINTER_TOOL_UNBOUNDED_PLACEMENT = 0x02u;
// SimpleObject::HandleQueuedEvent4 @ 0x00427cc0 tests the one-byte
// Object::bounds_flags field at +0x09 with 0x01 before allowing a Creature
// source to establish EXPLICIT_RECT bounds.  This is a flag bit, not a
// scalar enum value; keep ObjectBoundsFlags byte-sized and expose the
// promoted source mask.
static const unsigned int ALLOW_CREATURE_EXPLICIT_RECT_BOUNDS = 0x01u;
// SimpleObject::EndInteractionWithSource @ 0x00428bb0 tests the same
// byte-sized Object::bounds_flags field with 0x20 to retain unbounded
// placement after an interaction.  This is another independent flag bit,
// not a scalar enum value.
static const unsigned int KEEP_UNBOUNDED_AFTER_INTERACTION = 0x20u;
static const unsigned int C1_SCRIPT_DEACTIVATE = 0u;
// MacroManager::SCRIPT_TIMER is the ninth script event in the live C1 table.
static const unsigned int C1_SCRIPT_TIMER = 9u;

// AdvanceBacteriumServicePhase uses a byte-sized seven-step scheduler. The
// values are machine constants recovered from the phase comparisons; keep
// them as 32-bit constants rather than introducing an enum object into the
// exported ABI.
typedef unsigned int BacteriumServicePhase;
static const unsigned int BACTERIUM_SERVICE_IDLE_0 = 0u;
static const unsigned int BACTERIUM_SERVICE_INFECTION_ATTEMPT = 1u;
static const unsigned int BACTERIUM_SERVICE_IDLE_2 = 2u;
static const unsigned int BACTERIUM_SERVICE_IDLE_3 = 3u;
static const unsigned int BACTERIUM_SERVICE_IDLE_4 = 4u;
static const unsigned int BACTERIUM_SERVICE_IDLE_5 = 5u;
static const unsigned int BACTERIUM_SERVICE_IDLE_6 = 6u;
static const unsigned int C1_CLASSIFIER_FAMILY_SCENERY = 1u;
static const unsigned int C1_CLASSIFIER_FAMILY_SIMPLE_OBJECT = 2u;
static const unsigned int C1_CLASSIFIER_FAMILY_COMPOUND_OBJECT = 3u;
static const unsigned int C1_CLASSIFIER_FAMILY_CREATURE = 4u;
// SimpleObject::HandleQueuedEvent{0,1,2} tests the one-byte interaction
// flag at offsets 0x5b with 0x01, 0x02, and 0x04 respectively.  These are
// bit flags, not a scalar enum: keep the field byte-sized and expose the
// promoted masks used by the recovered source.
static const unsigned int EVENT_1_ENABLED = 0x01u;
static const unsigned int EVENT_2_ENABLED = 0x02u;
static const unsigned int EVENT_0_ENABLED = 0x04u;

// Ghidra's packed descriptor/chemical structs are four-byte wire values
// whose field view is byte-oriented.  This helper preserves the exact
// four-byte load when the decompiler expresses that load as a cast.
template <typename T>
static __forceinline unsigned int C1PackedU32(const T& value) {
    return *(const unsigned int *)&value;
}

// Exact live-disassembly constants. ObjectBoundsFlags remains an
// exact-width byte typedef; these promoted names are used at source
// call-sites until the complete flag family is recovered.
static const unsigned int IS_VEHICLE = 0x08u;
// Vehicle::Tick @ 0x0042bd90 tests ObjectBoundsFlags with TEST AL,0x40;
// the recovered Vehicle::bounds_flags field is one byte, so preserve the
// byte's bit value while exposing the promoted source constant.
static const unsigned int OBJECT_BOUNDS_USE_CURRENT_MAP_ROOM = 0x40u;
// /Creatures1/VehicleCollisionSideFlags, persisted in Ghidra after
// cross-checking Vehicle::Tick's boundary tests against the installed
// COLLISION script: RIGHT=1, LEFT=2, TOP=4, BOTTOM=8.
static const unsigned int VEHICLE_COLLISION_NONE = 0u;
static const unsigned int VEHICLE_COLLISION_RIGHT = 1u;
static const unsigned int VEHICLE_COLLISION_LEFT = 2u;
static const unsigned int VEHICLE_COLLISION_TOP = 4u;
static const unsigned int VEHICLE_COLLISION_BOTTOM = 8u;
// Skeleton's down-foot selector is a separate two-valued domain from the
// vehicle collision flags. Ghidra /Creatures1/C1DownFoot is LEFT=0, RIGHT=1;
// do not reuse VehicleCollisionSideFlags::LEFT (which is 2).
static const unsigned int RIGHT = 1u;
static const unsigned int LEFT = 0u;
// Exact event IDs used by the recovered script-event dispatch calls.  They
// remain source constants because the calls promote the byte-sized event
// value to the ABI's 32-bit argument; no wider field enum is being inferred.
static const unsigned int EVENT_0 = 0u;
static const unsigned int EVENT_1 = 1u;
static const unsigned int EVENT_2 = 2u;
static const unsigned int EVENT_3 = 3u;
static const unsigned int EVENT_4 = 4u;
static const unsigned int EVENT_5 = 5u;
static const unsigned int EVENT_6 = 6u;
static const unsigned int EVENT_7 = 7u;
static const unsigned int EVENT_8 = 8u;
static const unsigned int EVENT_9 = 9u;

// Exact /Creatures1/C1DriveThresholdState stores from
// Creature::UpdateDriveThresholdState @ 0x00409110:
// initial store 1, lower-threshold transition 0, upper-threshold transition 2.
static const unsigned int ALL_AT_OR_BELOW_LOWER = 1u;
static const unsigned int SOME_ABOVE_LOWER = 0u;
static const unsigned int SOME_ABOVE_UPPER = 2u;

// Exact C1CreatureAction values used by UpdateActionSelection: the action
// scan starts at zero, advances one action per 0x10-byte lobe-6 slot, and
// treats 15 as the last named action before the reserved range.
static const unsigned int C1_ACTION_QUIESCENT = 0u;
static const unsigned int C1_ACTION_ACTIVATE1 = 1u;
static const unsigned int C1_ACTION_RESERVED_15 = 15u;

// Exact MapRoomRecord.room_type discriminator from
// MapData::GetAmbientTemperatureAtPoint @ 0x00422e5b:
// CMP dword ptr [EAX+0x10], 1.  The corresponding four-byte Ghidra
// documentation enum is /Creatures1/C1MapRoomTypeValues.
static const unsigned int USES_MAP_AMBIENT_TEMPERATURE = 1u;

// Exact one-byte facing values proven by Skeleton::InitializePoseAndMotionState
// (0x0043acf1 writes 0x0102, so facing=2) and the three-way dispatch in
// Skeleton::RecomputeBodyPartLayout @ 0x0043b5a0 (1, 2, 3).  The default
// branch is the remaining NORTH value used by AdvancePoseAnimation.
static const unsigned char NORTH = 0u;
static const unsigned char SOUTH = 1u;
static const unsigned char EAST = 2u;
static const unsigned char WEST = 3u;

// Exact SFCView pending-input flag bits from OnLButtonDown @ 0x00437200
// and OnRButtonDown @ 0x00437240: left=0x1, right=0x2, left+shift=0x4,
// right+shift=0x8.
static const unsigned int SFCVIEW_INPUT_LEFT_BUTTON = 0x1u;
static const unsigned int SFCVIEW_INPUT_RIGHT_BUTTON = 0x2u;
static const unsigned int SFCVIEW_INPUT_LEFT_WITH_SHIFT = 0x4u;
static const unsigned int SFCVIEW_INPUT_RIGHT_WITH_SHIFT = 0x8u;

// Exact /Creatures1/C1ActionTargetRequirement members.  The table is a
// byte array in the binary; these constants retain that width at the local
// value boundary without introducing a compiler-owned enum representation.
static const unsigned char NOT_SELECTABLE = 0u;
static const unsigned char ALWAYS_ELIGIBLE = 1u;
static const unsigned char REQUIRES_NO_TARGET = 2u;
static const unsigned char REQUIRES_TARGET = 3u;

// InitializeFromGenome constructs CGenome in compiler-managed stack storage
// via placement new.  This wrapper expresses that exact storage operation
// without requiring a nonexistent default constructor.
template <typename T>
struct C1AlignedStorage {
    alignas(T) unsigned char bytes[sizeof(T)];
};

// Exact one-byte enum members from live Ghidra types/disassembly:
// CBacterium's constructor stores zero in activity_state, and CImage's
// cache-flag tests/stores establish these independent bits.
static const unsigned char INACTIVE_OR_DEAD = 0u;
static const unsigned char OWNS_PIXEL_DATA_DIRECTLY = 1u;
static const unsigned char PIXEL_DATA_RESIDENT = 2u;
static const unsigned char CACHE_PROTECTED = 4u;

// Live Bubble constructor/positioning code compares placement_mode with the
// exact centered-viewport selector value 2.
const unsigned int BUBBLE_PLACEMENT_VIEWPORT_CENTRED = 2;

// Real DDEML API type, see NATIVE_MAP's own C1DdeTransactionType
// comment for the evidence -- that entry only affects STRUCT FIELD
// generation, not direct .cpp source text (like DDEService.cpp's own
// restored function definitions) referencing the name directly, so it
// needs a real typedef here too.
typedef UINT C1DdeTransactionType;

// Real function-pointer shape for DDEServiceItem::create_data -- see
// its own NATIVE_MAP comment for the call-site evidence.
struct DDEServiceItem;
struct Macro;
typedef HDDEDATA (*DDEServiceItemDataHandlerFn)(DDEServiceItem *, Macro *);

// MFC's real CPtrArray/CObArray has this exact stable, publicly-documented
// field layout, but m_nSize etc. are PROTECTED -- this reproduces the
// layout with public access instead of claiming to be MFC's real
// class. IMPORTANT: every one of this project's own registry globals
// (g_EntityRegistry, g_gallery_registry, g_creature_registry,
// g_non_scenery_object_registry, g_creature_selection_array,
// g_SceneryRegistry, g_world_object_registry) is really a
// CTypedPtrArray<CPtrArray|CObArray, T*> instance, NOT a bare
// CPtrArray/CObArray -- CTypedPtrArray adds no fields of its own, but
// CObject (which CPtrArray/CObArray both derive from) puts a single
// vtable pointer at offset 0, shifting every real field down by 4
// bytes. Confirmed via live Ghidra evidence, not assumed: (1) each
// global's own real constructor-call address and its immediately
// following `<global> = CTypedPtrArray<...>::vftable;` write target
// the SAME base address (so the vtable pointer really does live at
// offset 0, not a separate object); (2) Ghidra had already
// auto-recovered g_gallery_registry's real field layout as a named
// 20-byte structure (`CGalleryPtrArray`) with fields at exactly these
// offsets and these exact names (cobject_vptr@0, m_pData@4, m_nSize@8,
// m_nMaxSize@12, m_nGrowBy@16) -- this struct's field names/offsets
// were taken directly from that real recovered structure, not
// invented. (Earlier versions of this struct omitted cobject_vptr, an
// undetected 4-byte-offset bug for every field on every registry
// global using it -- fixed here from this real evidence.)
struct Creature;
struct Object;
struct Entity;
// Ghidra's recovered source uses EntityPtr as a named pointer local in
// several buckets, while the field exporter correctly lowers the same
// evidence to Entity*. Keep the source-facing alias in the shared prelude
// so locals and fields describe the same four-byte pointer without relying
// on a generated Entity.hpp include order.
typedef Entity * EntityPtr;

struct C1PtrArrayLike {
    void *cobject_vptr;
    void *m_pData;
    int m_nSize;
    int m_nMaxSize;
    int m_nGrowBy;
};

// The gallery registry is the same 20-byte CTypedPtrArray layout, but its
// element storage is consumed as a pointer-to-pointer by the live
// CGallery destructor.  Keep that source-facing pointer type explicit while
// preserving the verified MFC layout.
struct C1GalleryPtrArrayLike {
    void *cobject_vptr;
    void **m_pData;
    int m_nSize;
    int m_nMaxSize;
    int m_nGrowBy;
};

// Same verified CTypedPtrArray layout as C1PtrArrayLike, with the element
// type made explicit where live consumers prove it.  The generic wrapper's
// void* m_pData is layout-correct but cannot reproduce the original
// Creature* array access in C++ without an emitted cast (the binary's
// registry consumers load Creature* directly).
struct C1CreaturePtrArrayLike {
    void *cobject_vptr;
    Creature **m_pData;
    int m_nSize;
    int m_nMaxSize;
    int m_nGrowBy;
};

// Same verified CTypedPtrArray layout, with the element type recovered
// from the non-scenery registry's consumers.  This is a distinct wrapper
// because the generated source dereferences m_pData as Object**, and a
// void* there would lose the source-level type without changing the
// binary layout.
struct C1ObjectPtrArrayLike {
    void *cobject_vptr;
    Object **m_pData;
    int m_nSize;
    int m_nMaxSize;
    int m_nGrowBy;
};

// Real symbol shape for the 7 registry globals' own initializer, e.g.
// `g_gallery_registry.cobject_vptr = CTypedPtrArray<CPtrArray,CGallery*>
// ::vftable;` -- a per-instantiation static member Ghidra's own call
// sites reference by this exact real MFC template name/member
// (afxtempl.h), confirmed real via 7 live call sites across
// _free_functions.cpp. IMPORTANT: real MFC's OWN `CTypedPtrArray<
// BASE_CLASS, TYPE>` template (afxtempl.h) already declares this exact
// name with this exact 2-parameter shape, and gets pulled in
// transitively by several of this project's own headers that need
// other real MFC facilities (afxocc.h/afxcontrolbars.h -> afxtempl.h)
// -- confirmed real via a bulk-compile run (C2953 "class template has
// already been defined") the FIRST time this was tried using the real
// name directly. Real MFC's CTypedPtrArray adds no member named
// `vftable` (or any static data member at all) -- it's a thin
// non-virtual wrapper adding no new v-methods over BASE_CLASS, so
// `CTypedPtrArray<Base,T>::vftable` was never really resolving against
// afxtempl.h's own class even in the original build; Ghidra's own
// decompiler is naming the DERIVED class's compiler-synthesized vtable
// object using the same dotted-template notation it uses for a real
// member, which happens to collide syntactically with the real MFC
// template of the same name. Given real MFC now owns that name, this
// project's own compile-enabling stand-in uses its own distinct name
// instead (`C1TypedPtrArrayVtableStub`) -- the 7 real call sites this
// affects were rewritten to reference it.
template <typename Base, typename T>
struct C1TypedPtrArrayVtableStub {
    static void *vftable;
};
template <typename Base, typename T>
void *C1TypedPtrArrayVtableStub<Base, T>::vftable = nullptr;
// Ghidra's own "executable code at this address" pseudo-type, used
// throughout the decompiled tree in the standard vtable-dispatch idiom
// `(**(code **)(vtablePtrExpr + offset))(args)`. MUST be a real
// FUNCTION type (`void code();`), not a scalar `void` -- confirmed via
// a live compile: `typedef void code;` makes `code**` a `void**`,
// whose OUTER dereference in that idiom tries to deref a `void*`,
// illegal in real C++ (confirmed real and widespread via a bulk-
// compile run, C2100 "you cannot dereference an operand of type
// 'code'", 34 files). A real function type's pointer CAN be
// dereferenced twice (`**fp` yields the function itself, an lvalue of
// function type, which decays back to callable) -- that's the whole
// idiom's actual real-C semantics, verified compiling clean here.
// The decompiler erases the target signature at these raw callback sites;
// several verified sites pass different argument counts through the same
// slot. Keep the function type callable, but use the erased-parameter form.
typedef void code(...);
// Same real vtable-dispatch idiom as `code` above, for the handful of
// real call sites (5, confirmed via grep across the whole decompiled
// tree) whose result is USED -- cast to a real pointer type
// immediately after the call (`(CWnd *)(**(code **)(...))()`-shaped).
// `code` itself returns void, so that outer pointer cast has nothing
// real to convert (confirmed fatal via a bulk-compile run, C2440
// "cannot convert from 'void' to 'CWnd *'") -- `codeptr` is the same
// idiom's function type but returning `void *`, which DOES implicitly
// convert to any real pointer type, matching every one of these call
// sites' own real usage.
typedef void *codeptr();
// Same idiom again, for the real, standard MSVC "scalar deleting
// destructor" vtable-slot shape (`(**(code **)(vptr + 4))(flags);`,
// taking one int "delete this too?" flag) -- confirmed real and
// widespread via a bulk-compile run (C2197 "too many arguments for
// call" once `code`'s own signature became a real, strict zero-arg
// `void()`; 28 call sites across 8 files all pass exactly one `1`
// literal argument, matching this real, well-known MFC/MSVC ABI
// idiom exactly, not a guess).
typedef void codeint(int);

// MSVC scalar deleting-destructor vtable entry: ECX receiver plus the
// one-word destruction flag. The Object slot is a live four-byte address;
// this typedef supplies only its verified callable ABI shape.
struct Object;
typedef void (__thiscall *ObjectVirtualDeletingDestructorFn)(Object *, unsigned int);

// Typed views for the two CRT callbacks that Creature keeps in locals.
// Ghidra exports them as `code *`; the underlying imports are the ordinary
// __cdecl CRT functions, and these views preserve their x86 call shape while
// allowing the recovered indirect calls to compile.
    typedef errno_t (__cdecl *C1StrcpySFn)(char *, size_t, const char *);
typedef unsigned char *(__cdecl *C1MbsIncFn)(const unsigned char *);

template <typename C1Rect>
static __forceinline BOOL SetRectEmpty(C1Rect *rect) {
    return ::SetRectEmpty((LPRECT)rect);
}

// Real address of a real, ordinary (non-virtual, non-static) MFC member
// function, for the handful of real call sites where Ghidra's decompile
// shows the address being loaded into a register/local as a plain value
// rather than through a direct-call idiom -- confirmed real via live
// call-site inspection: every one of these sits in the same real MSVC
// incremental-link fixup table shape as an ordinary free-function address
// reference (CloseHandle, strcpy_s, ...) that was already resolved
// to a real address, just for a real MEMBER function instead of a free
// one. Standard C++ forbids implicitly converting `R (C::*)(A...)` to a
// plain code pointer (multiple/virtual inheritance needs more than one
// word), but under the real MSVC ABI for a non-virtual member of a
// single, non-virtual-inheritance class (true for every real class used
// here -- CArchive, CString, CStatusBar, CPtrArray, COleDispatchDriver),
// the pointer-to-member IS represented as exactly that one real code
// address, recoverable via this real reinterpretation. This yields the
// REAL symbol's real address (a real relocation against the real MFC
// import), not a fabricated value.
template <typename R, typename C, typename... A>
inline void *pmf_addr(R (C::*pmf)(A...)) {
    union { R (C::*p)(A...); void *addr; } u;
    u.p = pmf;
    return u.addr;
}
template <typename R, typename C, typename... A>
inline void *pmf_addr(R (C::*pmf)(A...) const) {
    union { R (C::*p)(A...) const; void *addr; } u;
    u.p = pmf;
    return u.addr;
}
template <typename R, typename C, typename... A>
inline void *pmf_addr(R (C::*pmf)(A..., ...)) {
    union { R (C::*p)(A..., ...); void *addr; } u;
    u.p = pmf;
    return u.addr;
}

// Domain enum-like scalar Ghidra assigned only to a METHOD RETURN TYPE,
// never to any struct field, so it has no independent field-derived size
// evidence of its own. Confirmed via decompile (Object::GetSoundAudibilityState, returned in
// EAX, compared against small integer constants) that a 4-byte unsigned
// scalar is a safe placeholder, matching this project's established
// C1*-prefixed enum-like typedef convention -- not yet re-derived from
// real enum-value evidence, so treat this one typedef as lower-
// confidence than the field-derived ones above it.
typedef unsigned int C1SoundAudibilityState;

// Same class of gap as C1SoundAudibilityState above: names Ghidra/an
// earlier rename pass gave to a method's return type or by-value
// PARAMETER type, never backed by a field anywhere so this generator's
// field-driven typedef machinery never saw them. Confirmed 4-byte via
// Object.cpp's own comment on ObjectBoundsModeParam ("passed as a 32-bit
// ... stack argument; only its low byte is stored") -- real x86 cdecl/
// thiscall stack-argument promotion of a sub-int enum, same reasoning
// applies to ObjectEventId (also stack-passed, e.g.
// Object::DispatchScriptEvent's ObjectEventId parameter).
typedef unsigned int ObjectBoundsModeParam;
typedef unsigned int ObjectEventId;

// Same gap, found continuing the same sweep across CGenome.cpp/
// SimpleObject.cpp (2026-08-30). C1GenomeGeneFamily has real 1-byte
// evidence (CGenome::CountMatchingGenes/FindNextMatchingGene both cast
// it directly from a single dereferenced byte -- `(C1GenomeGeneFamily)
// pbVar3[4]`, `(C1GenomeGeneFamily)(pbVar3->identity).family_selector`,
// consistent with the genome stream's documented ten-byte packed gene
// header). C1GenomeGeneCount has real 4-byte evidence too (returned via
// a plain `int local_4`). The rest are PARAMETER-only types with no
// direct memory-read evidence found yet -- 4-byte stack-argument-
// promoted default, same reasoning as ObjectBoundsModeParam.
typedef unsigned char C1GenomeGeneFamily;
// Live CGenome::FindNextMatchingGene evidence: this is the byte cursor
// used for the gene's subtype selector, distinct from the promoted
// C1GenomeGeneSubtype method parameter.  Keep the recovered storage width
// explicit; it is not a guessed C++ enum underlying type.
typedef unsigned char C1GenomeGeneSubtypeCode;
// The packed gene header's applicability/selection byte, read directly by
// CGenome::FindNextMatchingGene before the cursor advances to its payload.
typedef unsigned char C1GenomeGeneFlags;
typedef unsigned char C1GenomeGeneGeneration;
typedef unsigned char C1GenomeGeneSequenceNumber;
typedef unsigned int C1GenomeGeneCount;
typedef unsigned int C1GenomeGeneSubtype;
typedef unsigned int C1GenomeGeneSubtypeModulus;
typedef unsigned int C1GeneStageFilterMode;
// NOTE: BubblePlacementMode was almost added here too, but turned out to
// already have a REAL, correctly-sized (1-byte) field-derived typedef
// declared locally in Bubble.hpp -- it's a genuine struct field there,
// not a method-signature-only gap like the others above. A second,
// wrong-size (4-byte) copy here would be a straight C2371 redefinition
// error against that one. Lesson applied: check struct fields first
// before assuming "method-only" for a name only seen so far in a method
// signature.

// CBrainLocusResolver::ResolveGenomeLocus's own parameter -- compared
// against enum-shaped constants (RECEPTOR/...) but no direct memory-read
// evidence found for its real size; 4-byte stack-argument-promoted
// default, same reasoning as the others above. Checked first (per the
// BubblePlacementMode lesson just above) that it has no existing field-
// derived typedef anywhere -- confirmed none.
typedef unsigned int C1BiochemistryLocusKind;
// SetViewportNavigationMode disassembly writes 0 for disabled and 1 for
// manual navigation; the recovered type itself is a 4-byte scalar.
static const unsigned int VIEWPORT_NAVIGATION_DISABLED = 0u;
static const unsigned int VIEWPORT_NAVIGATION_MANUAL = 1u;
static const unsigned int VIEWPORT_NAVIGATION_FOLLOW_SELECTED_CREATURE = 2u;

// The world-update latch is a packed scalar state in the original data;
// keep the source-level names without inventing a C++ enum until the
// underlying storage and all values are independently confirmed in Ghidra.
typedef int WorldUpdateTimerState;
static const int WORLD_UPDATE_TIMER_PAUSED = 0;
static const int WORLD_UPDATE_TIMER_RUNNING = 1;

// C1 neural-lobe packed domains. These values are established by the live
// lobe switch tables and normalization paths. The corresponding storage
// remains the exact-width byte typedef generated from the class fields;
// these promoted constants preserve the recovered source names without
// inventing ABI-unsafe C++ enums.
static const unsigned int PERCEPTION = 0u;
static const unsigned int DRIVE = 1u;
static const unsigned int STIMULUS_SOURCE = 2u;
static const unsigned int VERB = 3u;
static const unsigned int NOUN = 4u;
static const unsigned int GENERAL_SENSORY = 5u;
static const unsigned int DECISION = 6u;
static const unsigned int ATTENTION = 7u;
static const unsigned int CONCEPT = 8u;
static const unsigned int NOT_COPIED_TO_PERCEPTION = 0u;
static const unsigned int COPY_TO_PERCEPTION = 1u;
static const unsigned int COPY_TO_PERCEPTION_MUTUALLY_EXCLUSIVE = 2u;
static const unsigned int UNIFORM = 0u;
static const unsigned int TWO_SAMPLE_AVERAGE = 1u;
static const unsigned int TWO_SAMPLE_ABSOLUTE_DIFFERENCE = 2u;
static const unsigned int TWO_SAMPLE_COMPLEMENT_DIFFERENCE = 3u;
static const unsigned int ATTACH_LOOSE_DENDRITES = 1u;
static const unsigned int MIGRATE_CONNECTIONS = 2u;
static const unsigned int WINNER_TAKE_ALL_ENABLED = 1u;
static const unsigned int STOP_IF_ZERO = 22u;
static const unsigned int SATURATING_ADD = 23u;
static const unsigned int SATURATING_SUBTRACT = 24u;
static const unsigned int Q8_MULTIPLY = 25u;
static const unsigned int SATURATING_INCREMENT = 26u;
static const unsigned int SATURATING_DECREMENT = 27u;
static const unsigned int NO_OP_1C = 28u;
static const unsigned int NO_OP_1D = 29u;
static const unsigned int END = 30u;

// Ghidra's SBORROW4 helper is the signed-overflow predicate for a 32-bit
// subtraction. Compute the subtraction in unsigned arithmetic so the
// helper itself has no C++ signed-overflow undefined behavior.
static __forceinline bool C1SignedBorrow4(int left, int right) {
    unsigned int result = (unsigned int)left - (unsigned int)right;
    return (((unsigned int)(left ^ right) &
             (unsigned int)(left ^ (int)result)) & 0x80000000u) != 0;
}
#define SBORROW4(left,right) C1SignedBorrow4((int)(left),(int)(right))

// Live switch-table evidence for CBrain::FormatActivityReport is a dense
// 0..4 dispatch, and the genome stream terminator is the little-endian
// four-byte literal "gend" (0x646e6567).
// C1 genome life-stage zero is the newborn/baby stage; all live uses compare
// the one-byte C1GenomeLifeStage field against this zero value.
static const unsigned int STAGE_0 = 0u;
static const unsigned int STAGE_1 = 1u;
static const unsigned int STAGE_6 = 6u;
static const unsigned int TERMINAL_STAGE_7 = 7u;
// FindNextMatchingGene's live switch subtracts 0, 1, and 1 from the
// promoted stage-filter argument: 0 compares against genome_life_stage,
// 1 ignores stage, and 2 tests the gene byte against STAGE_0.
static const unsigned int MATCH_STAGE_ZERO = 2u;
static const unsigned int GENE_STREAM_END = 0x646e6567u;
static const unsigned int GENE = 0x656e6567u;
static const unsigned int C1_GENOME_SEX_APPLIES_TO_MALE = 0x8u;
static const unsigned int C1_GENOME_SEX_APPLIES_TO_FEMALE = 0x10u;
// Names used by the recovered FindNextMatchingGene source; the live
// tests are the same exact 0x08/0x10 flag bits above.
static const unsigned int MALE_APPLICABLE = C1_GENOME_SEX_APPLIES_TO_MALE;
static const unsigned int FEMALE_APPLICABLE = C1_GENOME_SEX_APPLIES_TO_FEMALE;
static const unsigned int MALE = 1u;
static const unsigned int FEMALE = 2u;
// Exact C1CreatureConstructionSex value from Creature::Creature @
// 0x0040d6e6: zero enters the random-sex selection path. The matching
// four-byte documentation enum is /Creatures1/C1CreatureConstructionSexValues.
static const unsigned int RANDOM = 0u;
// Exact C1ClassifierFamily value used by SimpleObject's default classifier:
// MOV dword [this+0x04], 0x02000000 at SimpleObject @ 0x00426d6a.
// The matching one-byte Ghidra documentation enum is
// /Creatures1/C1ClassifierFamilyValues.
static const unsigned int FIRING_STRENGTH = 0u;
static const unsigned int ACTIVATION = 1u;
static const unsigned int MAX_CURRENT_WEIGHT = 2u;
static const unsigned int AVERAGE_TARGET_WEIGHT = 3u;
static const unsigned int AVERAGE_DENDRITE_STATE = 4u;

// Macro.cpp's own decompiled comment settles this one explicitly: "Keep
// C1CAOSToken a 4-byte scalar, not an invented 5-byte struct" -- real
// evidence, not a default guess.
typedef unsigned int C1CAOSToken;
// Macro::ParseRValue uses this recovered local only as a four-byte scalar;
// there is no field or enum evidence for a distinct record type.
typedef unsigned int C1CAOSRValueToken;

// Same method-signature-only gap, found continuing the sweep across
// SoundManager.cpp/CBrain.cpp -- no field evidence found for any of
// these (checked first, per the BubblePlacementMode lesson), 4-byte
// stack-argument-promoted default.
typedef unsigned int C1SoundCacheCapacityResult;
typedef unsigned int C1SoundResult;
typedef unsigned int C1BrainActivityReportMode;

// Same gap, continuing the sweep across Creature.cpp/SFCView.cpp.
typedef unsigned int C1CreatureAction;
typedef unsigned int C1CreatureConstructionSex;
typedef unsigned int MfcScrollBarCommand;

// `operator_new`/`operator_delete` are Ghidra's own literal names for
// this codebase's calls into the REAL global `::operator new`/
// `::operator delete` (mfc140.dll's real export, statically resolved at
// this call site's address but printed as an ordinary free-function
// call since Ghidra didn't recognize the mangled operator-new/delete
// symbol). Confirmed real, widespread scope: 39 files call
// operator_new(size), 23 call operator_delete(ptr) -- every one of this
// codebase's `ClassName::CreateObject` factory methods needs it (manual
// allocation, since there's no real `new ClassName()` in the decompiled
// form). Declared here as ordinary free functions purely so call sites
// compile; MFC140.DLL.cpp's OWN "definitions" of these two names are NOT
// used or trusted -- they're a separate, broken RE artifact (Ghidra
// couldn't recover mfc140.dll's real indirect jump table and decompiled
// each as infinitely calling itself, "WARNING: Could not recover
// jumptable ... Treating indirect jump as call") on a DIFFERENT bucket
// file representing a separate DLL boundary, not part of Creatures.exe's
// own link unit -- a real, separate RE gap, out of scope here.
void *operator_new(unsigned int size);
void operator_delete(void *ptr);

// The decompiler sometimes carries the verified operator-new import in its
// erased `code *` temporary. Keep the call ABI typed at that source boundary;
// this inlines to the same indirect x86 call under /O2.
static inline void *C1CallOperatorNew(code *function, unsigned int size)
{
    return ((void *(*)(unsigned int))function)(size);
}
// Array-new/delete siblings (`operator new[]`/`operator delete[]`,
// matching the pattern found in MFC140.DLL) -- called but never
// themselves defined here; needed by CGallery's real call site.
void *operator_new__(unsigned int size);
void operator_delete__(void *ptr);

#include <new>     // placement new for recovered embedded MFC subobjects
#include <cstdio>  // for FILE
#include <cmath>   // pow() for the recovered SSE2 CRT math-helper call
#include <xmmintrin.h>  // scalar SSE intrinsics proven at CVolumeDialog's call site
#include <emmintrin.h>  // cvtps2pd lowering proven at the same call site
#include <immintrin.h>  // AVX2 record reversal recovered at ReverseVisibleSpriteRecordRange
#include <istream>  // for std::basic_istream/basic_streambuf -- real MSVC-CRT-internal
                    // decompiled functions (_free_functions.cpp) reference these directly.
#include <fstream>  // for real std::ofstream -- see REAL_MFC_BASE_CLASS's own
// MsvcBasicOfstreamCharOpaque entry doc comment.
#include <sstream>  // basic_istringstream/basic_stringbuf in SFCApp and CRT helpers
#include <locale>   // public facet/codecvt types used by the recovered STL boundary
// MSVC's private locale facet cleanup node is not declared by the public
// headers, but the target calls its destructor and msvcprt exports that
// decorated symbol.  This is an external type declaration, not a replacement
// implementation or storage definition.
namespace std {
    struct _Fac_node {
        ~_Fac_node();
    };
}
    using std::basic_streambuf;
    using std::basic_ostream;
    using std::basic_istream;
    using std::basic_ios;
using std::ios_base;
using std::char_traits;
using facet = std::locale::facet;

#ifndef LOCK
#define LOCK() ((void)0)
#define UNLOCK() ((void)0)
#endif

// The first 16 bytes of C1MsvcString are one MSVC string-storage union.
// The byte view is used by normal string operations; the four-dword view
// is the exact overlapping view emitted by Ghidra for SFCApp's directory
// table initialization.
#pragma pack(push, 1)
struct C1MsvcStringStorage {
    union {
        unsigned char bytes[16];
        struct {
            unsigned int _0_4_;
            unsigned int _4_4_;
            unsigned int _8_4_;
            unsigned int _12_4_;
        };
    };
    operator unsigned char *() { return bytes; }
    operator const unsigned char *() const { return bytes; }
    // The exporter emits several ABI-preserving casts from the overlapping
    // string-storage union to typed pointers.  Keep the byte view above for
    // normal indexing/arithmetic, and provide the equivalent raw void view
    // for those explicit recovered pointer casts.
    operator void *() { return bytes; }
    operator const void *() const { return bytes; }
    template <typename T> operator T *() { return reinterpret_cast<T *>(bytes); }
    template <typename T> operator const T *() const {
        return reinterpret_cast<const T *>(bytes);
    }
    unsigned char *operator+(int offset) { return bytes + offset; }
    const unsigned char *operator+(int offset) const { return bytes + offset; }
    unsigned char *operator+(unsigned int offset) { return bytes + offset; }
    const unsigned char *operator+(unsigned int offset) const { return bytes + offset; }
    unsigned char &operator[](int index) { return bytes[index]; }
    const unsigned char &operator[](int index) const { return bytes[index]; }
    unsigned char &operator[](unsigned int index) { return bytes[index]; }
    const unsigned char &operator[](unsigned int index) const { return bytes[index]; }
};
#pragma pack(pop)
static_assert(sizeof(C1MsvcStringStorage) == 16, "C1MsvcStringStorage size mismatch");

// C1MsvcString is defined here because globals.hpp is included from this
// prelude before the per-bucket headers. Its 24-byte layout is independently
// established by the live string helpers and is shared by the classifier
// name RB-tree below; C1MsvcString.hpp is emitted as an include-only shell.
#pragma pack(push, 1)
struct C1MsvcString {
    C1MsvcStringStorage short_buffer_or_heap_pointer;
    unsigned int length;
    unsigned int capacity;
};

// MSVC basic_string's small-string representation stores bytes inline and
// stores a heap data pointer in the first word once capacity exceeds 0xf.
// This accessor is used only where Ghidra's x86 decompile lost that
// source-level data-pointer distinction.
static inline void *C1MsvcStringDataPointer(C1MsvcString *value) {
    if (value->capacity > 0xf) {
        return (void *)value->short_buffer_or_heap_pointer._0_4_;
    }
    return (void *)value->short_buffer_or_heap_pointer.bytes;
}

static inline const void *C1MsvcStringDataPointer(const C1MsvcString *value) {
    if (value->capacity > 0xf) {
        return (const void *)value->short_buffer_or_heap_pointer._0_4_;
    }
    return (const void *)value->short_buffer_or_heap_pointer.bytes;
}

struct C1StringMapNode {
    C1StringMapNode *left;
    C1StringMapNode *parent;
    C1StringMapNode *right;
    unsigned char color_red_flag;
    unsigned char sentinel_flag;
    unsigned char _flags_padding[2];
    C1MsvcString key;
    C1MsvcString value;
};

struct C1StringMapInsertPosition {
    C1StringMapNode *lower_bound_node;
    C1StringMapNode *insertion_parent;
    unsigned int insert_left_flag;
};

struct C1ClassifierNameMap {
    C1StringMapNode *sentinel_node;
    unsigned int node_count;
};
#pragma pack(pop)
static_assert(sizeof(C1MsvcString) == 24, "C1MsvcString size mismatch");
static_assert(sizeof(C1StringMapNode) == 64, "C1StringMapNode size mismatch");
static_assert(sizeof(C1StringMapInsertPosition) == 12, "C1StringMapInsertPosition size mismatch");
static_assert(sizeof(C1ClassifierNameMap) == 8, "C1ClassifierNameMap size mismatch");

// The 256-entry sprite cache records are 12-byte triples: cached file
// handle, sprite-file id, and LRU stamp. The array indexing and three-word
// initialization are direct evidence for this layout.
#pragma pack(push, 1)
struct SpriteFileCacheEntry {
    void *file_handle;
    unsigned int sprite_file_id;
    unsigned int lru_stamp;
};
#pragma pack(pop)
static_assert(sizeof(SpriteFileCacheEntry) == 12, "SpriteFileCacheEntry size mismatch");

// SFCApp is an ABI-faithful byte layout rather than a C++-visible subclass
// of MFC's CWinApp. These accessors preserve the recovered calls while
// keeping protected MFC entry points legal at the generated-source boundary.
struct C1PublicWinAppAccess : CWinApp {
    using CWinApp::LoadStdProfileSettings;
    using CWinApp::AddDocTemplate;
    static void CallOnFileOpen(CWinApp *app) {
        ((C1PublicWinAppAccess *)app)->CWinApp::OnFileOpen();
    }
    static void CallOnFileNew(CWinApp *app) {
        ((C1PublicWinAppAccess *)app)->CWinApp::OnFileNew();
    }
};

// basic_ios' constructor is protected in the pinned MSVC CRT. The recovered
// SFCApp code constructs this subobject in-place, so expose only that
// constructor through a one-purpose derived access shim.
struct C1PublicBasicIosAccess : std::basic_ios<char, std::char_traits<char>> {
    explicit C1PublicBasicIosAccess(std::basic_streambuf<char, std::char_traits<char>> *buffer)
        : std::basic_ios<char, std::char_traits<char>>(buffer) {}
};

// basic_streambuf's default constructor is protected in the pinned CRT.
// This empty derived access type exposes the same zero-field storage for the
// recovered placement construction.
struct C1PublicBasicStreambufAccess : std::basic_streambuf<char, std::char_traits<char>> {
    using std::basic_streambuf<char, std::char_traits<char>>::basic_streambuf;

    // The pinned CRT keeps basic_streambuf::~basic_streambuf protected.  The
    // recovered EH cleanup path still needs to invoke that base destructor on
    // an existing streambuf object, so expose only that operation through a
    // derived access shim; do not construct or emulate CRT streambuf state.
    static __forceinline void DestroyBase(void *self) {
        static_cast<C1PublicBasicStreambufAccess *>(self)
            ->std::basic_streambuf<char, std::char_traits<char>>::~basic_streambuf();
    }
};

// SFCApp's LoadGenome path uses one four-byte temporary as both an
// allocation address and three pigment bytes. This union preserves the
// exact x86 byte layout while allowing Ghidra's named overlay fields.
#pragma pack(push, 1)
struct C1PackedPigmentChannels {
    union {
        unsigned int value;
        struct {
            unsigned char _0_1_;
            unsigned char _1_1_;
            unsigned short _2_2_;
        };
    };
    operator unsigned int() const { return value; }
};
#pragma pack(pop)
static_assert(sizeof(C1PackedPigmentChannels) == 4, "C1PackedPigmentChannels size mismatch");

static inline void C1WinAppLoadStdProfileSettings(CWinApp *app, UINT max_mru) {
    ((C1PublicWinAppAccess *)app)->LoadStdProfileSettings(max_mru);
}
static inline void C1WinAppAddDocTemplate(CWinApp *app, CDocTemplate *doc_template) {
    ((C1PublicWinAppAccess *)app)->AddDocTemplate(doc_template);
}
static inline void C1WinAppCallOnFileOpen(CWinApp *app) {
    C1PublicWinAppAccess::CallOnFileOpen(app);
}
static inline void C1WinAppCallOnFileNew(CWinApp *app) {
    C1PublicWinAppAccess::CallOnFileNew(app);
}

// DirectSound records used at the real SoundManager call boundary.  These
// are the SDK-compatible packed layouts (DSBUFFERDESC is 0x24 bytes and
// WAVEFORMATEX is 0x12 bytes on the target).  They live in the shared
// prelude because the decompiler emits them as function-local types rather
// than as exported Ghidra namespaces.
#pragma pack(push, 1)
struct C1WaveFormatEx {
    unsigned short format_tag;
    unsigned short channel_count;
    unsigned int samples_per_second;
    unsigned int average_bytes_per_second;
    unsigned short block_align;
    unsigned short bits_per_sample;
    unsigned short cb_size;
};
struct C1DirectSoundGuidBits {
    unsigned long long _0_8_;
    unsigned long long _8_8_;
};
struct C1DSBufferDesc {
    unsigned int struct_size;
    unsigned int flags;
    unsigned int buffer_bytes;
    unsigned int reserved;
    C1DirectSoundGuidBits guid_3d_algorithm;
    C1WaveFormatEx *wave_format;
};
#pragma pack(pop)
static_assert(sizeof(C1WaveFormatEx) == 0x12, "Creatures1/WAVEFORMATEX size mismatch");
static_assert(sizeof(C1DSBufferDesc) == 0x24, "Creatures1/DSBUFFERDESC size mismatch");

struct C1DirectSoundInterface;
struct C1DirectSoundBufferInterface;
extern "C" HRESULT WINAPI DirectSoundCreate(const GUID *, C1DirectSoundInterface **, void *);
static const unsigned int C1_SOUND_OK = 0u;
typedef void (__cdecl *C1AtexitCallback)(void);

// BuildCreaturePaletteRemap writes one byte for each of the 256 palette
// entries. Later ifstream accesses are a stack-slot overlay, not part of
// this value's persistent layout.
#pragma pack(push, 1)
struct C1PaletteRgb8 {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
};
struct C1PaletteChannelRamp {
    unsigned char intensity_by_control[256];
};
struct C1PaletteDtaBuffer {
    C1PaletteRgb8 rgb8_colours[236];
};
struct C1PaletteRemapTable {
    unsigned char remapped_palette_index[256];
};
#pragma pack(pop)
static_assert(sizeof(C1PaletteRgb8) == 3, "Creatures1/C1PaletteRgb8 size mismatch");
static_assert(sizeof(C1PaletteChannelRamp) == 256, "Creatures1/C1PaletteChannelRamp size mismatch");
static_assert(sizeof(C1PaletteDtaBuffer) == 708, "Creatures1/C1PaletteDtaBuffer size mismatch");

// WriteDibRectToFile allocates a 12-byte packed row header followed by the
// pixel rows. This is an application-owned transient buffer, not a Windows
// SDK type; the three dwords and exact size are recovered from its writes and
// the `+ 1` pointer step in the live function.
struct C1DibRectFileHeader {
    unsigned int aligned_row_stride_bytes;
    unsigned int height_rows;
    unsigned int stored_row_stride_bytes;
};
static_assert(sizeof(C1DibRectFileHeader) == 0x0c, "C1DibRectFileHeader size mismatch");

// LoadPaletteDtaIntoBuffer is supplied by the address-labelled live
// Ghidra section in _free_functions.cpp.

// Ghidra's SUB41(value, index) is the byte extractor used by the recovered
// x86 p-code. Keep its exact low-level meaning at the generated-source
// boundary; it is not a domain conversion or an invented enum.
static inline unsigned char SUB41(unsigned int value, unsigned int index) {
    return (unsigned char)(value >> (index * 8));
}
static inline unsigned char SUB14(unsigned int value, unsigned int index) {
    return (unsigned char)(value >> (index * 8));
}
static inline unsigned int SUB84(unsigned long long value, unsigned int index) {
    return (unsigned int)(value >> (index * 8));
}

// SFCDoc's recovered timer helper calls the WinMM API directly. The
// project headers do not consistently pull in <mmsystem.h>; this is the
// ordinary 32-bit WINAPI declaration used by the target.
extern "C" unsigned int __stdcall timeGetTime(void);

// A block of IAT-slot/link-recipe scaffolding for a separate,
// byte-identical-reconstruction build target lived here -- imported-slot
// proxy declarations, vtable externs, and an inline-assembly pow() shim,
// none of it referenced anywhere in this source tree. Removed as dead
// weight for this clean-room build; the semantics it once served belong
// to that other build target, not to this one.
// <windows.h> (pulled in transitively via <afxwin.h> in the bulk-compile
// aggregate, included before this file) #defines CreateWindow as a
// function-like macro that expands to CreateWindowA/CreateWindowW --
// this codebase has its own genuine method named CreateWindow (CEyeView,
// confirmed via a real bulk-compile error: C4003 "not enough arguments
// for function-like macro invocation 'CreateWindowA'"), which the
// preprocessor silently mangles before the compiler ever sees a member
// declaration. #undef it so our own identifier wins; add further WinAPI
// macro/identifier collisions here if a future bulk-compile run finds
// more (same failure shape: a real member name matching a WinAPI
// function-like macro name).
#ifdef CreateWindow
#undef CreateWindow
#endif

// Reserved slot for a future signature-aware auto-detection pass -- see
// include/auto_detected_domain_types.hpp's own header comment for why
// it's currently just an empty placeholder. This generator only
// guarantees the file exists and is included everywhere, so a fresh
// checkout still compiles even before that file has real content.
#include "auto_detected_domain_types.hpp"
#include "C1GoalDirectionActionCandidateScoreTable.hpp"
#include "C1PoseAnimationData.hpp"

// Shared exact-width scalar names used by globals, queue records, and
// vtable signatures. They are emitted before globals.hpp and before any
// per-class header can provide a local alias.
typedef unsigned int C1CAOSValue;
typedef unsigned int ObjectEventId;
typedef unsigned int C1GoalDirectionWeight;

// Exact packed records needed by globals.hpp before the per-class header
// pass.  These definitions are emitted here once; the corresponding
// generated headers are intentionally shells to avoid duplicate structs.
#pragma pack(push, 1)
typedef unsigned char C1AttentionRecordSlot;
typedef unsigned char C1GoalActionLobeNeuronSlot;
typedef unsigned char C1GoalDirectionCandidateScore;
struct LearnedWordPhonemeSubstitution {
    char source_fragment[4];
    char replacement_fragment[4];
};
struct C1VocabularyWordBank {
    char *word_0;
    char *word_1;
    char *word_2;
    char *word_3;
    char *word_4;
};
struct C1AmbientEnvironmentRecord {
    sbyte wind_delta;
    byte reserved_1;
    byte reserved_2;
    sbyte temperature_delta_raw;
    byte reserved_4;
};
struct C1AmbientLightProfile {
    byte light_level;
    byte gap_000001_000003[3];
};
#pragma pack(pop)

// Field-derived domain scalar typedefs individually confirmed
// needed here (see gen_prelude()'s own comment for why not the
// full set).
typedef unsigned char C1ActionTargetRequirement;

// The pipe server's recovered aggregate layouts. These records are
// shared by the global declaration and the free-function definitions. The
// live code uses the byte buffer at state+0x10; its node allocation is 0x14
// bytes with the holder key at +0x10.
struct C1ByteBuffer {
    unsigned int begin;
    unsigned int end;
    unsigned int capacity_end;
};
struct C1PipePendingCommandNode {
    C1PipePendingCommandNode *left;
    C1PipePendingCommandNode *parent;
    C1PipePendingCommandNode *right;
    unsigned char color_red_flag;
    unsigned char sentinel_flag;
    unsigned char _flags_padding[2];
    CMacroHolder *holder_handle;
};
// Three-word record returned by the recovered RB-tree lower-bound helper.
// The fields are written/read at offsets 0, 4, and 8 in the live function;
// this is application-owned iterator state, not an STL implementation type.
struct C1PipePendingCommandInsertPosition {
    C1PipePendingCommandNode *candidate_node;
    unsigned int insert_left;
    C1PipePendingCommandNode *parent_node;
};
// Source-level spelling of the MSVC RB-tree iterator increment that Ghidra
// emitted as a private STL template call. The node links and sentinel are
// application-owned and their 0x14-byte layout is recovered above; keeping
// this traversal here prevents a compiler-internal STL name from crossing
// the export boundary.
static inline C1PipePendingCommandNode *C1PipePendingCommandTree_Next(
    C1PipePendingCommandNode *node, C1PipePendingCommandNode *sentinel)
{
    if (node->right != sentinel) {
        node = node->right;
        while (node->left != sentinel) {
            node = node->left;
        }
        return node;
    }
    C1PipePendingCommandNode *parent = node->parent;
    while (parent != sentinel && node == parent->right) {
        node = parent;
        parent = parent->parent;
    }
    return parent;
}
struct C1PipePendingCommandTree {
    C1PipePendingCommandNode *head;
    int node_count;
};
struct PipeServerSharedState {
    int is_running;
    HANDLE stop_event;
    HANDLE command_done_event;
    HANDLE worker_thread_handle;
    C1ByteBuffer receive_buffer;
    C1PipePendingCommandTree pending_command_tree;
};

// Exact live-Ghidra layouts for the sprite-file cache's hash index.  The
// index is a 32-byte module-global state object; its nodes are 16-byte
// doubly-linked records and its bucket table is an array of 8-byte pairs.
// These types are reached from function locals/global use rather than a
// namespace-owned class, so keep them here with the other shared records.
#pragma pack(push, 1)
struct SpriteFileCacheIndexNode {
    SpriteFileCacheIndexNode *next;
    SpriteFileCacheIndexNode *previous;
    unsigned int sprite_file_id;
    unsigned int cache_slot_index;
};
struct SpriteFileCacheIndexBucket {
    SpriteFileCacheIndexNode *first;
    SpriteFileCacheIndexNode *last;
};
struct SpriteFileCacheIndexInsertResult {
    SpriteFileCacheIndexNode *node;
    unsigned int inserted;
};
struct SpriteFileCacheIndexLookupResult {
    SpriteFileCacheIndexNode *insertion_predecessor;
    SpriteFileCacheIndexNode *matched_node;
};
struct SpriteFileCacheIndex {
    float max_load_factor;
    SpriteFileCacheIndexNode *sentinel;
    unsigned int size;
    SpriteFileCacheIndexBucket *buckets;
    SpriteFileCacheIndexBucket *buckets_end;
    SpriteFileCacheIndexBucket *buckets_capacity;
    unsigned int bucket_mask;
    unsigned int bucket_count;
};
#pragma pack(pop)

// Exact byte-precise MFC metadata views persisted in the live Ghidra
// program. The target is a 32-bit PE, so these static records contain
// 32-bit address slots rather than host-sized C++ pointers.
#pragma pack(push, 1)
struct C1AfxMsgMap {
    unsigned int get_base_map;
    unsigned int entries;
};
struct C1AfxDispMap {
    unsigned int get_base_map;
    unsigned int entries;
    unsigned int entry_count;
    unsigned int stock_prop_mask;
};
struct C1AfxInterfaceMap {
    unsigned int get_base_map;
    unsigned int entries;
};
struct C1AfxInterfaceMapEntry {
    unsigned int iid_ptr;
    unsigned int vtable_offset;
};
#pragma pack(pop)

// Module-global variables (g_*) and free functions referenced across
// more than one translation unit, plus the per-class MFC runtime/message-map
// symbol declarations every DECLARE_DYNCREATE/DECLARE_MESSAGE_MAP class needs.
#include "globals.hpp"
#include "free_functions.hpp"
#include "runtime_class_data_symbols.hpp"
#include "message_map_symbols.hpp"
