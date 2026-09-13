#pragma once
// Free (non-member) function prototypes shared across translation units.

// Forward declarations for our own struct types used below.
struct BodyPart;
struct BodyPartAttachmentData;
struct C1AfxDispMap;
struct C1AfxInterfaceMap;
struct C1AfxMsgMap;
struct C1ByteBuffer;
struct C1GenomeGeneCountReport;
struct C1MsvcString;
struct C1PaletteChannelRamp;
struct C1PaletteDtaBuffer;
struct C1PaletteRemapTable;
struct C1PipePendingCommandInsertPosition;
struct C1PipePendingCommandNode;
struct C1PipePendingCommandTree;
struct C1RegistryKeyPair;
struct C1StringMapInsertPosition;
struct C1StringMapNode;
struct C1VisibleSpriteSortRecord;
struct CArchiveRuntimeState;
struct CDataExchange;
struct CDialog;
struct CGdiObject;
struct CGenome;
struct CImage;
struct CInstinct;
struct CMacroHolder;
struct CMenu;
struct CObArray;
struct COleDispatchDriver;
struct CPtrArray;
struct CRuntimeClass;
struct CScore;
struct CSliderCtrl;
struct CStatic;
struct CSystemInfoWnd;
struct CWorldRenderer;
struct Creature;
struct Macro;
struct Object;
struct PipeServerSharedState;
struct RenderableObjectSet;
struct RenderableObjectSetLookupResult;
struct RenderableObjectSetNode;
struct SFCDoc;
struct ScriptClassifier;
struct SpriteFileCacheIndexInsertResult;
struct SpriteFileCacheIndexLookupResult;
struct SpriteFileCacheIndexNode;
struct WorldRect;
struct _Fac_node;

// Local re-declarations of scalar typedefs already real elsewhere in the
// tree, needed here because this header is included earlier -- see the
// comment above this loop in main().
typedef unsigned int C1BodyPartIndex;
typedef unsigned int C1Bool32;
typedef unsigned char C1CreatureSex;
typedef unsigned int C1GenomeFilenameId;
typedef unsigned char C1GenomeLifeStage;
typedef unsigned int C1ResourceDirectoryIndex;
typedef unsigned int C1ResourceFilenameStem4;

void __fastcall DeserializeScriptsForClassifier(CArchiveRuntimeState *archive); // explicit live signature @ 0x0041a8e0
basic_istream<char,std::char_traits<char>> * ConstructAndOpenBasicIfstream(void *self,char *file_path); // explicit live signature @ 0x0043dd40
basic_istream<char,std::char_traits<char>> * __fastcall MsvcBasicIstream_Getline(basic_istream<char,std::char_traits<char>> *input_stream,C1MsvcString *output_string,byte delimiter); // explicit live signature @ 0x0043ff10
basic_ostream<char,std::char_traits<char>> * __fastcall StdBasicOstream_WritePaddedBuffer(basic_ostream<char,std::char_traits<char>> *stream,char *buffer,uint buffer_length); // explicit live signature @ 0x00439e40
void InitializeSoundSystem(void); // explicit live signature @ 0x0043f5b0

undefined4 * ATL_IDocument_Destructor(void *self,byte param_1); // from _free_functions.cpp @ 0x00436020
CFile * __fastcall AcquireCachedSpriteFile(uint sprite_file_id); // from _free_functions.cpp @ 0x00441b90
void __fastcall AdaptiveInplaceMergeVisibleSpriteRecordsByPlane(C1VisibleSpriteSortRecord *first,C1VisibleSpriteSortRecord *middle,
          C1VisibleSpriteSortRecord *last,int left_count,int right_count,
          C1VisibleSpriteSortRecord *scratch_buffer,int scratch_capacity,int total_count); // from _free_functions.cpp @ 0x00414640
void __fastcall AdaptiveStableSortVisibleSpriteRecordsByPlane(C1VisibleSpriteSortRecord *begin,C1VisibleSpriteSortRecord *end,int record_count,
          C1VisibleSpriteSortRecord *scratch_buffer); // from _free_functions.cpp @ 0x00414260
LRESULT __fastcall AddKitToolbarBitmapFromProgId(LPCSTR kit_prog_id); // from _free_functions.cpp @ 0x00443ff0
void AddObjectToEventBarDisplayList(Object *object); // from _free_functions.cpp @ 0x00416540
void AdvanceBacteriumServicePhase(void); // from _free_functions.cpp @ 0x00433310
void AfxClassInit_Blackboard(void); // from _free_functions.cpp @ 0x00401340
void AfxClassInit_Body(void); // from _free_functions.cpp @ 0x004011c0
void AfxClassInit_BodyPart(void); // from _free_functions.cpp @ 0x004011b0
void AfxClassInit_Bubble(void); // from _free_functions.cpp @ 0x00401300
void AfxClassInit_CBacterium(void); // from _free_functions.cpp @ 0x00401000
void AfxClassInit_CBiochemistry(void); // from _free_functions.cpp @ 0x00401360
void AfxClassInit_CBrain(void); // from _free_functions.cpp @ 0x00401010
void AfxClassInit_CCreatureRegister(void); // from _free_functions.cpp @ 0x00401020
void AfxClassInit_CEventBar(void); // from _free_functions.cpp @ 0x004011e0
void AfxClassInit_CFavouritePlace(void); // from _free_functions.cpp @ 0x004011f0
void AfxClassInit_CGallery(void); // from _free_functions.cpp @ 0x00401670
void AfxClassInit_CGenome(void); // from _free_functions.cpp @ 0x00401200
void AfxClassInit_CImage(void); // from _free_functions.cpp @ 0x00401680
void AfxClassInit_CInstinct(void); // from _free_functions.cpp @ 0x00401030
void AfxClassInit_COwner(void); // from _free_functions.cpp @ 0x00401350
void AfxClassInit_CScore(void); // from _free_functions.cpp @ 0x00401370
void AfxClassInit_CallButton(void); // from _free_functions.cpp @ 0x004012f0
void AfxClassInit_CompoundObject(void); // from _free_functions.cpp @ 0x00401310
void AfxClassInit_Creature(void); // from _free_functions.cpp @ 0x00401040
void AfxClassInit_Entity(void); // from _free_functions.cpp @ 0x004011a0
void AfxClassInit_Lift(void); // from _free_functions.cpp @ 0x00401330
void AfxClassInit_Limb(void); // from _free_functions.cpp @ 0x004011d0
void AfxClassInit_Macro(void); // from _free_functions.cpp @ 0x00401210
void AfxClassInit_MapData(void); // from _free_functions.cpp @ 0x00401260
void AfxClassInit_MyToolBar(void); // from _free_functions.cpp @ 0x00401250
void AfxClassInit_Object(void); // from _free_functions.cpp @ 0x004012b0
void AfxClassInit_PointerTool(void); // from _free_functions.cpp @ 0x004012e0
void AfxClassInit_Scenery(void); // from _free_functions.cpp @ 0x004012c0
void AfxClassInit_SimpleObject(void); // from _free_functions.cpp @ 0x004012d0
void AfxClassInit_Skeleton(void); // from _free_functions.cpp @ 0x004015d0
void AfxClassInit_Vehicle(void); // from _free_functions.cpp @ 0x00401320
void AfxClassInit_Voice(void); // from _free_functions.cpp @ 0x004017e0
int __stdcall AfxWinMain_ImportThunk(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow); // from _free_functions.cpp @ 0x0044bc6a
CImage * AllocateAndConstructCImage(void); // from _free_functions.cpp @ 0x00441b20
CScore * AllocateAndConstructCScore(void); // from _free_functions.cpp @ 0x0042f6b0
CInstinct * AllocateCInstinct(void); // from _free_functions.cpp @ 0x00406f10
void ArchiveScratchBuffer_Allocate(void *self,size_t param_1); // from _free_functions.cpp @ 0x00406c80
void __fastcall ArchiveScratchBuffer_Free(undefined4 *param_1); // from _free_functions.cpp @ 0x00406cb0
void __fastcall ArchiveScratchBuffer_FreeIfHeap(undefined4 *param_1); // from _free_functions.cpp @ 0x00406cc0
void __fastcall ArmWorldUpdateTimer(SFCDoc *document); // from _free_functions.cpp @ 0x004337e0
void AtExit_CloseCrashReportFile(void); // from _free_functions.cpp @ 0x0044ee30
void AtExit_DeleteGamePaletteHandle(void); // from _free_functions.cpp @ 0x0044ef00
void AtExit_DestroyCreatureRegistry(void); // from _free_functions.cpp @ 0x0044eec0
void AtExit_DestroyCreatureSelectionArray(void); // from _free_functions.cpp @ 0x0044eed0
void AtExit_DestroyEntityRegistry(void); // from _free_functions.cpp @ 0x0044ee50
void AtExit_DestroyGalleryRegistry(void); // from _free_functions.cpp @ 0x0044ef90
void AtExit_DestroyNonSceneryObjectRegistry(void); // from _free_functions.cpp @ 0x0044ee60
void AtExit_DestroyOleObjectFactory(void); // from _free_functions.cpp @ 0x0044ee40
void AtExit_DestroySceneryRegistry(void); // from _free_functions.cpp @ 0x0044eee0
void AtExit_DestroyWorldObjectRegistry(void); // from _free_functions.cpp @ 0x0044eef0
void AtExit_DestroyWorldSavePathStorage(void); // from _free_functions.cpp @ 0x0044ef30
void __fastcall BlitZeroTransparent8BitPixels(byte *src_pixels,int src_x,int src_y,int src_stride,byte *dst_pixels,int dst_x,int dst_y,
          int dst_stride,uint copy_width,int copy_height); // from _free_functions.cpp @ 0x00401f40
void __fastcall BroadcastEmbeddedControlState(undefined4 param_1,byte state_code); // from _free_functions.cpp @ 0x004449e0
void BuildCreaturePaletteRemap(C1PaletteRemapTable *remap_table,byte red_control,byte green_control,
               byte blue_control,byte hue_rotation_control,byte colour_swap_control); // from _free_functions.cpp @ 0x00413d20
void BuildCurrentCreatureGenomeGeneCounts(C1GenomeGeneCountReport *out_report); // from _free_functions.cpp @ 0x0040e350
void BuildPaletteChannelRamp(C1PaletteChannelRamp *output_ramp,int unused_arg_1,int low_intensity,
               int middle_intensity,int high_intensity,int unused_arg_5); // from _free_functions.cpp @ 0x00413c50
C1AfxDispMap * CApplication_GetMfcMapRecordA(void); // from _free_functions.cpp @ 0x004303f0
C1AfxInterfaceMap * CApplication_GetMfcMapRecordB(void); // from _free_functions.cpp @ 0x00430400
undefined ** CApplication_GetThisMessageMap(void); // from _free_functions.cpp @ 0x004303e0
int __fastcall CDocumentAdapter_AddRef(int param_1); // from _free_functions.cpp @ 0x004361a0
undefined4 * CDocumentAdapter_Destructor(void *self,byte param_1); // from _free_functions.cpp @ 0x00435fe0
undefined4 __fastcall CDocumentAdapter_ForwardSlot0xB4(int param_1); // from _free_functions.cpp @ 0x00436070
undefined4 __fastcall CDocumentAdapter_ForwardSlot0xB8(int param_1); // from _free_functions.cpp @ 0x004360e0
void __fastcall CDocumentAdapter_ForwardSlot0xBC(int param_1); // from _free_functions.cpp @ 0x004360c0
void __fastcall CDocumentAdapter_ForwardSlot0xC0(int param_1); // from _free_functions.cpp @ 0x00436100
undefined4 __fastcall CDocumentAdapter_ForwardSlot0xC4(int param_1); // from _free_functions.cpp @ 0x00436110
void __fastcall CDocumentAdapter_ForwardSlot0xC8(int param_1); // from _free_functions.cpp @ 0x004360d0
undefined4 __fastcall CDocumentAdapter_ForwardSlot0xCC(int param_1); // from _free_functions.cpp @ 0x00436130
void __fastcall CDocumentAdapter_ForwardSlot0xD0(int param_1); // from _free_functions.cpp @ 0x00436150
undefined4 __fastcall CDocumentAdapter_ForwardSlot0xD4(int param_1); // from _free_functions.cpp @ 0x00436170
undefined4 __fastcall CDocumentAdapter_GetOwner(int param_1); // from _free_functions.cpp @ 0x00436190
undefined4 __fastcall CDocumentAdapter_Release(undefined4 *param_1); // from _free_functions.cpp @ 0x00436050
int __fastcall CDocument_GetDocumentAdapter(int param_1); // from _free_functions.cpp @ 0x004361b0
void __fastcall CFavouritePlace_NameMemberDestructor(int param_1); // from _free_functions.cpp @ 0x004179e0
void __fastcall CImage_ScalarDeletingDestructor(undefined4 *param_1); // from _free_functions.cpp @ 0x00442840
C1AfxInterfaceMap * CSfcOLE_GetAutomationDispatchMap(void); // from _free_functions.cpp @ 0x0042f960
C1AfxMsgMap * CSfcOLE_GetAutomationMapRecordA(void); // from _free_functions.cpp @ 0x0042f940
C1AfxDispMap * CSfcOLE_GetAutomationMapRecordB(void); // from _free_functions.cpp @ 0x0042f950
CRuntimeClass * CWinBMP_GetRuntimeClassMetadata(void); // from _free_functions.cpp @ 0x00445890
undefined * Catch_004081b8(void); // from _free_functions.cpp @ 0x004081b8
void Catch_004081ce(void); // from _free_functions.cpp @ 0x004081ce
undefined4 Catch_00414ad2(void); // from _free_functions.cpp @ 0x00414ad2
undefined * Catch_0042fe03(void); // from _free_functions.cpp @ 0x0042fe03
undefined4 Catch_0042ff71(void); // from _free_functions.cpp @ 0x0042ff71
undefined1 * Catch_0043007e(void); // from _free_functions.cpp @ 0x0043007e
undefined4 Catch_00431076(void); // from _free_functions.cpp @ 0x00431076
undefined1 * Catch_00431163(void); // from _free_functions.cpp @ 0x00431163
undefined * Catch_004355b6(void); // from _free_functions.cpp @ 0x004355b6
undefined * Catch_00436225(void); // from _free_functions.cpp @ 0x00436225
undefined * Catch_004489fa(void); // from _free_functions.cpp @ 0x004489fa
undefined * Catch_00448a86(void); // from _free_functions.cpp @ 0x00448a86
undefined1 * Catch_All_0040f963(void); // from _free_functions.cpp @ 0x0040f963
undefined * Catch_All_00410175(void); // from _free_functions.cpp @ 0x00410175
undefined * Catch_All_00410d8b(void); // from _free_functions.cpp @ 0x00410d8b
undefined4 Catch_All_004193c1(void); // from _free_functions.cpp @ 0x004193c1
undefined4 Catch_All_004194cd(void); // from _free_functions.cpp @ 0x004194cd
void Catch_All_0041a0be(void); // from _free_functions.cpp @ 0x0041a0be
undefined4 Catch_All_00420cdb(void); // from _free_functions.cpp @ 0x00420cdb
undefined * Catch_All_00439f5f(void); // from _free_functions.cpp @ 0x00439f5f
undefined * Catch_All_0044000a(void); // from _free_functions.cpp @ 0x0044000a
undefined * Catch_All_00447185(void); // from _free_functions.cpp @ 0x00447185
undefined * Catch_All_00447869(void); // from _free_functions.cpp @ 0x00447869
undefined * Catch_All_00447bab(void); // from _free_functions.cpp @ 0x00447bab
undefined * Catch_All_00447f13(void); // from _free_functions.cpp @ 0x00447f13
undefined * Catch_All_00448b12(void); // from _free_functions.cpp @ 0x00448b12
undefined * Catch_All_0044ae6c(void); // from _free_functions.cpp @ 0x0044ae6c
void __fastcall CenterViewportOnSelectedCreatureIfInPanRegion(CWorldRenderer *world_renderer); // from _free_functions.cpp @ 0x00413360
uint CheckExecutableSecurityDirectoryPresent(void); // from _free_functions.cpp @ 0x0044b5d3
void __fastcall ClearAndResetSelectedComboBoxItem(int control_owner); // from _free_functions.cpp @ 0x004109f0
void __fastcall ClearCaosConsoleEditText(int param_1); // from _free_functions.cpp @ 0x0040f360
void ClearDelayedObjectEventQueue(void); // from _free_functions.cpp @ 0x00401290
void ClearImmediateObjectEventQueue(void); // from _free_functions.cpp @ 0x00401270
void __fastcall ClearSpriteFileCache(void); // from _free_functions.cpp @ 0x00441ee0
void __fastcall CloseRegistryKeyPair(undefined4 *param_1); // from _free_functions.cpp @ 0x0042f450
void CloseResourceRegistryKeys(void); // from _free_functions.cpp @ 0x0044ef10
undefined ** CommunityEditionVersionDialog_GetThisMessageMap(void); // from _free_functions.cpp @ 0x00444ee0
undefined4 __fastcall CommunityEditionVersionDialog_OnInitDialog(CDialog *dialog); // from _free_functions.cpp @ 0x00444ef0
void __fastcall ConfigureWorldUpdateTimerInterval(uint interval_or_adjustment_code); // from _free_functions.cpp @ 0x00417f80
void ConstructSfcAppGlobalInstance(void); // from _free_functions.cpp @ 0x00401650
void ConvertWorldRectToViewportRect(void *self,RECT *viewport_rect_out,WorldRect *world_rect); // from _free_functions.cpp @ 0x00412ea0
void CopyFilesMatchingPattern(char *source_directory,char *destination_directory,char *filename_pattern); // from _free_functions.cpp @ 0x00435680
char * CopyPrimaryResourceDirectoryPath(C1ResourceDirectoryIndex directory_index,char *path_out); // from _free_functions.cpp @ 0x00412280
void CreateAndSelectGeneratedFemaleCreature(void); // from _free_functions.cpp @ 0x00434830
void CreateAndSelectGeneratedMaleCreature(void); // from _free_functions.cpp @ 0x004347a0
BodyPart * CreateBodyPart(void); // from _free_functions.cpp @ 0x00414e60
undefined4 * CreateVoice(void); // from _free_functions.cpp @ 0x00444fc0
void CrtStartupExitFragment_0044b0c4(void); // from _free_functions.cpp @ 0x0044b0c4
void __cdecl DebugLog(uint category_mask,char *format, ...); // from _free_functions.cpp @ 0x00418000
void __fastcall DeleteHeapAllocation(void *param_1); // from _free_functions.cpp @ 0x00401f30
void __fastcall DeleteObjectAndPurgeRuntimeReferences(Object *object); // from _free_functions.cpp @ 0x0040e9f0
void DestroyAtlBaseModuleAtExit(void); // from _free_functions.cpp @ 0x0044f04b
void __fastcall DestroyBasicIfstream(int *ifstream_object); // from _free_functions.cpp @ 0x0043c610
void DestroyClassifierNameMap(void); // from _free_functions.cpp @ 0x0044ef40
void DestroyDdeItem_BrainActivity(void); // from _free_functions.cpp @ 0x0044ed90
void DestroyDdeItem_BrainWiring(void); // from _free_functions.cpp @ 0x0044edb0
void DestroyDdeItem_Macro(void); // from _free_functions.cpp @ 0x0044ed70
void DestroyDdeItem_SysInfo(void); // from _free_functions.cpp @ 0x0044edd0
void DestroyMsvcFacetNodeListAtExit(void); // from _free_functions.cpp @ 0x0044f055
void DestroyRenderableObjectSet(void); // from _free_functions.cpp @ 0x0044ee70
void DestroySfcAppGlobalInstance(void); // from _free_functions.cpp @ 0x0044ef70
void DestroySpriteFileCacheIndex(void); // from _free_functions.cpp @ 0x0044efa0
void __fastcall Dialog_CopyWindowTextControlToCString(int param_1); // from _free_functions.cpp @ 0x004108e0
void Dialog_DDX_CheckAndTextFields(void *self,CDataExchange *param_1); // from _free_functions.cpp @ 0x004439c0
void Dialog_DDX_Control40A(void *self,CDataExchange *param_1); // from _free_functions.cpp @ 0x00444c80
void Dialog_DDX_TextField3F2(void *self,CDataExchange *param_1); // from _free_functions.cpp @ 0x00444ec0
void __fastcall Dialog_HideWindow(CWnd *param_1); // from _free_functions.cpp @ 0x00444e60
undefined4 __fastcall Dialog_OnInit_DisableControl3EAIfFieldB0Null(CDialog *param_1); // from _free_functions.cpp @ 0x00443b40
void DispatchMainFrameCommand(undefined4 caller_context,WPARAM command_id); // from _free_functions.cpp @ 0x00420eb0
void EnsureDebugConsoleDialog(void); // from _free_functions.cpp @ 0x00434ac0
void EuthaniseSelectedCreature(void); // from _free_functions.cpp @ 0x00434920
C1Bool32 __cdecl ExecuteEmbeddedKitTool(uint tool_index); // from _free_functions.cpp @ 0x004444e0
C1Bool32 __fastcall ExecuteScriptForClassifier(Object *script_owner,Object *from_object,ScriptClassifier classifier_event,
          C1Bool32 force_restart); // from _free_functions.cpp @ 0x00419e00
void __fastcall FillU32Range(void *param_1,void *param_2,void *param_3); // from _free_functions.cpp @ 0x0042d560
void __fastcall FillU32RangeOptimized(uint *destination_first,uint *destination_last,uint *source_word_ptr); // from _free_functions.cpp @ 0x0042d580
CMenu * FindCameraSubmenu(void); // from _free_functions.cpp @ 0x004350e0
void FindNearestMapRoomBoundsAtPoint(LONG world_x,LONG world_y,RECT *out_bounds); // from _free_functions.cpp @ 0x00422d70
RenderableObjectSetLookupResult * FindRenderableObjectSetNode(RenderableObjectSetLookupResult *out_result,Object **object_key_ptr,uint hash); // from _free_functions.cpp @ 0x0042d180
void __fastcall FlushFuneralKitDocumentStateWords(COleDispatchDriver *document); // from _free_functions.cpp @ 0x00435c10
void __fastcall FollowSelectedCreatureViewport(CWorldRenderer *renderer); // from _free_functions.cpp @ 0x00413420
void ForceAgeSelectedCreatureOneStage(void); // from _free_functions.cpp @ 0x00435490
undefined * __fastcall FormatObjectDebugLabel(Object *object); // from _free_functions.cpp @ 0x00418210
void FreeU32ArrayAllocation(void *param_1,int param_2); // from _free_functions.cpp @ 0x0042d5e0
void __fastcall GenerateMagicProfilerReport(CWnd *report_window); // from _free_functions.cpp @ 0x00437ef0
C1GenomeFilenameId __fastcall GenerateOffspringGenomeFile(C1GenomeFilenameId maternal_source_filename,C1GenomeFilenameId paternal_source_filename); // from _free_functions.cpp @ 0x004187f0
void __fastcall GenerateUniqueGenomeFilename(CGenome *genome); // from _free_functions.cpp @ 0x00418e90
uint __fastcall GetAttentionRecordIndex(Object *object); // from _free_functions.cpp @ 0x00426430
__time64_t * __fastcall GetCurrentTime64(__time64_t *out_time); // from _free_functions.cpp @ 0x00406cd0
undefined ** GetRuntimeClass(void); // from _free_functions.cpp @ 0x0042d9b0
void GrowRenderableObjectSetForInsert(void); // from _free_functions.cpp @ 0x0042d210
void __fastcall HandleMacroScriptExecutionException(Macro *macro); // from _free_functions.cpp @ 0x00420cf0
void InfectSelectedCreatureWithRandomBacterium(void); // from _free_functions.cpp @ 0x004348c0
void InitializeAtlBaseModuleAndRegisterCleanup(void); // from _free_functions.cpp @ 0x00401851
void InitializeClassifierNameMap(void); // from _free_functions.cpp @ 0x00401620
void InitializeCreatureRegistry(void); // from _free_functions.cpp @ 0x00401500
void InitializeCreatureSelectionArray(void); // from _free_functions.cpp @ 0x00401530
uint __fastcall InitializeCriticalSectionExAsHRESULT(LPCRITICAL_SECTION critical_section); // from _free_functions.cpp @ 0x004435f0
void InitializeDdeService(void); // from _free_functions.cpp @ 0x00401090
void InitializeEntityRegistry(void); // from _free_functions.cpp @ 0x004013b0
void InitializeGalleryRegistry(void); // from _free_functions.cpp @ 0x00401690
C1Bool32 InitializeGamePalette(void); // from _free_functions.cpp @ 0x00430b10
void InitializeNonSceneryObjectRegistry(void); // from _free_functions.cpp @ 0x004013e0
void InitializePipeServerPendingCommandTree(void); // from _free_functions.cpp @ 0x004017f0
void InitializePrimaryResourceRegistryKey(void); // from _free_functions.cpp @ 0x004015e0
void InitializeRenderableObjectSet(void); // from _free_functions.cpp @ 0x00401410
void InitializeSceneryRegistry(void); // from _free_functions.cpp @ 0x00401560
void InitializeScriptDefinitionTable(void); // from _free_functions.cpp @ 0x00401220
void InitializeSpriteFileCacheEntries(void); // from _free_functions.cpp @ 0x004017b0
void InitializeSpriteFileCacheIndex(void); // from _free_functions.cpp @ 0x004016c0
void InitializeWorldObjectRegistry(void); // from _free_functions.cpp @ 0x00401590
void InitializeWorldSavePathStorage(void); // from _free_functions.cpp @ 0x00401600
void __fastcall InplaceMergeVisibleSpriteRecordsByPlane(C1VisibleSpriteSortRecord *first,C1VisibleSpriteSortRecord *middle,
          C1VisibleSpriteSortRecord *last,int left_count,int right_count,
          C1VisibleSpriteSortRecord *scratch_buffer,int scratch_capacity,int total_count); // from _free_functions.cpp @ 0x004144a0
RenderableObjectSetNode * InsertRenderableObjectSetNode(uint hash,RenderableObjectSetNode *before,RenderableObjectSetNode *node); // from _free_functions.cpp @ 0x0042d2a0
undefined4 * __fastcall InsertionSortVisibleSpriteRecordsByPlane(C1VisibleSpriteSortRecord *begin,C1VisibleSpriteSortRecord *end); // from _free_functions.cpp @ 0x004140d0
void __fastcall InstallScriptTextForClassifier(ScriptClassifier classifier_event,char *script_text,bool confirm_replace); // from _free_functions.cpp @ 0x0041a440
C1Bool32 __fastcall IntersectWrappedWorldRects(WorldRect *in_out_first_rect,WorldRect *second_rect); // from _free_functions.cpp @ 0x00417bb0
void InvokeVTableSlot0x0(undefined4 *param_1); // from _free_functions.cpp @ 0x00431f80
void InvokeVTableSlot0x0_at4335d0(undefined4 *param_1); // from _free_functions.cpp @ 0x004335d0
void InvokeVTableSlot0x0_at435460(undefined4 *param_1); // from _free_functions.cpp @ 0x00435460
void InvokeVTableSlot0x0_at435be0(undefined4 *param_1); // from _free_functions.cpp @ 0x00435be0
void InvokeVTableSlot0x4_at434f10(int *param_1); // from _free_functions.cpp @ 0x00434f10
void __fastcall InvokeVTableSlot0x68(int *param_1); // from _free_functions.cpp @ 0x0040f1b0
bool IsRunningUnderWine(void); // from _free_functions.cpp @ 0x00443dd0
BOOL __fastcall IsSelectedCreatureWithinViewportSafeArea(CWorldRenderer *renderer); // from _free_functions.cpp @ 0x00413090
BodyPartAttachmentData * __fastcall LoadBodyPartAttachmentData(C1BodyPartIndex body_part_index,uint genus,C1CreatureSex sex,C1GenomeLifeStage life_stage
          ,uint variant); // from _free_functions.cpp @ 0x0043c6d0
void __fastcall LoadCaosScriptFromFile(CWnd *script_editor_window); // from _free_functions.cpp @ 0x0040f380
void LoadCharsetData(void); // from _free_functions.cpp @ 0x00442eb0
C1Bool32 __fastcall LoadPaletteDtaIntoBuffer(C1PaletteDtaBuffer *out_palette); // from _free_functions.cpp @ 0x00413b50
tagSIZE * MeasureCStringTextExtent(void *self,tagSIZE *out_extent,undefined4 *text_string); // from _free_functions.cpp @ 0x004171d0
void __fastcall MergeVisibleSpriteRecordRunsToBuffer(C1VisibleSpriteSortRecord *source_begin,C1VisibleSpriteSortRecord *source_end,
          C1VisibleSpriteSortRecord *destination_begin,int run_length,int record_count); // from _free_functions.cpp @ 0x00414570
void MfcAfxTryCleanupWithSEH(void); // from _free_functions.cpp @ 0x0040ee40
undefined4 * MfcCFileException_DeletingDestructor(void *self,uint param_1); // from _free_functions.cpp @ 0x00445af0
void __fastcall MfcCFileException_Destructor(undefined4 *param_1); // from _free_functions.cpp @ 0x00445ae0
void __fastcall MfcCGdiObject_Destructor(CGdiObject *self); // from _free_functions.cpp @ 0x00414a60
CObArray * MfcCObArray_DeletingDestructor(void *self,uint param_1); // from _free_functions.cpp @ 0x004301b0
undefined4 * MfcCOleException_DeletingDestructor(void *self,uint param_1); // from _free_functions.cpp @ 0x00444bf0
void __fastcall MfcCOleException_Destructor(undefined4 *param_1); // from _free_functions.cpp @ 0x00444be0
CPtrArray * MfcCPtrArray_DeletingDestructor(void *self,uint param_1); // from _free_functions.cpp @ 0x00430170
CSliderCtrl * MfcCSliderCtrl_DeletingDestructor(void *self,uint param_1); // from _free_functions.cpp @ 0x00444e80
CStatic * MfcCStatic_DeletingDestructor(void *self,uint param_1); // from _free_functions.cpp @ 0x00449f30
void __fastcall MfcCmdUI_DisableWindow(void *command_ui); // from _free_functions.cpp @ 0x0040fd80
void __fastcall MfcCmdUI_EnableWindow(void *command_ui); // from _free_functions.cpp @ 0x0040fd90
void MfcDialog_DoDataExchange_Control3F5(void *self,CDataExchange *param_1); // from _free_functions.cpp @ 0x0042f550
void __fastcall MfcDialog_PaintTextField(CWnd *param_1); // from _free_functions.cpp @ 0x004362d0
undefined4 __fastcall MfcFrameWnd_GetInternalFieldD4(int param_1); // from _free_functions.cpp @ 0x004227f0
uint __fastcall MfcGenerated_GetFlagBit0x01(int param_1); // from _free_functions.cpp @ 0x00440370
uint __fastcall MfcGenerated_GetFlagBit0x02(int param_1); // from _free_functions.cpp @ 0x00440360
uint __fastcall MfcGenerated_GetFlagBit0x04(int param_1); // from _free_functions.cpp @ 0x00440350
uint __fastcall MfcGenerated_GetFlagBit0x08(int param_1); // from _free_functions.cpp @ 0x00440340
uint __fastcall MfcGenerated_GetFlagBit0x10(int param_1); // from _free_functions.cpp @ 0x00440330
uint __fastcall MfcGenerated_GetFlagBit0x20(int param_1); // from _free_functions.cpp @ 0x00440320
undefined ** MfcGenerated_GetMapRecord0x455934(void); // from _free_functions.cpp @ 0x004173c0
undefined ** MfcGenerated_GetMapRecord0x45A994(void); // from _free_functions.cpp @ 0x00436290
undefined4 MfcGenerated_GetMetadata0x45B0E4(void); // from _free_functions.cpp @ 0x0043df20
undefined ** MfcGenerated_GetThisMessageMap0x457F7C(void); // from _free_functions.cpp @ 0x0042f570
undefined4 MfcGenerated_GetTimeout5000(void); // from _free_functions.cpp @ 0x00440310
void * MfcOperatorNewWithSEH(uint param_1,void *allocation_cookie); // from _free_functions.cpp @ 0x0044ae31
void MfcOperatorNewWithSEH_Epilogue(void); // from _free_functions.cpp @ 0x0044ae74
undefined4 MfcSetModuleCodePageState(int param_1,undefined4 param_2); // from _free_functions.cpp @ 0x0044bc42
void MfcThrowStatusException(long status_code); // from _free_functions.cpp @ 0x00406d00
void * __fastcall MsvcAlignedAllocateFromSize(void *param_1,uint *param_2); // from _free_functions.cpp @ 0x00406680
void MsvcAlignedDeallocate(void *param_1,uint param_2); // from _free_functions.cpp @ 0x004066d0
void __fastcall MsvcAlignedDeallocateWithCapacity(undefined4 param_1,void *param_2,int param_3); // from _free_functions.cpp @ 0x00406640
undefined4 * MsvcBadAlloc_CopyConstruct(void *self,int param_1); // from _free_functions.cpp @ 0x00402150
undefined4 * __fastcall MsvcBadArrayNewLength_Construct(undefined4 *param_1); // from _free_functions.cpp @ 0x004020b0
undefined4 * MsvcBadArrayNewLength_CopyConstruct(void *self,int param_1); // from _free_functions.cpp @ 0x00402110
uint __fastcall MsvcBasicFilebuf_ConsumeCharacter(int param_1); // from _free_functions.cpp @ 0x00411460
bool __fastcall MsvcBasicFilebuf_FlushShiftState(int *param_1); // from _free_functions.cpp @ 0x00411ee0
uint MsvcBasicFilebuf_PutbackCharacter(void *self,uint param_1); // from _free_functions.cpp @ 0x004117f0
void MsvcBasicFilebuf_SeekOffset(void *self,undefined4 *param_1,int param_2,int param_3,int param_4); // from _free_functions.cpp @ 0x00411120
void MsvcBasicFilebuf_SeekPosition(void *self,int *param_1,uint param_2,int param_3,uint param_4,int param_5,int param_6,
          int param_7); // from _free_functions.cpp @ 0x00411050
__int64 MsvcBasicFilebuf_WriteBlock(void *self,char *param_1,uint param_2,int param_3); // from _free_functions.cpp @ 0x00411230
int MsvcBasicFilebuf_WriteCharacter(void *self,int param_1); // from _free_functions.cpp @ 0x004118a0
int * MsvcBasicIfstream_OffsetDeletingDestructor(void *self,byte param_1); // from _free_functions.cpp @ 0x0043de66
void __fastcall MsvcBasicIstringstream_Destructor(int *param_1); // from _free_functions.cpp @ 0x0043f270
void MsvcBasicIstringstream_OffsetDeletingDestructor(void *self,byte param_1); // from _free_functions.cpp @ 0x00440303
void MsvcBasicOfstream_OffsetDeletingDestructor(void *self,byte param_1); // from _free_functions.cpp @ 0x0041221c
undefined4 * MsvcBasicString_AppendBytes(void *self,const void *source_bytes,void *param_2); // from _free_functions.cpp @ 0x004064e0
void MsvcBasicString_AppendChar(void *self,char appended_char); // from _free_functions.cpp @ 0x00411dc0
void MsvcBasicString_AssignBytes(void *self,char *source_bytes,uint byte_count); // from _free_functions.cpp @ 0x00439fe0
int * MsvcBasicString_AssignBytesReuseStorage(void *self,char *source_bytes,uint byte_count); // from _free_functions.cpp @ 0x00440080
undefined4 * MsvcBasicString_AssignCString(void *self,char *param_1); // from _free_functions.cpp @ 0x00439a60
undefined4 * MsvcBasicString_CopyConstruct(void *self,C1MsvcString *source); // from _free_functions.cpp @ 0x00439aa0
void __fastcall MsvcBasicString_Destructor(int param_1); // from _free_functions.cpp @ 0x00446bb0
bool __fastcall MsvcBasicString_EqualsCString(byte *string_object,byte *cstring); // from _free_functions.cpp @ 0x00448d30
int MsvcBasicString_FindByteBeforeLimit(void *self,undefined4 search_byte,uint search_limit); // from _free_functions.cpp @ 0x00448f30
void MsvcBasicString_GetEndPointer(void *self,int *end_pointer_out); // from _free_functions.cpp @ 0x00448f70
uint MsvcBasicString_Less(C1MsvcString *lhs,C1MsvcString *rhs); // from _free_functions.cpp @ 0x0043a8a0
undefined4 * MsvcBasicString_MoveConstruct(void *self,C1MsvcString *param_1); // from _free_functions.cpp @ 0x00439a20
void MsvcBasicString_Reserve(void *self,uint requested_capacity); // from _free_functions.cpp @ 0x004398d0
void __fastcall MsvcBasicString_Reset(C1MsvcString *string_state); // from _free_functions.cpp @ 0x00406420
int * MsvcBasicString_ResetThenMoveAssign(void *self,int *source_string); // from _free_functions.cpp @ 0x00448f90
undefined4 * MsvcBasicString_Substring(void *self,undefined4 *substring_result,uint start_offset,uint requested_length); // from _free_functions.cpp @ 0x00448ed0
void __fastcall MsvcBasicStringbuf_DestroyStorage(int param_1); // from _free_functions.cpp @ 0x00440170
void MsvcByteVector_AllocateStorage(void *self,uint allocation_size_bytes); // from _free_functions.cpp @ 0x00449580
void __fastcall MsvcByteVector_Destroy(C1ByteBuffer *self); // from _free_functions.cpp @ 0x00448ec0
void __fastcall MsvcByteVector_DestroyDuplicate(C1ByteBuffer *self); // from _free_functions.cpp @ 0x004491b0
undefined4 * MsvcByteVector_InitFromRange(void *self,void *param_1,int param_2); // from _free_functions.cpp @ 0x00448dd0
undefined1 * MsvcByteVector_InsertByte(void *self,undefined1 *byte_source); // from _free_functions.cpp @ 0x00449010
void MsvcByteVector_ReplaceStorage(void *self,void *new_storage_begin,int length_bytes,int capacity_bytes); // from _free_functions.cpp @ 0x004496e0
void MsvcCaptureContextAndInvokeFatalHandler(void); // from _free_functions.cpp @ 0x0044a920
C1MsvcString * MsvcClassifierNameMap_GetOrInsert(C1MsvcString *key); // from _free_functions.cpp @ 0x004397a0
int * MsvcClassifierRegistryTree_FindLowerBound(int *param_1,int *param_2,int *param_3); // from _free_functions.cpp @ 0x0043a3b0
void MsvcCompilerFragment_004bdbd(void); // from _free_functions.cpp @ 0x0044bdbd
void MsvcCompilerFragment_004be4d(void); // from _free_functions.cpp @ 0x0044be4d
void MsvcCompilerFragment_004c242(void); // from _free_functions.cpp @ 0x0044c242
void MsvcCompilerFragment_004c6fe(void); // from _free_functions.cpp @ 0x0044c6fe
void MsvcCompilerFragment_004c81e(void); // from _free_functions.cpp @ 0x0044c81e
void MsvcCompilerFragment_004c93a(void); // from _free_functions.cpp @ 0x0044c93a
void MsvcCompilerFragment_004cbce(void); // from _free_functions.cpp @ 0x0044cbce
void MsvcCompilerFragment_004d096(void); // from _free_functions.cpp @ 0x0044d096
void MsvcCompilerFragment_004d19e(void); // from _free_functions.cpp @ 0x0044d19e
void MsvcCompilerFragment_004d2ed(void); // from _free_functions.cpp @ 0x0044d2ed
void MsvcCompilerFragment_004d5c9(void); // from _free_functions.cpp @ 0x0044d5c9
void MsvcCompilerFragment_004d629(void); // from _free_functions.cpp @ 0x0044d629
void MsvcCompilerFragment_004d6be(void); // from _free_functions.cpp @ 0x0044d6be
void MsvcCompilerFragment_004de24(void); // from _free_functions.cpp @ 0x0044de24
void __fastcall MsvcConcurrencyObject_Destructor(int param_1); // from _free_functions.cpp @ 0x0044b859
int * MsvcCppEhFunclet_0040024(void); // from _free_functions.cpp @ 0x00440024
void MsvcCppEhFunclet_004002d(void); // from _free_functions.cpp @ 0x0044002d
undefined4 MsvcCppEhFunclet_0040097(void); // from _free_functions.cpp @ 0x00430097
HDDEDATA MsvcCppEhFunclet_0040191(void); // from _free_functions.cpp @ 0x00410191
HDDEDATA MsvcCppEhFunclet_004019a(void); // from _free_functions.cpp @ 0x0041019a
int * MsvcCppEhFunclet_0040da5(void); // from _free_functions.cpp @ 0x00410da5
int * MsvcCppEhFunclet_0040daf(void); // from _free_functions.cpp @ 0x00410daf
undefined4 MsvcCppEhFunclet_004116f(void); // from _free_functions.cpp @ 0x0043116f
void MsvcCppEhFunclet_004560c(void); // from _free_functions.cpp @ 0x0043560c
void MsvcCppEhFunclet_0045618(void); // from _free_functions.cpp @ 0x00435618
int MsvcCppEhFunclet_0046234(void); // from _free_functions.cpp @ 0x00436234
int MsvcCppEhFunclet_0046237(void); // from _free_functions.cpp @ 0x00436237
undefined4 MsvcCppEhFunclet_0047c02(void); // from _free_functions.cpp @ 0x00447c02
undefined4 MsvcCppEhFunclet_0047f6a(void); // from _free_functions.cpp @ 0x00447f6a
void MsvcCppEhFunclet_00481c8(void); // from _free_functions.cpp @ 0x004081c8
int * MsvcCppEhFunclet_0049f79(void); // from _free_functions.cpp @ 0x00439f79
void MsvcCppEhFunclet_0049f7f(void); // from _free_functions.cpp @ 0x00439f7f
void MsvcCppEhFunclet_004a7cc(void); // from _free_functions.cpp @ 0x0044a7cc
void MsvcCppEhFunclet_004a8db(void); // from _free_functions.cpp @ 0x0044a8db
void __cdecl MsvcCppEhFunclet_004aa28(undefined4 param_1); // from _free_functions.cpp @ 0x0044aa28
int __cdecl MsvcCppEhFunclet_004aaf8(int param_1,uint param_2); // from _free_functions.cpp @ 0x0044aaf8
undefined4 MsvcCppEhFunclet_004ae72(void); // from _free_functions.cpp @ 0x0044ae72
undefined4 MsvcCppEhFunclet_004b15f(void); // from _free_functions.cpp @ 0x0044b15f
undefined4 MsvcCppEhFunclet_004b621(int *param_1); // from _free_functions.cpp @ 0x0044b621
void MsvcCppEhFunclet_004b677(void); // from _free_functions.cpp @ 0x0044b677
uint MsvcCppEhFunclet_004b682(void); // from _free_functions.cpp @ 0x0044b682
void MsvcCppEhFunclet_004b78b(void); // from _free_functions.cpp @ 0x0044b78b
void __fastcall MsvcCppEhFunclet_004b86f(int *param_1); // from _free_functions.cpp @ 0x0044b86f
void MsvcCppEhFunclet_004b8d6(void); // from _free_functions.cpp @ 0x0044b8c6
void __cdecl MsvcCppEhFunclet_004b912(undefined4 param_1); // from _free_functions.cpp @ 0x0044b912
void MsvcCppEhFunclet_004be0d(void); // from _free_functions.cpp @ 0x0044be0d
void MsvcCppEhFunclet_004be9b(void); // from _free_functions.cpp @ 0x0044be9b
void MsvcCppEhFunclet_004bf3a(void); // from _free_functions.cpp @ 0x0044bf3a
void MsvcCppEhFunclet_004bfa1(void); // from _free_functions.cpp @ 0x0044bfa1
void MsvcCppEhFunclet_004c01e(void); // from _free_functions.cpp @ 0x0044c01e
void MsvcCppEhFunclet_004c051(void); // from _free_functions.cpp @ 0x0044c051
void MsvcCppEhFunclet_004c0dd(void); // from _free_functions.cpp @ 0x0044c0dd
void MsvcCppEhFunclet_004c11d(void); // from _free_functions.cpp @ 0x0044c11d
void MsvcCppEhFunclet_004c14d(void); // from _free_functions.cpp @ 0x0044c14d
void MsvcCppEhFunclet_004c17e(void); // from _free_functions.cpp @ 0x0044c17e
void MsvcCppEhFunclet_004c1e7(void); // from _free_functions.cpp @ 0x0044c1e7
void MsvcCppEhFunclet_004c2b3(void); // from _free_functions.cpp @ 0x0044c2b3
void MsvcCppEhFunclet_004c2e7(void); // from _free_functions.cpp @ 0x0044c2e7
void MsvcCppEhFunclet_004c324(void); // from _free_functions.cpp @ 0x0044c324
void MsvcCppEhFunclet_004c381(void); // from _free_functions.cpp @ 0x0044c381
void MsvcCppEhFunclet_004c3ad(void); // from _free_functions.cpp @ 0x0044c3ad
void MsvcCppEhFunclet_004c3fe(void); // from _free_functions.cpp @ 0x0044c3fe
void MsvcCppEhFunclet_004c42d(void); // from _free_functions.cpp @ 0x0044c42d
void MsvcCppEhFunclet_004c4b6(void); // from _free_functions.cpp @ 0x0044c4b6
void MsvcCppEhFunclet_004c4ed(void); // from _free_functions.cpp @ 0x0044c4ed
void MsvcCppEhFunclet_004c51d(void); // from _free_functions.cpp @ 0x0044c51d
void MsvcCppEhFunclet_004c54e(void); // from _free_functions.cpp @ 0x0044c54e
void MsvcCppEhFunclet_004c57d(void); // from _free_functions.cpp @ 0x0044c57d
void MsvcCppEhFunclet_004c5fe(void); // from _free_functions.cpp @ 0x0044c5fe
void MsvcCppEhFunclet_004c62e(void); // from _free_functions.cpp @ 0x0044c62e
void MsvcCppEhFunclet_004c65e(void); // from _free_functions.cpp @ 0x0044c65e
void MsvcCppEhFunclet_004c6be(void); // from _free_functions.cpp @ 0x0044c6be
void MsvcCppEhFunclet_004c747(void); // from _free_functions.cpp @ 0x0044c747
void MsvcCppEhFunclet_004c7ab(void); // from _free_functions.cpp @ 0x0044c7ab
void MsvcCppEhFunclet_004c7e7(void); // from _free_functions.cpp @ 0x0044c7e7
void MsvcCppEhFunclet_004c85e(void); // from _free_functions.cpp @ 0x0044c85e
void MsvcCppEhFunclet_004c8a9(void); // from _free_functions.cpp @ 0x0044c8a9
void MsvcCppEhFunclet_004c8e1(void); // from _free_functions.cpp @ 0x0044c8e1
void MsvcCppEhFunclet_004c98d(void); // from _free_functions.cpp @ 0x0044c98d
void MsvcCppEhFunclet_004c9d1(void); // from _free_functions.cpp @ 0x0044c9d1
void MsvcCppEhFunclet_004ca1d(void); // from _free_functions.cpp @ 0x0044ca1d
void MsvcCppEhFunclet_004ca4e(void); // from _free_functions.cpp @ 0x0044ca4e
void MsvcCppEhFunclet_004cad7(void); // from _free_functions.cpp @ 0x0044cad7
void MsvcCppEhFunclet_004cb2e(void); // from _free_functions.cpp @ 0x0044cb2e
void MsvcCppEhFunclet_004cb67(void); // from _free_functions.cpp @ 0x0044cb67
void MsvcCppEhFunclet_004cb9e(void); // from _free_functions.cpp @ 0x0044cb9e
void MsvcCppEhFunclet_004cc18(void); // from _free_functions.cpp @ 0x0044cc18
void MsvcCppEhFunclet_004ccad(void); // from _free_functions.cpp @ 0x0044ccad
void MsvcCppEhFunclet_004cd58(void); // from _free_functions.cpp @ 0x0044cd58
void MsvcCppEhFunclet_004cdb7(void); // from _free_functions.cpp @ 0x0044cdb7
void MsvcCppEhFunclet_004cdf6(void); // from _free_functions.cpp @ 0x0044cdf6
void MsvcCppEhFunclet_004ce37(void); // from _free_functions.cpp @ 0x0044ce37
void MsvcCppEhFunclet_004ceac(void); // from _free_functions.cpp @ 0x0044ceac
void MsvcCppEhFunclet_004cef5(void); // from _free_functions.cpp @ 0x0044cef5
void MsvcCppEhFunclet_004cf35(void); // from _free_functions.cpp @ 0x0044cf35
void MsvcCppEhFunclet_004cf6d(void); // from _free_functions.cpp @ 0x0044cf6d
void MsvcCppEhFunclet_004cfca(void); // from _free_functions.cpp @ 0x0044cfca
void MsvcCppEhFunclet_004d00d(void); // from _free_functions.cpp @ 0x0044d00d
void MsvcCppEhFunclet_004d045(void); // from _free_functions.cpp @ 0x0044d045
void MsvcCppEhFunclet_004d0e6(void); // from _free_functions.cpp @ 0x0044d0e6
void MsvcCppEhFunclet_004d136(void); // from _free_functions.cpp @ 0x0044d136
void MsvcCppEhFunclet_004d16d(void); // from _free_functions.cpp @ 0x0044d16d
void MsvcCppEhFunclet_004d1de(void); // from _free_functions.cpp @ 0x0044d1de
void MsvcCppEhFunclet_004d23e(void); // from _free_functions.cpp @ 0x0044d23e
void MsvcCppEhFunclet_004d28a(void); // from _free_functions.cpp @ 0x0044d28a
void MsvcCppEhFunclet_004d342(void); // from _free_functions.cpp @ 0x0044d342
void MsvcCppEhFunclet_004d36d(void); // from _free_functions.cpp @ 0x0044d36d
void MsvcCppEhFunclet_004d39d(void); // from _free_functions.cpp @ 0x0044d39d
void MsvcCppEhFunclet_004d3ce(void); // from _free_functions.cpp @ 0x0044d3ce
void MsvcCppEhFunclet_004d3fe(void); // from _free_functions.cpp @ 0x0044d3fe
void MsvcCppEhFunclet_004d439(void); // from _free_functions.cpp @ 0x0044d439
void MsvcCppEhFunclet_004d48e(void); // from _free_functions.cpp @ 0x0044d48e
void MsvcCppEhFunclet_004d4e5(void); // from _free_functions.cpp @ 0x0044d4e5
void MsvcCppEhFunclet_004d52e(void); // from _free_functions.cpp @ 0x0044d52e
void MsvcCppEhFunclet_004d56e(void); // from _free_functions.cpp @ 0x0044d56e
void MsvcCppEhFunclet_004d691(void); // from _free_functions.cpp @ 0x0044d691
void MsvcCppEhFunclet_004d746(void); // from _free_functions.cpp @ 0x0044d746
void MsvcCppEhFunclet_004d798(void); // from _free_functions.cpp @ 0x0044d798
void MsvcCppEhFunclet_004d889(void); // from _free_functions.cpp @ 0x0044d889
void MsvcCppEhFunclet_004d911(void); // from _free_functions.cpp @ 0x0044d911
void MsvcCppEhFunclet_004d955(void); // from _free_functions.cpp @ 0x0044d955
void MsvcCppEhFunclet_004d98e(void); // from _free_functions.cpp @ 0x0044d98e
void MsvcCppEhFunclet_004d9f4(void); // from _free_functions.cpp @ 0x0044d9f4
void MsvcCppEhFunclet_004da54(void); // from _free_functions.cpp @ 0x0044da54
void MsvcCppEhFunclet_004dac3(void); // from _free_functions.cpp @ 0x0044dac3
void MsvcCppEhFunclet_004db34(void); // from _free_functions.cpp @ 0x0044db34
void MsvcCppEhFunclet_004db7c(void); // from _free_functions.cpp @ 0x0044db7c
void MsvcCppEhFunclet_004dbde(void); // from _free_functions.cpp @ 0x0044dbde
void MsvcCppEhFunclet_004dc2c(void); // from _free_functions.cpp @ 0x0044dc2c
void MsvcCppEhFunclet_004dc5e(void); // from _free_functions.cpp @ 0x0044dc5e
void MsvcCppEhFunclet_004dcbc(void); // from _free_functions.cpp @ 0x0044dcbc
void MsvcCppEhFunclet_004dd06(void); // from _free_functions.cpp @ 0x0044dd06
void MsvcCppEhFunclet_004de6e(void); // from _free_functions.cpp @ 0x0044de6e
void MsvcCppEhFunclet_004de9d(void); // from _free_functions.cpp @ 0x0044de9d
void MsvcCppEhFunclet_004decd(void); // from _free_functions.cpp @ 0x0044decd
void MsvcCppEhFunclet_004df19(void); // from _free_functions.cpp @ 0x0044df19
void MsvcCppEhFunclet_004e026(void); // from _free_functions.cpp @ 0x0044e026
void MsvcCppEhFunclet_004e089(void); // from _free_functions.cpp @ 0x0044e089
void MsvcCppEhFunclet_004e0be(void); // from _free_functions.cpp @ 0x0044e0be
void MsvcCppEhFunclet_004e206(void); // from _free_functions.cpp @ 0x0044e206
void MsvcCppEhFunclet_004e255(void); // from _free_functions.cpp @ 0x0044e255
void MsvcCppEhFunclet_004e2bd(void); // from _free_functions.cpp @ 0x0044e2bd
void MsvcCppEhFunclet_004e305(void); // from _free_functions.cpp @ 0x0044e305
void MsvcCppEhFunclet_004e383(void); // from _free_functions.cpp @ 0x0044e383
void MsvcCppEhFunclet_004e3c1(void); // from _free_functions.cpp @ 0x0044e3c1
void MsvcCppEhFunclet_004e3fe(void); // from _free_functions.cpp @ 0x0044e3fe
void MsvcCppEhFunclet_004e439(void); // from _free_functions.cpp @ 0x0044e439
void MsvcCppEhFunclet_004e46d(void); // from _free_functions.cpp @ 0x0044e46d
void MsvcCppEhFunclet_004e4cb(void); // from _free_functions.cpp @ 0x0044e4cb
void MsvcCppEhFunclet_004e4fd(void); // from _free_functions.cpp @ 0x0044e4fd
void MsvcCppEhFunclet_004e552(void); // from _free_functions.cpp @ 0x0044e552
void MsvcCppEhFunclet_004e57e(void); // from _free_functions.cpp @ 0x0044e57e
void MsvcCppEhFunclet_004e5c5(void); // from _free_functions.cpp @ 0x0044e5c5
void MsvcCppEhFunclet_004e61f(void); // from _free_functions.cpp @ 0x0044e61f
void MsvcCppEhFunclet_004e6cc(void); // from _free_functions.cpp @ 0x0044e6cc
void MsvcCppEhFunclet_004e722(void); // from _free_functions.cpp @ 0x0044e722
void MsvcCppEhFunclet_004e764(void); // from _free_functions.cpp @ 0x0044e764
void MsvcCppEhFunclet_004e7a7(void); // from _free_functions.cpp @ 0x0044e7a7
void MsvcCppEhFunclet_004e7de(void); // from _free_functions.cpp @ 0x0044e7de
void MsvcCppEhFunclet_004e82f(void); // from _free_functions.cpp @ 0x0044e82f
void MsvcCppEhFunclet_004e85f(void); // from _free_functions.cpp @ 0x0044e85f
void MsvcCppEhFunclet_004e8b5(void); // from _free_functions.cpp @ 0x0044e8b5
void MsvcCppEhFunclet_004e94d(void); // from _free_functions.cpp @ 0x0044e94d
void MsvcCppEhFunclet_004ec49(void); // from _free_functions.cpp @ 0x0044ec49
void MsvcCppEhFunclet_004ed26(void); // from _free_functions.cpp @ 0x0044ed26
void MsvcCppEhFunclet_004f996(void); // from _free_functions.cpp @ 0x0040f996
undefined4 MsvcCppEhFunclet_004fe15(void); // from _free_functions.cpp @ 0x0042fe15
void MsvcCppEhHandler_004bde0(void); // from _free_functions.cpp @ 0x0044bde0
void MsvcCppEhHandler_004bed0(void); // from _free_functions.cpp @ 0x0044bed0
void MsvcCppEhHandler_004bef0(void); // from _free_functions.cpp @ 0x0044bef0
void MsvcCppEhHandler_004bfe0(void); // from _free_functions.cpp @ 0x0044bfe0
void MsvcCppEhHandler_004c080(void); // from _free_functions.cpp @ 0x0044c080
void MsvcCppEhHandler_004c1b0(void); // from _free_functions.cpp @ 0x0044c1b0
void MsvcCppEhHandler_004c350(void); // from _free_functions.cpp @ 0x0044c350
void MsvcCppEhHandler_004c460(void); // from _free_functions.cpp @ 0x0044c460
void MsvcCppEhHandler_004c480(void); // from _free_functions.cpp @ 0x0044c480
void MsvcCppEhHandler_004c5c0(void); // from _free_functions.cpp @ 0x0044c5c0
void MsvcCppEhHandler_004c690(void); // from _free_functions.cpp @ 0x0044c690
void MsvcCppEhHandler_004ca70(void); // from _free_functions.cpp @ 0x0044ca70
void MsvcCppEhHandler_004caa0(void); // from _free_functions.cpp @ 0x0044caa0
void MsvcCppEhHandler_004cb00(void); // from _free_functions.cpp @ 0x0044cb00
void MsvcCppEhHandler_004cce0(void); // from _free_functions.cpp @ 0x0044cce0
void MsvcCppEhHandler_004cd80(void); // from _free_functions.cpp @ 0x0044cd80
void MsvcCppEhHandler_004cf90(void); // from _free_functions.cpp @ 0x0044cf90
void MsvcCppEhHandler_004d210(void); // from _free_functions.cpp @ 0x0044d210
void MsvcCppEhHandler_004d710(void); // from _free_functions.cpp @ 0x0044d710
void MsvcCppEhHandler_004dbb0(void); // from _free_functions.cpp @ 0x0044dbb0
void MsvcCppEhHandler_004df50(void); // from _free_functions.cpp @ 0x0044df50
void MsvcCppEhHandler_004e290(void); // from _free_functions.cpp @ 0x0044e290
void MsvcCppEhHandler_004e330(void); // from _free_functions.cpp @ 0x0044e330
void MsvcCppEhHandler_004e680(void); // from _free_functions.cpp @ 0x0044e680
void MsvcCppEhHandler_004e880(void); // from _free_functions.cpp @ 0x0044e880
void MsvcCppEhHandler_004ed50(void); // from _free_functions.cpp @ 0x0044ed50
void MsvcCppEhOrphanFragment_004a7c6(void); // from _free_functions.cpp @ 0x0044a7c6
void MsvcCppEhOrphanFragment_004a84c(void); // from _free_functions.cpp @ 0x0044a84c
undefined4 MsvcCppEhOrphanFragment_004a89b(void); // from _free_functions.cpp @ 0x0044a89b
void MsvcCppEhOrphanFragment_004a8bf(void); // from _free_functions.cpp @ 0x0044a8bf
bool MsvcCppEhOrphanFragment_004ac94(void); // from _free_functions.cpp @ 0x0044ac94
undefined1 MsvcCppEhOrphanFragment_004aca7(void); // from _free_functions.cpp @ 0x0044aca7
void MsvcCppEhOrphanFragment_004ae89(void); // from _free_functions.cpp @ 0x0044ae89
undefined4 MsvcCppEhOrphanFragment_004af34(void); // from _free_functions.cpp @ 0x0044af34
void MsvcCppEhOrphanFragment_004b7b7(void); // from _free_functions.cpp @ 0x0044b7b7
int MsvcCrt_Snprintf(void *self,char *buffer,uint buffer_size,char *format,...); // from _free_functions.cpp @ 0x00406790
int __fastcall MsvcCrt_VsnprintfSecure(char *buffer,uint buffer_count,char *format,void *param_4,va_list arg_list); // from _free_functions.cpp @ 0x00406760
int __cdecl MsvcCrt_VsnprintfSecureBounded(char *output_buffer,uint buffer_count,uint max_count,char *format_string,...); // from _free_functions.cpp @ 0x00449740
void __fastcall MsvcEhCleanupCStringAt248(int eh_frame_base); // from _free_functions.cpp @ 0x00408950
void * MsvcException_ScalarDeletingDestructor(void *self,byte param_1); // from _free_functions.cpp @ 0x00402080
longlong MsvcFloatToInt64(void); // from _free_functions.cpp @ 0x0044bd20
void __fastcall MsvcHeapMember_Release(int param_1); // from _free_functions.cpp @ 0x0043a850
void __fastcall MsvcHeapMember_ReleaseDuplicate(int param_1); // from _free_functions.cpp @ 0x0043a870
void __fastcall MsvcHeapMember_ReleaseDuplicate2(int param_1); // from _free_functions.cpp @ 0x00443440
void __fastcall MsvcHeapMember_ReleaseDuplicate3(int param_1); // from _free_functions.cpp @ 0x00449610
void __fastcall MsvcHeapPointer_Release(undefined4 *param_1); // from _free_functions.cpp @ 0x00448e20
void MsvcInstallUnhandledExceptionFilter(void); // from _free_functions.cpp @ 0x0044b615
void MsvcInvokeRuntimeFatalHandler(void); // from _free_functions.cpp @ 0x0044aa1b
void __fastcall MsvcListStorage_Release(int *param_1); // from _free_functions.cpp @ 0x00435f80
void __fastcall MsvcListStorage_ReleaseDuplicate(int *param_1); // from _free_functions.cpp @ 0x00442fd0
undefined * MsvcLocalStdioPrintfOptions(void); // from _free_functions.cpp @ 0x004067b0
void * MsvcMemchrAvx2Wrapper(void *begin,void *end,byte search_byte); // from _free_functions.cpp @ 0x0044ba70
void __cdecl MsvcOperatorDeleteAdapter(void *param_1); // from _free_functions.cpp @ 0x0044a75e
void __cdecl MsvcOperatorDeleteWrapper(void *allocation); // from _free_functions.cpp @ 0x0044a750
void MsvcPendingCommandMap_DestroyNodeSubtree(C1PipePendingCommandTree *tree,C1PipePendingCommandNode *subtree_root); // from _free_functions.cpp @ 0x00448fd0
void __fastcall MsvcPendingCommandMap_DestroyNodesAndHeader(C1PipePendingCommandTree *tree); // from _free_functions.cpp @ 0x00448e80
C1PipePendingCommandNode * MsvcPendingCommandMap_EraseNode(void *self,C1PipePendingCommandNode *node); // from _free_functions.cpp @ 0x00449260
C1PipePendingCommandInsertPosition * MsvcPendingCommandMap_FindInsertPosition(void *self,C1PipePendingCommandInsertPosition *position,CMacroHolder **holder_key); // from _free_functions.cpp @ 0x00449210
C1PipePendingCommandNode ** MsvcPendingCommandMap_FindLowerBound(void *self,C1PipePendingCommandNode **output_node,CMacroHolder **holder_key); // from _free_functions.cpp @ 0x00448e30
void MsvcPendingCommandMap_RotateLeft(void *self,C1PipePendingCommandNode *node); // from _free_functions.cpp @ 0x00449690
void MsvcPendingCommandMap_RotateRight(void *self,C1PipePendingCommandNode **subtree_link); // from _free_functions.cpp @ 0x00449630
void MsvcReportFatalExceptionToDebugger(void); // from _free_functions.cpp @ 0x0044b489
undefined1 MsvcReturnTrueCallback(void); // from _free_functions.cpp @ 0x0044b72c
undefined4 MsvcRuntime_GetDefaultFileModeText(void); // from _free_functions.cpp @ 0x0044b71a
undefined4 MsvcRuntime_GetFlagStorage(void); // from _free_functions.cpp @ 0x0044b750
undefined4 MsvcRuntime_GetNarrowArgvMode(void); // from _free_functions.cpp @ 0x0044b479
WORD MsvcRuntime_GetWinMainShowCommand(void); // from _free_functions.cpp @ 0x0044b59e
void MsvcRuntime_InitializeFloatingPointControl(void); // from _free_functions.cpp @ 0x0044b72f
void MsvcRuntime_InitializeSListHead(void); // from _free_functions.cpp @ 0x0044b720
void MsvcRuntime_InitializeStdioAndFlags(void); // from _free_functions.cpp @ 0x0044b756
void MsvcRuntime_InitializeUnhandledExceptionFilterAndNewMode(void); // from _free_functions.cpp @ 0x0044af3c
uint __cdecl MsvcRuntime_IsPointerInExecutableExceptionDirectory(int param_1); // from _free_functions.cpp @ 0x0044ac2c
undefined4 MsvcSehCatchContinuation_00448b92(void); // from _free_functions.cpp @ 0x00448b92
void __fastcall MsvcSmallBuffer_ReleaseHeapIfLarge(undefined4 *param_1); // from _free_functions.cpp @ 0x00414240
void * __fastcall MsvcStringAllocatorAllocate(void *param_1,uint *param_2); // from _free_functions.cpp @ 0x00406480
void MsvcStringPairTreeNode_DestroySubtree(void *param_1,int *param_2); // from _free_functions.cpp @ 0x004402b0
void __fastcall MsvcStringPair_Destroy(int *param_1); // from _free_functions.cpp @ 0x0043a940
int * MsvcStringTree_InsertAndRebalance(void *self,C1StringMapNode *insertion_parent,C1Bool32 insert_left_flag,
          C1StringMapNode *new_node); // from _free_functions.cpp @ 0x0043a560
int * MsvcStringTree_LowerBound(C1StringMapInsertPosition *position_out,C1MsvcString *key); // from _free_functions.cpp @ 0x0043a460
void __cdecl MsvcTerminateAfterUnhandledException(_EXCEPTION_POINTERS *param_1); // from _free_functions.cpp @ 0x0044a8f8
void MsvcThrowBadArrayNewLength(void); // from _free_functions.cpp @ 0x004020f0
void MsvcThrowStringLengthError(void); // from _free_functions.cpp @ 0x00402190
void MsvcTreeNode_DestroySubtree(void *param_1,int *param_2); // from _free_functions.cpp @ 0x0043a420
void __fastcall MsvcTreeStorage_Release(int *param_1); // from _free_functions.cpp @ 0x004395e0
undefined4 * MsvcTypeInfo_DeletingDestructor(void *self,byte param_1); // from _free_functions.cpp @ 0x0044a72d
void __fastcall MsvcUnwindCleanup_FreeHeapBufferAtFramePlus4(int param_1); // from _free_functions.cpp @ 0x0042d1f0
void __fastcall MsvcVectorStorage_Release(int *param_1); // from _free_functions.cpp @ 0x00435f20
undefined4 * MsvcVector_ConstructFromClassifierProfilerTreeRange(undefined4 *param_1,int *param_2,int *param_3); // from _free_functions.cpp @ 0x004395f0
void NoOpVirtualMethod(void); // from _free_functions.cpp @ 0x00401ed0
void NotifyDDEScoreChanged(void); // from _free_functions.cpp @ 0x0042f740
void __fastcall NotifyEmbeddedKit9OfCreatureDeath(Creature *self); // from _free_functions.cpp @ 0x0040e2d0
void OnActivateFlashWindow(CWnd *param_1,int param_2); // from _free_functions.cpp @ 0x00410bf0
void OnBeginDragResize(CWnd *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4); // from _free_functions.cpp @ 0x00436ad0
void OnCharQueueTextInput(CWnd *param_1,char param_2); // from _free_functions.cpp @ 0x00437270
void OnExportCurrentCreature(void); // from _free_functions.cpp @ 0x00431d20
void OnImportCreature(void); // from _free_functions.cpp @ 0x00431fb0
void __fastcall OnRemovePlaceListSelectionChanged(CWnd *remove_places_dialog); // from _free_functions.cpp @ 0x0042f580
void OnSelectCreatureByMenuIndex_00(void); // from _free_functions.cpp @ 0x00433a10
void OnSelectCreatureByMenuIndex_01(void); // from _free_functions.cpp @ 0x00433a20
void OnSelectCreatureByMenuIndex_02(void); // from _free_functions.cpp @ 0x00433a30
void OnSelectCreatureByMenuIndex_03(void); // from _free_functions.cpp @ 0x00433a40
void OnSelectCreatureByMenuIndex_04(void); // from _free_functions.cpp @ 0x00433a50
void OnSelectCreatureByMenuIndex_05(void); // from _free_functions.cpp @ 0x00433a60
void OnSelectCreatureByMenuIndex_06(void); // from _free_functions.cpp @ 0x00433a70
void OnSelectCreatureByMenuIndex_07(void); // from _free_functions.cpp @ 0x00433a80
void OnSelectCreatureByMenuIndex_08(void); // from _free_functions.cpp @ 0x00433a90
void OnSelectCreatureByMenuIndex_09(void); // from _free_functions.cpp @ 0x00433aa0
void OnSelectCreatureByMenuIndex_10(void); // from _free_functions.cpp @ 0x00433ab0
void OnSelectCreatureByMenuIndex_11(void); // from _free_functions.cpp @ 0x00433ac0
void OnSelectCreatureByMenuIndex_12(void); // from _free_functions.cpp @ 0x00433ad0
void OnSelectCreatureByMenuIndex_13(void); // from _free_functions.cpp @ 0x00433ae0
void OnSelectCreatureByMenuIndex_14(void); // from _free_functions.cpp @ 0x00433af0
void OnSelectCreatureByMenuIndex_15(void); // from _free_functions.cpp @ 0x00433b00
void OnSelectCreatureByMenuIndex_16(void); // from _free_functions.cpp @ 0x00433b10
void OnSelectCreatureByMenuIndex_17(void); // from _free_functions.cpp @ 0x00433b20
void OnSelectCreatureByMenuIndex_18(void); // from _free_functions.cpp @ 0x00433b30
void OnSelectCreatureByMenuIndex_19(void); // from _free_functions.cpp @ 0x00433b40
void OnSelectCreatureByMenuIndex_20(void); // from _free_functions.cpp @ 0x00433b50
void OnSelectCreatureByMenuIndex_21(void); // from _free_functions.cpp @ 0x00433b60
void OnSelectCreatureByMenuIndex_22(void); // from _free_functions.cpp @ 0x00433b70
void OnSelectCreatureByMenuIndex_23(void); // from _free_functions.cpp @ 0x00433b80
void OnSelectCreatureByMenuIndex_24(void); // from _free_functions.cpp @ 0x00433b90
void OnSelectCreatureByMenuIndex_25(void); // from _free_functions.cpp @ 0x00433ba0
void OnSelectCreatureByMenuIndex_26(void); // from _free_functions.cpp @ 0x00433bb0
void OnSelectCreatureByMenuIndex_27(void); // from _free_functions.cpp @ 0x00433bc0
void OnSelectCreatureByMenuIndex_28(void); // from _free_functions.cpp @ 0x00433bd0
void OnSelectCreatureByMenuIndex_29(void); // from _free_functions.cpp @ 0x00433be0
void OnSelectCreatureByMenuIndex_30(void); // from _free_functions.cpp @ 0x00433bf0
void OnSetFocusRestoreSoundMixer(CWnd *param_1,CWnd *param_2); // from _free_functions.cpp @ 0x004375a0
void OnUpdateSelectCreatureByMenuIndex_00(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00433c00
void OnUpdateSelectCreatureByMenuIndex_01(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00433c60
void OnUpdateSelectCreatureByMenuIndex_02(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00433cc0
void OnUpdateSelectCreatureByMenuIndex_03(int *param_1,int *cmd_ui); // from _free_functions.cpp @ 0x00433d20
void OnUpdateSelectCreatureByMenuIndex_04(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00433d80
void OnUpdateSelectCreatureByMenuIndex_05(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00433de0
void OnUpdateSelectCreatureByMenuIndex_06(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00433e40
void OnUpdateSelectCreatureByMenuIndex_07(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00433ea0
void OnUpdateSelectCreatureByMenuIndex_08(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00433f00
void OnUpdateSelectCreatureByMenuIndex_09(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00433f60
void OnUpdateSelectCreatureByMenuIndex_10(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00433fc0
void OnUpdateSelectCreatureByMenuIndex_11(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00434020
void OnUpdateSelectCreatureByMenuIndex_12(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00434080
void OnUpdateSelectCreatureByMenuIndex_13(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x004340e0
void OnUpdateSelectCreatureByMenuIndex_14(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00434140
void OnUpdateSelectCreatureByMenuIndex_15(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x004341a0
void OnUpdateSelectCreatureByMenuIndex_16(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00434200
void OnUpdateSelectCreatureByMenuIndex_17(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00434260
void OnUpdateSelectCreatureByMenuIndex_18(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x004342c0
void OnUpdateSelectCreatureByMenuIndex_19(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00434320
void OnUpdateSelectCreatureByMenuIndex_20(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00434380
void OnUpdateSelectCreatureByMenuIndex_21(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x004343e0
void OnUpdateSelectCreatureByMenuIndex_22(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00434440
void OnUpdateSelectCreatureByMenuIndex_23(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x004344a0
void OnUpdateSelectCreatureByMenuIndex_24(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00434500
void OnUpdateSelectCreatureByMenuIndex_25(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00434560
void OnUpdateSelectCreatureByMenuIndex_26(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x004345c0
void OnUpdateSelectCreatureByMenuIndex_27(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00434620
void OnUpdateSelectCreatureByMenuIndex_28(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00434680
void OnUpdateSelectCreatureByMenuIndex_29(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x004346e0
void OnUpdateSelectCreatureByMenuIndex_30(int *param_1,int *command_ui); // from _free_functions.cpp @ 0x00434740
void OnUpdateShowDebugConsole(int *param_1); // from _free_functions.cpp @ 0x00434ce0
void OnUpdateToggleBurbleSetting(int param_1,int *param_2); // from _free_functions.cpp @ 0x0043f480
void OnUpdateToggleBurbleSetting_ViaGlobalAppPointer(int *param_1); // from _free_functions.cpp @ 0x00433480
void OnUpdateToggleDebugLogCategory(int *param_1); // from _free_functions.cpp @ 0x00434a30
void OnUpdateToggleDebugLoggingEnabled(int *param_1); // from _free_functions.cpp @ 0x00434a90
void OnUpdateToggleEyeView(int *param_1); // from _free_functions.cpp @ 0x004322d0
void OnUpdateToggleInformativeSelectionMenu(int param_1,int *param_2); // from _free_functions.cpp @ 0x00433380
void OnUpdateToggleMute(int param_1,int *param_2); // from _free_functions.cpp @ 0x00433410
void OnUpdateToggleViewportFollowSelectedCreatureMode(int param_1,int *param_2); // from _free_functions.cpp @ 0x00437aa0
void OnUpdateWorldHalfHeightSetting(int *param_1); // from _free_functions.cpp @ 0x00437560
C1RegistryKeyPair * OpenCreaturesRegistryKeys(void *self,undefined4 param_1,undefined4 param_2,char *param_3); // from _free_functions.cpp @ 0x0042f360
void OpenWebpageUrlShortcut(void); // from _free_functions.cpp @ 0x004335f0
void PersistEyeViewWindowPosition(void); // from _free_functions.cpp @ 0x004175e0
undefined4 * __fastcall PipeResponse_AppendAndMoveResult(C1MsvcString *result_string,C1MsvcString *destination_string,C1MsvcString *source_string); // from _free_functions.cpp @ 0x0043f8b0
undefined4 * __fastcall PipeResponse_MoveAppendCString(undefined4 *param_1,void *param_2,char *param_3); // from _free_functions.cpp @ 0x0043f840
undefined4 * __fastcall PipeResponse_ReserveAndMove(undefined4 *destination_string,undefined4 *source_string); // from _free_functions.cpp @ 0x00448cd0
C1StringMapNode * PipeServerDispatchCommand(C1StringMapNode *param_1,C1MsvcString *param_2); // from _free_functions.cpp @ 0x00446c10
C1MsvcString * PipeServerExecuteFireCommand(C1MsvcString *response,ushort macro_type,C1MsvcString *macro_source); // from _free_functions.cpp @ 0x00448570
void PipeServerGlobalCleanup(void); // from _free_functions.cpp @ 0x0044eff0
undefined4 * PipeServerMarshalCommandToMainThread(void *self,C1MsvcString *response,C1MsvcString *command); // from _free_functions.cpp @ 0x00446620
void PipeServerServeClient(void *self,HANDLE pipe_handle); // from _free_functions.cpp @ 0x00446110
void __fastcall PipeServerStop(PipeServerSharedState *state); // from _free_functions.cpp @ 0x00445d00
undefined4 PipeServerThreadEntry(void *param_1); // from _free_functions.cpp @ 0x00445f00
void __fastcall PipeServerThreadRun(PipeServerSharedState *state); // from _free_functions.cpp @ 0x00445f10
int PointInRectXY(void *self,LONG param_1,LONG param_2); // from _free_functions.cpp @ 0x0040ee20
void PopulateEmbeddedKitMenuAndToolbarFromRegistry(void); // from _free_functions.cpp @ 0x004440e0
void ProcessQueuedObjectEventsAndStimuli(void); // from _free_functions.cpp @ 0x00432d20
void PromoteTemporaryWorldBackup(void); // from _free_functions.cpp @ 0x004359b0
undefined4 __fastcall PromptForExpFilePath(int open_dialog,char *output_path_buffer,char *file_filter,undefined4 param_4,char *param_5); // from _free_functions.cpp @ 0x00417d70
void __fastcall PurgeDestroyWhenFinishedMacrosForOwner(Object *script_owner); // from _free_functions.cpp @ 0x00419d50
void __fastcall QueueEventForCreaturesInSpeechRange(Object *source_object,ObjectEventId event_id,C1CAOSValue event_argument,
          undefined4 unused_abi_argument,int delay_world_ticks); // from _free_functions.cpp @ 0x00422f90
void __fastcall QueueEventForCreaturesThatCanPerceiveObject(Object *source_object,ObjectEventId event_id); // from _free_functions.cpp @ 0x00423030
void __fastcall QueueObjectEvent(Object *source_object,Object *target_object,ObjectEventId event_id,
                C1CAOSValue event_argument,undefined4 reserved_abi_word,int delay_world_ticks); // from _free_functions.cpp @ 0x00422e80
void __fastcall QueueSignStimulusForPerceivingCreatures(Object *source_object,int stimulus_index); // from _free_functions.cpp @ 0x00423390
void __fastcall QueueTactEventForOverlappingCreatures(Object *source_object,ObjectEventId event_id); // from _free_functions.cpp @ 0x00423090
void __fastcall QueueTactStimulusForOverlappingCreatures(Object *source_object,int stimulus_index); // from _free_functions.cpp @ 0x00423470
void ReadOrInitializeRegistryDwordPair(undefined4 param_1,LPCSTR registry_value_name,DWORD *output_pair,
               DWORD first_default_value,DWORD second_default_value_arg); // from _free_functions.cpp @ 0x0042f470
void __fastcall RealizeWorldRendererPalette(CWnd *param_1); // from _free_functions.cpp @ 0x004176c0
void RealizeWorldRendererPaletteIfOwnerChanged(void *self,CWnd *param_1); // from _free_functions.cpp @ 0x00417690
undefined4 RebuildCreatureSelectionMenu(void); // from _free_functions.cpp @ 0x00422250
void __fastcall RedrawWorldRendererFullViewOnPaint(CWnd *paint_window); // from _free_functions.cpp @ 0x004173d0
void RefreshTemporaryWorldBackup(void); // from _free_functions.cpp @ 0x004357e0
void RegisterAtExit_DeleteGamePaletteHandle(void); // from _free_functions.cpp @ 0x004015c0
void RegisterAtExit_DestroyDdeItem_BrainActivity(void); // from _free_functions.cpp @ 0x00401060
void RegisterAtExit_DestroyDdeItem_BrainWiring(void); // from _free_functions.cpp @ 0x00401070
void RegisterAtExit_DestroyDdeItem_Macro(void); // from _free_functions.cpp @ 0x00401050
void RegisterAtExit_DestroyDdeItem_SysInfo(void); // from _free_functions.cpp @ 0x00401080
void RegisterMsvcFacetNodeCleanup(void); // from _free_functions.cpp @ 0x00401867
void RegisterSfcOleObjectFactory(void); // from _free_functions.cpp @ 0x00401380
void RehashRenderableObjectSet(uint requested_bucket_count); // from _free_functions.cpp @ 0x0042d310
void __fastcall ReleaseSystemInfoWindowSingleton(CSystemInfoWnd *window); // from _free_functions.cpp @ 0x00449f10
void __fastcall RemoveCreatureFromRegistry(Creature *creature); // from _free_functions.cpp @ 0x00408270
void RemoveFavouritePlaceAtIndex(int param_1); // from _free_functions.cpp @ 0x00435200
void RemoveFirstMatchingFile(char *directory_path,char *filename_pattern); // from _free_functions.cpp @ 0x004354f0
bool RemoveObjectFromRenderableSet(Object **object_ptr); // from _free_functions.cpp @ 0x0042d0c0
void __fastcall RenderableObjectSet_Clear(RenderableObjectSet *self); // from _free_functions.cpp @ 0x0042d630
RenderableObjectSetNode * RenderableObjectSet_EraseNodeRange(void *self,RenderableObjectSetNode *first,RenderableObjectSetNode *end); // from _free_functions.cpp @ 0x0042d6b0
void __fastcall ResetComboBoxContents(int param_1); // from _free_functions.cpp @ 0x004109b0
void ResizeU32VectorWithFill(void *self,uint requested_element_count,uint fill_value); // from _free_functions.cpp @ 0x0042d490
C1MsvcString * ResolveClassifierDisplayName(void *self,C1MsvcString *out_display_name,void *param_2); // from _free_functions.cpp @ 0x00439310
undefined4 __fastcall ResolveComProgIdLocalServerPath(LPCSTR prog_id,char *server_path_out); // from _free_functions.cpp @ 0x00443e20
C1ResourceFilenameStem4 __fastcall ResolveExistingBodyPartFilenameWithFallback(C1BodyPartIndex body_part_index,uint genus,C1CreatureSex sex,C1GenomeLifeStage life_stage
          ,uint variant,char *extension3,int resource_directory_index); // from _free_functions.cpp @ 0x0043c3e0
void RestoreSoundMixer(void); // from _free_functions.cpp @ 0x00441310
void ReturnViewportNavigationToSelectedCreature(void); // from _free_functions.cpp @ 0x00437910
void __cdecl ReverseVisibleSpriteRecordRange(C1VisibleSpriteSortRecord *first,C1VisibleSpriteSortRecord *last); // from _free_functions.cpp @ 0x0044ba80
AFX_MSGMAP * SFCView_GetMessageMapMetadata(void); // from _free_functions.cpp @ 0x00436430
undefined ** SFCView_GetRuntimeClassMetadata(void); // from _free_functions.cpp @ 0x00436420
void __fastcall SaveActiveDocumentAndResumeWorldTimer(CDocument *param_1); // from _free_functions.cpp @ 0x00434950
void __fastcall SaveActiveDocumentAsAndResumeWorldTimer(CDocument *param_1); // from _free_functions.cpp @ 0x004349b0
void __fastcall SaveGeneratedGenomeFile(CGenome *generated_genome); // from _free_functions.cpp @ 0x00418950
void ScrollViewportIfNonZero(void *self,int delta_x,int delta_y); // from _free_functions.cpp @ 0x00412f10
void SelectCreatureByMenuIndex(int menu_index); // from _free_functions.cpp @ 0x00433920
void SelectNextCreature(void); // from _free_functions.cpp @ 0x00435ce0
void SelectPreviousCreature(void); // from _free_functions.cpp @ 0x00435e00
undefined4 SendYourIdIsMessageToEmbeddedKit(void *self,void *invoke_argument_0,void *invoke_argument_1); // from _free_functions.cpp @ 0x0042d860
void __fastcall SerializeScriptsForClassifier(CArchiveRuntimeState *archive,ScriptClassifier classifier_base); // from _free_functions.cpp @ 0x0041a680
void SetEmbeddedKitToolAvailabilityByName(char *tool_name,byte is_available); // from _free_functions.cpp @ 0x004215f0
void SetSelectedCreature(void *self,Creature *selected_creature,int update_view); // from _free_functions.cpp @ 0x00433850
void SetWorldUpdatePaused(C1Bool32 paused); // from _free_functions.cpp @ 0x00435cb0
void ShowCAOSConsoleDialog(void); // from _free_functions.cpp @ 0x00434d20
void ShowRemoveFavouritePlaceDialog(void); // from _free_functions.cpp @ 0x00435b20
void ShowStartupTipDialog(void); // from _free_functions.cpp @ 0x0043f3d0
void ShowVersionDialog(void); // from _free_functions.cpp @ 0x0043f2c0
void ShowVolumeDialog(void); // from _free_functions.cpp @ 0x004334c0
void ShutdownDdeService(void); // from _free_functions.cpp @ 0x0044edf0
C1Bool32 __fastcall ShutdownEmbeddedKitTool(uint tool_index); // from _free_functions.cpp @ 0x00444a70
SpriteFileCacheIndexNode * SpriteFileCacheIndex_EraseNodeRange(void *self,SpriteFileCacheIndexNode *first_node,SpriteFileCacheIndexNode *end_node); // from _free_functions.cpp @ 0x00443220
SpriteFileCacheIndexLookupResult * SpriteFileCacheIndex_Find(SpriteFileCacheIndexLookupResult *lookup_result,uint *sprite_file_id_ptr,
          uint sprite_file_id_hash); // from _free_functions.cpp @ 0x004433d0
SpriteFileCacheIndexInsertResult * SpriteFileCacheIndex_InsertIfAbsent(SpriteFileCacheIndexInsertResult *insert_result,uint *sprite_file_id_ptr); // from _free_functions.cpp @ 0x00443010
void SpriteFileCacheIndex_Rehash(uint minimum_bucket_count); // from _free_functions.cpp @ 0x00443460
void __fastcall StableSortVisibleSpriteRecordsByPlane(C1VisibleSpriteSortRecord *begin,C1VisibleSpriteSortRecord *end,uint range_count,
          C1VisibleSpriteSortRecord *scratch_buffer,int scratch_capacity,int full_sort_count); // from _free_functions.cpp @ 0x00414190
undefined4 * StdBadCast_CopyConstruct(void *self,int param_1); // from _free_functions.cpp @ 0x00410380
void __fastcall StdBasicFilebuf_Destructor(std::basic_streambuf<char,std::char_traits<char>> *filebuf_object); // from _free_functions.cpp @ 0x00411a30
void __fastcall StdBasicFilebuf_LockFile(int param_1); // from _free_functions.cpp @ 0x00411a20
int __fastcall StdBasicFilebuf_Sync(int *file_buffer); // from _free_functions.cpp @ 0x00410f30
void __fastcall StdBasicFilebuf_UnlockFile(int param_1); // from _free_functions.cpp @ 0x00411a10
void __fastcall StdBasicOfstream_CloseAndDispatch(int *ofstream_object); // from _free_functions.cpp @ 0x00410900
int * StdBasicOfstream_DeletingDestructor(void *self,byte param_1); // from _free_functions.cpp @ 0x00411ab0
void __fastcall StdBasicOstreamSentry_Destructor(int *sentry_ptr); // from _free_functions.cpp @ 0x00440250
void __fastcall StdBasicOstreamSentry_Finalize(int *param_1); // from _free_functions.cpp @ 0x00411cb0
void __fastcall StdBasicOstreamSentry_ReleaseTiedStream(int *sentry_ptr); // from _free_functions.cpp @ 0x004121c0
void __fastcall StdExceptionMember_Release(int exception_member); // from _free_functions.cpp @ 0x004103c0
_Fac_node * StdFacetNode_DeletingDestructor(void *self,byte param_1); // from _free_functions.cpp @ 0x0044b8e1
void __fastcall StdFacet_Release(int *param_1); // from _free_functions.cpp @ 0x00411ca0
undefined4 * StdFpos_SetInvalid(void *self,undefined4 param_1,undefined4 param_2); // from _free_functions.cpp @ 0x00412190
void __fastcall StdSort16ByteRecordsByDwordOffset0xC(undefined8 *param_1,undefined8 *param_2,int param_3,void *param_4); // from _free_functions.cpp @ 0x00439b50
void __fastcall StdSortMedianOfThree16ByteRecordsByDwordOffset0xC(undefined8 *record_a,undefined8 *record_b,undefined8 *record_c); // from _free_functions.cpp @ 0x0043a9e0
void __fastcall StdSortPartition16ByteRecordsByDwordOffset0xC(undefined8 **partition_bounds,undefined8 *records_begin,undefined8 *records_end); // from _free_functions.cpp @ 0x0043a0d0
void __fastcall StdSortSiftDown16ByteRecordsByDwordOffset0xC(int param_1,int param_2,uint param_3,undefined8 *param_4); // from _free_functions.cpp @ 0x0043a750
void StdThrowInvalidStringPosition(void); // from _free_functions.cpp @ 0x00412210
void StdThrowMapSetTooLong(void); // from _free_functions.cpp @ 0x00436280
void StdThrowVectorTooLong(void); // from _free_functions.cpp @ 0x0043a890
undefined4 SuspendSoundMixer(void); // from _free_functions.cpp @ 0x004412c0
void __fastcall SymmetricMergeVisibleSpriteRecordRangesByPlane(C1VisibleSpriteSortRecord *first,C1VisibleSpriteSortRecord *middle,
          C1VisibleSpriteSortRecord *last,int left_count,int right_count,
          C1VisibleSpriteSortRecord *scratch_buffer,int scratch_capacity,int total_count,
          C1VisibleSpriteSortRecord *left_split,C1VisibleSpriteSortRecord *right_split,
          int left_lower_count,int right_lower_count); // from _free_functions.cpp @ 0x004148d0
void __fastcall TerminateLaunchedKitProcesses(DWORD unused_initial_exit_code); // from _free_functions.cpp @ 0x00444b50
void ThrowBadCastException(void); // from _free_functions.cpp @ 0x00410360
void __fastcall ToggleBurbleSetting(int settings_state_ptr); // from _free_functions.cpp @ 0x0043f440
void ToggleBurbleSetting_ViaGlobalAppPointer(void); // from _free_functions.cpp @ 0x00433440
void __fastcall ToggleCaosConsoleAlwaysOnTop(CWnd *param_1); // from _free_functions.cpp @ 0x0040f570
void __fastcall ToggleControlEnabledByFlag(int control_owner); // from _free_functions.cpp @ 0x00410940
void ToggleDebugLogCategory(int param_1); // from _free_functions.cpp @ 0x00434a10
void ToggleDebugLoggingEnabled(void); // from _free_functions.cpp @ 0x00434a70
void ToggleEyeView(void); // from _free_functions.cpp @ 0x00432140
void __fastcall ToggleInfiniteScrollWorldSize(CWnd *param_1); // from _free_functions.cpp @ 0x004374e0
undefined4 __fastcall ToggleInformativeSelectionMenuAndRebuild(int param_1); // from _free_functions.cpp @ 0x00433340
void __fastcall ToggleMuteAndStopSounds(int param_1); // from _free_functions.cpp @ 0x004333b0
void __fastcall ToggleViewportFollowSelectedCreatureMode(CWnd *param_1); // from _free_functions.cpp @ 0x004379a0
void __fastcall ToggleWindowAlwaysOnTop(CWnd *param_1); // from _free_functions.cpp @ 0x00410970
void TriggerSelectedCreatureScriptEvent(int param_1); // from _free_functions.cpp @ 0x004324c0
WorldRect * __fastcall UnionWrappedWorldRects(WorldRect *out_rect,WorldRect *first_rect,WorldRect *second_rect); // from _free_functions.cpp @ 0x00417cc0
void Unwind_0044bb88(void); // from _free_functions.cpp @ 0x0044bb88
void Unwind_0044bdb0(void); // from _free_functions.cpp @ 0x0044bdb0
void Unwind_0044be00(void); // from _free_functions.cpp @ 0x0044be00
void Unwind_0044be40(void); // from _free_functions.cpp @ 0x0044be40
void Unwind_0044be70(void); // from _free_functions.cpp @ 0x0044be70
void Unwind_0044be8a(void); // from _free_functions.cpp @ 0x0044be8a
void Unwind_0044bf10(void); // from _free_functions.cpp @ 0x0044bf10
void Unwind_0044bf1b(void); // from _free_functions.cpp @ 0x0044bf1b
void Unwind_0044bf24(void); // from _free_functions.cpp @ 0x0044bf24
void Unwind_0044bf2c(void); // from _free_functions.cpp @ 0x0044bf2c
void Unwind_0044bf70(void); // from _free_functions.cpp @ 0x0044bf70
void Unwind_0044bf78(void); // from _free_functions.cpp @ 0x0044bf78
void Unwind_0044bf80(void); // from _free_functions.cpp @ 0x0044bf80
void Unwind_0044bf8e(void); // from _free_functions.cpp @ 0x0044bf8e
void Unwind_0044bfc0(void); // from _free_functions.cpp @ 0x0044bfc0
void Unwind_0044bfc9(void); // from _free_functions.cpp @ 0x0044bfc9
void Unwind_0044bfd2(void); // from _free_functions.cpp @ 0x0044bfd2
void Unwind_0044c000(void); // from _free_functions.cpp @ 0x0044c000
void Unwind_0044c008(void); // from _free_functions.cpp @ 0x0044c008
void Unwind_0044c011(void); // from _free_functions.cpp @ 0x0044c011
void Unwind_0044c040(void); // from _free_functions.cpp @ 0x0044c040
void Unwind_0044c0a0(void); // from _free_functions.cpp @ 0x0044c0a0
void Unwind_0044c0ab(void); // from _free_functions.cpp @ 0x0044c0ab
void Unwind_0044c0bc(void); // from _free_functions.cpp @ 0x0044c0bc
void Unwind_0044c0cd(void); // from _free_functions.cpp @ 0x0044c0cd
void Unwind_0044c110(void); // from _free_functions.cpp @ 0x0044c110
void Unwind_0044c140(void); // from _free_functions.cpp @ 0x0044c140
void Unwind_0044c170(void); // from _free_functions.cpp @ 0x0044c170
void Unwind_0044c1d0(void); // from _free_functions.cpp @ 0x0044c1d0
void Unwind_0044c1d9(void); // from _free_functions.cpp @ 0x0044c1d9
void Unwind_0044c210(void); // from _free_functions.cpp @ 0x0044c210
void Unwind_0044c219(void); // from _free_functions.cpp @ 0x0044c219
void Unwind_0044c222(void); // from _free_functions.cpp @ 0x0044c222
void Unwind_0044c22b(void); // from _free_functions.cpp @ 0x0044c22b
void Unwind_0044c234(void); // from _free_functions.cpp @ 0x0044c234
void Unwind_0044c270(void); // from _free_functions.cpp @ 0x0044c270
void Unwind_0044c278(void); // from _free_functions.cpp @ 0x0044c278
void Unwind_0044c281(void); // from _free_functions.cpp @ 0x0044c281
void Unwind_0044c28a(void); // from _free_functions.cpp @ 0x0044c28a
void Unwind_0044c293(void); // from _free_functions.cpp @ 0x0044c293
void Unwind_0044c29c(void); // from _free_functions.cpp @ 0x0044c29c
void Unwind_0044c2a5(void); // from _free_functions.cpp @ 0x0044c2a5
void Unwind_0044c2d0(void); // from _free_functions.cpp @ 0x0044c2d0
void Unwind_0044c2d9(void); // from _free_functions.cpp @ 0x0044c2d9
void Unwind_0044c310(void); // from _free_functions.cpp @ 0x0044c310
void Unwind_0044c370(void); // from _free_functions.cpp @ 0x0044c370
void Unwind_0044c3a0(void); // from _free_functions.cpp @ 0x0044c3a0
void Unwind_0044c3d0(void); // from _free_functions.cpp @ 0x0044c3d0
void Unwind_0044c3ed(void); // from _free_functions.cpp @ 0x0044c3ed
void Unwind_0044c420(void); // from _free_functions.cpp @ 0x0044c420
void Unwind_0044c4a0(void); // from _free_functions.cpp @ 0x0044c4a0
void Unwind_0044c4a9(void); // from _free_functions.cpp @ 0x0044c4a9
void Unwind_0044c4e0(void); // from _free_functions.cpp @ 0x0044c4e0
void Unwind_0044c510(void); // from _free_functions.cpp @ 0x0044c510
void Unwind_0044c540(void); // from _free_functions.cpp @ 0x0044c540
void Unwind_0044c570(void); // from _free_functions.cpp @ 0x0044c570
void Unwind_0044c5b0(void); // from _free_functions.cpp @ 0x0044c5b0
void Unwind_0044c5f0(void); // from _free_functions.cpp @ 0x0044c5f0
void Unwind_0044c620(void); // from _free_functions.cpp @ 0x0044c620
void Unwind_0044c650(void); // from _free_functions.cpp @ 0x0044c650
void Unwind_0044c6b0(void); // from _free_functions.cpp @ 0x0044c6b0
void Unwind_0044c6f0(void); // from _free_functions.cpp @ 0x0044c6f0
void Unwind_0044c730(void); // from _free_functions.cpp @ 0x0044c730
void Unwind_0044c739(void); // from _free_functions.cpp @ 0x0044c739
void Unwind_0044c770(void); // from _free_functions.cpp @ 0x0044c770
void Unwind_0044c779(void); // from _free_functions.cpp @ 0x0044c779
void Unwind_0044c782(void); // from _free_functions.cpp @ 0x0044c782
void Unwind_0044c78b(void); // from _free_functions.cpp @ 0x0044c78b
void Unwind_0044c794(void); // from _free_functions.cpp @ 0x0044c794
void Unwind_0044c79d(void); // from _free_functions.cpp @ 0x0044c79d
void Unwind_0044c7d0(void); // from _free_functions.cpp @ 0x0044c7d0
void Unwind_0044c7d9(void); // from _free_functions.cpp @ 0x0044c7d9
void Unwind_0044c810(void); // from _free_functions.cpp @ 0x0044c810
void Unwind_0044c850(void); // from _free_functions.cpp @ 0x0044c850
void Unwind_0044c890(void); // from _free_functions.cpp @ 0x0044c890
void Unwind_0044c898(void); // from _free_functions.cpp @ 0x0044c898
void Unwind_0044c8d0(void); // from _free_functions.cpp @ 0x0044c8d0
void Unwind_0044c900(void); // from _free_functions.cpp @ 0x0044c900
void Unwind_0044c909(void); // from _free_functions.cpp @ 0x0044c909
void Unwind_0044c912(void); // from _free_functions.cpp @ 0x0044c912
void Unwind_0044c92c(void); // from _free_functions.cpp @ 0x0044c92c
void Unwind_0044c970(void); // from _free_functions.cpp @ 0x0044c970
void Unwind_0044c97c(void); // from _free_functions.cpp @ 0x0044c97c
void Unwind_0044c9c0(void); // from _free_functions.cpp @ 0x0044c9c0
void Unwind_0044ca00(void); // from _free_functions.cpp @ 0x0044ca00
void Unwind_0044ca08(void); // from _free_functions.cpp @ 0x0044ca08
void Unwind_0044ca10(void); // from _free_functions.cpp @ 0x0044ca10
void Unwind_0044ca40(void); // from _free_functions.cpp @ 0x0044ca40
void Unwind_0044cac0(void); // from _free_functions.cpp @ 0x0044cac0
void Unwind_0044cac9(void); // from _free_functions.cpp @ 0x0044cac9
void Unwind_0044cb20(void); // from _free_functions.cpp @ 0x0044cb20
void Unwind_0044cb50(void); // from _free_functions.cpp @ 0x0044cb50
void Unwind_0044cb59(void); // from _free_functions.cpp @ 0x0044cb59
void Unwind_0044cb90(void); // from _free_functions.cpp @ 0x0044cb90
void Unwind_0044cbc0(void); // from _free_functions.cpp @ 0x0044cbc0
void Unwind_0044cbf0(void); // from _free_functions.cpp @ 0x0044cbf0
void Unwind_0044cbfc(void); // from _free_functions.cpp @ 0x0044cbfc
void Unwind_0044cc08(void); // from _free_functions.cpp @ 0x0044cc08
void Unwind_0044cc50(void); // from _free_functions.cpp @ 0x0044cc50
void Unwind_0044cc58(void); // from _free_functions.cpp @ 0x0044cc58
void Unwind_0044cc60(void); // from _free_functions.cpp @ 0x0044cc60
void Unwind_0044cc68(void); // from _free_functions.cpp @ 0x0044cc68
void Unwind_0044cc70(void); // from _free_functions.cpp @ 0x0044cc70
void Unwind_0044cc78(void); // from _free_functions.cpp @ 0x0044cc78
void Unwind_0044cc80(void); // from _free_functions.cpp @ 0x0044cc80
void Unwind_0044cc88(void); // from _free_functions.cpp @ 0x0044cc88
void Unwind_0044cc90(void); // from _free_functions.cpp @ 0x0044cc90
void Unwind_0044cc98(void); // from _free_functions.cpp @ 0x0044cc98
void Unwind_0044cca0(void); // from _free_functions.cpp @ 0x0044cca0
void Unwind_0044ccd0(void); // from _free_functions.cpp @ 0x0044ccd0
void Unwind_0044cd10(void); // from _free_functions.cpp @ 0x0044cd10
void Unwind_0044cd18(void); // from _free_functions.cpp @ 0x0044cd18
void Unwind_0044cd21(void); // from _free_functions.cpp @ 0x0044cd21
void Unwind_0044cd3c(void); // from _free_functions.cpp @ 0x0044cd3c
void Unwind_0044cd4a(void); // from _free_functions.cpp @ 0x0044cd4a
void Unwind_0044cda0(void); // from _free_functions.cpp @ 0x0044cda0
void Unwind_0044cda9(void); // from _free_functions.cpp @ 0x0044cda9
void Unwind_0044cde0(void); // from _free_functions.cpp @ 0x0044cde0
void Unwind_0044cde8(void); // from _free_functions.cpp @ 0x0044cde8
void Unwind_0044ce20(void); // from _free_functions.cpp @ 0x0044ce20
void Unwind_0044ce29(void); // from _free_functions.cpp @ 0x0044ce29
void Unwind_0044ce60(void); // from _free_functions.cpp @ 0x0044ce60
void Unwind_0044ce69(void); // from _free_functions.cpp @ 0x0044ce69
void Unwind_0044ce72(void); // from _free_functions.cpp @ 0x0044ce72
void Unwind_0044ce7b(void); // from _free_functions.cpp @ 0x0044ce7b
void Unwind_0044ce95(void); // from _free_functions.cpp @ 0x0044ce95
void Unwind_0044ce9e(void); // from _free_functions.cpp @ 0x0044ce9e
void Unwind_0044cee0(void); // from _free_functions.cpp @ 0x0044cee0
void Unwind_0044cee8(void); // from _free_functions.cpp @ 0x0044cee8
void Unwind_0044cf20(void); // from _free_functions.cpp @ 0x0044cf20
void Unwind_0044cf28(void); // from _free_functions.cpp @ 0x0044cf28
void Unwind_0044cf60(void); // from _free_functions.cpp @ 0x0044cf60
void Unwind_0044cfb0(void); // from _free_functions.cpp @ 0x0044cfb0
void Unwind_0044cfbc(void); // from _free_functions.cpp @ 0x0044cfbc
void Unwind_0044d000(void); // from _free_functions.cpp @ 0x0044d000
void Unwind_0044d030(void); // from _free_functions.cpp @ 0x0044d030
void Unwind_0044d038(void); // from _free_functions.cpp @ 0x0044d038
void Unwind_0044d070(void); // from _free_functions.cpp @ 0x0044d070
void Unwind_0044d079(void); // from _free_functions.cpp @ 0x0044d079
void Unwind_0044d085(void); // from _free_functions.cpp @ 0x0044d085
void Unwind_0044d0c0(void); // from _free_functions.cpp @ 0x0044d0c0
void Unwind_0044d0cc(void); // from _free_functions.cpp @ 0x0044d0cc
void Unwind_0044d0d8(void); // from _free_functions.cpp @ 0x0044d0d8
void Unwind_0044d110(void); // from _free_functions.cpp @ 0x0044d110
void Unwind_0044d119(void); // from _free_functions.cpp @ 0x0044d119
void Unwind_0044d125(void); // from _free_functions.cpp @ 0x0044d125
void Unwind_0044d160(void); // from _free_functions.cpp @ 0x0044d160
void Unwind_0044d190(void); // from _free_functions.cpp @ 0x0044d190
void Unwind_0044d1d0(void); // from _free_functions.cpp @ 0x0044d1d0
void Unwind_0044d230(void); // from _free_functions.cpp @ 0x0044d230
void Unwind_0044d270(void); // from _free_functions.cpp @ 0x0044d270
void Unwind_0044d27c(void); // from _free_functions.cpp @ 0x0044d27c
void Unwind_0044d2c0(void); // from _free_functions.cpp @ 0x0044d2c0
void Unwind_0044d2c8(void); // from _free_functions.cpp @ 0x0044d2c8
void Unwind_0044d2df(void); // from _free_functions.cpp @ 0x0044d2df
void Unwind_0044d320(void); // from _free_functions.cpp @ 0x0044d320
void Unwind_0044d328(void); // from _free_functions.cpp @ 0x0044d328
void Unwind_0044d331(void); // from _free_functions.cpp @ 0x0044d331
void Unwind_0044d360(void); // from _free_functions.cpp @ 0x0044d360
void Unwind_0044d390(void); // from _free_functions.cpp @ 0x0044d390
void Unwind_0044d3c0(void); // from _free_functions.cpp @ 0x0044d3c0
void Unwind_0044d3f0(void); // from _free_functions.cpp @ 0x0044d3f0
void Unwind_0044d420(void); // from _free_functions.cpp @ 0x0044d420
void Unwind_0044d42a(void); // from _free_functions.cpp @ 0x0044d42a
void Unwind_0044d460(void); // from _free_functions.cpp @ 0x0044d460
void Unwind_0044d468(void); // from _free_functions.cpp @ 0x0044d468
void Unwind_0044d471(void); // from _free_functions.cpp @ 0x0044d471
void Unwind_0044d4b0(void); // from _free_functions.cpp @ 0x0044d4b0
void Unwind_0044d4b8(void); // from _free_functions.cpp @ 0x0044d4b8
void Unwind_0044d4d0(void); // from _free_functions.cpp @ 0x0044d4d0
void Unwind_0044d4d8(void); // from _free_functions.cpp @ 0x0044d4d8
void Unwind_0044d510(void); // from _free_functions.cpp @ 0x0044d510
void Unwind_0044d518(void); // from _free_functions.cpp @ 0x0044d518
void Unwind_0044d521(void); // from _free_functions.cpp @ 0x0044d521
void Unwind_0044d550(void); // from _free_functions.cpp @ 0x0044d550
void Unwind_0044d559(void); // from _free_functions.cpp @ 0x0044d559
void Unwind_0044d561(void); // from _free_functions.cpp @ 0x0044d561
void Unwind_0044d590(void); // from _free_functions.cpp @ 0x0044d590
void Unwind_0044d599(void); // from _free_functions.cpp @ 0x0044d599
void Unwind_0044d5a2(void); // from _free_functions.cpp @ 0x0044d5a2
void Unwind_0044d5ab(void); // from _free_functions.cpp @ 0x0044d5ab
void Unwind_0044d5b4(void); // from _free_functions.cpp @ 0x0044d5b4
void Unwind_0044d5bc(void); // from _free_functions.cpp @ 0x0044d5bc
void Unwind_0044d600(void); // from _free_functions.cpp @ 0x0044d600
void Unwind_0044d609(void); // from _free_functions.cpp @ 0x0044d609
void Unwind_0044d612(void); // from _free_functions.cpp @ 0x0044d612
void Unwind_0044d61b(void); // from _free_functions.cpp @ 0x0044d61b
void Unwind_0044d660(void); // from _free_functions.cpp @ 0x0044d660
void Unwind_0044d668(void); // from _free_functions.cpp @ 0x0044d668
void Unwind_0044d671(void); // from _free_functions.cpp @ 0x0044d671
void Unwind_0044d683(void); // from _free_functions.cpp @ 0x0044d683
void Unwind_0044d6b0(void); // from _free_functions.cpp @ 0x0044d6b0
void Unwind_0044d6f0(void); // from _free_functions.cpp @ 0x0044d6f0
void Unwind_0044d6f9(void); // from _free_functions.cpp @ 0x0044d6f9
void Unwind_0044d702(void); // from _free_functions.cpp @ 0x0044d702
void Unwind_0044d730(void); // from _free_functions.cpp @ 0x0044d730
void Unwind_0044d738(void); // from _free_functions.cpp @ 0x0044d738
void Unwind_0044d770(void); // from _free_functions.cpp @ 0x0044d770
void Unwind_0044d779(void); // from _free_functions.cpp @ 0x0044d779
void Unwind_0044d7c0(void); // from _free_functions.cpp @ 0x0044d7c0
void Unwind_0044d7c8(void); // from _free_functions.cpp @ 0x0044d7c8
void Unwind_0044d7d1(void); // from _free_functions.cpp @ 0x0044d7d1
void Unwind_0044d7e0(void); // from _free_functions.cpp @ 0x0044d7e0
void Unwind_0044d7ef(void); // from _free_functions.cpp @ 0x0044d7ef
void Unwind_0044d7fe(void); // from _free_functions.cpp @ 0x0044d7fe
void Unwind_0044d80d(void); // from _free_functions.cpp @ 0x0044d80d
void Unwind_0044d81c(void); // from _free_functions.cpp @ 0x0044d81c
void Unwind_0044d82b(void); // from _free_functions.cpp @ 0x0044d82b
void Unwind_0044d83a(void); // from _free_functions.cpp @ 0x0044d83a
void Unwind_0044d849(void); // from _free_functions.cpp @ 0x0044d849
void Unwind_0044d858(void); // from _free_functions.cpp @ 0x0044d858
void Unwind_0044d867(void); // from _free_functions.cpp @ 0x0044d867
void Unwind_0044d876(void); // from _free_functions.cpp @ 0x0044d876
void Unwind_0044d8b0(void); // from _free_functions.cpp @ 0x0044d8b0
void Unwind_0044d8b8(void); // from _free_functions.cpp @ 0x0044d8b8
void Unwind_0044d8c1(void); // from _free_functions.cpp @ 0x0044d8c1
void Unwind_0044d8d0(void); // from _free_functions.cpp @ 0x0044d8d0
void Unwind_0044d8df(void); // from _free_functions.cpp @ 0x0044d8df
void Unwind_0044d8ee(void); // from _free_functions.cpp @ 0x0044d8ee
void Unwind_0044d8fd(void); // from _free_functions.cpp @ 0x0044d8fd
void Unwind_0044d930(void); // from _free_functions.cpp @ 0x0044d930
void Unwind_0044d93c(void); // from _free_functions.cpp @ 0x0044d93c
void Unwind_0044d945(void); // from _free_functions.cpp @ 0x0044d945
void Unwind_0044d980(void); // from _free_functions.cpp @ 0x0044d980
void Unwind_0044d9c0(void); // from _free_functions.cpp @ 0x0044d9c0
void Unwind_0044d9cc(void); // from _free_functions.cpp @ 0x0044d9cc
void Unwind_0044d9d8(void); // from _free_functions.cpp @ 0x0044d9d8
void Unwind_0044d9e3(void); // from _free_functions.cpp @ 0x0044d9e3
void Unwind_0044da20(void); // from _free_functions.cpp @ 0x0044da20
void Unwind_0044da2c(void); // from _free_functions.cpp @ 0x0044da2c
void Unwind_0044da38(void); // from _free_functions.cpp @ 0x0044da38
void Unwind_0044da44(void); // from _free_functions.cpp @ 0x0044da44
void Unwind_0044da80(void); // from _free_functions.cpp @ 0x0044da80
void Unwind_0044da89(void); // from _free_functions.cpp @ 0x0044da89
void Unwind_0044da92(void); // from _free_functions.cpp @ 0x0044da92
void Unwind_0044da9b(void); // from _free_functions.cpp @ 0x0044da9b
void Unwind_0044dab5(void); // from _free_functions.cpp @ 0x0044dab5
void Unwind_0044dae0(void); // from _free_functions.cpp @ 0x0044dae0
void Unwind_0044dae9(void); // from _free_functions.cpp @ 0x0044dae9
void Unwind_0044daf2(void); // from _free_functions.cpp @ 0x0044daf2
void Unwind_0044dafb(void); // from _free_functions.cpp @ 0x0044dafb
void Unwind_0044db15(void); // from _free_functions.cpp @ 0x0044db15
void Unwind_0044db60(void); // from _free_functions.cpp @ 0x0044db60
void Unwind_0044db6c(void); // from _free_functions.cpp @ 0x0044db6c
void Unwind_0044dbd0(void); // from _free_functions.cpp @ 0x0044dbd0
void Unwind_0044dc10(void); // from _free_functions.cpp @ 0x0044dc10
void Unwind_0044dc19(void); // from _free_functions.cpp @ 0x0044dc19
void Unwind_0044dc50(void); // from _free_functions.cpp @ 0x0044dc50
void Unwind_0044dc80(void); // from _free_functions.cpp @ 0x0044dc80
void Unwind_0044dc89(void); // from _free_functions.cpp @ 0x0044dc89
void Unwind_0044dc92(void); // from _free_functions.cpp @ 0x0044dc92
void Unwind_0044dc9b(void); // from _free_functions.cpp @ 0x0044dc9b
void Unwind_0044dca3(void); // from _free_functions.cpp @ 0x0044dca3
void Unwind_0044dcab(void); // from _free_functions.cpp @ 0x0044dcab
void Unwind_0044dcf0(void); // from _free_functions.cpp @ 0x0044dcf0
void Unwind_0044dcf8(void); // from _free_functions.cpp @ 0x0044dcf8
void Unwind_0044dd30(void); // from _free_functions.cpp @ 0x0044dd30
void Unwind_0044dd3c(void); // from _free_functions.cpp @ 0x0044dd3c
void Unwind_0044dd48(void); // from _free_functions.cpp @ 0x0044dd48
void Unwind_0044dd6b(void); // from _free_functions.cpp @ 0x0044dd6b
void Unwind_0044dd77(void); // from _free_functions.cpp @ 0x0044dd77
void Unwind_0044dd82(void); // from _free_functions.cpp @ 0x0044dd82
void Unwind_0044dd8d(void); // from _free_functions.cpp @ 0x0044dd8d
void Unwind_0044dd98(void); // from _free_functions.cpp @ 0x0044dd98
void Unwind_0044dda3(void); // from _free_functions.cpp @ 0x0044dda3
void Unwind_0044ddae(void); // from _free_functions.cpp @ 0x0044ddae
void Unwind_0044ddb9(void); // from _free_functions.cpp @ 0x0044ddb9
void Unwind_0044ddc4(void); // from _free_functions.cpp @ 0x0044ddc4
void Unwind_0044ddcf(void); // from _free_functions.cpp @ 0x0044ddcf
void Unwind_0044dddb(void); // from _free_functions.cpp @ 0x0044dddb
void Unwind_0044dde7(void); // from _free_functions.cpp @ 0x0044dde7
void Unwind_0044ddf2(void); // from _free_functions.cpp @ 0x0044ddf2
void Unwind_0044ddfd(void); // from _free_functions.cpp @ 0x0044ddfd
void Unwind_0044de08(void); // from _free_functions.cpp @ 0x0044de08
void Unwind_0044de13(void); // from _free_functions.cpp @ 0x0044de13
void Unwind_0044de50(void); // from _free_functions.cpp @ 0x0044de50
void Unwind_0044de90(void); // from _free_functions.cpp @ 0x0044de90
void Unwind_0044dec0(void); // from _free_functions.cpp @ 0x0044dec0
void Unwind_0044def0(void); // from _free_functions.cpp @ 0x0044def0
void Unwind_0044def9(void); // from _free_functions.cpp @ 0x0044def9
void Unwind_0044df02(void); // from _free_functions.cpp @ 0x0044df02
void Unwind_0044df0b(void); // from _free_functions.cpp @ 0x0044df0b
void Unwind_0044df40(void); // from _free_functions.cpp @ 0x0044df40
void Unwind_0044df70(void); // from _free_functions.cpp @ 0x0044df70
void Unwind_0044df7c(void); // from _free_functions.cpp @ 0x0044df7c
void Unwind_0044df87(void); // from _free_functions.cpp @ 0x0044df87
void Unwind_0044df92(void); // from _free_functions.cpp @ 0x0044df92
void Unwind_0044df9d(void); // from _free_functions.cpp @ 0x0044df9d
void Unwind_0044dfa8(void); // from _free_functions.cpp @ 0x0044dfa8
void Unwind_0044dfb3(void); // from _free_functions.cpp @ 0x0044dfb3
void Unwind_0044dfbe(void); // from _free_functions.cpp @ 0x0044dfbe
void Unwind_0044dfc9(void); // from _free_functions.cpp @ 0x0044dfc9
void Unwind_0044dfd4(void); // from _free_functions.cpp @ 0x0044dfd4
void Unwind_0044dfdf(void); // from _free_functions.cpp @ 0x0044dfdf
void Unwind_0044dfea(void); // from _free_functions.cpp @ 0x0044dfea
void Unwind_0044dff5(void); // from _free_functions.cpp @ 0x0044dff5
void Unwind_0044e000(void); // from _free_functions.cpp @ 0x0044e000
void Unwind_0044e00b(void); // from _free_functions.cpp @ 0x0044e00b
void Unwind_0044e016(void); // from _free_functions.cpp @ 0x0044e016
void Unwind_0044e050(void); // from _free_functions.cpp @ 0x0044e050
void Unwind_0044e06d(void); // from _free_functions.cpp @ 0x0044e06d
void Unwind_0044e079(void); // from _free_functions.cpp @ 0x0044e079
void Unwind_0044e0b0(void); // from _free_functions.cpp @ 0x0044e0b0
void Unwind_0044e0f0(void); // from _free_functions.cpp @ 0x0044e0f0
void Unwind_0044e0fb(void); // from _free_functions.cpp @ 0x0044e0fb
void Unwind_0044e107(void); // from _free_functions.cpp @ 0x0044e107
void Unwind_0044e113(void); // from _free_functions.cpp @ 0x0044e113
void Unwind_0044e11f(void); // from _free_functions.cpp @ 0x0044e11f
void Unwind_0044e12a(void); // from _free_functions.cpp @ 0x0044e12a
void Unwind_0044e135(void); // from _free_functions.cpp @ 0x0044e135
void Unwind_0044e158(void); // from _free_functions.cpp @ 0x0044e158
void Unwind_0044e164(void); // from _free_functions.cpp @ 0x0044e164
void Unwind_0044e170(void); // from _free_functions.cpp @ 0x0044e170
void Unwind_0044e17b(void); // from _free_functions.cpp @ 0x0044e17b
void Unwind_0044e186(void); // from _free_functions.cpp @ 0x0044e186
void Unwind_0044e191(void); // from _free_functions.cpp @ 0x0044e191
void Unwind_0044e19c(void); // from _free_functions.cpp @ 0x0044e19c
void Unwind_0044e1a7(void); // from _free_functions.cpp @ 0x0044e1a7
void Unwind_0044e1b2(void); // from _free_functions.cpp @ 0x0044e1b2
void Unwind_0044e1bd(void); // from _free_functions.cpp @ 0x0044e1bd
void Unwind_0044e1c8(void); // from _free_functions.cpp @ 0x0044e1c8
void Unwind_0044e1d3(void); // from _free_functions.cpp @ 0x0044e1d3
void Unwind_0044e1df(void); // from _free_functions.cpp @ 0x0044e1df
void Unwind_0044e1ea(void); // from _free_functions.cpp @ 0x0044e1ea
void Unwind_0044e1f6(void); // from _free_functions.cpp @ 0x0044e1f6
void Unwind_0044e230(void); // from _free_functions.cpp @ 0x0044e230
void Unwind_0044e23c(void); // from _free_functions.cpp @ 0x0044e23c
void Unwind_0044e245(void); // from _free_functions.cpp @ 0x0044e245
void Unwind_0044e280(void); // from _free_functions.cpp @ 0x0044e280
void Unwind_0044e2b0(void); // from _free_functions.cpp @ 0x0044e2b0
void Unwind_0044e2f0(void); // from _free_functions.cpp @ 0x0044e2f0
void Unwind_0044e2f8(void); // from _free_functions.cpp @ 0x0044e2f8
void Unwind_0044e360(void); // from _free_functions.cpp @ 0x0044e360
void Unwind_0044e36f(void); // from _free_functions.cpp @ 0x0044e36f
void Unwind_0044e3b0(void); // from _free_functions.cpp @ 0x0044e3b0
void Unwind_0044e3f0(void); // from _free_functions.cpp @ 0x0044e3f0
void Unwind_0044e420(void); // from _free_functions.cpp @ 0x0044e420
void Unwind_0044e42a(void); // from _free_functions.cpp @ 0x0044e42a
void Unwind_0044e460(void); // from _free_functions.cpp @ 0x0044e460
void Unwind_0044e490(void); // from _free_functions.cpp @ 0x0044e490
void Unwind_0044e4bd(void); // from _free_functions.cpp @ 0x0044e4bd
void Unwind_0044e4f0(void); // from _free_functions.cpp @ 0x0044e4f0
void Unwind_0044e520(void); // from _free_functions.cpp @ 0x0044e520
void Unwind_0044e570(void); // from _free_functions.cpp @ 0x0044e570
void Unwind_0044e5b0(void); // from _free_functions.cpp @ 0x0044e5b0
void Unwind_0044e5b8(void); // from _free_functions.cpp @ 0x0044e5b8
void Unwind_0044e5f0(void); // from _free_functions.cpp @ 0x0044e5f0
void Unwind_0044e5f9(void); // from _free_functions.cpp @ 0x0044e5f9
void Unwind_0044e608(void); // from _free_functions.cpp @ 0x0044e608
void Unwind_0044e611(void); // from _free_functions.cpp @ 0x0044e611
void Unwind_0044e650(void); // from _free_functions.cpp @ 0x0044e650
void Unwind_0044e659(void); // from _free_functions.cpp @ 0x0044e659
void Unwind_0044e661(void); // from _free_functions.cpp @ 0x0044e661
void Unwind_0044e669(void); // from _free_functions.cpp @ 0x0044e669
void Unwind_0044e672(void); // from _free_functions.cpp @ 0x0044e672
void Unwind_0044e6b0(void); // from _free_functions.cpp @ 0x0044e6b0
void Unwind_0044e6bb(void); // from _free_functions.cpp @ 0x0044e6bb
void Unwind_0044e700(void); // from _free_functions.cpp @ 0x0044e700
void Unwind_0044e70b(void); // from _free_functions.cpp @ 0x0044e70b
void Unwind_0044e750(void); // from _free_functions.cpp @ 0x0044e750
void Unwind_0044e790(void); // from _free_functions.cpp @ 0x0044e790
void Unwind_0044e799(void); // from _free_functions.cpp @ 0x0044e799
void Unwind_0044e7d0(void); // from _free_functions.cpp @ 0x0044e7d0
void Unwind_0044e810(void); // from _free_functions.cpp @ 0x0044e810
void Unwind_0044e819(void); // from _free_functions.cpp @ 0x0044e819
void Unwind_0044e822(void); // from _free_functions.cpp @ 0x0044e822
void Unwind_0044e850(void); // from _free_functions.cpp @ 0x0044e850
void Unwind_0044e8a0(void); // from _free_functions.cpp @ 0x0044e8a0
void Unwind_0044e8a8(void); // from _free_functions.cpp @ 0x0044e8a8
void Unwind_0044e8e0(void); // from _free_functions.cpp @ 0x0044e8e0
void Unwind_0044e8e8(void); // from _free_functions.cpp @ 0x0044e8e8
void Unwind_0044e8f0(void); // from _free_functions.cpp @ 0x0044e8f0
void Unwind_0044e8f8(void); // from _free_functions.cpp @ 0x0044e8f8
void Unwind_0044e900(void); // from _free_functions.cpp @ 0x0044e900
void Unwind_0044e908(void); // from _free_functions.cpp @ 0x0044e908
void Unwind_0044e910(void); // from _free_functions.cpp @ 0x0044e910
void Unwind_0044e918(void); // from _free_functions.cpp @ 0x0044e918
void Unwind_0044e920(void); // from _free_functions.cpp @ 0x0044e920
void Unwind_0044e928(void); // from _free_functions.cpp @ 0x0044e928
void Unwind_0044e930(void); // from _free_functions.cpp @ 0x0044e930
void Unwind_0044e938(void); // from _free_functions.cpp @ 0x0044e938
void Unwind_0044e940(void); // from _free_functions.cpp @ 0x0044e940
void Unwind_0044e980(void); // from _free_functions.cpp @ 0x0044e980
void Unwind_0044e988(void); // from _free_functions.cpp @ 0x0044e988
void Unwind_0044e993(void); // from _free_functions.cpp @ 0x0044e993
void Unwind_0044e99e(void); // from _free_functions.cpp @ 0x0044e99e
void Unwind_0044e9a9(void); // from _free_functions.cpp @ 0x0044e9a9
void Unwind_0044e9b4(void); // from _free_functions.cpp @ 0x0044e9b4
void Unwind_0044e9bc(void); // from _free_functions.cpp @ 0x0044e9bc
void Unwind_0044e9c7(void); // from _free_functions.cpp @ 0x0044e9c7
void Unwind_0044e9d2(void); // from _free_functions.cpp @ 0x0044e9d2
void Unwind_0044e9da(void); // from _free_functions.cpp @ 0x0044e9da
void Unwind_0044e9e5(void); // from _free_functions.cpp @ 0x0044e9e5
void Unwind_0044e9ed(void); // from _free_functions.cpp @ 0x0044e9ed
void Unwind_0044e9f8(void); // from _free_functions.cpp @ 0x0044e9f8
void Unwind_0044ea00(void); // from _free_functions.cpp @ 0x0044ea00
void Unwind_0044ea0b(void); // from _free_functions.cpp @ 0x0044ea0b
void Unwind_0044ea13(void); // from _free_functions.cpp @ 0x0044ea13
void Unwind_0044ea1e(void); // from _free_functions.cpp @ 0x0044ea1e
void Unwind_0044ea26(void); // from _free_functions.cpp @ 0x0044ea26
void Unwind_0044ea31(void); // from _free_functions.cpp @ 0x0044ea31
void Unwind_0044ea3c(void); // from _free_functions.cpp @ 0x0044ea3c
void Unwind_0044ea47(void); // from _free_functions.cpp @ 0x0044ea47
void Unwind_0044ea4f(void); // from _free_functions.cpp @ 0x0044ea4f
void Unwind_0044ea5a(void); // from _free_functions.cpp @ 0x0044ea5a
void Unwind_0044ea62(void); // from _free_functions.cpp @ 0x0044ea62
void Unwind_0044ea6d(void); // from _free_functions.cpp @ 0x0044ea6d
void Unwind_0044ea78(void); // from _free_functions.cpp @ 0x0044ea78
void Unwind_0044ea83(void); // from _free_functions.cpp @ 0x0044ea83
void Unwind_0044ea8b(void); // from _free_functions.cpp @ 0x0044ea8b
void Unwind_0044ea96(void); // from _free_functions.cpp @ 0x0044ea96
void Unwind_0044eaa1(void); // from _free_functions.cpp @ 0x0044eaa1
void Unwind_0044eaa9(void); // from _free_functions.cpp @ 0x0044eaa9
void Unwind_0044eab1(void); // from _free_functions.cpp @ 0x0044eab1
void Unwind_0044eabc(void); // from _free_functions.cpp @ 0x0044eabc
void Unwind_0044eac7(void); // from _free_functions.cpp @ 0x0044eac7
void Unwind_0044ead2(void); // from _free_functions.cpp @ 0x0044ead2
void Unwind_0044eada(void); // from _free_functions.cpp @ 0x0044eada
void Unwind_0044eae5(void); // from _free_functions.cpp @ 0x0044eae5
void Unwind_0044eaed(void); // from _free_functions.cpp @ 0x0044eaed
void Unwind_0044eaf8(void); // from _free_functions.cpp @ 0x0044eaf8
void Unwind_0044eb03(void); // from _free_functions.cpp @ 0x0044eb03
void Unwind_0044eb0e(void); // from _free_functions.cpp @ 0x0044eb0e
void Unwind_0044eb19(void); // from _free_functions.cpp @ 0x0044eb19
void Unwind_0044eb21(void); // from _free_functions.cpp @ 0x0044eb21
void Unwind_0044eb2c(void); // from _free_functions.cpp @ 0x0044eb2c
void Unwind_0044eb37(void); // from _free_functions.cpp @ 0x0044eb37
void Unwind_0044eb42(void); // from _free_functions.cpp @ 0x0044eb42
void Unwind_0044eb4d(void); // from _free_functions.cpp @ 0x0044eb4d
void Unwind_0044eb58(void); // from _free_functions.cpp @ 0x0044eb58
void Unwind_0044eb60(void); // from _free_functions.cpp @ 0x0044eb60
void Unwind_0044eb6b(void); // from _free_functions.cpp @ 0x0044eb6b
void Unwind_0044eb76(void); // from _free_functions.cpp @ 0x0044eb76
void Unwind_0044eb7e(void); // from _free_functions.cpp @ 0x0044eb7e
void Unwind_0044eb89(void); // from _free_functions.cpp @ 0x0044eb89
void Unwind_0044eb94(void); // from _free_functions.cpp @ 0x0044eb94
void Unwind_0044eb9c(void); // from _free_functions.cpp @ 0x0044eb9c
void Unwind_0044eba7(void); // from _free_functions.cpp @ 0x0044eba7
void Unwind_0044ebaf(void); // from _free_functions.cpp @ 0x0044ebaf
void Unwind_0044ebb7(void); // from _free_functions.cpp @ 0x0044ebb7
void Unwind_0044ebbf(void); // from _free_functions.cpp @ 0x0044ebbf
void Unwind_0044ebca(void); // from _free_functions.cpp @ 0x0044ebca
void Unwind_0044ebd5(void); // from _free_functions.cpp @ 0x0044ebd5
void Unwind_0044ebdd(void); // from _free_functions.cpp @ 0x0044ebdd
void Unwind_0044ebe5(void); // from _free_functions.cpp @ 0x0044ebe5
void Unwind_0044ebf0(void); // from _free_functions.cpp @ 0x0044ebf0
void Unwind_0044ebfb(void); // from _free_functions.cpp @ 0x0044ebfb
void Unwind_0044ec03(void); // from _free_functions.cpp @ 0x0044ec03
void Unwind_0044ec0e(void); // from _free_functions.cpp @ 0x0044ec0e
void Unwind_0044ec16(void); // from _free_functions.cpp @ 0x0044ec16
void Unwind_0044ec21(void); // from _free_functions.cpp @ 0x0044ec21
void Unwind_0044ec29(void); // from _free_functions.cpp @ 0x0044ec29
void Unwind_0044ec31(void); // from _free_functions.cpp @ 0x0044ec31
void Unwind_0044ec3c(void); // from _free_functions.cpp @ 0x0044ec3c
void Unwind_0044ec80(void); // from _free_functions.cpp @ 0x0044ec80
void Unwind_0044ec8b(void); // from _free_functions.cpp @ 0x0044ec8b
void Unwind_0044ec96(void); // from _free_functions.cpp @ 0x0044ec96
void Unwind_0044ec9e(void); // from _free_functions.cpp @ 0x0044ec9e
void Unwind_0044eca9(void); // from _free_functions.cpp @ 0x0044eca9
void Unwind_0044ecb4(void); // from _free_functions.cpp @ 0x0044ecb4
void Unwind_0044ecbc(void); // from _free_functions.cpp @ 0x0044ecbc
void Unwind_0044ecc7(void); // from _free_functions.cpp @ 0x0044ecc7
void Unwind_0044ecd2(void); // from _free_functions.cpp @ 0x0044ecd2
void Unwind_0044ecda(void); // from _free_functions.cpp @ 0x0044ecda
void Unwind_0044ece5(void); // from _free_functions.cpp @ 0x0044ece5
void Unwind_0044eced(void); // from _free_functions.cpp @ 0x0044eced
void Unwind_0044ecf8(void); // from _free_functions.cpp @ 0x0044ecf8
void Unwind_0044ed00(void); // from _free_functions.cpp @ 0x0044ed00
void Unwind_0044ed0b(void); // from _free_functions.cpp @ 0x0044ed0b
void Unwind_0044ed16(void); // from _free_functions.cpp @ 0x0044ed16
void UpdateAllCreatureBrainInputs(void); // from _free_functions.cpp @ 0x00432ee0
void UpdateAllCreatureDriveThresholdStates(void); // from _free_functions.cpp @ 0x00432ce0
void UpdateAllCreaturePerceptionAndAttention(void); // from _free_functions.cpp @ 0x00433020
void UpdateCreatureNameComboHistory(void); // from _free_functions.cpp @ 0x00432360
void UpdateMainWindowTitleForSelectedCreature(void); // from _free_functions.cpp @ 0x00422720
void __fastcall UpdateSoundSystem(void); // from _free_functions.cpp @ 0x00440fc0
void UpdateViewAnchoredObjects(void); // from _free_functions.cpp @ 0x00413950
void __fastcall VirtualDispatchSlot0x34WithZeros(int *param_1); // from _free_functions.cpp @ 0x0043de70
void __fastcall VtableForwarder_ECX_0118(int *param_1); // from _free_functions.cpp @ 0x004402f3
void __fastcall VtableForwarder_ECX_011c(int *param_1); // from _free_functions.cpp @ 0x004402fb
undefined * Win32BuildCurrentUserSecurityDescriptor(void); // from _free_functions.cpp @ 0x00445b30
void __fastcall WorldTickPhase_UpdateCreatureBacteriaAndEnvironment(void); // from _free_functions.cpp @ 0x00433200
void WorldUpdateTimer_Dispatch(int *param_1); // from _free_functions.cpp @ 0x00434ed0
void WorldUpdateTimer_DispatchWithState(int *param_1); // from _free_functions.cpp @ 0x00434ef0
C1Bool32 __fastcall WrappedWorldRectsOverlap(WorldRect *first_rect,WorldRect *second_rect); // from _free_functions.cpp @ 0x00417b40
C1Bool32 WriteDibRectToFile(BITMAPINFOHEADER *dib_info,byte *dib_pixels,RECT *source_rect,
                           char *output_path); // from _free_functions.cpp @ 0x00445900
undefined4 * XmlEscapeCopy(C1MsvcString *output,C1MsvcString *input); // from _free_functions.cpp @ 0x004390a0
void __stdcall __ArrayUnwind(void *array,uint element_size,uint element_count,C1MsvcVectorThiscallCallback destructor); // from _free_functions.cpp @ 0x0044a868
extern "C" void __cdecl __Init_thread_abort(int *guard_state); // from _free_functions.cpp @ 0x0044ad4b
extern "C" void __cdecl __Init_thread_footer(int *param_1); // from _free_functions.cpp @ 0x0044ad79
extern "C" void __cdecl __Init_thread_header(int *param_1); // from _free_functions.cpp @ 0x0044adca
extern "C" void __Init_thread_wait_v2(void); // from _free_functions.cpp @ 0x0044ae1c
void __cdecl __SEH_prolog4(undefined4 param_1,int param_2); // from _free_functions.cpp @ 0x0044b0e0
undefined4 ___scrt_acquire_startup_lock(void); // from _free_functions.cpp @ 0x0044ab3a
int ___scrt_common_main_seh(void); // from _free_functions.cpp @ 0x0044af4e
int __cdecl ___scrt_initialize_crt(int param_1); // from _free_functions.cpp @ 0x0044ab6c
undefined4 __cdecl ___scrt_initialize_onexit_tables(int param_1); // from _free_functions.cpp @ 0x0044aba5
bool ___scrt_is_ucrt_dll_in_use(void); // from _free_functions.cpp @ 0x0044b47d
int __cdecl ___scrt_release_startup_lock(char param_1); // from _free_functions.cpp @ 0x0044acc3
undefined1 __cdecl ___scrt_uninitialize_crt(undefined4 param_1,char param_2); // from _free_functions.cpp @ 0x0044ace0
void __cdecl ___security_init_cookie(void); // from _free_functions.cpp @ 0x0044b6cf
void __stdcall __ehvec_dtor(void *array,uint element_size,uint element_count,C1MsvcVectorThiscallCallback destructor); // from _free_functions.cpp @ 0x0044a7e0
void __ehvec_dtor_unwind(void); // from _free_functions.cpp @ 0x0044a858
void __cdecl __except_handler4(int *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4); // from _free_functions.cpp @ 0x0044b130
int __cdecl __filter_x86_sse2_floating_point_exception(int param_1); // from _free_functions.cpp @ 0x0044bc79
extern "C" C1AtexitCallback __onexit(C1AtexitCallback param_1); // from _free_functions.cpp @ 0x0044ad08
undefined * __scrt_get_dyn_tls_dtor_callback(void); // from _free_functions.cpp @ 0x0044b785
undefined * __scrt_get_dyn_tls_init_callback(void); // from _free_functions.cpp @ 0x0044b77f
extern "C" int _atexit(C1AtexitCallback param_1); // from _free_functions.cpp @ 0x0044ad36
void __stdcall _eh_vector_constructor_iterator_(void *array,uint element_size,uint element_count,C1MsvcVectorThiscallCallback constructor,C1MsvcVectorThiscallCallback destructor); // from _free_functions.cpp @ 0x0044a76c
void entry(void); // from _free_functions.cpp @ 0x0044b0cd
void __cdecl guard_check_icall(void); // from _free_functions.cpp @ 0x004019d0
