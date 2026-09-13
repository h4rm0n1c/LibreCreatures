#pragma once
// Module-global variables (this project's own `g_*` naming convention)
// referenced from more than one translation unit. Each declaration
// carries its own type-recovery evidence in an adjacent comment.

// Forward declarations for our own struct types used above.
struct BodyAttachmentData;
struct BodyPartAttachmentData;
struct C1AfxDispMap;
struct C1AfxInterfaceMap;
struct C1AfxMsgMap;
struct C1AmbientEnvironmentRecord;
struct C1AmbientLightProfile;
struct C1CharsetGlyphBitmap;
struct C1VisibleSpriteSortRecord;
struct C1VocabularyWordBank;
struct CAOSConsoleDlg;
struct CEventBar;
struct CEyeView;
struct CImage;
struct CMainFrame;
struct CScore;
struct CSystemInfoWnd;
struct CVolumeDialog;
struct Creature;
struct CreatureStimulusContext;
struct DDEServiceConversation;
struct DDEServiceItem;
struct DebugConsoleDialog;
struct Entity;
struct LearnedWordPhonemeSubstitution;
struct Lift;
struct Macro;
struct MapData;
struct MapRoomRecord;
struct MsvcFacetNode;
struct MyToolBar;
struct Object;
struct PointerTool;
struct QueuedCreatureStimulus;
struct QueuedObjectEvent;
struct RenderableObjectSet;
struct SFCApp;
struct SFCDoc;
struct SFCView;
struct ScriptDefinitionEntry;
struct SoundManager;
struct WorldRect;

// Local re-declarations of scalar typedefs already real elsewhere in
// the tree, needed here because this header is included earlier.
typedef unsigned int BacteriumServicePhase;
typedef unsigned char C1ActionTargetRequirement;
typedef unsigned int C1Bool32;
typedef unsigned int WorldTickPhase;

extern CRuntimeClass g_CGenomeRuntimeClass; // verified_ghidra_query
extern CMainFrame * g_CMainFrame; // verified_ghidra_query
extern C1PtrArrayLike g_EntityRegistry; // verified_ghidra_query
extern int g_EntityRegistry_count; // verified_ghidra_query
extern Entity ** g_EntityRegistry_entries; // verified_ghidra_query
extern MapRoomRecord g_InitialWorldRoomRecords[11]; // verified_ghidra_query
extern Lift * g_Lift; // verified_ghidra_query
extern MapData * g_MapData; // verified_ghidra_query
extern SFCApp * g_SFCApp; // verified_ghidra_query
extern SFCDoc * g_SFCDoc; // verified_ghidra_query
extern SFCView * g_SFCView; // verified_ghidra_query
extern C1PtrArrayLike g_SceneryRegistry; // verified_ghidra_query
extern char g_action_debug_label_scratch[32]; // verified_ghidra_query
extern C1ActionTargetRequirement g_action_target_requirements[16]; // verified_ghidra_query
extern C1AmbientEnvironmentRecord g_ambient_environment_records[10]; // verified_ghidra_query
extern C1AmbientLightProfile g_ambient_light_level_by_environment[4]; // verified_ghidra_query
extern int g_ambient_sound_cooldown_ticks; // verified_ghidra_query
extern GUID g_atl_base_module_guid; // verified_ghidra_query
extern unsigned char g_atl_base_module_init_failure; // verified_ghidra_query
extern char * g_attention_record_labels[40]; // verified_ghidra_query
extern BacteriumServicePhase g_bacterium_service_phase; // verified_ghidra_query
extern unsigned int g_biochemistry_tick_selector_masks[32]; // verified_ghidra_query
extern unsigned int g_biochemistry_tick_selector_q16_multipliers[32]; // verified_ghidra_query
extern BodyAttachmentData g_body_attachment_data; // verified_ghidra_query
extern BodyPartAttachmentData g_body_part_attachment_data; // verified_ghidra_query
extern unsigned char g_body_part_sprite_frame_counts[14]; // verified_ghidra_query
extern CAOSConsoleDlg * g_caos_console_dialog; // verified_ghidra_query
extern int g_caos_language_version; // verified_ghidra_query
extern C1AfxDispMap g_capplication_dispatch_map; // verified_ghidra_query
extern C1AfxInterfaceMap g_capplication_interface_map; // verified_ghidra_query
extern ushort g_charset_glyph_advance_widths[128]; // verified_ghidra_query
extern C1CharsetGlyphBitmap * g_charset_glyph_raster_data; // verified_ghidra_query
extern unsigned int g_classifier_event_masks[4]; // verified_ghidra_query
extern C1ClassifierNameMap g_classifier_name_map; // verified_ghidra_query
extern int g_conception_crossover_count; // verified_ghidra_query
extern int g_conception_duplication_count; // verified_ghidra_query
extern int g_conception_mutation_count; // verified_ghidra_query
extern int g_conception_omission_count; // verified_ghidra_query
extern unsigned char g_crash_report_stream[80]; // verified_ghidra_query
extern int g_crash_report_stream_init_guard; // verified_ghidra_query
extern unsigned char g_creature_action_phrase_needs_target[16]; // verified_ghidra_query
extern unsigned char g_creature_action_phrase_word_indices[16]; // verified_ghidra_query
extern C1CreaturePtrArrayLike g_creature_registry; // verified_ghidra_query
extern C1CreaturePtrArrayLike g_creature_selection_array; // verified_ghidra_query
extern int g_creature_update_cohort; // verified_ghidra_query
extern C1AfxDispMap g_csfc_ole_dispatch_map; // verified_ghidra_query
extern C1AfxInterfaceMap g_csfc_ole_interface_map; // verified_ghidra_query
extern C1AfxMsgMap g_csfc_ole_message_map; // verified_ghidra_query
extern int g_current_idle_cycle_index; // verified_ghidra_query
extern C1SecurityAttributes g_current_user_security_attributes; // verified_ghidra_query
extern void * g_current_user_security_descriptor; // verified_ghidra_query
extern int g_current_user_security_descriptor_init_state; // verified_ghidra_query
extern dword g_dde_conversation_count; // verified_ghidra_query
extern DDEServiceConversation * g_dde_conversation_slots[]; // verified_ghidra_query
extern DWORD g_dde_instance_id; // verified_ghidra_query
extern int g_dde_item_count; // verified_ghidra_query
extern DDEServiceItem * g_dde_item_slots[8]; // verified_ghidra_query
extern HSZ g_dde_service_hsz; // verified_ghidra_query
extern char * g_dde_service_name; // verified_ghidra_query
extern DebugConsoleDialog * g_debug_console_dialog; // verified_ghidra_query
extern unsigned int g_debug_log_category_mask; // verified_ghidra_query
extern char g_debug_log_category_tags[160]; // verified_ghidra_query
extern unsigned int g_debug_log_flush_timer_id; // verified_ghidra_query
extern C1Bool32 g_debug_logging_enabled; // verified_ghidra_query
extern CreatureStimulusContext * g_default_creature_stimulus_context_ptrs[36]; // verified_ghidra_query
extern char * g_default_vocabulary_words_group_1[40]; // verified_ghidra_query
extern char * g_default_vocabulary_words_group_2[16]; // verified_ghidra_query
extern char ** g_default_vocabulary_words_group_3; // verified_ghidra_query
extern int g_delayed_object_event_count; // verified_ghidra_query
extern QueuedObjectEvent g_delayed_object_events[200]; // verified_ghidra_query
extern int g_drive_lower_thresholds[16]; // verified_ghidra_query
extern int g_drive_upper_thresholds[16]; // verified_ghidra_query
extern Object * g_edit_object; // verified_ghidra_query
extern CEventBar * g_event_bar; // verified_ghidra_query
extern tagRECT g_eye_default_rect; // verified_ghidra_query
extern CEyeView * g_eye_view; // verified_ghidra_query
extern C1GalleryPtrArrayLike g_gallery_registry; // verified_ghidra_query
extern HPALETTE g_game_palette_handle; // verified_ghidra_query
extern int * g_genome_appearance_part_group_map; // verified_ghidra_query
extern unsigned int g_goal_direction_drive_scale_factors[16]; // verified_ghidra_query
extern unsigned int g_image_cache_access_stamp; // verified_ghidra_query
extern unsigned int g_image_cache_bytes_used; // verified_ghidra_query
extern unsigned int g_image_cache_entry_count; // verified_ghidra_query
extern CImage * g_image_cache_lru_head; // verified_ghidra_query
extern CImage * g_image_cache_lru_tail; // verified_ghidra_query
extern QueuedObjectEvent g_immediate_object_events[200]; // verified_ghidra_query
extern QueuedObjectEvent * g_immediate_object_events_begin; // verified_ghidra_query
extern QueuedObjectEvent * g_immediate_object_events_write; // verified_ghidra_query
extern int g_is_running_under_wine_cached; // verified_ghidra_query
extern char g_key_event_ring_buffer[16]; // verified_ghidra_query
extern char * g_key_event_ring_read_cursor; // verified_ghidra_query
extern char * g_key_event_ring_write_cursor; // verified_ghidra_query
extern int g_last_logged_running_macro_count; // verified_ghidra_query
extern int g_last_logged_script_definition_count; // verified_ghidra_query
extern LearnedWordPhonemeSubstitution g_learned_word_phoneme_substitution_table[18]; // verified_ghidra_query
extern char g_lobe_connection_debug_description[512]; // verified_ghidra_query
extern char g_lobe_connection_label_table[672]; // verified_ghidra_query
extern char g_lobe_connection_separator_arrow[3]; // verified_ghidra_query
extern char g_lobe_connection_separator_comma; // verified_ghidra_query
extern char g_lobe_neuron_label_table[96]; // verified_ghidra_query
extern Object * g_macro_initial_auxiliary_object; // verified_ghidra_query
extern unsigned char g_macro_object_attribute_table_004579a4[15]; // verified_ghidra_query
extern tagRECT g_main_default_window_rect; // verified_ghidra_query
extern MyToolBar * g_main_toolbar; // verified_ghidra_query
extern int g_max_embedded_kits_setting; // verified_ghidra_query
extern undefined * g_mfc_get_this_message_map_ptr_a; // verified_ghidra_query
extern undefined * g_mfc_get_this_message_map_ptr_b; // verified_ghidra_query
extern unsigned char g_mfc_operator_new_cookie; // verified_ghidra_query
extern char g_motion_pose_digit_by_vertical_bin_and_distance_bin[10][4]; // verified_ghidra_query
extern unsigned int g_msvc_basic_filebuf_state_word_0; // verified_ghidra_query
extern unsigned int g_msvc_basic_filebuf_state_word_1; // verified_ghidra_query
extern MsvcFacetNode * g_msvc_facet_node_head; // verified_ghidra_query
extern unsigned int g_msvc_local_stdio_printf_options[2]; // verified_ghidra_query
extern char * g_need_state_label_table[16]; // verified_ghidra_query
extern C1ObjectPtrArrayLike g_non_scenery_object_registry; // verified_ghidra_query
extern char g_object_debug_label[80]; // verified_ghidra_query
extern char g_object_debug_label_suffix_scratch[16]; // verified_ghidra_query
extern int g_on_get_min_max_info_guard; // verified_ghidra_query
extern double g_palette_double_127; // verified_ghidra_query
extern double g_palette_double_128; // verified_ghidra_query
extern double g_palette_double_128_alt; // verified_ghidra_query
extern double g_palette_double_half; // verified_ghidra_query
extern C1PaletteDtaBuffer g_palette_dta_buffer_0; // verified_ghidra_query
extern C1PaletteDtaBuffer g_palette_dta_buffer_1; // verified_ghidra_query
extern C1PaletteDtaBuffer g_palette_dta_buffer_2; // verified_ghidra_query
extern C1PaletteDtaBuffer g_palette_dta_buffer_3; // verified_ghidra_query
extern float g_palette_float_one; // verified_ghidra_query
extern int g_palette_remap_build_count; // verified_ghidra_query
extern IID g_pipe_dispatch_supported_iids[2]; // verified_ghidra_query
extern PipeServerSharedState g_pipe_server_state; // verified_ghidra_query
extern PointerTool * g_pointer_tool; // verified_ghidra_query
extern int g_pointer_tool_name_update_deadline; // verified_ghidra_query
extern char g_pose_chain_image_index_offsets[24]; // verified_ghidra_query
extern int g_pose_string_chain_lengths[6]; // verified_ghidra_query
extern int g_pose_string_chain_offsets[6]; // verified_ghidra_query
extern HKEY g_primary_resource_registry_key; // verified_ghidra_query
extern QueuedCreatureStimulus g_queued_creature_stimuli[200]; // verified_ghidra_query
extern QueuedCreatureStimulus * g_queued_creature_stimuli_read; // verified_ghidra_query
extern QueuedCreatureStimulus * g_queued_creature_stimuli_write; // verified_ghidra_query
extern C1VocabularyWordBank g_randomized_vocabulary_banks_by_gender[8]; // verified_ghidra_query
extern unsigned int g_registry_key_disposition; // verified_ghidra_query
extern RenderableObjectSet g_renderable_object_set_state; // verified_ghidra_query
extern int g_running_macro_count; // verified_ghidra_query
extern Macro ** g_running_macro_slots; // verified_ghidra_query
extern CScore * g_score; // verified_ghidra_query
extern int g_script_definition_count; // verified_ghidra_query
extern ScriptDefinitionEntry * g_script_definition_entries; // verified_ghidra_query
extern char g_scrt_onexit_tables_initialized; // verified_ghidra_query
extern bool g_scrt_ucrt_dll_in_use; // verified_ghidra_query
extern HKEY g_secondary_resource_registry_key; // verified_ghidra_query
extern unsigned int g_security_cookie_complement; // verified_ghidra_query
extern Creature * g_selected_creature; // verified_ghidra_query
extern GUID g_sfc_document_template_clsid; // verified_ghidra_query
extern GUID g_sfc_ole_clsid; // verified_ghidra_query
extern char g_shared_filename_component_buffer[80]; // verified_ghidra_query
extern char g_shared_path_buffer[260]; // verified_ghidra_query
extern int g_simple_object_render_plane_offsets[4]; // verified_ghidra_query
extern int g_smoothed_idle_cycle_index; // verified_ghidra_query
extern SoundManager * g_sound_manager; // verified_ghidra_query
extern SpriteFileCacheEntry g_sprite_file_cache[256]; // verified_ghidra_query
extern unsigned int g_sprite_file_cache_access_stamp; // verified_ghidra_query
extern SpriteFileCacheIndex g_sprite_file_cache_index; // verified_ghidra_query
extern unsigned int g_status_bar_indicator_ids[14]; // verified_ghidra_query
extern unsigned int g_std_stringbuf_vftable_terminator; // verified_ghidra_query
extern CSystemInfoWnd * g_system_info_window; // verified_ghidra_query
extern int g_text_input_allowed_character_flags; // verified_ghidra_query
extern char g_text_input_buffer[80]; // verified_ghidra_query
extern unsigned int g_text_input_length; // verified_ghidra_query
extern int g_text_input_max_length; // verified_ghidra_query
extern Object * g_text_input_target; // verified_ghidra_query
extern unsigned char g_unresolved_creature_locus_sentinel; // verified_ghidra_query
extern double g_unsigned_int_double_biases[2]; // verified_ghidra_query
extern WorldRect g_viewport_navigation_world_bounds; // verified_ghidra_query
extern C1VisibleSpriteSortRecord g_visible_sprite_sort_records[4000]; // verified_ghidra_query
extern double g_volume_attenuation_log_base; // verified_ghidra_query
extern float g_volume_attenuation_log_scale; // verified_ghidra_query
extern CVolumeDialog * g_volume_dialog; // verified_ghidra_query
extern float g_volume_slider_maximum; // verified_ghidra_query
extern float g_volume_slider_rounding_bias; // verified_ghidra_query
extern int g_world_half_height; // verified_ghidra_query
extern int g_world_half_width; // verified_ghidra_query
extern C1PtrArrayLike g_world_object_registry; // verified_ghidra_query
extern CString g_world_save_path_storage; // verified_ghidra_query
extern int g_world_tick_count; // verified_ghidra_query
extern WorldTickPhase g_world_tick_phase; // verified_ghidra_query
extern C1WorldTickPhaseDispatchFn g_world_tick_phase_dispatch_entries[16]; // verified_ghidra_query
extern bool g_world_update_in_progress; // verified_ghidra_query
extern int g_world_update_timer_interval_ms; // verified_ghidra_query
extern WorldUpdateTimerState g_world_update_timer_state; // verified_ghidra_query
extern const char g_xml_escape_ampersand[]; // verified_ghidra_query
extern const char g_xml_escape_greater_than[]; // verified_ghidra_query
extern const char g_xml_escape_less_than[]; // verified_ghidra_query
extern const char g_xml_escape_quoted[]; // verified_ghidra_query
extern C1InteractionPosePair g_interaction_pose_pairs_ascii_0_to_5[22]; // verified_ghidra_query
extern char s_scrambled_sfcapp_primary[13]; // verified_ghidra_query
extern char s_scrambled_sfcapp_secondary[13]; // verified_ghidra_query
extern char s_scrambled_sound_primary[13]; // verified_ghidra_query
extern char s_scrambled_sound_secondary[13]; // verified_ghidra_query
extern char s_scrambled_sound_tertiary[13]; // verified_ghidra_query
extern char s_learned_word_vowels[11]; // verified_ghidra_query
extern char s_learned_word_consonants[43]; // verified_ghidra_query
extern C1GoalDirectionActionCandidateScoreTable C1GoalDirectionActionCandidateScoreTable_0045424c; // verified_ghidra_query
