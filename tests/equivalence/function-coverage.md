# Function coverage of the equivalence tests

Snapshot of 2026-11-10: `run_all.sh --coverage` (quick case counts) and then
`run_world.sh` on the four-norn world (10 ticks), on the CE 10.3 exe.

A function is covered when a test ran it as the original's code, inside a call whose
result the test compares with LibreCreatures.

| Owner | Functions | Covered | Share |
| --- | ---: | ---: | ---: |
| game | 924 | 333 | 36.0% |
| toolchain | 955 | 35 | 3.7% |
| sdk | 453 | 13 | 2.9% |

## Game functions by category

| Category | Functions | Covered | Share |
| --- | ---: | ---: | ---: |
| class_metadata | 93 | 56 | 60.2% |
| class_method | 655 | 230 | 35.1% |
| platform_or_application_glue | 27 | 1 | 3.7% |
| startup_registration | 31 | 17 | 54.8% |
| subsystem_free_function | 118 | 29 | 24.6% |

## Game functions by LibreCreatures source file

| Source | Functions | Covered | Share |
| --- | ---: | ---: | ---: |
| application/application.cpp | 105 | 2 | 1.9% |
| creatures/creature.cpp | 57 | 36 | 63.2% |
| objects/object.cpp | 38 | 17 | 44.7% |
| objects/simple_object.cpp | 37 | 25 | 67.6% |
| creatures/skeleton.cpp | 33 | 23 | 69.7% |
| objects/compound_object.cpp | 31 | 17 | 54.8% |
| ui/views.cpp | 29 | 1 | 3.4% |
| display/rendering.cpp | 26 | 1 | 3.8% |
| scripting/macro.cpp | 24 | 12 | 50.0% |
| application/document.cpp | 22 | 3 | 13.6% |
| scripting/pipe_server.cpp | 20 | 1 | 5.0% |
| application/main_frame.cpp | 19 | 2 | 10.5% |
| application/sfc_ole.cpp | 18 | 1 | 5.6% |
| creatures/genome.cpp | 17 | 6 | 35.3% |
| scripting/dde.cpp | 17 | 5 | 29.4% |
| sound/sound.cpp | 17 | 0 | 0.0% |
| ui/caos_console.cpp | 16 | 0 | 0.0% |
| creatures/body.cpp | 15 | 13 | 86.7% |
| objects/lift.cpp | 15 | 14 | 93.3% |
| ui/debug_console.cpp | 14 | 0 | 0.0% |
| scripting/macro_holder.cpp | 13 | 0 | 0.0% |
| objects/entity.cpp | 12 | 10 | 83.3% |
| ui/event_bar.cpp | 12 | 2 | 16.7% |
| ui/tip_dialog.cpp | 11 | 0 | 0.0% |
| ui/tools.cpp | 11 | 7 | 63.6% |
| brain/blackboard.cpp | 10 | 9 | 90.0% |
| brain/brain.cpp | 10 | 9 | 90.0% |
| creatures/voice.cpp | 10 | 6 | 60.0% |
| display/image.cpp | 10 | 7 | 70.0% |
| objects/bubble.cpp | 10 | 1 | 10.0% |
| biochemistry/biochemistry.cpp | 9 | 8 | 88.9% |
| creatures/registry.cpp | 9 | 4 | 44.4% |
| display/gallery.cpp | 9 | 8 | 88.9% |
| objects/vehicle.cpp | 9 | 6 | 66.7% |
| creatures/bacterium.cpp | 8 | 3 | 37.5% |
| creatures/events.cpp | 8 | 8 | 100.0% |
| ui/eye_view.cpp | 8 | 0 | 0.0% |
| application/embedded_kits.cpp | 7 | 0 | 0.0% |
| brain/lobe.cpp | 7 | 5 | 71.4% |
| objects/call_button.cpp | 7 | 6 | 85.7% |
| ui/place_dialog.cpp | 7 | 0 | 0.0% |
| ui/toolbars.cpp | 7 | 2 | 28.6% |
| world/map.cpp | 7 | 7 | 100.0% |
| display/palette.cpp | 6 | 3 | 50.0% |
| ui/world_statistics.cpp | 6 | 0 | 0.0% |
| brain/instinct.cpp | 5 | 2 | 40.0% |
| display/sprite_cache.cpp | 5 | 4 | 80.0% |
| objects/scenery.cpp | 5 | 4 | 80.0% |
| ui/creature_selection.cpp | 5 | 1 | 20.0% |
| ui/dialogs.cpp | 5 | 0 | 0.0% |
| world/places.cpp | 5 | 2 | 40.0% |
| creatures/owner.cpp | 4 | 1 | 25.0% |
| display/bitmap.cpp | 4 | 0 | 0.0% |
| objects/events.cpp | 4 | 4 | 100.0% |
| scripting/classifier_scripts.cpp | 4 | 4 | 100.0% |
| ui/score.cpp | 4 | 2 | 50.0% |
| world/viewport.cpp | 4 | 0 | 0.0% |
| application/resources.cpp | 3 | 1 | 33.3% |
| archive/archive.cpp | 3 | 2 | 66.7% |
| brain/classifiers.cpp | 3 | 1 | 33.3% |
| creatures/update.cpp | 3 | 3 | 100.0% |
| platform/formatting.cpp | 3 | 0 | 0.0% |
| platform/registry.cpp | 3 | 1 | 33.3% |
| ui/version_dialog.cpp | 3 | 0 | 0.0% |
| ui/volume_dialog.cpp | 3 | 0 | 0.0% |
| world/geometry.cpp | 3 | 1 | 33.3% |
| world/update_timer.cpp | 3 | 0 | 0.0% |
| application/file_commands.cpp | 2 | 0 | 0.0% |
| archive/backup.cpp | 2 | 0 | 0.0% |
| brain/neuron.cpp | 2 | 1 | 50.0% |
| brain/rules.cpp | 2 | 2 | 100.0% |
| common/filesystem.cpp | 2 | 0 | 0.0% |
| common/logging.cpp | 2 | 0 | 0.0% |
| objects/lifecycle.cpp | 2 | 0 | 0.0% |
| objects/renderable_set.cpp | 2 | 1 | 50.0% |
| platform/mfc_adapters.cpp | 2 | 0 | 0.0% |
| ui/windows.cpp | 2 | 0 | 0.0% |
| world/settings.cpp | 2 | 1 | 50.0% |
| world/tick.cpp | 2 | 0 | 0.0% |
| application/commands.cpp | 1 | 0 | 0.0% |
| application/file_dialog.cpp | 1 | 0 | 0.0% |
| application/kit_processes.cpp | 1 | 0 | 0.0% |
| archive/funeral_kit.cpp | 1 | 0 | 0.0% |
| brain/locus_resolver.cpp | 1 | 1 | 100.0% |
| common/time.cpp | 1 | 1 | 100.0% |
| creatures/attention.cpp | 1 | 1 | 100.0% |
| creatures/learned_words.cpp | 1 | 1 | 100.0% |
| display/blit.cpp | 1 | 0 | 0.0% |
| display/font.cpp | 1 | 0 | 0.0% |
| display/profiler.cpp | 1 | 0 | 0.0% |
| objects/debug.cpp | 1 | 0 | 0.0% |
| platform/com.cpp | 1 | 0 | 0.0% |
| platform/environment.cpp | 1 | 0 | 0.0% |
| platform/gdi.cpp | 1 | 0 | 0.0% |
| platform/security.cpp | 1 | 0 | 0.0% |
| scripting/tables.cpp | 1 | 1 | 100.0% |
| sound/cache.cpp | 1 | 0 | 0.0% |
| ui/classifier_tip.cpp | 1 | 0 | 0.0% |
| ui/magic_profiler.cpp | 1 | 0 | 0.0% |
| ui/main_window.cpp | 1 | 0 | 0.0% |
| ui/menus.cpp | 1 | 0 | 0.0% |
| ui/system_info.cpp | 1 | 0 | 0.0% |

## Game functions each test reaches

| Test | Game functions run |
| --- | ---: |
| world_roundtrip | 234 |
| vehicle | 30 |
| skelgenes | 26 |
| simple_object | 20 |
| skeleton | 18 |
| heard | 11 |
| fanout | 11 |
| words | 9 |
| dream | 8 |
| blackboard | 6 |
| braingenes | 6 |
| perception | 5 |
| voice | 5 |
| brain | 4 |
| goal | 4 |
| geometry | 4 |
| pointer_tool | 4 |
| object | 4 |
| breed | 3 |
| action | 2 |
| attention | 2 |
| life | 2 |
| bacterium | 2 |
| express | 2 |
| biogenes | 2 |
| biochemistry | 1 |
| stimulus | 1 |
| rvalue | 1 |
| locus | 1 |
| status | 1 |
| initialize | 1 |

## Game functions no test runs, by source file

### application/application.cpp (103 of 105)

- `004303e0` CApplication_GetThisMessageMap (class_metadata)
- `004303f0` CApplication_GetMfcMapRecordA (class_metadata)
- `00430400` CApplication_GetMfcMapRecordB (class_metadata)
- `00431d20` CApplication::OnExportCurrentCreature (class_method)
- `00431fb0` CApplication::OnImportCreature (class_method)
- `00432140` CApplication::ToggleEyeView (class_method)
- `004322d0` CApplication::OnUpdateToggleEyeView (class_method)
- `00432360` CApplication::UpdateCreatureNameComboHistory (class_method)
- `004324c0` CApplication::TriggerSelectedCreatureScriptEvent (class_method)
- `004333b0` CApplication::ToggleMuteAndStopSounds (class_method)
- `00433440` CApplication::ToggleBurbleSetting_ViaGlobalAppPointer (class_method)
- `00433480` CApplication::OnUpdateToggleBurbleSetting_ViaGlobalAppPointer (class_method)
- `004334c0` CApplication::ShowVolumeDialog (class_method)
- `004335f0` CApplication::OpenWebpageUrlShortcut (class_method)
- `004337e0` CApplication::ArmWorldUpdateTimer (class_method)
- `00433a10` CApplication::OnSelectCreatureByMenuIndex_00 (class_method)
- `00433a20` CApplication::OnSelectCreatureByMenuIndex_01 (class_method)
- `00433a30` CApplication::OnSelectCreatureByMenuIndex_02 (class_method)
- `00433a40` CApplication::OnSelectCreatureByMenuIndex_03 (class_method)
- `00433a50` CApplication::OnSelectCreatureByMenuIndex_04 (class_method)
- `00433a60` CApplication::OnSelectCreatureByMenuIndex_05 (class_method)
- `00433a70` CApplication::OnSelectCreatureByMenuIndex_06 (class_method)
- `00433a80` CApplication::OnSelectCreatureByMenuIndex_07 (class_method)
- `00433a90` CApplication::OnSelectCreatureByMenuIndex_08 (class_method)
- `00433aa0` CApplication::OnSelectCreatureByMenuIndex_09 (class_method)
- `00433ab0` CApplication::OnSelectCreatureByMenuIndex_10 (class_method)
- `00433ac0` CApplication::OnSelectCreatureByMenuIndex_11 (class_method)
- `00433ad0` CApplication::OnSelectCreatureByMenuIndex_12 (class_method)
- `00433ae0` CApplication::OnSelectCreatureByMenuIndex_13 (class_method)
- `00433af0` CApplication::OnSelectCreatureByMenuIndex_14 (class_method)
- `00433b00` CApplication::OnSelectCreatureByMenuIndex_15 (class_method)
- `00433b10` CApplication::OnSelectCreatureByMenuIndex_16 (class_method)
- `00433b20` CApplication::OnSelectCreatureByMenuIndex_17 (class_method)
- `00433b30` CApplication::OnSelectCreatureByMenuIndex_18 (class_method)
- `00433b40` CApplication::OnSelectCreatureByMenuIndex_19 (class_method)
- `00433b50` CApplication::OnSelectCreatureByMenuIndex_20 (class_method)
- `00433b60` CApplication::OnSelectCreatureByMenuIndex_21 (class_method)
- `00433b70` CApplication::OnSelectCreatureByMenuIndex_22 (class_method)
- `00433b80` CApplication::OnSelectCreatureByMenuIndex_23 (class_method)
- `00433b90` CApplication::OnSelectCreatureByMenuIndex_24 (class_method)
- `00433ba0` CApplication::OnSelectCreatureByMenuIndex_25 (class_method)
- `00433bb0` CApplication::OnSelectCreatureByMenuIndex_26 (class_method)
- `00433bc0` CApplication::OnSelectCreatureByMenuIndex_27 (class_method)
- `00433bd0` CApplication::OnSelectCreatureByMenuIndex_28 (class_method)
- `00433be0` CApplication::OnSelectCreatureByMenuIndex_29 (class_method)
- `00433bf0` CApplication::OnSelectCreatureByMenuIndex_30 (class_method)
- `00433c00` CApplication::OnUpdateSelectCreatureByMenuIndex_00 (class_method)
- `00433c60` CApplication::OnUpdateSelectCreatureByMenuIndex_01 (class_method)
- `00433cc0` CApplication::OnUpdateSelectCreatureByMenuIndex_02 (class_method)
- `00433d20` CApplication::OnUpdateSelectCreatureByMenuIndex_03 (class_method)
- `00433d80` CApplication::OnUpdateSelectCreatureByMenuIndex_04 (class_method)
- `00433de0` CApplication::OnUpdateSelectCreatureByMenuIndex_05 (class_method)
- `00433e40` CApplication::OnUpdateSelectCreatureByMenuIndex_06 (class_method)
- `00433ea0` CApplication::OnUpdateSelectCreatureByMenuIndex_07 (class_method)
- `00433f00` CApplication::OnUpdateSelectCreatureByMenuIndex_08 (class_method)
- `00433f60` CApplication::OnUpdateSelectCreatureByMenuIndex_09 (class_method)
- `00433fc0` CApplication::OnUpdateSelectCreatureByMenuIndex_10 (class_method)
- `00434020` CApplication::OnUpdateSelectCreatureByMenuIndex_11 (class_method)
- `00434080` CApplication::OnUpdateSelectCreatureByMenuIndex_12 (class_method)
- `004340e0` CApplication::OnUpdateSelectCreatureByMenuIndex_13 (class_method)
- `00434140` CApplication::OnUpdateSelectCreatureByMenuIndex_14 (class_method)
- `004341a0` CApplication::OnUpdateSelectCreatureByMenuIndex_15 (class_method)
- `00434200` CApplication::OnUpdateSelectCreatureByMenuIndex_16 (class_method)
- `00434260` CApplication::OnUpdateSelectCreatureByMenuIndex_17 (class_method)
- `004342c0` CApplication::OnUpdateSelectCreatureByMenuIndex_18 (class_method)
- `00434320` CApplication::OnUpdateSelectCreatureByMenuIndex_19 (class_method)
- `00434380` CApplication::OnUpdateSelectCreatureByMenuIndex_20 (class_method)
- `004343e0` CApplication::OnUpdateSelectCreatureByMenuIndex_21 (class_method)
- `00434440` CApplication::OnUpdateSelectCreatureByMenuIndex_22 (class_method)
- `004344a0` CApplication::OnUpdateSelectCreatureByMenuIndex_23 (class_method)
- `00434500` CApplication::OnUpdateSelectCreatureByMenuIndex_24 (class_method)
- `00434560` CApplication::OnUpdateSelectCreatureByMenuIndex_25 (class_method)
- `004345c0` CApplication::OnUpdateSelectCreatureByMenuIndex_26 (class_method)
- `00434620` CApplication::OnUpdateSelectCreatureByMenuIndex_27 (class_method)
- `00434680` CApplication::OnUpdateSelectCreatureByMenuIndex_28 (class_method)
- `004346e0` CApplication::OnUpdateSelectCreatureByMenuIndex_29 (class_method)
- `00434740` CApplication::OnUpdateSelectCreatureByMenuIndex_30 (class_method)
- `004347a0` CApplication::CreateAndSelectGeneratedMaleCreature (class_method)
- `00434830` CApplication::CreateAndSelectGeneratedFemaleCreature (class_method)
- `004348c0` CApplication::InfectSelectedCreatureWithRandomBacterium (class_method)
- `00434920` CApplication::EuthaniseSelectedCreature (class_method)
- `00434a10` CApplication::ToggleDebugLogCategory (class_method)
- `00434a30` CApplication::OnUpdateToggleDebugLogCategory (class_method)
- `00434a70` CApplication::ToggleDebugLoggingEnabled (class_method)
- `00434a90` CApplication::OnUpdateToggleDebugLoggingEnabled (class_method)
- `00434ac0` CApplication::EnsureDebugConsoleDialog (class_method)
- `00434ce0` CApplication::OnUpdateShowDebugConsole (class_method)
- `00434d20` CApplication::ShowCAOSConsoleDialog (class_method)
- `00434ed0` CApplication::WorldUpdateTimer_Dispatch (class_metadata)
- `00435490` CApplication::ForceAgeSelectedCreatureOneStage (class_method)
- `00435b20` CApplication::ShowRemoveFavouritePlaceDialog (class_method)
- `00435ce0` CApplication::SelectNextCreature (class_method)
- `00435e00` CApplication::SelectPreviousCreature (class_method)
- `0043de90` SFCApp::CreateObject (class_method)
- `0043df10` SFCApp::GetThisClass (class_metadata)
- `0043e1d0` SFCApp::InitInstance (class_method)
- `0043f360` SFCApp::OnIdle (class_method)
- `0043f440` SFCApp::ToggleBurbleSetting (class_method)
- `0043f480` SFCApp::OnUpdateToggleBurbleSetting (class_method)
- `0043f4b0` SFCApp::ExitInstance (class_method)
- `0043f4f0` SFCApp::OnFileOpen (class_method)
- `0043f550` SFCApp::OnFileNew (class_method)
- `0044ef70` DestroySfcAppGlobalInstance (startup_registration)

### application/commands.cpp (1 of 1)

- `00420eb0` DispatchMainFrameCommand (subsystem_free_function)

### application/document.cpp (19 of 22)

- `004303d0` SFCDoc::GetThisClass (class_method)
- `00430410` SFCDoc::~SFCDoc (class_method)
- `004304c0` SFCDoc::OnNewDocument (class_method)
- `004309a0` SFCDoc::OnOpenDocument (class_method)
- `00430cd0` SFCDoc::DeleteContents (class_method)
- `00430e90` SFCDoc::OnCloseDocument (class_method)
- `00430f10` SFCDoc::OnSaveDocument (class_method)
- `00432320` SFCDoc::GoToFirstFavouritePlace (class_method)
- `00433340` SFCDoc::ToggleInformativeSelectionMenuAndRebuild (class_method)
- `00433380` SFCDoc::OnUpdateToggleInformativeSelectionMenu (class_method)
- `00433410` SFCDoc::OnUpdateToggleMute (class_method)
- `004336d0` SFCDoc::ServiceWorldUpdateTimer (class_method)
- `00434f30` SFCDoc::AddFavouritePlaceFromCurrentViewport (class_method)
- `00435320` SFCDoc::GoToSecondFavouritePlace (class_method)
- `00435360` SFCDoc::GoToThirdFavouritePlace (class_method)
- `004353a0` SFCDoc::GoToFourthFavouritePlace (class_method)
- `004353e0` SFCDoc::GoToFifthFavouritePlace (class_method)
- `00435420` SFCDoc::GoToSixthFavouritePlace (class_method)
- `004361b0` CDocument_GetDocumentAdapter (class_method)

### application/embedded_kits.cpp (7 of 7)

- `0040e2d0` NotifyEmbeddedKit9OfCreatureDeath (subsystem_free_function)
- `004215f0` SetEmbeddedKitToolAvailabilityByName (subsystem_free_function)
- `0042d860` SendYourIdIsMessageToEmbeddedKit (platform_or_application_glue)
- `004440e0` PopulateEmbeddedKitMenuAndToolbarFromRegistry (platform_or_application_glue)
- `004444e0` ExecuteEmbeddedKitTool (platform_or_application_glue)
- `004449e0` BroadcastEmbeddedControlState (platform_or_application_glue)
- `00444a70` ShutdownEmbeddedKitTool (platform_or_application_glue)

### application/file_commands.cpp (2 of 2)

- `00434950` SaveActiveDocumentAndResumeWorldTimer (subsystem_free_function)
- `004349b0` SaveActiveDocumentAsAndResumeWorldTimer (subsystem_free_function)

### application/file_dialog.cpp (1 of 1)

- `00417d70` PromptForExpFilePath (subsystem_free_function)

### application/kit_processes.cpp (1 of 1)

- `00444b50` TerminateLaunchedKitProcesses (subsystem_free_function)

### application/main_frame.cpp (17 of 19)

- `00421060` CMainFrame::GetRuntimeClass (class_metadata)
- `00421070` CMainFrame::GetThisMessageMap (class_metadata)
- `00421080` CMainFrame::~CMainFrame (class_method)
- `004211b0` CMainFrame::OnCreate (class_method)
- `004214a0` CMainFrame::PreCreateWindow (class_method)
- `004215b0` CMainFrame::GetMessageString (class_method)
- `00421670` CMainFrame::ToggleEmbeddedKitToolForCommand (class_method)
- `004216d0` CMainFrame::OnUpdateEmbeddedKitToolCommand (class_method)
- `004218a0` CMainFrame::OnQueryNewPalette (class_method)
- `004218c0` CMainFrame::OnPaletteChanged (class_method)
- `00421900` CMainFrame::OnClose (class_method)
- `004219b0` CMainFrame::OnGetMinMaxInfo (class_method)
- `00421a00` CMainFrame::OnTimer (class_method)
- `00421a10` CMainFrame::OnShutdownEmbeddedKitTool (class_method)
- `00421a30` CMainFrame::OnPipeServerCommand (class_method)
- `00422040` CMainFrame::OnAgeSelectedCreatureAndSeedWords (class_method)
- `00422180` CMainFrame::ActivateFrame (class_method)

### application/resources.cpp (2 of 3)

- `00412280` CopyPrimaryResourceDirectoryPath (subsystem_free_function)
- `0044ef10` CloseResourceRegistryKeys (startup_registration)

### application/sfc_ole.cpp (17 of 18)

- `0042f880` CSfcOLE::CreateObject (class_metadata)
- `0042f930` CSfcOLE::GetRuntimeClass (class_metadata)
- `0042f940` CSfcOLE_GetAutomationMapRecordA (class_metadata)
- `0042f950` CSfcOLE_GetAutomationMapRecordB (class_metadata)
- `0042f960` CSfcOLE_GetAutomationDispatchMap (class_metadata)
- `0042f970` CSfcOLE::~CSfcOLE_Deleting (class_method)
- `0042fa30` CSfcOLE::CreateMacro (class_method)
- `0042fb00` CSfcOLE::DestroyMacro (class_method)
- `0042fb60` CSfcOLE::LoadMacro (class_method)
- `0042fbf0` CSfcOLE::CSfcOLE::RequestMacro (class_method)
- `0042fc90` CSfcOLE::CSfcOLE::ExecuteMacro (class_method)
- `0042fd60` CSfcOLE::CreateCommand (class_method)
- `0042fe30` CSfcOLE::DestroyCommand (class_method)
- `0042fea0` CSfcOLE::LoadCommand (class_method)
- `0042ffa0` CSfcOLE::RequestCommand (class_method)
- `004300d0` CSfcOLE::FireCommand (class_method)
- `0044ee40` AtExit_DestroyOleObjectFactory (startup_registration)

### archive/archive.cpp (1 of 3)

- `00406cc0` ArchiveScratchBuffer_FreeIfHeap (subsystem_free_function)

### archive/backup.cpp (2 of 2)

- `004357e0` RefreshTemporaryWorldBackup (subsystem_free_function)
- `004359b0` PromoteTemporaryWorldBackup (subsystem_free_function)

### archive/funeral_kit.cpp (1 of 1)

- `00435c10` FlushFuneralKitDocumentStateWords (subsystem_free_function)

### biochemistry/biochemistry.cpp (1 of 9)

- `0042dba0` CBiochemistry::~CBiochemistry (class_method)

### brain/blackboard.cpp (1 of 10)

- `0042cb00` Blackboard::FinalizeTextInput (class_method)

### brain/brain.cpp (1 of 10)

- `00404a90` CBrain::FormatActivityReport (class_method)

### brain/classifiers.cpp (2 of 3)

- `00439310` ResolveClassifierDisplayName (subsystem_free_function)
- `0044ef40` DestroyClassifierNameMap (startup_registration)

### brain/instinct.cpp (3 of 5)

- `00406f10` AllocateCInstinct (class_metadata)
- `00406f30` CInstinct::GetRuntimeClass (class_metadata)
- `00407070` CInstinct::Serialize (class_method)

### brain/lobe.cpp (2 of 7)

- `00402580` CLobe::~CLobe (class_method)
- `004025c0` CLobe::DispatchTableSlot0 (class_method)

### brain/neuron.cpp (1 of 2)

- `00406220` CLobeNeuron::FormatConnectionDebugDescription (class_method)

### common/filesystem.cpp (2 of 2)

- `004354f0` RemoveFirstMatchingFile (subsystem_free_function)
- `00435680` CopyFilesMatchingPattern (subsystem_free_function)

### common/logging.cpp (2 of 2)

- `00418000` DebugLog (subsystem_free_function)
- `0044ee30` AtExit_CloseCrashReportFile (startup_registration)

### creatures/bacterium.cpp (5 of 8)

- `00401900` CBacterium::CreateObject (class_method)
- `00401940` CBacterium::GetRuntimeClass (class_method)
- `004019a0` CBacterium::~CBacterium_Deleting (class_method)
- `00401c70` CBacterium::ReplicateAndMutate (class_method)
- `00401d80` CBacterium::Update (class_method)

### creatures/body.cpp (2 of 15)

- `00414e60` CreateBodyPart (subsystem_free_function)
- `00414ed0` BodyPart::GetRuntimeClass (class_metadata)

### creatures/creature.cpp (21 of 57)

- `00408420` Creature::NotifyDependentsOnRemoval (class_method)
- `004095c0` Creature::GetClickEventIdAtWorldPosition (class_method)
- `00409880` Creature::HandleQueuedEventSlot5 (class_method)
- `004098a0` Creature::HandleQueuedEventSlot6 (class_method)
- `004098c0` Creature::HandleQueuedEventSlot7 (class_method)
- `004098e0` Creature::HandlePickupEvent (class_method)
- `00409ae0` Creature::HandleDropEvent (class_method)
- `00409ce0` Creature::UpdateUnboundedWorldPosition (class_method)
- `00409da0` Creature::HandleHeardWordsEvent (class_method)
- `00409e00` Creature::HandleWordLearningEvent (class_method)
- `0040b5f0` Creature::Speak (class_method)
- `0040cbc0` Creature::ProcessInsemination (class_method)
- `0040d580` Creature::Creature (class_method)
- `0040d930` Creature::InitializeFromGenome (class_method)
- `0040da20` Creature::QueueEvent8AfterBoundsUpdate (class_method)
- `0040da80` Creature::SetSleepIndicator (class_method)
- `0040dc30` Creature::Die (class_method)
- `0040dda0` Creature::Deserialize (class_method)
- `0040e0d0` Creature::RemoveFromWorld (class_method)
- `0040e4d0` Creature::ReferencesObject (class_method)
- `0040edc0` Creature::ClearReferencesToObject (class_method)

### creatures/genome.cpp (11 of 17)

- `0040e350` BuildCurrentCreatureGenomeGeneCounts (subsystem_free_function)
- `00418330` CGenome::CreateObject (class_method)
- `00418380` CGenome::GetRuntimeClass (class_method)
- `00418390` CGenome::~CGenome_Deleting (class_method)
- `004183e0` CGenome::~CGenome (class_method)
- `00418400` CGenome::CGenome (class_method)
- `004185c0` CGenome::Serialize (class_method)
- `004187f0` GenerateOffspringGenomeFile (subsystem_free_function)
- `00418950` SaveGeneratedGenomeFile (subsystem_free_function)
- `00418e90` GenerateUniqueGenomeFilename (subsystem_free_function)
- `00418f70` CGenome::CountMatchingGenes (class_method)

### creatures/owner.cpp (3 of 4)

- `0042d890` COwner::CreateObject (class_method)
- `0042d9b0` GetRuntimeClass (class_metadata)
- `0042da60` COwner::Serialize (class_method)

### creatures/registry.cpp (5 of 9)

- `004067c0` CCreatureRegister::CreateObject (class_metadata)
- `00406830` CCreatureRegister::GetRuntimeClass (class_metadata)
- `004069f0` CCreatureRegister::~CCreatureRegister (class_method)
- `00408270` RemoveCreatureFromRegistry (subsystem_free_function)
- `0044eec0` AtExit_DestroyCreatureRegistry (startup_registration)

### creatures/skeleton.cpp (10 of 33)

- `00406d20` Skeleton::GetSpriteBoundsWidth (class_method)
- `00406d30` Skeleton::GetSpriteBoundsHeight (class_method)
- `00406dc0` Skeleton::SelectTargetPoseForMotionGuarded (class_method)
- `00408080` Skeleton::ValidateBodySprites (class_method)
- `0043aa80` Skeleton::CreateObject (class_metadata)
- `0043abd0` Skeleton::~Skeleton (class_method)
- `0043beb0` Skeleton::ParseAnimationSequence (class_method)
- `0043bf00` Skeleton::IsAnimationSequenceComplete (class_method)
- `0043dcd0` Skeleton::ReferencesObject (class_method)
- `0043dd00` Skeleton::ClearReferencesToObject (class_method)

### creatures/voice.cpp (4 of 10)

- `00444fc0` CreateVoice (subsystem_free_function)
- `00445030` Voice::GetRuntimeClass (class_method)
- `004450f0` Voice::~Voice_Deleting (class_method)
- `004454e0` Voice::LoadVoiceFile (class_method)

### display/bitmap.cpp (4 of 4)

- `00445810` CWinBMP::CreateObject (class_metadata)
- `00445890` CWinBMP_GetRuntimeClassMetadata (class_metadata)
- `004458f0` CWinBMP::GetRuntimeClass (class_metadata)
- `00445900` WriteDibRectToFile (subsystem_free_function)

### display/blit.cpp (1 of 1)

- `00401f40` BlitZeroTransparent8BitPixels (subsystem_free_function)

### display/font.cpp (1 of 1)

- `00442eb0` LoadCharsetData (subsystem_free_function)

### display/gallery.cpp (1 of 9)

- `0044ef90` AtExit_DestroyGalleryRegistry (startup_registration)

### display/image.cpp (3 of 10)

- `00441b20` AllocateAndConstructCImage (class_metadata)
- `00441b80` CImage::GetRuntimeClass (class_metadata)
- `00442b40` CImage::BlitToDIB (class_method)

### display/palette.cpp (3 of 6)

- `00413b50` LoadPaletteDtaIntoBuffer (subsystem_free_function)
- `00430b10` InitializeGamePalette (subsystem_free_function)
- `0044ef00` AtExit_DeleteGamePaletteHandle (startup_registration)

### display/profiler.cpp (1 of 1)

- `004390a0` XmlEscapeCopy (subsystem_free_function)

### display/rendering.cpp (25 of 26)

- `004122b0` CWorldRenderer::CWorldRenderer (class_method)
- `00412490` CWorldRenderer::RedrawFullView (class_method)
- `00412510` CWorldRenderer::ResizeBackBufferForViewport (class_method)
- `00412590` CWorldRenderer::RealizePalette (class_method)
- `00412610` CWorldRenderer::QueueDirtyWorldRect (class_method)
- `004127a0` CWorldRenderer::PresentWorldRect (class_method)
- `00412830` CWorldRenderer::RenderAndPresentRect (class_method)
- `00412aa0` CWorldRenderer::RenderWorldRectToDIB (class_method)
- `00412f30` CWorldRenderer::ScrollViewport (class_method)
- `00413090` IsSelectedCreatureWithinViewportSafeArea (class_method)
- `004131a0` CWorldRenderer::RequestViewportOrigin (class_method)
- `004132a0` CWorldRenderer::RequestViewportOriginForSelectedCreature (class_method)
- `00413810` CWorldRenderer::SetViewportOrigin (class_method)
- `00413a20` CWorldRenderer::CreateBackBufferDIB (class_method)
- `00413aa0` CWorldRenderer::UpdateDIBPalette (class_method)
- `004140d0` InsertionSortVisibleSpriteRecordsByPlane (subsystem_free_function)
- `00414190` StableSortVisibleSpriteRecordsByPlane (subsystem_free_function)
- `00414260` AdaptiveStableSortVisibleSpriteRecordsByPlane (subsystem_free_function)
- `004144a0` InplaceMergeVisibleSpriteRecordsByPlane (subsystem_free_function)
- `00414570` MergeVisibleSpriteRecordRunsToBuffer (subsystem_free_function)
- `00414640` AdaptiveInplaceMergeVisibleSpriteRecordsByPlane (subsystem_free_function)
- `004148d0` SymmetricMergeVisibleSpriteRecordRangesByPlane (subsystem_free_function)
- `00417270` CWorldRenderer::~CWorldRenderer (class_method)
- `004173d0` RedrawWorldRendererFullViewOnPaint (subsystem_free_function)
- `0044ba80` ReverseVisibleSpriteRecordRange (subsystem_free_function)

### display/sprite_cache.cpp (1 of 5)

- `0044efa0` DestroySpriteFileCacheIndex (startup_registration)

### objects/bubble.cpp (9 of 10)

- `00423c10` Bubble::CreateObject (class_metadata)
- `00423ca0` Bubble::GetRuntimeClass (class_metadata)
- `004249e0` Bubble::Serialize (class_method)
- `00429da0` Bubble::Bubble (class_method)
- `00429f80` Bubble::ReferencesObject (class_method)
- `00429f90` Bubble::DestroyAndRedraw (class_method)
- `0042a1a0` Bubble::SetText (class_method)
- `0042a4c0` Bubble::Tick (class_method)
- `0042a580` Bubble::ClearReferencesToObject (class_method)

### objects/call_button.cpp (1 of 7)

- `00429d50` CallButton::HandleQueuedEvent1 (class_method)

### objects/compound_object.cpp (14 of 31)

- `0042a800` CompoundObject::CompoundObject (class_method)
- `0042a950` CompoundObject::~CompoundObject (class_method)
- `0042aa70` CompoundObject::MoveTo (class_method)
- `0042ae40` CompoundObject::MoveToAndRedraw (class_method)
- `0042b2b0` CompoundObject::QueuePrimaryPartDirtyRect (class_method)
- `0042b4c0` CompoundObject::HandleQueuedEvent0 (class_method)
- `0042b570` CompoundObject::HandleQueuedEvent1 (class_method)
- `0042b620` CompoundObject::HandleQueuedEvent2 (class_method)
- `0042b8b0` CompoundObject::ParseImageSequence (class_method)
- `0042b910` CompoundObject::IsImageSequenceEmpty (class_method)
- `0042b940` CompoundObject::SetRelativeImageIndex (class_method)
- `0042b960` CompoundObject::GetRelativeImageIndex (class_method)
- `0042b980` CompoundObject::PreloadImageSequence (class_method)
- `0042b9d0` CompoundObject::SetImageIndex (class_method)

### objects/debug.cpp (1 of 1)

- `00418210` FormatObjectDebugLabel (subsystem_free_function)

### objects/entity.cpp (2 of 12)

- `00414ff0` Entity::~Entity_Deleting (class_method)
- `00415b90` Entity::SetImageIndexAndRedraw (class_method)

### objects/lifecycle.cpp (2 of 2)

- `0040e9f0` DeleteObjectAndPurgeRuntimeReferences (subsystem_free_function)
- `00413950` UpdateViewAnchoredObjects (subsystem_free_function)

### objects/lift.cpp (1 of 15)

- `0042c3e0` Lift::UpdateBoundsAndQueueRedraw (class_method)

### objects/object.cpp (21 of 38)

- `00410260` Object::DefaultImageVirtualReturnZero (class_method)
- `00419440` Object::IsImageSequenceEmpty (class_method)
- `00423780` Object::MoveBy (class_method)
- `00423790` Object::GetBounds (class_method)
- `004237b0` Object::DefaultVirtualNoOp (class_method)
- `004238e0` Object::CreateObject (class_method)
- `004259a0` Object::~Object_Deleting (class_method)
- `00425b00` Object::QueueEvent8AfterBoundsUpdate (class_method)
- `00425ba0` Object::HandleQueuedCreatureEvent (class_method)
- `00425c20` Object::DispatchEvent8 (class_method)
- `00425e20` Object::IsSoundSourceBelowWorldY (class_method)
- `00425e40` Object::ComputeSoundAttenuationAndPan (class_method)
- `00426420` Object::GetClickEventIdAtWorldPosition (class_method)
- `00426660` Object::ParseImageSequence (class_method)
- `00426670` Object::SetRelativeImageIndex (class_method)
- `00426680` Object::PreloadImageSequence (class_method)
- `00427c60` Object::HandleQueuedEvent3 (class_method)
- `0042cc00` Object::CanBeDestroyed (class_method)
- `0042cd50` Object::ReferencesObject (class_method)
- `0042cd70` Object::InitializeRuntimeState (class_method)
- `0042d0b0` Object::ClearReferenceAndSetDefaultBounds (class_method)

### objects/renderable_set.cpp (1 of 2)

- `0044ee70` DestroyRenderableObjectSet (startup_registration)

### objects/scenery.cpp (1 of 5)

- `00426840` Scenery::MoveToAndRedraw (class_method)

### objects/simple_object.cpp (12 of 37)

- `004237e0` SimpleObject::GetCurrentVisualWidth (class_method)
- `00426580` SimpleObject::CreateBubble (class_method)
- `00426c00` SimpleObject::GetRenderPlane (class_method)
- `00426cf0` SimpleObject::SimpleObject (class_method)
- `00426e30` SimpleObject::~SimpleObject (class_method)
- `00426f90` SimpleObject::MoveToAndRedraw (class_method)
- `00427ae0` SimpleObject::HandleQueuedEvent0 (class_method)
- `00427b60` SimpleObject::HandleQueuedEvent1 (class_method)
- `00427be0` SimpleObject::HandleQueuedEvent2 (class_method)
- `004294c0` SimpleObject::GetRelativeImageIndex (class_method)
- `004294d0` SimpleObject::PreloadImageSequence (class_method)
- `00429510` SimpleObject::SetImageIndex (class_method)

### objects/vehicle.cpp (3 of 9)

- `0042bad0` Vehicle::Vehicle (class_method)
- `0042bb80` Vehicle::SynchronizeFixedPointPositionAndQueueRedraw (class_method)
- `0042bc00` Vehicle::QueueEvent4ForCreaturesInLocalBounds (class_method)

### platform/com.cpp (1 of 1)

- `00443e20` ResolveComProgIdLocalServerPath (platform_or_application_glue)

### platform/environment.cpp (1 of 1)

- `00443dd0` IsRunningUnderWine (platform_or_application_glue)

### platform/formatting.cpp (3 of 3)

- `00406760` MsvcCrt_VsnprintfSecure (platform_or_application_glue)
- `00406790` MsvcCrt_Snprintf (platform_or_application_glue)
- `00449740` MsvcCrt_VsnprintfSecureBounded (platform_or_application_glue)

### platform/gdi.cpp (1 of 1)

- `004171d0` MeasureCStringTextExtent (platform_or_application_glue)

### platform/mfc_adapters.cpp (2 of 2)

- `0040ee20` PointInRectXY (platform_or_application_glue)
- `0040fcd0` PlatformEditControl::SetEditSelectionAndScroll (platform_or_application_glue)

### platform/registry.cpp (2 of 3)

- `0042f450` CloseRegistryKeyPair (platform_or_application_glue)
- `0042f470` ReadOrInitializeRegistryDwordPair (platform_or_application_glue)

### platform/security.cpp (1 of 1)

- `00445b30` Win32BuildCurrentUserSecurityDescriptor (platform_or_application_glue)

### scripting/dde.cpp (12 of 17)

- `0040fda0` DDEService::DdeCallbackEntryThunk (class_method)
- `0040fdb0` DDEService::DdeCallback (class_method)
- `004100e0` DDEService::FindConversation (class_method)
- `00410120` DDEServiceItem::DDEServiceItem::CreateMacroData (class_method)
- `004101e0` DDEServiceItem::CreateBrainActivityData (class_method)
- `00410270` DDEServiceItem::CreateSystemInfoData (class_method)
- `0042f740` NotifyDDEScoreChanged (subsystem_free_function)
- `0044ed70` DestroyDdeItem_Macro (subsystem_free_function)
- `0044ed90` DestroyDdeItem_BrainActivity (subsystem_free_function)
- `0044edb0` DestroyDdeItem_BrainWiring (subsystem_free_function)
- `0044edd0` DestroyDdeItem_SysInfo (subsystem_free_function)
- `0044edf0` ShutdownDdeService (startup_registration)

### scripting/macro.cpp (12 of 24)

- `00419550` Macro::ReadNextToken (class_method)
- `00419d50` PurgeDestroyWhenFinishedMacrosForOwner (subsystem_free_function)
- `0041a020` Macro::ExecuteToOutputBuffer (class_method)
- `0041a210` Macro::RemoveFromRunningSchedulerAndRelease (class_method)
- `0041bc30` Macro::ReadBracketedTextArgument (class_method)
- `0041bcb0` Macro::ExecuteDDECommand (class_method)
- `0041cd30` Macro::ExecuteSystemCAOSCommand (class_method)
- `0041d130` Macro::ExecuteNewCommand (class_method)
- `0041d8a0` Macro::ExecuteBlackboardCAOSCommand (class_method)
- `0041daa0` Macro::ReportSyntaxError (class_method)
- `00420cf0` HandleMacroScriptExecutionException (subsystem_free_function)
- `00420e70` Macro::CensorScriptProfanity (class_method)

### scripting/macro_holder.cpp (13 of 13)

- `00419130` CMacroHolder::CreateObject (class_metadata)
- `004191a0` CMacroHolder::GetRuntimeClass (class_method)
- `004191b0` CMacroHolder::CMacroHolder (class_method)
- `00419280` CMacroHolder::~CMacroHolder_Deleting (class_method)
- `004192d0` CMacroHolder::InvokeDispatchEntry (class_method)
- `00419300` CMacroHolder::ResetResultAndInvokeResultEntry (class_method)
- `00419330` CMacroHolder::DispatchStartMacroExecution (class_method)
- `00419340` CMacroHolder::ExecuteAndPublishMacroOutput (class_method)
- `00419400` CMacroHolder::DispatchFormatBrainActivityReport (class_method)
- `00419450` CMacroHolder::StartMacroExecution (class_method)
- `00419470` CMacroHolder::ExecuteMacroToOutputBuffer (class_method)
- `00419500` CMacroHolder::FormatBrainActivityReport (class_method)
- `00419540` CMacroHolder::SetZeroCallbackResult (class_method)

### scripting/pipe_server.cpp (19 of 20)

- `0043f840` PipeResponse_MoveAppendCString (subsystem_free_function)
- `0043f8b0` PipeResponse_AppendAndMoveResult (subsystem_free_function)
- `00445d00` PipeServerStop (subsystem_free_function)
- `00445f00` PipeServerThreadEntry (platform_or_application_glue)
- `00445f10` PipeServerThreadRun (class_method)
- `00446110` PipeServerServeClient (subsystem_free_function)
- `00446620` PipeServerMarshalCommandToMainThread (class_method)
- `00446c10` PipeServerDispatchCommand (subsystem_free_function)
- `00448570` PipeServerExecuteFireCommand (subsystem_free_function)
- `00448cd0` PipeResponse_ReserveAndMove (subsystem_free_function)
- `00449780` CPipeDispatchProxy::DeletingDestructor (platform_or_application_glue)
- `00449800` CPipeDispatchProxy::QueryInterface (platform_or_application_glue)
- `00449870` CPipeDispatchProxy::AddRef (platform_or_application_glue)
- `00449890` CPipeDispatchProxy::Release (platform_or_application_glue)
- `004498c0` CPipeDispatchProxy::GetTypeInfoCount (platform_or_application_glue)
- `004498d0` CPipeDispatchProxy::GetTypeInfo (platform_or_application_glue)
- `004498f0` CPipeDispatchProxy::GetIDsOfNames (platform_or_application_glue)
- `00449950` CPipeDispatchProxy::Invoke (platform_or_application_glue)
- `0044eff0` PipeServerGlobalCleanup (startup_registration)

### sound/cache.cpp (1 of 1)

- `00440380` CachedSound::~CachedSound_Deleting (class_method)

### sound/sound.cpp (17 of 17)

- `0043f5b0` InitializeSoundSystem (subsystem_free_function)
- `00440450` SoundManager::SoundManager (class_method)
- `00440710` SoundManager::~SoundManager (class_method)
- `004407d0` SoundManager::LoadCachedSound (class_method)
- `00440dc0` SoundManager::StartChannel (class_method)
- `00440ee0` SoundManager::StopAllSounds (class_method)
- `00440fc0` UpdateSoundSystem (subsystem_free_function)
- `00441150` SoundManager::StopChannel (class_method)
- `004411c0` SoundManager::DestroyCachedSound (class_method)
- `00441250` SoundManager::ClearSoundCache (class_method)
- `004412c0` SuspendSoundMixer (class_method)
- `00441310` RestoreSoundMixer (class_method)
- `00441360` SoundManager::EnsureSoundCacheCapacity (class_method)
- `004414f0` SoundManager::FindOrLoadCacheEntry (class_method)
- `00441740` SoundManager::PlayOrQueue (class_method)
- `00441850` SoundManager::StartContinuousSound (class_method)
- `004419d0` SoundManager::StopContinuousSound (class_method)

### ui/caos_console.cpp (16 of 16)

- `0040ee80` CAOSConsoleDlg::~CAOSConsoleDlg_Deleting (class_method)
- `0040ef00` CAOSConsoleDlg::DoDataExchange (class_method)
- `0040ef70` CAOSConsoleDlg::GetMessageMap (class_method)
- `0040ef80` CAOSConsoleDlg::OnInitDialog (class_method)
- `0040f180` CAOSConsoleDlg::PostNcDestroy (class_method)
- `0040f1c0` CAOSConsoleDlg::ClearCommandInput (class_method)
- `0040f1f0` CAOSConsoleDlg::SubmitCommandText (class_method)
- `0040f360` CAOSConsoleDlg::ClearOutputText (class_method)
- `0040f380` CAOSConsoleDlg::LoadCaosScriptFromFile (class_method)
- `0040f570` CAOSConsoleDlg::ToggleAlwaysOnTop (class_method)
- `0040f5b0` CAOSConsoleDlg::OnSize (class_method)
- `0040f760` CAOSConsoleDlg::OnGetMinMaxInfo (class_method)
- `0040f780` CAOSConsoleDlg::ExecuteCommandText (class_method)
- `0040fa10` CAOSConsoleDlg::AppendOutputText (class_method)
- `0040fb60` CAOSConsoleDlg::PreTranslateMessage (class_method)
- `0040fca0` CAOSConsoleDlg::Create (class_method)

### ui/classifier_tip.cpp (1 of 1)

- `004362a0` CClassifierTip::Hide (class_method)

### ui/creature_selection.cpp (4 of 5)

- `00422250` RebuildCreatureSelectionMenu (subsystem_free_function)
- `00433850` SetSelectedCreature (subsystem_free_function)
- `00433920` SelectCreatureByMenuIndex (subsystem_free_function)
- `0044eed0` AtExit_DestroyCreatureSelectionArray (startup_registration)

### ui/debug_console.cpp (14 of 14)

- `004103e0` DebugConsoleDialog::~DebugConsoleDialog (class_method)
- `00410560` DebugConsoleDialog::DoDataExchange (class_method)
- `00410620` DebugConsoleDialog::GetRuntimeClass (class_method)
- `00410630` DebugConsoleDialog::OnInitDialog (class_method)
- `004106e0` DebugConsoleDialog::PostNcDestroy (class_method)
- `00410710` DebugConsoleDialog::RefreshLogOutput (class_method)
- `004107a0` DebugConsoleDialog::ClearLogOutput (class_method)
- `00410840` DebugConsoleDialog::PauseWorldUpdate (class_method)
- `00410870` DebugConsoleDialog::ToggleWorldUpdatePause (class_method)
- `004108e0` DebugConsoleDialog::CopyFilterControlTextToState (class_method)
- `00410940` DebugConsoleDialog::ToggleFilterEnabled (class_method)
- `004109b0` DebugConsoleDialog::CopyAllLogOutput (class_method)
- `004109f0` DebugConsoleDialog::CopyVisibleLogOutput (class_method)
- `00410a50` DebugConsoleDialog::ToggleLogFileMirroring (class_method)

### ui/dialogs.cpp (5 of 5)

- `004439c0` Dialog_DDX_CheckAndTextFields (subsystem_free_function)
- `00443b40` Dialog_OnInit_DisableControl3EAIfFieldB0Null (subsystem_free_function)
- `00444c80` Dialog_DDX_Control40A (subsystem_free_function)
- `00444e60` Dialog_HideWindow (subsystem_free_function)
- `00444ec0` Dialog_DDX_TextField3F2 (subsystem_free_function)

### ui/event_bar.cpp (10 of 12)

- `00416430` CEventBar::GetRuntimeClass (class_method)
- `00416440` CEventBar::CreateObject (class_method)
- `004164e0` CEventBar::GetRuntimeClass (class_method)
- `004164f0` CEventBar::~CEventBar_Deleting (class_method)
- `00416540` AddObjectToEventBarDisplayList (subsystem_free_function)
- `004165b0` CEventBar::RemoveObjectFromDisplayList (class_method)
- `004166c0` CEventBar::RefreshObjectDisplayPanes (class_method)
- `004168b0` CEventBar::UpdateStatusPanes (class_method)
- `00416b80` CEventBar::WindowProc (class_method)
- `00416da0` CEventBar::OnLButtonDown (class_method)

### ui/eye_view.cpp (8 of 8)

- `00417210` CEyeView::~CEyeView_Deleting (class_method)
- `004172f0` CEyeView::UpdateWindowTitleForSelectedCreature (class_method)
- `00417440` CEyeView::CreateWindow (class_method)
- `004175e0` PersistEyeViewWindowPosition (subsystem_free_function)
- `00417660` CEyeView::OnSize (class_method)
- `00417690` CEyeView::OnPaletteChanged (class_method)
- `004176c0` CEyeView::OnQueryNewPalette (class_method)
- `004176e0` CEyeView::UpdateSelectedCreatureFollowViewport (class_method)

### ui/magic_profiler.cpp (1 of 1)

- `00437ef0` GenerateMagicProfilerReport (subsystem_free_function)

### ui/main_window.cpp (1 of 1)

- `00422720` UpdateMainWindowTitleForSelectedCreature (subsystem_free_function)

### ui/menus.cpp (1 of 1)

- `004350e0` FindCameraSubmenu (subsystem_free_function)

### ui/place_dialog.cpp (7 of 7)

- `0042f2e0` CPlaceDlg::DoDataExchange (class_method)
- `0042f310` CPlaceDlg::GetThisMessageMap (class_metadata)
- `0042f320` CPlaceDlg::OnPlaceNameChanged (class_method)
- `0042f330` CPlaceDlg::OnInitDialog (class_method)
- `0042f570` CRemovePlaceDlg::GetThisMessageMap (class_metadata)
- `0042f580` CRemovePlaceDlg::OnRemovePlaceListSelectionChanged (class_method)
- `0042f5e0` CRemovePlaceDlg::OnInitDialog (class_method)

### ui/score.cpp (2 of 4)

- `0042f6b0` AllocateAndConstructCScore (subsystem_free_function)
- `0042f700` CScore::GetRuntimeClass (class_metadata)

### ui/system_info.cpp (1 of 1)

- `00449f10` ReleaseSystemInfoWindowSingleton (platform_or_application_glue)

### ui/tip_dialog.cpp (11 of 11)

- `00412230` CTipDlg::OnTimer (class_method)
- `0043f3d0` ShowStartupTipDialog (subsystem_free_function)
- `00443620` CTipDlg::CTipDlg (class_method)
- `00443910` CTipDlg::DeletingDestructor (class_method)
- `00443950` CTipDlg::~CTipDlg (class_method)
- `00443a00` CTipDlg::GetMessageMap (class_method)
- `00443a10` CTipDlg::OnNextTip (class_method)
- `00443a40` CTipDlg::ReadNextTipText (class_method)
- `00443ac0` CTipDlg::OnCtlColor (class_method)
- `00443b00` CTipDlg::OnOK (class_method)
- `00443b70` CTipDlg::OnPaint (class_method)

### ui/toolbars.cpp (5 of 7)

- `00421ad0` MyToolBar::CreateObject (class_method)
- `00421b90` MyToolBar::GetRuntimeClass (class_method)
- `00421ba0` MyToolBar::~MyToolBar (class_method)
- `00421c00` MyToolBar::Create (class_method)
- `00443ff0` AddKitToolbarBitmapFromProgId (subsystem_free_function)

### ui/tools.cpp (4 of 11)

- `00429620` PointerTool::HandleQueuedEvent4 (class_method)
- `00429720` PointerTool::HandleQueuedEvent5 (class_method)
- `00429860` PointerTool::SetTextWithToolbarUpdate (class_method)
- `00429a00` PointerTool::ExecuteClickScriptFallback (class_method)

### ui/version_dialog.cpp (3 of 3)

- `0043f2c0` ShowVersionDialog (subsystem_free_function)
- `00444ee0` CommunityEditionVersionDialog::GetThisMessageMap (class_metadata)
- `00444ef0` CommunityEditionVersionDialog_OnInitDialog (subsystem_free_function)

### ui/views.cpp (28 of 29)

- `004363a0` SFCView::CreateObject (class_method)
- `00436420` SFCView_GetRuntimeClassMetadata (class_metadata)
- `00436430` SFCView_GetMessageMapMetadata (class_metadata)
- `00436440` SFCView::SFCView (class_method)
- `00436710` SFCView::~SFCView (class_method)
- `00436830` SFCView::~SFCView (class_method)
- `004368a0` SFCView::PreCreateWindow (class_method)
- `00436910` SFCView::OnInitialUpdate (class_method)
- `00436a20` SFCView::OnDraw (class_method)
- `00436aa0` SFCView::OnSize (class_method)
- `00436ad0` SFCView::OnBeginDragResize (class_method)
- `00436b10` SFCView::OnMouseButtonRelease (class_method)
- `00436b30` SFCView::OnMouseMove (class_method)
- `00437200` SFCView::OnLButtonDown (class_method)
- `00437240` SFCView::OnRButtonDown (class_method)
- `00437270` SFCView::OnCharQueueTextInput (class_method)
- `004372c0` SFCView::ResetWorldScrollbars (class_method)
- `00437320` SFCView::OnHScroll (class_method)
- `00437400` SFCView::OnVScroll (class_method)
- `004374e0` SFCView::ToggleInfiniteScrollWorldSize (class_method)
- `00437560` SFCView::OnUpdateWorldHalfHeightSetting (class_method)
- `004375a0` SFCView::OnSetFocusRestoreSoundMixer (class_method)
- `004375e0` SFCView::OnKillFocus (class_method)
- `00437610` SFCView::SetViewportNavigationMode (class_method)
- `00437910` ReturnViewportNavigationToSelectedCreature (class_method)
- `004379a0` SFCView::ToggleViewportFollowSelectedCreatureMode (class_method)
- `00437aa0` SFCView::OnUpdateToggleViewportFollowSelectedCreatureMode (class_method)
- `00437b30` SFCView::OnKeyDown (class_method)

### ui/volume_dialog.cpp (3 of 3)

- `00444ca0` CVolumeDialog::GetThisMessageMap (class_metadata)
- `00444cb0` CVolumeDialog::OnInitDialog (class_method)
- `00444d80` CVolumeDialog::OnHScroll (class_method)

### ui/windows.cpp (2 of 2)

- `00410970` ToggleWindowAlwaysOnTop (subsystem_free_function)
- `00410bf0` OnActivateFlashWindow (subsystem_free_function)

### ui/world_statistics.cpp (6 of 6)

- `00449b90` CWorldStatisticsFrame::GetThisMessageMap (class_metadata)
- `00449c00` CWorldStatisticsFrame::OnCreate (class_method)
- `00449d20` CWorldStatisticsFrame::OnTimer (class_method)
- `00449d40` CWorldStatisticsFrame::RefreshStatistics (class_method)
- `00449ea0` CWorldStatisticsFrame::PreTranslateMessage (class_method)
- `00449ef0` CWorldStatisticsFrame::CloseAndDestroy (class_method)

### world/geometry.cpp (2 of 3)

- `00417b40` WrappedWorldRectsOverlap (subsystem_free_function)
- `00417bb0` IntersectWrappedWorldRects (subsystem_free_function)

### world/places.cpp (3 of 5)

- `00417890` CFavouritePlace::CreateObject (class_metadata)
- `00417920` CFavouritePlace::GetRuntimeClass (class_metadata)
- `00435200` RemoveFavouritePlaceAtIndex (subsystem_free_function)

### world/settings.cpp (1 of 2)

- `0044ef30` AtExit_DestroyWorldSavePathStorage (startup_registration)

### world/tick.cpp (2 of 2)

- `00433200` WorldTickPhase_UpdateCreatureBacteriaAndEnvironment (subsystem_free_function)
- `00433310` AdvanceBacteriumServicePhase (subsystem_free_function)

### world/update_timer.cpp (3 of 3)

- `00417f80` ConfigureWorldUpdateTimerInterval (subsystem_free_function)
- `00434ef0` WorldUpdateTimer_DispatchWithState (subsystem_free_function)
- `00435cb0` SetWorldUpdatePaused (subsystem_free_function)

### world/viewport.cpp (4 of 4)

- `00412ea0` ConvertWorldRectToViewportRect (subsystem_free_function)
- `00412f10` ScrollViewportIfNonZero (subsystem_free_function)
- `00413360` CenterViewportOnSelectedCreatureIfInPanRegion (subsystem_free_function)
- `00413420` FollowSelectedCreatureViewport (subsystem_free_function)

