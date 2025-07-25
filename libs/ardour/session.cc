/*
 * Copyright (C) 1999-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2007 Jesse Chappell <jesse@essej.net>
 * Copyright (C) 2006-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2019 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 GZharun <grygoriiz@wavesglobal.com>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio> /* sprintf(3) ... grrr */
#include <cmath>
#include <cerrno>
#include <unistd.h>
#include <limits.h>

#include <glibmm/datetime.h>
#include <glibmm/threads.h>
#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include "pbd/atomic.h"
#include "pbd/basename.h"
#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/md5.h"
#include "pbd/pthread_utils.h"
#include "pbd/search_path.h"
#include "pbd/stl_delete.h"
#include "pbd/replace_all.h"
#include "pbd/types_convert.h"
#include "pbd/unwind.h"

#include "temporal/types_convert.h"

#include "ardour/amp.h"
#include "ardour/analyser.h"
#include "ardour/async_midi_port.h"
#include "ardour/audio_buffer.h"
#include "ardour/audio_port.h"
#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/audiofilesource.h"
#include "ardour/auditioner.h"
#include "ardour/boost_debug.h"
#include "ardour/buffer_manager.h"
#include "ardour/buffer_set.h"
#include "ardour/bundle.h"
#include "ardour/butler.h"
#include "ardour/click.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/data_type.h"
#include "ardour/debug.h"
#include "ardour/disk_reader.h"
#include "ardour/directory_names.h"
#include "ardour/filename_extensions.h"
#include "ardour/gain_control.h"
#include "ardour/graph.h"
#include "ardour/io_plug.h"
#include "ardour/io_tasklist.h"
#include "ardour/luabindings.h"
#include "ardour/lv2_plugin.h"
#include "ardour/midiport_manager.h"
#include "ardour/scene_changer.h"
#include "ardour/midi_patch_manager.h"
#include "ardour/midi_track.h"
#include "ardour/midi_ui.h"
#include "ardour/mixer_scene.h"
#include "ardour/operations.h"
#include "ardour/playlist.h"
#include "ardour/playlist_factory.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/plugin_manager.h"
#include "ardour/polarity_processor.h"
#include "ardour/presentation_info.h"
#include "ardour/process_thread.h"
#include "ardour/profile.h"
#include "ardour/rc_configuration.h"
#include "ardour/recent_sessions.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/revision.h"
#include "ardour/route_group.h"
#include "ardour/rt_tasklist.h"
#include "ardour/wrong_program.h"

#include "ardour/rt_safe_delete.h"
#include "ardour/silentfilesource.h"
#include "ardour/send.h"
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"
#include "ardour/session_playlists.h"
#include "ardour/session_route.h"
#include "ardour/smf_source.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/source_factory.h"
#include "ardour/speakers.h"
#include "ardour/surround_return.h"
#include "ardour/tempo.h"
#include "ardour/ticker.h"
#include "ardour/transport_fsm.h"
#include "ardour/transport_master.h"
#include "ardour/transport_master_manager.h"
#include "ardour/track.h"
#include "ardour/triggerbox.h"
#include "ardour/types_convert.h"
#include "ardour/user_bundle.h"
#include "ardour/utils.h"
#include "ardour/vca_manager.h"
#include "ardour/vca.h"

#ifdef VST3_SUPPORT
#include "ardour/vst3_plugin.h"
#endif // VST3_SUPPORT

#include "midi++/port.h"
#include "midi++/mmc.h"

#include "LuaBridge/LuaBridge.h"

#include <glibmm/checksum.h>

#include "pbd/i18n.h"

namespace ARDOUR {
class MidiSource;
class Processor;
class Speakers;
}

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Temporal;

bool Session::_disable_all_loaded_plugins = false;
bool Session::_bypass_all_loaded_plugins = false;
std::atomic<unsigned int> Session::_name_id_counter (0);

PBD::Signal<void(std::string)> Session::Dialog;
PBD::Signal<int()> Session::AskAboutPendingState;
PBD::Signal<int(samplecnt_t, samplecnt_t)> Session::AskAboutSampleRateMismatch;
PBD::Signal<void(samplecnt_t, samplecnt_t)> Session::NotifyAboutSampleRateMismatch;
PBD::Signal<void()> Session::SendFeedback;
PBD::Signal<int(Session*,std::string,DataType)> Session::MissingFile;

PBD::Signal<void(samplepos_t)> Session::StartTimeChanged;
PBD::Signal<void(samplepos_t)> Session::EndTimeChanged;
PBD::Signal<void(std::string, std::string, bool, samplepos_t)> Session::Exported;
PBD::Signal<int(std::shared_ptr<Playlist> )> Session::AskAboutPlaylistDeletion;
PBD::Signal<void()> Session::Quit;
PBD::Signal<void()> Session::FeedbackDetected;
PBD::Signal<void()> Session::SuccessfulGraphSort;
PBD::Signal<void(std::string,std::string)> Session::VersionMismatch;
PBD::Signal<void()> Session::AfterConnect;

const samplecnt_t Session::bounce_chunk_size = 8192;
static void clean_up_session_event (SessionEvent* ev) { delete ev; }
const SessionEvent::RTeventCallback Session::rt_cleanup (clean_up_session_event);
const uint32_t Session::session_end_shift = 0;

/** @param snapshot_name Snapshot name, without .ardour suffix */
Session::Session (AudioEngine &eng,
                  const string& fullpath,
                  const string& snapshot_name,
                  BusProfile const * bus_profile,
                  string mix_template,
                  bool unnamed,
                  samplecnt_t sr)
	: HistoryOwner (X_("editor"))
	,  _playlists (new SessionPlaylists)
	, _engine (eng)
	, process_function (&Session::process_with_events)
	, _bounce_processing_active (false)
	, waiting_for_sync_offset (false)
	, _base_sample_rate (sr)
	, _current_sample_rate (0)
	, _transport_sample (0)
	, _session_range_location (0)
	, _session_range_is_free (true)
	, _silent (false)
	, _remaining_latency_preroll (0)
	, _last_touched_mixer_scene_idx (std::numeric_limits<size_t>::max())
	, _engine_speed (1.0)
	, _signalled_varispeed (0)
	, auto_play_legal (false)
	, _requested_return_sample (-1)
	, current_block_size (0)
	, _worst_output_latency (0)
	, _worst_input_latency (0)
	, _worst_route_latency (0)
	, _io_latency (0)
	, _send_latency_changes (0)
	, _update_send_delaylines (false)
	, _have_captured (false)
	, _capture_duration (0)
	, _capture_xruns (0)
	, _export_xruns (0)
	, _non_soloed_outs_muted (false)
	, _listening (false)
	, _listen_cnt (0)
	, _solo_isolated_cnt (0)
	, _writable (false)
	, _under_nsm_control (false)
	, _xrun_count (0)
	, _required_thread_buffersize (0)
	, master_wait_end (0)
	, post_export_sync (false)
	, post_export_position (0)
	, _exporting (false)
	, _export_rolling (false)
	, _realtime_export (false)
	, _region_export (false)
	, _export_preroll (0)
	, _pre_export_mmc_enabled (false)
	, _name (snapshot_name)
	, _is_new (true)
	, _send_qf_mtc (false)
	, _pframes_since_last_mtc (0)
	, play_loop (false)
	, loop_changing (false)
	, last_loopend (0)
	, _session_dir (new SessionDirectory (fullpath))
	, _current_snapshot_name (snapshot_name)
	, state_tree (0)
	, _state_of_the_state (StateOfTheState (CannotSave | InitialConnecting | Loading))
	, _save_queued (false)
	, _save_queued_pending (false)
	, _no_save_signal (false)
	, _last_roll_location (0)
	, _last_roll_or_reversal_location (0)
	, _last_record_location (0)
	, pending_auto_loop (false)
	, _mempool ("Session", 4194304)
#ifdef USE_TLSF
	, lua (lua_newstate (&PBD::TLSF::lalloc, &_mempool))
#elif defined USE_MALLOC
	, lua (lua_newstate (true, true))
#else
	, lua (lua_newstate (&PBD::ReallocPool::lalloc, &_mempool))
#endif
	, _lua_run (0)
	, _lua_add (0)
	, _lua_del (0)
	, _lua_list (0)
	, _lua_load (0)
	, _lua_save (0)
	, _lua_cleanup (0)
	, _n_lua_scripts (0)
	, _io_plugins (new IOPlugList)
	, _butler (new Butler (*this))
	, _transport_fsm (new TransportFSM (*this))
	, _locations (new Locations (*this))
	, _ignore_skips_updates (false)
	, _rt_thread_active (false)
	, _rt_emit_pending (false)
	, _ac_thread_active (0)
	, step_speed (0)
	, outbound_mtc_timecode_frame (0)
	, next_quarter_frame_to_send (-1)
	, _samples_per_timecode_frame (0)
	, _frames_per_hour (0)
	, _timecode_frames_per_hour (0)
	, last_timecode_valid (false)
	, last_timecode_when (0)
	, _send_timecode_update (false)
	, ltc_encoder (0)
	, ltc_enc_buf(0)
	, ltc_buf_off (0)
	, ltc_buf_len (0)
	, ltc_speed (0)
	, ltc_enc_byte (0)
	, ltc_enc_pos (0)
	, ltc_enc_cnt (0)
	, ltc_enc_off (0)
	, restarting (false)
	, ltc_prev_cycle (0)
	, ltc_timecode_offset (0)
	, ltc_timecode_negative_offset (false)
	, midi_control_ui (0)
	, _punch_or_loop (NoConstraint)
	, _all_route_group (new RouteGroup (*this, "all"))
	, routes (new RouteList)
	, _adding_routes_in_progress (false)
	, _reconnecting_routes_in_progress (false)
	, _route_deletion_in_progress (false)
	, _route_reorder_in_progress (false)
	, _track_number_decimals(1)
	, default_fade_steepness (0)
	, default_fade_msecs (0)
	, _total_free_4k_blocks (0)
	, _total_free_4k_blocks_uncertain (false)
	, no_questions_about_missing_files (false)
	, _bundles (new BundleList)
	, _bundle_xml_node (0)
	, _clicking (false)
	, _click_rec_only (false)
	, click_data (0)
	, click_emphasis_data (0)
	, click_length (0)
	, click_emphasis_length (0)
	, _clicks_cleared (0)
	, _count_in_samples (0)
	, _play_range (false)
	, _range_selection (timepos_t::max (Temporal::AudioTime), timepos_t::max (Temporal::AudioTime))
	, _object_selection (timepos_t::max (Temporal::AudioTime), timepos_t::max (Temporal::AudioTime))
	, _preroll_record_trim_len (0)
	, _count_in_once (false)
	, main_outs (0)
	, first_file_data_format_reset (true)
	, first_file_header_format_reset (true)
	, have_looped (false)
	, roll_started_loop (false)
	, _step_editors (0)
	,  _speakers (new Speakers)
	, _ignore_route_processor_changes (0)
	, _ignored_a_processor_change (0)
	, midi_clock (0)
	, _scene_changer (0)
	, _midi_ports (0)
	, _mmc (0)
	, _vca_manager (new VCAManager (*this))
	, _selection (new CoreSelection (*this))
	, _global_locate_pending (false)
	, _had_destructive_tracks (false)
	, _pending_cue (-1)
	, _active_cue (-1)
	, tb_with_filled_slots (0)
	, _global_quantization (Config->get_default_quantization())
{
	_suspend_save.store (0);
	_playback_load.store (0);
	_capture_load.store (0);
	_post_transport_work.store (PostTransportWork (0));
	_processing_prohibited.store (Disabled);
	_record_status.store (Disabled);
	_punch_or_loop.store (NoConstraint);
	_current_usecs_per_track.store (1000);
	_have_rec_enabled_track.store (0);
	_have_rec_disabled_track.store (1);
	_latency_recompute_pending.store (0);
	_suspend_timecode_transmission.store (0);
	_update_pretty_names.store (0);
	_seek_counter.store (0);
	_butler_seek_counter.store (0);

	created_with = string_compose ("%1 %2", PROGRAM_NAME, revision);

	pthread_mutex_init (&_rt_emit_mutex, 0);
	pthread_cond_init (&_rt_emit_cond, 0);

	pthread_mutex_init (&_auto_connect_mutex, 0);
	pthread_cond_init (&_auto_connect_cond, 0);

	init_name_id_counter (1); // reset for new sessions, start at 1
	VCA::set_next_vca_number (1); // reset for new sessions, start at 1

	_cue_events.reserve (1024);

	Temporal::reset();

	pre_engine_init (fullpath); // sets _is_new

	setup_lua ();

	/* The engine sould be running at this point */
	if (!AudioEngine::instance()->running()) {
		destroy ();
		throw SessionException (_("Session initialization failed because Audio/MIDI engine is not running."));
	}

	immediately_post_engine ();

	bool need_template_resave = false;
	std::string template_description;

	if (_is_new) {

		Stateful::loading_state_version = CURRENT_SESSION_FILE_VERSION;

		if (create (mix_template, bus_profile, unnamed)) {
			destroy ();
			throw SessionException (_("Session initialization failed"));
		}

		/* if a mix template was provided, then ::create() will
		 * have copied it into the session and we need to load it
		 * so that we have the state ready for ::set_state()
		 * after the engine is started.
		 *
		 * Note that templates are saved without sample rate, and the
		 * current / previous sample rate will thus also be used after load_state()
		 */

		if (!mix_template.empty()) {
			try {
				if (load_state (_current_snapshot_name, /* from_template = */ true)) {
					destroy ();
					throw SessionException (_("Failed to load template/snapshot state"));
				}
			} catch (PBD::unknown_enumeration& e) {
				destroy ();
				throw SessionException (_("Failed to parse template/snapshot state"));
			}

			if (state_tree && Stateful::loading_state_version < CURRENT_SESSION_FILE_VERSION) {
				need_template_resave = true;
				XMLNode const & root (*state_tree->root());
				XMLNode* desc_nd = root.child (X_("description"));
				if (desc_nd) {
					template_description = desc_nd->attribute_value();
				}
			}
			store_recent_templates (mix_template);
		}

		/* load default session properties - if any */
		config.load_state();

	} else {

		if (load_state (_current_snapshot_name)) {
			destroy ();
			throw SessionException (_("Failed to load state"));
		}

		ensure_subdirs (); // archived or zipped sessions may lack peaks/ analysis/ etc
	}

	/* apply the loaded state_tree */
	int err = post_engine_init ();

	if (err) {
		destroy ();
		switch (err) {
		case -1:
			throw SessionException (string_compose (_("Cannot initialize session/engine: %1"), _("Failed to create background threads.")));
			break;
		case -2:
		case -3:
			throw SessionException (string_compose (_("Cannot initialize session/engine: %1"), _("Invalid TempoMap in session-file.")));
			break;
		case -4:
			throw SessionException (string_compose (_("Cannot initialize session/engine: %1"), _("Invalid or corrupt session state.")));
			break;
		case -5:
			throw SessionException (string_compose (_("Cannot initialize session/engine: %1"), _("Port registration failed.")));
			break;
		case -6:
			throw SessionException (string_compose (_("Cannot initialize session/engine: %1"), _("Audio/MIDI Engine is not running or sample-rate mismatches.")));
			break;
		case -8:
			throw SessionException (string_compose (_("Cannot initialize session/engine: %1"), _("Required Plugin/Processor is missing.")));
			break;
		case -9:
			throw WrongProgram (modified_with);
			break;
		default:
			throw SessionException (string_compose (_("Cannot initialize session/engine: %1"), _("Unexpected exception during session setup, possibly invalid audio/midi engine parameters. Please see stdout/stderr for details")));
			break;
		}
	}

	if (!mix_template.empty()) {
		/* fixup monitor-sends */
		if (Config->get_use_monitor_bus ()) {
			/* Session::config_changed will have set use-monitor-bus to match the template.
			 * search for want_ms, have_ms
			 */
			assert (_monitor_out);
			/* ..but sends do not exist, since templated track bitslots are unset */
			setup_route_monitor_sends (true, true);
		} else {
			/* remove any monitor-sends that may be in the template */
			assert (!_monitor_out);
			setup_route_monitor_sends (false, true);
		}
	}

	if (!unnamed) {
		store_recent_sessions (_name, _path);
	}

	bool was_dirty = dirty();

	PresentationInfo::Change.connect_same_thread (*this, std::bind (&Session::notify_presentation_info_change, this, _1));

	Config->ParameterChanged.connect_same_thread (*this, std::bind (&Session::config_changed, this, _1, false));
	config.ParameterChanged.connect_same_thread (*this, std::bind (&Session::config_changed, this, _1, true));

	StartTimeChanged.connect_same_thread (*this, std::bind (&Session::start_time_changed, this, _1));
	EndTimeChanged.connect_same_thread (*this, std::bind (&Session::end_time_changed, this, _1));

	LatentSend::ChangedLatency.connect_same_thread (*this, std::bind (&Session::send_latency_compensation_change, this));
	LatentSend::QueueUpdate.connect_same_thread (*this, std::bind (&Session::update_send_delaylines, this));
	Latent::DisableSwitchChanged.connect_same_thread (*this, std::bind (&Session::queue_latency_recompute, this));

	Controllable::ControlTouched.connect_same_thread (*this, std::bind (&Session::controllable_touched, this, _1));

	Location::cue_change.connect_same_thread (*this, std::bind (&Session::cue_marker_change, this, _1));

	IOPluginsChanged.connect_same_thread (*this, std::bind (&Session::resort_io_plugs, this));

	TempoMap::MapChanged.connect_same_thread (*this, std::bind (&Session::tempo_map_changed, this));

	emit_thread_start ();
	auto_connect_thread_start ();

	/* hook us up to the engine since we are now completely constructed */

	BootMessage (_("Connect to engine"));

	_engine.set_session (this);
	_engine.reset_timebase ();

	if (!mix_template.empty ()) {
		/* ::create() unsets _is_new after creating the session.
		 * But for templated sessions, the sample-rate is initially unset
		 * (not read from template), so we need to save it (again).
		 */
		_is_new = true;
	}

	/* unsets dirty flag */
	session_loaded ();

	if (_is_new && unnamed) {
		set_dirty ();
		was_dirty = false;
	}

	if (was_dirty) {
		DirtyChanged (); /* EMIT SIGNAL */
	}

	_is_new = false;

	if (need_template_resave) {
		save_template (mix_template, template_description, true);
	}

	BootMessage (_("Session loading complete"));
}

Session::~Session ()
{
#ifdef PT_TIMING
	ST.dump ("ST.dump");
#endif
	destroy ();
}

unsigned int
Session::next_name_id ()
{
	return _name_id_counter.fetch_add (1);
}

unsigned int
Session::name_id_counter ()
{
	return _name_id_counter.load ();
}

void
Session::init_name_id_counter (guint n)
{
	_name_id_counter.store (n);
}

int
Session::immediately_post_engine ()
{
	/* Do various initializations that should take place directly after we
	 * know that the engine is running, but before we either create a
	 * session or set state for an existing one.
	 */
	Port::setup_resampler (Config->get_port_resampler_quality ());

	_process_graph.reset (new Graph (*this));
	_rt_tasklist.reset (new RTTaskList (_process_graph));

	_io_tasklist.reset (new IOTaskList (how_many_io_threads ()));

	/* every time we reconnect, recompute worst case output latencies */

	_engine.Running.connect_same_thread (*this, std::bind (&Session::initialize_latencies, this));

	/* Restart transport FSM */

	_transport_fsm->start ();

	/* every time we reconnect, do stuff ... */

	_engine.Running.connect_same_thread (*this, std::bind (&Session::engine_running, this));

	try {
		BootMessage (_("Set up LTC"));
		setup_ltc ();
		BootMessage (_("Set up Click"));
		setup_click ();
		BootMessage (_("Set up standard connections"));
		setup_bundles ();
	}

	catch (failed_constructor& err) {
		return -1;
	}

	/* TODO, connect in different thread. (PortRegisteredOrUnregistered may be in RT context)
	 * can we do that? */
	 _engine.PortRegisteredOrUnregistered.connect_same_thread (*this, std::bind (&Session::port_registry_changed, this));
	 _engine.PortPrettyNameChanged.connect_same_thread (*this, std::bind (&Session::setup_bundles, this));

	// set samplerate for plugins added early
	// e.g from templates or MB channelstrip
	set_block_size (_engine.samples_per_cycle());
	set_sample_rate (_engine.sample_rate());

	return 0;
}

void
Session::destroy ()
{
	/* if we got to here, leaving pending state around
	 * is a mistake.
	 */

	remove_pending_capture_state ();

	Analyser::flush ();

	_state_of_the_state = StateOfTheState (CannotSave | Deletion);

	{
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		ltc_tx_cleanup();
		if (_ltc_output_port) {
			AudioEngine::instance()->unregister_port (_ltc_output_port);
		}
	}

	/* disconnect from any and all signals that we are connected to */

	Port::PortSignalDrop (); /* EMIT SIGNAL */
	drop_connections ();

	/* stop auto dis/connecting */
	auto_connect_thread_terminate ();

	/* shutdown control surface protocols while we still have ports
	 * and the engine to move data to any devices.
	 */
	ControlProtocolManager::instance().drop_protocols ();

	_engine.remove_session ();

	/* deregister all ports - there will be no process or any other
	 * callbacks from the engine any more.
	 */

	Port::PortDrop (); /* EMIT SIGNAL */

	/* remove I/O objects that we (the session) own */
	_click_io.reset ();
	_click_io_connection.disconnect ();

	{
		Glib::Threads::Mutex::Lock lm (controllables_lock);
		for (Controllables::iterator i = controllables.begin(); i != controllables.end(); ++i) {
			(*i)->DropReferences (); /* EMIT SIGNAL */
		}
		controllables.clear ();
	}

	/* clear history so that no references to objects are held any more */

	_history.clear ();

	/* clear state tree so that no references to objects are held any more */

	delete state_tree;
	state_tree = 0;

	{
		/* unregister all lua functions, drop held references (if any) */
		Glib::Threads::Mutex::Lock tm (lua_lock, Glib::Threads::TRY_LOCK);
		if (_lua_cleanup) {
			(*_lua_cleanup)();
		}
		lua.do_command ("Session = nil");
		delete _lua_run;
		delete _lua_add;
		delete _lua_del;
		delete _lua_list;
		delete _lua_save;
		delete _lua_load;
		delete _lua_cleanup;
		lua.collect_garbage ();
	}

	/* reset dynamic state version back to default */
	Stateful::loading_state_version = 0;

	/* drop GraphNode references */
	_graph_chain.reset ();
	_current_route_graph = GraphEdges ();

	_io_graph_chain[0].reset ();
	_io_graph_chain[1].reset ();

	_io_tasklist.reset ();

	_butler->drop_references ();
	delete _butler;
	_butler = 0;

	delete _all_route_group;

	DEBUG_TRACE (DEBUG::Destruction, "delete route groups\n");
	for (list<RouteGroup *>::iterator i = _route_groups.begin(); i != _route_groups.end(); ++i) {
		delete *i;
	}

	if (click_data != default_click) {
		delete [] click_data;
	}

	if (click_emphasis_data != default_click_emphasis) {
		delete [] click_emphasis_data;
	}

	clear_clicks ();

	/* need to remove auditioner before monitoring section
	 * otherwise it is re-connected.
	 * Note: If a session was never successfully loaded, there
	 * may not yet be an auditioner.
	 */
	if (auditioner) {
		auditioner->drop_references ();
	}
	auditioner.reset ();

	/* unregister IO Plugin */
	{
		RCUWriter<IOPlugList> writer (_io_plugins);
		std::shared_ptr<IOPlugList> iop = writer.get_copy ();
		for (auto const& i : *iop) {
			i->DropReferences ();
		}
		iop->clear ();
	}

	/* drop references to routes held by the monitoring section
	 * specifically _monitor_out aux/listen references */
	remove_monitor_section();

	/* clear out any pending dead wood from RCU managed objects */

	routes.flush ();
	_bundles.flush ();
	_io_plugins.flush ();

	/* tell everyone who is still standing that we're about to die */
	drop_references ();

	/* tell everyone to drop references and delete objects as we go */

	DEBUG_TRACE (DEBUG::Destruction, "delete regions\n");
	RegionFactory::delete_all_regions ();

	/* Do this early so that VCAs no longer hold references to routes */

	DEBUG_TRACE (DEBUG::Destruction, "delete vcas\n");
	delete _vca_manager;

	DEBUG_TRACE (DEBUG::Destruction, "delete routes\n");

	/* reset these three references to special routes before we do the usual route delete thing */

	_master_out.reset ();
	_monitor_out.reset ();
	_surround_master.reset ();

	{
		RCUWriter<RouteList> writer (routes);
		std::shared_ptr<RouteList> r = writer.get_copy ();

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			DEBUG_TRACE(DEBUG::Destruction, string_compose ("Dropping for route %1 ; pre-ref = %2\n", (*i)->name(), (*i).use_count()));
			(*i)->drop_references ();
			DEBUG_TRACE(DEBUG::Destruction, string_compose ("post pre-ref = %2\n", (*i)->name(), (*i).use_count()));
		}

		r->clear ();
		/* writer goes out of scope and updates master */
	}
	routes.flush ();

	{
		DEBUG_TRACE (DEBUG::Destruction, "delete sources\n");
		Glib::Threads::Mutex::Lock lm (source_lock);
		for (SourceMap::iterator i = sources.begin(); i != sources.end(); ++i) {
			DEBUG_TRACE(DEBUG::Destruction, string_compose ("Dropping for source %1 ; pre-ref = %2\n", i->second->name(), i->second.use_count()));
			i->second->drop_references ();
		}

		sources.clear ();
	}

	/* not strictly necessary, but doing it here allows the shared_ptr debugging to work */
	_playlists.reset ();

	emit_thread_terminate ();

	pthread_cond_destroy (&_rt_emit_cond);
	pthread_mutex_destroy (&_rt_emit_mutex);

	pthread_cond_destroy (&_auto_connect_cond);
	pthread_mutex_destroy (&_auto_connect_mutex);

	delete _scene_changer; _scene_changer = 0;
	delete midi_control_ui; midi_control_ui = 0;

	delete _mmc; _mmc = 0;
	delete _midi_ports; _midi_ports = 0;
	delete _locations; _locations = 0;

	delete midi_clock;

	/* clear event queue, the session is gone, nobody is interested in
	 * those anymore, but they do leak memory if not removed
	 */
	while (!immediate_events.empty ()) {
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		SessionEvent *ev = immediate_events.front ();
		DEBUG_TRACE (DEBUG::SessionEvents, string_compose ("Drop event: %1\n", enum_2_string (ev->type)));
		immediate_events.pop_front ();
		bool remove = true;
		bool del = true;
		switch (ev->type) {
		case SessionEvent::AutoLoop:
		case SessionEvent::Skip:
		case SessionEvent::PunchIn:
		case SessionEvent::PunchOut:
		case SessionEvent::RangeStop:
		case SessionEvent::RangeLocate:
		case SessionEvent::RealTimeOperation:
			process_rtop (ev);
			del = false;
			break;
		default:
			break;
		}
		if (remove) {
			del = del && !_remove_event (ev);
		}
		if (del) {
			delete ev;
		}
	}

	{
		/* unregister all dropped ports, process pending port deletion. */
		// this may call ARDOUR::Port::drop ... jack_port_unregister ()
		// jack1 cannot cope with removing ports while processing
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		AudioEngine::instance()->clear_pending_port_deletions ();
	}

	DEBUG_TRACE (DEBUG::Destruction, "delete selection\n");
	delete _selection;
	_selection = 0;

	_transport_fsm->stop ();

#ifdef VST3_SUPPORT
	/* close VST3 Modules */
	for (auto const& nfo : PluginManager::instance().vst3_plugin_info()) {
		std::dynamic_pointer_cast<VST3PluginInfo> (nfo)->m.reset ();
	}
#endif // VST3_SUPPORT

	DEBUG_TRACE (DEBUG::Destruction, "Session::destroy() done\n");

#ifndef NDEBUG
	Controllable::dump_registry ();
#endif

	BOOST_SHOW_POINTERS ();
}

void
Session::port_registry_changed()
{
	setup_bundles ();
	_butler->delegate (std::bind (&Session::probe_ctrl_surfaces, this));
}

void
Session::probe_ctrl_surfaces()
{
	if (!_engine.running() || deletion_in_progress ()) {
		return;
	}
	ControlProtocolManager::instance ().probe_midi_control_protocols ();
}

void
Session::block_processing()
{
	_processing_prohibited.store (1);

	/* processing_blocked() is only checked at the beginning
	 * of the next cycle. So wait until any ongoing
	 * process-callback returns.
	 */
	Glib::Threads::Mutex::Lock lm (_engine.process_lock());
	/* latency callback may be in process, wait until it completed */
	Glib::Threads::Mutex::Lock lx (_engine.latency_lock());
}

void
Session::setup_ltc ()
{
	_ltc_output_port = AudioEngine::instance()->register_output_port (DataType::AUDIO, X_("LTC-Out"), false, TransportGenerator);

	{
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		/* TODO use auto-connect thread */
		reconnect_ltc_output ();
	}
}

void
Session::setup_click ()
{
	_clicking = false;

	std::shared_ptr<AutomationList> gl (new AutomationList (Evoral::Parameter (GainAutomation), Temporal::TimeDomainProvider (Temporal::AudioTime)));
	std::shared_ptr<GainControl> gain_control = std::shared_ptr<GainControl> (new GainControl (*this, Evoral::Parameter(GainAutomation), gl));

	_click_io.reset (new ClickIO (*this, X_("Click")));
	_click_gain.reset (new Amp (*this, _("Fader"), gain_control, true));
	_click_gain->activate ();
	if (state_tree) {
		setup_click_state (state_tree->root());
	} else {
		setup_click_state (0);
	}
	click_io_resync_latency (true);
	LatencyUpdated.connect_same_thread (_click_io_connection, std::bind (&Session::click_io_resync_latency, this, _1));
}

void
Session::setup_click_state (const XMLNode* node)
{
	const XMLNode* child = 0;

	if (node && (child = find_named_node (*node, "Click")) != 0) {

		/* existing state for Click */
		int c = 0;

		if (Stateful::loading_state_version < 3000) {
			c = _click_io->set_state_2X (*child->children().front(), Stateful::loading_state_version, false);
		} else {
			const XMLNodeList& children (child->children());
			XMLNodeList::const_iterator i = children.begin();
			if ((c = _click_io->set_state (**i, Stateful::loading_state_version)) == 0) {
				++i;
				if (i != children.end()) {
					c = _click_gain->set_state (**i, Stateful::loading_state_version);
				}
			}
		}

		if (c == 0) {
			_clicking = Config->get_clicking ();

		} else {

			error << _("could not setup Click I/O") << endmsg;
			_clicking = false;
		}


	} else {

		/* default state for Click: dual-mono to first 2 physical outputs */

		vector<string> outs;
		_engine.get_physical_outputs (DataType::AUDIO, outs);

		for (uint32_t physport = 0; physport < 2; ++physport) {
			if (outs.size() > physport) {
				if (_click_io->add_port (outs[physport], this)) {
					// relax, even though its an error
				}
			}
		}

		if (_click_io->n_ports () > ChanCount::ZERO) {
			_clicking = Config->get_clicking ();
		}
	}
}

void
Session::get_physical_ports (vector<string>& inputs, vector<string>& outputs, DataType type,
                             MidiPortFlags include, MidiPortFlags exclude)
{
	_engine.get_physical_inputs (type, inputs, include, exclude);
	_engine.get_physical_outputs (type, outputs, include, exclude);
}

void
Session::auto_connect_io (std::shared_ptr<IO> io)
{
	vector<string> outputs[DataType::num_types];

	for (uint32_t i = 0; i < DataType::num_types; ++i) {
		_engine.get_physical_outputs (DataType (DataType::Symbol (i)), outputs[i]);
	}

	uint32_t limit = io->n_ports ().n_total ();

	for (uint32_t n = 0; n < limit; ++n) {
		std::shared_ptr<Port> p = io->nth (n);
		string connect_to;
		if (outputs[p->type()].size() > n) {
			connect_to = outputs[p->type()][n];
		}
		if (connect_to.empty() || p->connected_to (connect_to)) {
			continue;
		}

		if (io->connect (p, connect_to, this)) {
			error << string_compose (_("cannot connect %1 output %2 to %3"), io->name(), n, connect_to) << endmsg;
			break;
		}
	}
}

void
Session::auto_connect_master_bus ()
{
	if (!_master_out || !Config->get_auto_connect_standard_busses() || _monitor_out) {
		return;
	}

	/* if requested auto-connect the outputs to the first N physical ports.  */
	auto_connect_io (_master_out->output());
}

std::shared_ptr<GainControl>
Session::master_volume () const
{
	if (_master_out) {
		return _master_out->volume_control ();
	}
	return std::shared_ptr<GainControl> ();
}

void
Session::remove_monitor_section ()
{
	if (!_monitor_out) {
		return;
	}

	/* allow deletion when session is unloaded */
	if (!_engine.running() && !deletion_in_progress ()) {
		error << _("Cannot remove monitor section while the engine is offline.") << endmsg;
		return;
	}

	/* force reversion to Solo-In-Place */
	Config->set_solo_control_is_listen_control (false);

	/* if we are auditioning, cancel it ... this is a workaround
	   to a problem (auditioning does not execute the process graph,
	   which is needed to remove routes when using >1 core for processing)
	*/
	cancel_audition ();

	if (!deletion_in_progress ()) {
		setup_route_monitor_sends (false, true);
		_engine.monitor_port().clear_ports (true);
	}

	remove_route (_monitor_out);
	_monitor_out.reset ();
	if (deletion_in_progress ()) {
		return;
	}

	auto_connect_master_bus ();

	if (auditioner) {
		auditioner->connect ();
	}

	MonitorBusAddedOrRemoved (); /* EMIT SIGNAL */
}

void
Session::add_monitor_section ()
{
	RouteList rl;

	if (!_engine.running()) {
		error << _("Cannot create monitor section while the engine is offline.") << endmsg;
		return;
	}

	if (_monitor_out || !_master_out) {
		return;
	}

	std::shared_ptr<Route> r (new Route (*this, _("Monitor"), PresentationInfo::MonitorOut, DataType::AUDIO));

	if (r->init ()) {
		return;
	}

	BOOST_MARK_ROUTE(r);

	try {
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		r->input()->ensure_io (_master_out->output()->n_ports(), false, this);
		r->output()->ensure_io (_master_out->output()->n_ports(), false, this);
	} catch (...) {
		error << _("Cannot create monitor section. 'Monitor' Port name is not unique.") << endmsg;
		return;
	}

	rl.push_back (r);
	add_routes (rl, false, false, 0);

	assert (_monitor_out);

	/* AUDIO ONLY as of june 29th 2009, because listen semantics for anything else
	   are undefined, at best.
	*/

	uint32_t limit = _monitor_out->n_inputs().n_audio();

	if (_master_out) {

		/* connect the inputs to the master bus outputs. this
		 * represents a separate data feed from the internal sends from
		 * each route. as of jan 2011, it allows the monitor section to
		 * conditionally ignore either the internal sends or the normal
		 * input feed, but we should really find a better way to do
		 * this, i think.
		 */

		_master_out->output()->disconnect (this);

		for (uint32_t n = 0; n < limit; ++n) {
			std::shared_ptr<AudioPort> p = _monitor_out->input()->ports()->nth_audio_port (n);
			std::shared_ptr<AudioPort> o = _master_out->output()->ports()->nth_audio_port (n);

			if (o) {
				string connect_to = o->name();
				if (_monitor_out->input()->connect (p, connect_to, this)) {
					error << string_compose (_("cannot connect control input %1 to %2"), n, connect_to)
					      << endmsg;
					break;
				}
			}
		}
	}

	auto_connect_monitor_bus ();

	/* Hold process lock while doing this so that we don't hear bits and
	 * pieces of audio as we work on each route.
	 */

	setup_route_monitor_sends (true, true);

	MonitorBusAddedOrRemoved (); /* EMIT SIGNAL */
}

void
Session::auto_connect_monitor_bus ()
{
	if (!_master_out || !_monitor_out) {
		return;
	}

	if ((!Config->get_auto_connect_standard_busses () && !Profile->get_mixbus ()) || _monitor_out->output()->connected ()) {
		return;
	}

	/* if monitor section is not connected, connect it to physical outs */

	if (!Config->get_monitor_bus_preferred_bundle().empty()) {

		std::shared_ptr<Bundle> b = bundle_by_name (Config->get_monitor_bus_preferred_bundle());

		if (b) {
			_monitor_out->output()->connect_ports_to_bundle (b, true, this);
		} else {
			warning << string_compose (_("The preferred I/O for the monitor bus (%1) cannot be found"),
					Config->get_monitor_bus_preferred_bundle())
				<< endmsg;
		}

	} else {

		/* Monitor bus is audio only */

		vector<string> outputs[DataType::num_types];

		for (uint32_t i = 0; i < DataType::num_types; ++i) {
			_engine.get_physical_outputs (DataType (DataType::Symbol (i)), outputs[i]);
		}

		uint32_t mod = outputs[DataType::AUDIO].size();
		uint32_t limit = _monitor_out->n_outputs().get (DataType::AUDIO);

		if (mod != 0) {

			for (uint32_t n = 0; n < limit; ++n) {

				std::shared_ptr<Port> p = _monitor_out->output()->ports()->port(DataType::AUDIO, n);
				string connect_to;
				if (outputs[DataType::AUDIO].size() > (n % mod)) {
					connect_to = outputs[DataType::AUDIO][n % mod];
				}

				if (!connect_to.empty()) {
					if (_monitor_out->output()->connect (p, connect_to, this)) {
						error << string_compose (
								_("cannot connect control output %1 to %2"),
								n, connect_to)
							<< endmsg;
						break;
					}
				}
			}
		}
	}
}

void
Session::setup_route_monitor_sends (bool enable, bool need_process_lock)
{
	Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock (), Glib::Threads::NOT_LOCK);
	if (need_process_lock) {
		/* Hold process lock while doing this so that we don't hear bits and
		 * pieces of audio as we work on each route.
		 */
		lx.acquire();
	}

	std::shared_ptr<RouteList const> rl = routes.reader ();
	ProcessorChangeBlocker  pcb (this, false /* XXX */);

	for (auto const& x : *rl) {
		if (x->can_monitor ()) {
			if (enable) {
				x->enable_monitor_send ();
			} else {
				x->remove_monitor_send ();
			}
		}
	}

	if (auditioner) {
		auditioner->connect ();
	}
}


void
Session::reset_monitor_section ()
{
	/* Process lock should be held by the caller.*/

	if (!_monitor_out) {
		return;
	}

	uint32_t limit = _master_out->n_outputs().n_audio();

	/* connect the inputs to the master bus outputs. this
	 * represents a separate data feed from the internal sends from
	 * each route. as of jan 2011, it allows the monitor section to
	 * conditionally ignore either the internal sends or the normal
	 * input feed, but we should really find a better way to do
	 * this, i think.
	 */

	_master_out->output()->disconnect (this);
	_monitor_out->output()->disconnect (this);

	// monitor section follow master bus - except midi
	ChanCount mon_chn (_master_out->output()->n_ports());
	mon_chn.set_midi (0);

	_monitor_out->input()->ensure_io (mon_chn, false, this);
	_monitor_out->output()->ensure_io (mon_chn, false, this);

	for (uint32_t n = 0; n < limit; ++n) {
		std::shared_ptr<AudioPort> p = _monitor_out->input()->ports()->nth_audio_port (n);
		std::shared_ptr<AudioPort> o = _master_out->output()->ports()->nth_audio_port (n);

		if (o) {
			string connect_to = o->name();
			if (_monitor_out->input()->connect (p, connect_to, this)) {
				error << string_compose (_("cannot connect control input %1 to %2"), n, connect_to)
				      << endmsg;
				break;
			}
		}
	}

	/* connect monitor section to physical outs */

	if (Config->get_auto_connect_standard_busses()) {

		if (!Config->get_monitor_bus_preferred_bundle().empty()) {

			std::shared_ptr<Bundle> b = bundle_by_name (Config->get_monitor_bus_preferred_bundle());

			if (b) {
				_monitor_out->output()->connect_ports_to_bundle (b, true, this);
			} else {
				warning << string_compose (_("The preferred I/O for the monitor bus (%1) cannot be found"),
							   Config->get_monitor_bus_preferred_bundle())
					<< endmsg;
			}

		} else {

			/* Monitor bus is audio only */

			vector<string> outputs[DataType::num_types];

			for (uint32_t i = 0; i < DataType::num_types; ++i) {
				_engine.get_physical_outputs (DataType (DataType::Symbol (i)), outputs[i]);
			}

			uint32_t mod = outputs[DataType::AUDIO].size();
			uint32_t limit = _monitor_out->n_outputs().get (DataType::AUDIO);

			if (mod != 0) {

				for (uint32_t n = 0; n < limit; ++n) {

					std::shared_ptr<Port> p = _monitor_out->output()->ports()->port(DataType::AUDIO, n);
					string connect_to;
					if (outputs[DataType::AUDIO].size() > (n % mod)) {
						connect_to = outputs[DataType::AUDIO][n % mod];
					}

					if (!connect_to.empty()) {
						if (_monitor_out->output()->connect (p, connect_to, this)) {
							error << string_compose (
								_("cannot connect control output %1 to %2"),
								n, connect_to)
							      << endmsg;
							break;
						}
					}
				}
			}
		}
	}

	setup_route_monitor_sends (true, false);
}

void
Session::remove_surround_master ()
{
	if (!_surround_master) {
		return;
	}

	/* allow deletion when session is unloaded */
	if (!_engine.running() && !deletion_in_progress ()) {
		error << _("Cannot remove monitor section while the engine is offline.") << endmsg;
		return;
	}

	/* if we are auditioning, cancel it ... this is a workaround
	   to a problem (auditioning does not execute the process graph,
	   which is needed to remove routes when using >1 core for processing)
	*/
	cancel_audition ();

	if (!deletion_in_progress ()) {
		setup_route_surround_sends (false, true);
		_engine.monitor_port().clear_ports (true);
	}

	remove_route (_surround_master);
	_surround_master.reset ();

	if (deletion_in_progress ()) {
		return;
	}

	SurroundMasterAddedOrRemoved (); /* EMIT SIGNAL */
}

bool
Session::vapor_barrier ()
{
#if !(defined (LV2_EXTENDED) && defined (HAVE_LV2_1_10_0))
	return false;
#else
	if (_vapor_available.has_value ()) {
		return _vapor_available.value ();
	}

	bool ok = false;
	bool ex = false;

	if (nominal_sample_rate () == 48000 || nominal_sample_rate () == 96000) {
		std::shared_ptr<LV2Plugin> p;

		if (_surround_master) {
			p = _surround_master->surround_return ()->surround_processor ();
		} else {
			PluginManager& mgr (PluginManager::instance ());
			for (auto const& i : mgr.lv2_plugin_info ()) {
				if ("urn:ardour:a-vapor" != i->unique_id) {
					continue;
				}
				p = std::dynamic_pointer_cast<LV2Plugin> (i->load (*this));
				break;
			}
		}
		if (p) {
			ok = true;
			ex = p->can_export ();
		}
	}

	_vapor_exportable = ex;
	_vapor_available = ok;

	return ok;
#endif
}

bool
Session::vapor_export_barrier ()
{
#if !(defined (LV2_EXTENDED) && defined (HAVE_LV2_1_10_0))
	return false;
#endif
	if (!_vapor_exportable.has_value ()) {
		vapor_barrier ();
	}
	assert (_vapor_exportable.has_value ());
	return _vapor_exportable.value ();
}

void
Session::add_surround_master ()
{
	RouteList rl;

	if (_surround_master) {
		return;
	}

	if (!_engine.running()) {
		error << _("Cannot create surround master while the engine is offline.") << endmsg;
		return;
	}

	if (!vapor_barrier()) {
		error << _("Some surround sound systems require a sample-rate of 48kHz or 96kHz.") << endmsg;
		return;
	}

	std::shared_ptr<Route> r (new Route (*this, _("Surround"), PresentationInfo::SurroundMaster, DataType::AUDIO));

	if (r->init ()) {
		return;
	}

	BOOST_MARK_ROUTE(r);

	try {
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		r->input()->ensure_io (ChanCount (), false, this);
		r->output()->ensure_io (ChanCount (DataType::AUDIO, 16), false, this);
	} catch (...) {
		error << _("Cannot create surround master. 'Surround' Port name is not unique.") << endmsg;
		return;
	}

	rl.push_back (r);
	add_routes (rl, false, false, 0);

	assert (_surround_master);

	auto_connect_surround_master ();

	/* Hold process lock while doing this so that we don't hear bits and
	 * pieces of audio as we work on each route.
	 */

	setup_route_surround_sends (true, true);

	SurroundMasterAddedOrRemoved (); /* EMIT SIGNAL */
}

void
Session::auto_connect_surround_master ()
{
	/* compare to auto_connect_io */
	vector<string> outputs;
	_engine.get_physical_outputs (DataType::AUDIO, outputs);

	std::shared_ptr<IO> io    = _surround_master->output ();
	uint32_t            limit = io->n_ports ().n_audio ();

	Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
	/* connect binaural outputs, port 12, 13 */
	for (uint32_t n = 12, p = 0; n < limit && outputs.size () > p; ++n, ++p) {
		std::shared_ptr<AudioPort> ap = io->audio (n);

		if (io->connect (ap, outputs[p], this)) {
			error << string_compose (_("cannot connect %1 output %2 to %3"), io->name(), n, outputs[p]) << endmsg;
			break;
		}
	}
	lm.release ();

	if (_master_out) {
		_master_out->mute_control ()->set_value (true, PBD::Controllable::NoGroup);
	}

}

void
Session::setup_route_surround_sends (bool enable, bool need_process_lock)
{
	Glib::Threads::Mutex::Lock lx (AudioEngine::instance()->process_lock (), Glib::Threads::NOT_LOCK);
	if (need_process_lock) {
		/* Hold process lock while doing this so that we don't hear bits and
		 * pieces of audio as we work on each route.
		 */
		lx.acquire();
	}

	std::shared_ptr<RouteList const> rl = routes.reader ();
	ProcessorChangeBlocker  pcb (this, false /* XXX */);

	for (auto const& x : *rl) {
		if (x->can_monitor ()) {
			if (enable) {
				x->enable_surround_send ();
			} else {
				x->remove_surround_send ();
			}
		}
	}
}

int
Session::add_master_bus (ChanCount const& count)
{
	if (master_out ()) {
		return -1;
	}

	RouteList rl;

	std::shared_ptr<Route> r (new Route (*this, _("Master"), PresentationInfo::MasterOut, DataType::AUDIO));
	if (r->init ()) {
		return -1;
	}

	BOOST_MARK_ROUTE(r);

	{
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		r->input()->ensure_io (count, false, this);
		r->output()->ensure_io (count, false, this);
	}

	rl.push_back (r);
	add_routes (rl, false, false, PresentationInfo::max_order);
	return 0;
}

void
Session::hookup_io ()
{
	/* stop graph reordering notifications from
	   causing resorts, etc.
	*/

	_state_of_the_state = StateOfTheState (_state_of_the_state | InitialConnecting);

	if (!auditioner) {

		/* we delay creating the auditioner till now because
		   it makes its own connections to ports.
		*/

		try {
			std::shared_ptr<Auditioner> a (new Auditioner (*this));
			if (a->init()) {
				throw failed_constructor ();
			}
			auditioner = a;
		}

		catch (failed_constructor& err) {
			warning << _("cannot create Auditioner: no auditioning of regions possible") << endmsg;
		}
	}

	/* load bundles, which we may have postponed earlier on */
	if (_bundle_xml_node) {
		load_bundles (*_bundle_xml_node);
		delete _bundle_xml_node;
	}

	/* Get everything connected */

	AudioEngine::instance()->reconnect_ports ();

	AfterConnect (); /* EMIT SIGNAL */

	/* Anyone who cares about input state, wake up and do something */

	IOConnectionsComplete (); /* EMIT SIGNAL */

	_state_of_the_state = StateOfTheState (_state_of_the_state & ~InitialConnecting);

	/* now handle the whole enchilada as if it was one
	   graph reorder event.
	*/

	graph_reordered (false);

	/* update the full solo state, which can't be
	   correctly determined on a per-route basis, but
	   needs the global overview that only the session
	   has.
	*/

	update_route_solo_state ();
}

void
Session::track_playlist_changed (std::weak_ptr<Track> wp)
{
	std::shared_ptr<Track> track = wp.lock ();
	if (!track) {
		return;
	}

	std::shared_ptr<Playlist> playlist;

	if ((playlist = track->playlist()) != 0) {
		playlist->RegionAdded.connect_same_thread (*this, std::bind (&Session::playlist_region_added, this, _1));
		playlist->RangesMoved.connect_same_thread (*this, std::bind (&Session::playlist_ranges_moved, this, _1));
		playlist->RegionsExtended.connect_same_thread (*this, std::bind (&Session::playlist_regions_extended, this, _1));
	}
}

bool
Session::record_enabling_legal () const
{
	if (Config->get_all_safe()) {
		return false;
	}
	return true;
}

void
Session::set_track_monitor_input_status (bool yn)
{
	std::shared_ptr<RouteList const> rl = routes.reader ();

	for (auto const& i : *rl) {
		std::shared_ptr<AudioTrack> tr = std::dynamic_pointer_cast<AudioTrack> (i);
		if (tr && tr->rec_enable_control()->get_value()) {
			tr->request_input_monitoring (yn);
		}
	}
}

void
Session::auto_punch_start_changed (Location* location)
{
	replace_event (SessionEvent::PunchIn, location->start_sample());

	if (get_record_enabled() && config.get_punch_in() && !actively_recording ()) {
		/* capture start has been changed, so save new pending state */
		save_state ("", true);
	}
}

bool
Session::punch_active () const
{
	if (!get_record_enabled ()) {
		return false;
	}
	if (!_locations->auto_punch_location ()) {
		return false;
	}
	return config.get_punch_in () || config.get_punch_out ();
}

bool
Session::punch_is_possible () const
{
	return _punch_or_loop.load () != OnlyLoop;
}

bool
Session::loop_is_possible () const
{
#if 0 /* maybe prevent looping even when not rolling ? */
	if (get_record_enabled () && punch_active ()) {
			return false;
		}
	}
#endif
	return _punch_or_loop.load () != OnlyPunch;
}

void
Session::reset_punch_loop_constraint ()
{
	if (_punch_or_loop.load () == NoConstraint) {
		return;
	}
	_punch_or_loop.store (NoConstraint);
	PunchLoopConstraintChange (); /* EMIT SIGNAL */
}

bool
Session::maybe_allow_only_loop (bool play_loop) {
	if (!(get_play_loop () || play_loop)) {
		return false;
	}
	PunchLoopLock nocon (NoConstraint);
	bool rv = _punch_or_loop.compare_exchange_strong (nocon, OnlyLoop);
	if (rv) {
		PunchLoopConstraintChange (); /* EMIT SIGNAL */
	}
	if (rv || loop_is_possible ()) {
		unset_punch ();
		return true;
	}
	return false;
}

bool
Session::maybe_allow_only_punch () {
	if (!punch_active ()) {
		return false;
	}
	PunchLoopLock nocon (NoConstraint);
	bool rv = _punch_or_loop.compare_exchange_strong (nocon, OnlyPunch);
	if (rv) {
		PunchLoopConstraintChange (); /* EMIT SIGNAL */
	}
	return rv || punch_is_possible ();
}

void
Session::unset_punch ()
{
	/* used when enabling looping
	 * -> _punch_or_loop = OnlyLoop;
	 */
	if (config.get_punch_in ()) {
		config.set_punch_in (false);
	}
	if (config.get_punch_out ()) {
		config.set_punch_out (false);
	}
}

void
Session::auto_punch_end_changed (Location* location)
{
	replace_event (SessionEvent::PunchOut, location->end_sample());
}

void
Session::auto_punch_changed (Location* location)
{
	auto_punch_start_changed (location);
	auto_punch_end_changed (location);
}

void
Session::auto_loop_changed (Location* location)
{
	if (!location) {
		return;
	}

	replace_event (SessionEvent::AutoLoop, location->end_sample(), location->start_sample());

	if (transport_rolling()) {

		if (get_play_loop ()) {

			if (_transport_sample < location->start_sample() || _transport_sample > location->end_sample()) {

				/* new loop range excludes current transport
				 * sample => relocate to beginning of loop and roll.
				 */

				/* Set this so that when/if we have to stop the
				 * transport for a locate, we know that it is triggered
				 * by loop-changing, and we do not cancel play loop
				 */

				loop_changing = true;
				request_locate (location->start_sample(), false, MustRoll);

			} else {

				// schedule a locate-roll to refill the diskstreams at the
				// previous loop end

				/* schedule a buffer overwrite to refill buffers with the new loop. */
				SessionEvent *ev = new SessionEvent (SessionEvent::OverwriteAll, SessionEvent::Add, SessionEvent::Immediate, 0, 0, 0.0);
				ev->overwrite = LoopChanged;
				queue_event (ev);
			}
		}

	} else {

		/* possibly move playhead if not rolling; if we are rolling we'll move
		   to the loop start on stop if that is appropriate.
		*/

		samplepos_t pos;

		if (select_playhead_priority_target (pos)) {
			if (pos == location->start_sample()) {
				request_locate (pos);
			}
		}
	}

	last_loopend = location->end_sample();
	set_dirty ();
}

void
Session::set_auto_punch_location (Location* location)
{
	Location* existing;

	if ((existing = _locations->auto_punch_location()) != 0 && existing != location) {
		punch_connections.drop_connections();
		existing->set_auto_punch (false, this);
		clear_events (SessionEvent::PunchIn);
		clear_events (SessionEvent::PunchOut);
		auto_punch_location_changed (0);
	}

	set_dirty();

	if (location == 0) {
		return;
	}

	if (location->end() <= location->start()) {
		error << _("Session: you can't use that location for auto punch (start <= end)") << endmsg;
		return;
	}

	punch_connections.drop_connections ();

	location->StartChanged.connect_same_thread (punch_connections, std::bind (&Session::auto_punch_start_changed, this, location));
	location->EndChanged.connect_same_thread (punch_connections, std::bind (&Session::auto_punch_end_changed, this, location));
	location->Changed.connect_same_thread (punch_connections, std::bind (&Session::auto_punch_changed, this, location));

	location->set_auto_punch (true, this);

	auto_punch_changed (location);

	auto_punch_location_changed (location);
}

void
Session::set_session_extents (timepos_t const & start, timepos_t const & end)
{
	if (end <= start) {
		error << _("Session: you can't use that location for session start/end)") << endmsg;
		return;
	}

	Location* existing;
	if ((existing = _locations->session_range_location()) == 0) {
		_session_range_location = new Location (*this, start, end, _("session"), Location::IsSessionRange);
		_locations->add (_session_range_location);
	} else {
		existing->set( start, end );
	}

	set_dirty();
}

void
Session::set_auto_loop_location (Location* location)
{
	Location* existing;

	if ((existing = _locations->auto_loop_location()) != 0 && existing != location) {
		loop_connections.drop_connections ();
		existing->set_auto_loop (false, this);
		remove_event (existing->end_sample(), SessionEvent::AutoLoop);
		auto_loop_location_changed (0);
	}

	set_dirty();

	if (location == 0) {
		return;
	}

	if (location->end() <= location->start()) {
		error << _("You cannot use this location for auto-loop because it has zero or negative length") << endmsg;
		return;
	}

	last_loopend = location->end_sample();

	loop_connections.drop_connections ();

	location->StartChanged.connect_same_thread (loop_connections, std::bind (&Session::auto_loop_changed, this, location));
	location->EndChanged.connect_same_thread (loop_connections, std::bind (&Session::auto_loop_changed, this, location));
	location->Changed.connect_same_thread (loop_connections, std::bind (&Session::auto_loop_changed, this, location));
	location->FlagsChanged.connect_same_thread (loop_connections, std::bind (&Session::auto_loop_changed, this, location));

	location->set_auto_loop (true, this);

	if (Config->get_loop_is_mode() && get_play_loop ()) {
		/* set all tracks to use internal looping */
		set_track_loop (true);
	}

	/* take care of our stuff first */

	auto_loop_changed (location);

	/* now tell everyone else */

	auto_loop_location_changed (location);
}

void
Session::update_marks (Location*)
{
	set_dirty ();
}

void
Session::update_skips (Location* loc, bool consolidate)
{
	if (_ignore_skips_updates) {
		return;
	}

	Locations::LocationList skips;

	if (consolidate) {
		PBD::Unwinder<bool> uw (_ignore_skips_updates, true);
		consolidate_skips (loc);
	}

	sync_locations_to_skips ();

	set_dirty ();
}

void
Session::consolidate_skips (Location* loc)
{
	Locations::LocationList all_locations = _locations->list ();

	for (Locations::LocationList::iterator l = all_locations.begin(); l != all_locations.end(); ) {

		if (!(*l)->is_skip ()) {
			++l;
			continue;
		}

		/* don't test against self */

		if (*l == loc) {
			++l;
			continue;
		}

		switch (Temporal::coverage_exclusive_ends ((*l)->start(), (*l)->end(), loc->start(), loc->end())) {
			case Temporal::OverlapInternal:
			case Temporal::OverlapExternal:
			case Temporal::OverlapStart:
			case Temporal::OverlapEnd:
				/* adjust new location to cover existing one */
				loc->set_start (min (loc->start(), (*l)->start()));
				loc->set_end (max (loc->end(), (*l)->end()));
				/* we don't need this one any more */
				_locations->remove (*l);
				/* the location has been deleted, so remove reference to it in our local list */
				l = all_locations.erase (l);
				break;

			case Temporal::OverlapNone:
				++l;
				break;
		}
	}
}

void
Session::sync_locations_to_skips ()
{
	/* This happens asynchronously (in the audioengine thread). After the clear is done, we will call
	 * Session::_sync_locations_to_skips() from the audioengine thread.
	 */
	clear_events (SessionEvent::Skip, std::bind (&Session::_sync_locations_to_skips, this));
}

void
Session::_sync_locations_to_skips ()
{
	/* called as a callback after existing Skip events have been cleared from a realtime audioengine thread */

	Locations::LocationList const & locs (_locations->list());

	for (Locations::LocationList::const_iterator i = locs.begin(); i != locs.end(); ++i) {

		Location* location = *i;

		if (location->is_skip() && location->is_skipping()) {
			SessionEvent* ev = new SessionEvent (SessionEvent::Skip, SessionEvent::Add, location->start_sample(), location->end_sample(), 1.0);
			queue_event (ev);
		}
	}
}


void
Session::location_added (Location *location)
{
	if (location->is_auto_punch()) {
		set_auto_punch_location (location);
	}

	if (location->is_auto_loop()) {
		set_auto_loop_location (location);
	}

	if (location->is_session_range()) {
		/* no need for any signal handling or event setting with the session range,
			 because we keep a direct reference to it and use its start/end directly.
			 */
		_session_range_location = location;
	}

	if (location->is_mark()) {
		/* listen for per-location signals that require us to do any * global updates for marks */

		location->StartChanged.connect_same_thread (skip_update_connections, std::bind (&Session::update_marks, this, location));
		location->EndChanged.connect_same_thread (skip_update_connections, std::bind (&Session::update_marks, this, location));
		location->Changed.connect_same_thread (skip_update_connections, std::bind (&Session::update_marks, this, location));
		location->FlagsChanged.connect_same_thread (skip_update_connections, std::bind (&Session::update_marks, this, location));
		location->TimeDomainChanged.connect_same_thread (skip_update_connections, std::bind (&Session::update_marks, this, location));
	}

	if (location->is_range_marker()) {
		/* listen for per-location signals that require us to do any * global updates for marks */

		location->StartChanged.connect_same_thread (skip_update_connections, std::bind (&Session::update_marks, this, location));
		location->EndChanged.connect_same_thread (skip_update_connections, std::bind (&Session::update_marks, this, location));
		location->Changed.connect_same_thread (skip_update_connections, std::bind (&Session::update_marks, this, location));
		location->FlagsChanged.connect_same_thread (skip_update_connections, std::bind (&Session::update_marks, this, location));
		location->TimeDomainChanged.connect_same_thread (skip_update_connections, std::bind (&Session::update_marks, this, location));
	}

	if (location->is_skip()) {
		/* listen for per-location signals that require us to update skip-locate events */

		location->StartChanged.connect_same_thread (skip_update_connections, std::bind (&Session::update_skips, this, location, true));
		location->EndChanged.connect_same_thread (skip_update_connections, std::bind (&Session::update_skips, this, location, true));
		location->Changed.connect_same_thread (skip_update_connections, std::bind (&Session::update_skips, this, location, true));
		location->FlagsChanged.connect_same_thread (skip_update_connections, std::bind (&Session::update_skips, this, location, false));
		location->TimeDomainChanged.connect_same_thread (skip_update_connections, std::bind (&Session::update_marks, this, location));

		update_skips (location, true);
	}

	set_dirty ();
}

void
Session::location_removed (Location *location)
{
	if (location->is_auto_loop()) {
		set_auto_loop_location (0);
		if (!get_play_loop ()) {
			set_track_loop (false);
		}
		unset_play_loop ();
	}

	if (location->is_auto_punch()) {
		set_auto_punch_location (0);
	}

	if (location->is_session_range()) {
		/* this is never supposed to happen */
		error << _("programming error: session range removed!") << endl;
	}

	if (location->is_skip()) {

		update_skips (location, false);
	}

	set_dirty ();
}

void
Session::locations_changed ()
{
	_locations->apply (*this, &Session::_locations_changed);
}

void
Session::_locations_changed (const Locations::LocationList& locations)
{
	/* There was some mass-change in the Locations object.
	 *
	 * We might be re-adding a location here but it doesn't actually matter
	 * for all the locations that the Session takes an interest in.
	 */

	{
		PBD::Unwinder<bool> protect_ignore_skip_updates (_ignore_skips_updates, true);
		for (Locations::LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
			location_added (*i);
		}
	}

	update_skips (NULL, false);
}

void
Session::enable_record ()
{
	if (_transport_fsm->transport_speed() != 0.0 && _transport_fsm->transport_speed() != 1.0) {
		/* no recording at anything except normal speed */
		return;
	}

	while (1) {
		RecordState rs = (RecordState) _record_status.load ();

		if (rs == Recording) {
			break;
		}

		if (_record_status.compare_exchange_strong (rs, Recording)) {

			_last_record_location = _transport_sample;
			send_immediate_mmc (MIDI::MachineControlCommand (MIDI::MachineControl::cmdRecordStrobe));

			if (Config->get_recording_resets_xrun_count ()) {
				reset_xrun_count ();
			}
			if (Config->get_monitoring_model() == HardwareMonitoring && config.get_auto_input()) {
				set_track_monitor_input_status (true);
			}

			_capture_duration = 0;
			_capture_xruns = 0;

			RecordStateChanged ();
			break;
		}
	}
}

void
Session::set_all_tracks_record_enabled (bool enable )
{
	set_controls (route_list_to_control_list (routes.reader (), &Stripable::rec_enable_control), enable, Controllable::NoGroup);
}

void
Session::disable_record (bool rt_context, bool force)
{
	RecordState rs;

	if ((rs = (RecordState) _record_status.load ()) != Disabled) {

		if (!Config->get_latched_record_enable () || force) {
			_record_status.store (Disabled);
			send_immediate_mmc (MIDI::MachineControlCommand (MIDI::MachineControl::cmdRecordExit));
		} else {
			if (rs == Recording) {
				_record_status.store (Enabled);
			}
		}

		if (Config->get_monitoring_model() == HardwareMonitoring && config.get_auto_input()) {
			set_track_monitor_input_status (false);
		}

		RecordStateChanged (); /* emit signal */
	}
}

void
Session::step_back_from_record ()
{
	RecordState rs (Recording);

	if (_record_status.compare_exchange_strong (rs, Enabled)) {

		if (Config->get_monitoring_model() == HardwareMonitoring && config.get_auto_input()) {
			set_track_monitor_input_status (false);
		}

		RecordStateChanged (); /* emit signal */
	}
}

void
Session::maybe_enable_record (bool rt_context)
{
	if (_step_editors > 0) {
		return;
	}

	_record_status.store (Enabled);

	// TODO make configurable, perhaps capture-buffer-seconds dependnet?
	bool quick_start = true;

	/* Save pending state of which sources the next record will use,
	 * which gives us some chance of recovering from a crash during the record.
	 */
	if (!rt_context && (!quick_start || _transport_fsm->transport_speed() == 0)) {
		save_state ("", true);
	}

	if (_transport_fsm->transport_speed() != 0) {
		maybe_allow_only_punch ();
		if (!config.get_punch_in() || 0 == locations()->auto_punch_location ()) {
			enable_record ();
		}
		/* When rolling, start recording immediately.
		 * Do not wait for .pending state save to complete
		 * because that may take some time (up to a second
		 * for huge sessions).
		 *
		 * This is potentially dangerous!! If a crash happens
		 * while recording before the .pending save completed,
		 * the data until then may be lost or overwritten.
		 * (However disk-writer buffers are usually longer,
		 *  compared to the time it takes to save a session.
		 *  disk I/O may not be a bottleneck either. Except
		 *  perhaps plugin-state saves taking a lock.
		 */
		 if (!rt_context && quick_start) {
			 save_state ("", true);
		 }
	} else {
		send_immediate_mmc (MIDI::MachineControlCommand (MIDI::MachineControl::cmdRecordPause));
		RecordStateChanged (); /* EMIT SIGNAL */
	}

	set_dirty();
}

samplepos_t
Session::audible_sample (bool* latent_locate) const
{
	if (latent_locate) {
		*latent_locate = false;
	}

	samplepos_t ret;

	if (synced_to_engine()) {
		/* Note: this is basically just sync-to-JACK */
		ret = _engine.transport_sample();
	} else {
		ret = _transport_sample;
	}

	assert (ret >= 0);

	if (!transport_rolling()) {
		return ret;
	}

#if 0 // TODO looping
	if (_transport_fsm->transport_speed() > 0.0f) {
		if (play_loop && have_looped) {
			/* the play-position wrapped at the loop-point
			 * ardour is already playing the beginning of the loop,
			 * but due to playback latency, the "audible frame"
			 * is still at the end of the loop.
			 */
			Location *location = _locations->auto_loop_location();
			sampleoffset_t lo = location->start() - ret;
			if (lo > 0) {
				ret = location->end () - lo;
				if (latent_locate) {
					*latent_locate = true;
				}
			}
		}
	} else if (_transport_fsm->transport_speed() < 0.0f) {
		/* XXX wot? no backward looping? */
	}
#endif

	return std::max ((samplepos_t)0, ret);
}

samplecnt_t
Session::preroll_samples (samplepos_t pos) const
{
	const float pr = Config->get_preroll_seconds();
	if (pos >= 0 && pr < 0) {
		Temporal::TempoMetric const & metric (TempoMap::use()->metric_at (timepos_t (pos)));
		return metric.samples_per_bar (sample_rate()) * -pr;
	}
	if (pr < 0) {
		return 0;
	}
	return pr * sample_rate();
}

void
Session::set_sample_rate (samplecnt_t frames_per_second)
{
	/* this is called from the engine when SR changes,
	 * and after creating or loading a session
	 * via post_engine_init().
	 *
	 * In the latter case this call can happen
	 * concurrently with processing.
	 */

	if (_base_sample_rate == 0) {
		_base_sample_rate = frames_per_second;
	}
	else if (_base_sample_rate != frames_per_second && _engine.running ()) {
		NotifyAboutSampleRateMismatch (_base_sample_rate, frames_per_second);
	}

	/* The session's actual SR does not change.
	 * _engine.Running calls Session::initialize_latencies ()
	 * which sets up resampling, so the following really needs
	 * to be called only once.
	 */

	Temporal::set_sample_rate (_base_sample_rate);

	sync_time_vars();

	clear_clicks ();
	reset_write_sources (false);

	DiskReader::alloc_loop_declick (nominal_sample_rate());
	Location* loc = _locations->auto_loop_location ();
	DiskReader::reset_loop_declick (loc, nominal_sample_rate());

	set_dirty();
}

void
Session::set_block_size (pframes_t nframes)
{
	/* the AudioEngine guarantees
	 * that it will not be called while we are also in
	 * ::process(). It is therefore fine to do things that block
	 * here.
	 */
	current_block_size = nframes;
	_required_thread_buffersize = -1;

	ensure_buffers ();

	foreach_route (&Route::set_block_size, nframes);

	std::shared_ptr<IOPlugList const> iop (_io_plugins.reader ());
	for (auto const& i : *iop) {
		i->set_block_size (nframes);
	}

	DEBUG_TRACE (DEBUG::LatencyCompensation, "Session::set_block_size -> update worst i/o latency\n");
	/* when this is called from the auto-connect thread, the process-lock is held */
	Glib::Threads::Mutex::Lock lx (_update_latency_lock);
	set_worst_output_latency ();
	set_worst_input_latency ();
}


void
Session::resort_routes ()
{
	/* don't do anything here with signals emitted
	   by Routes during initial setup or while we
	   are being destroyed.
	*/

	if (inital_connect_or_deletion_in_progress ()) {
		/* drop any references during delete */
		GraphEdges edges;
		_current_route_graph = edges;
		return;
	}

	if (_route_deletion_in_progress) {
		return;
	}

	{
		RCUWriter<RouteList> writer (routes);
		std::shared_ptr<RouteList> r = writer.get_copy ();
		resort_routes_using (r);
		/* writer goes out of scope and forces update */
	}

#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::Graph)) {
		DEBUG_TRACE (DEBUG::Graph, "---- Session::resort_routes ----\n");
		for (auto const& i : *routes.reader ()) {
			DEBUG_TRACE (DEBUG::Graph, string_compose ("%1 fed by ...\n", i->name()));
			for (auto const& f : i->signal_sources ()) {
				DEBUG_TRACE (DEBUG::Graph, string_compose ("\t%1\n", f->graph_node_name ()));
			}
		}
		DEBUG_TRACE (DEBUG::Graph, "---- EOF ----\n");
	}
#endif
}

/** This is called whenever we need to rebuild the graph of how we will process
 *  routes.
 *  @param r List of routes, in any order.
 */

void
Session::resort_routes_using (std::shared_ptr<RouteList> r)
{
#ifndef NDEBUG
	Timing t;
#endif

	GraphNodeList gnl;
	for (auto const& rt : *r) {
		gnl.push_back (rt);
	}

	bool ok = true;

	if (rechain_process_graph (gnl)) {
		/* Update routelist for single-threaded processing, use topologically sorted nodelist */
		r->clear ();
		for (auto const& nd : gnl) {
			r->push_back (std::dynamic_pointer_cast<Route> (nd));
		}
	} else {
		ok = false;
	}

	/* now create IOPlugs graph-chains */
	std::shared_ptr<IOPlugList const> io_plugins (_io_plugins.reader ());
	GraphNodeList gnl_pre;
	GraphNodeList gnl_post;
	for (auto const& p : *io_plugins) {
		if (p->is_pre ()) {
			gnl_pre.push_back (p);
		} else {
			gnl_post.push_back (p);
		}
	}

	if (!rechain_ioplug_graph (true)) {
		ok = false;
	}

	if (!rechain_ioplug_graph (false)) {
		ok = false;
	}

#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::TopologyTiming)) {
		t.update ();
		std::cerr << string_compose ("Session::resort_route took %1ms ; DSP %2 %%\n",
				t.elapsed () / 1000., 100.0 * t.elapsed() / _engine.usecs_per_cycle ());

		DEBUG_TRACE (DEBUG::Graph, "Routes resorted, order follows:\n");
		for (auto const& i : *r) {
			DEBUG_TRACE (DEBUG::Graph, string_compose ("\t%1 (presentation order %2)\n", i->name(), i->presentation_info().order()));
		}
	}
#endif

	if (ok) {
		SuccessfulGraphSort (); /* EMIT SIGNAL */
		return;
	}

	/* The topological sort failed, so we have a problem.  Tell everyone
	 * and stick to the old graph; this will continue to be processed, so
	 * until the feedback is fixed, what is played back will not quite
	 * reflect what is actually connected.
	 */

	FeedbackDetected (); /* EMIT SIGNAL */
}

void
Session::resort_io_plugs ()
{
	bool ok_pre = rechain_ioplug_graph (true);
	bool ok_post = rechain_ioplug_graph (false);

	if (!ok_pre || !ok_post) {
		FeedbackDetected (); /* EMIT SIGNAL */
	}
}

bool
Session::rechain_process_graph (GraphNodeList& g)
{
	/* This may be called from the GUI thread (concurrrently with processing),
	 * when a user adds/removes routes.
	 *
	 * Or it may be called from the engine when connections are changed.
	 * In that case processing is blocked until the graph change is handled.
	 */
	GraphEdges edges;
	if (topological_sort (g, edges)) {
		/* We got a satisfactory topological sort, so there is no feedback;
		 * use this new graph.
		 *
		 * Note: the process graph chain does not require a
		 * topologically-sorted list, but hey ho.
		 */
		if (_process_graph->n_threads () > 1) {
			/* Ideally we'd use a memory pool to allocate the GraphChain, however node_lists
			 * inside the change are STL list/set. It was never rt-safe to re-chain the graph.
			 * Furthermore graph-changes are usually caused by connection changes, which are not
			 * rt-safe either.
			 *
			 * However, the graph-chain may be in use (session process), and the last reference
			 * be helf by the process-callback. So we delegate deletion to the butler thread.
			 */
			_graph_chain = std::shared_ptr<GraphChain> (new GraphChain (g, edges), std::bind (&rt_safe_delete<GraphChain>, this, _1));
		} else {
			_graph_chain.reset ();
		}

		_current_route_graph = edges;

		return true;
	}

	return false;
}

bool
Session::rechain_ioplug_graph (bool pre)
{
	std::shared_ptr<IOPlugList const> io_plugins (_io_plugins.reader ());

	if (io_plugins->empty ()) {
		_io_graph_chain[pre ? 0 : 1].reset ();
		return true;
	}

	GraphNodeList gnl;
	for (auto const& p : *io_plugins) {
		if (p->is_pre () == pre) {
			gnl.push_back (p);
		}
	}

	GraphEdges edges;

	if (topological_sort (gnl, edges)) {
		_io_graph_chain[pre ? 0 : 1] = std::shared_ptr<GraphChain> (new GraphChain (gnl, edges), std::bind (&rt_safe_delete<GraphChain>, this, _1));
		return true;
	}
	return false;
}

/** Find a route name starting with \a base, maybe followed by the
 *  lowest \a id.  \a id will always be added if \a definitely_add_number
 *  is true on entry; otherwise it will only be added if required
 *  to make the name unique.
 *
 *  Names are constructed like e.g. "Audio 3" for base="Audio" and id=3.
 *  The available route name with the lowest ID will be used, and \a id
 *  will be set to the ID.
 *
 *  \return false if a route name could not be found, and \a track_name
 *  and \a id do not reflect a free route name.
 */
bool
Session::find_route_name (string const & base, uint32_t& id, string& name, bool definitely_add_number)
{
	/* the base may conflict with ports that do not belong to existing
	   routes, but hidden objects like the click track. So check port names
	   before anything else.
	*/

	for (map<string,bool>::const_iterator reserved = reserved_io_names.begin(); reserved != reserved_io_names.end(); ++reserved) {
		if (base == reserved->first) {
			/* Check if this reserved name already exists, and if
			   so, disallow it without a numeric suffix.
			*/
			if (!reserved->second || route_by_name (reserved->first)) {
				definitely_add_number = true;
				if (id < 1) {
					id = 1;
				}
			}
			break;
		}
	}

	/* if we have "base 1" already, it doesn't make sense to add "base"
	 * if "base 1" has been deleted, adding "base" is no worse than "base 1"
	 */
	if (!definitely_add_number && route_by_name (base) == 0 && (route_by_name (string_compose("%1 1", base)) == 0)) {
		/* just use the base */
		name = base;
		return true;
	}

	do {
		name = string_compose ("%1 %2", base, id);

		if (route_by_name (name) == 0) {
			return true;
		}

		++id;

	} while (id < (UINT_MAX-1));

	return false;
}

/** Count the total ins and outs of all non-hidden tracks in the session and return them in in and out */
void
Session::count_existing_track_channels (ChanCount& in, ChanCount& out)
{
	in  = ChanCount::ZERO;
	out = ChanCount::ZERO;

	std::shared_ptr<RouteList const> r = routes.reader ();

	for (auto const& i : *r) {
		std::shared_ptr<Track> tr = std::dynamic_pointer_cast<Track> (i);
		if (!tr) {
			continue;
		}
		assert (!tr->is_auditioner()); // XXX remove me
		in  += tr->n_inputs();
		out += tr->n_outputs();
	}
}

string
Session::default_track_name_pattern (DataType t)
{
	switch (t) {
	case DataType::AUDIO:
		return _("Audio");
		break;

	case DataType::MIDI:
		return _("MIDI");
	}

	return "";
}

/** Caller must not hold process lock
 *  @param name_template string to use for the start of the name, or "" to use "MIDI".
 *  @param instrument plugin info for the instrument to insert pre-fader, if any
 */
list<std::shared_ptr<MidiTrack> >
Session::new_midi_track (const ChanCount& input, const ChanCount& output, bool strict_io,
                         std::shared_ptr<PluginInfo> instrument, Plugin::PresetRecord* pset,
                         RouteGroup* route_group, uint32_t how_many,
                         string name_template, PresentationInfo::order_t order,
                         TrackMode mode, bool input_auto_connect,
                         bool trigger_visibility)
{
	string track_name;
	uint32_t track_id = 0;
	string port;
	RouteList new_routes;
	list<std::shared_ptr<MidiTrack> > ret;

	const string name_pattern = default_track_name_pattern (DataType::MIDI);
	bool const use_number = (how_many != 1) || name_template.empty () || (name_template == name_pattern);

	while (how_many) {
		if (!find_route_name (name_template.empty() ? _("MIDI") : name_template, ++track_id, track_name, use_number)) {
			error << "cannot find name for new midi track" << endmsg;
			goto failed;
		}

		std::shared_ptr<MidiTrack> track;

		try {
			track.reset (new MidiTrack (*this, track_name, mode));

			if (track->init ()) {
				goto failed;
			}

			if (strict_io) {
				track->set_strict_io (true);
			}

			BOOST_MARK_TRACK (track);

			{
				Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
				if (track->input()->ensure_io (input, false, this)) {
					error << "cannot configure " << input << " out configuration for new midi track" << endmsg;
					goto failed;
				}

				if (track->output()->ensure_io (output, false, this)) {
					error << "cannot configure " << output << " out configuration for new midi track" << endmsg;
					goto failed;
				}
			}

			if (route_group) {
				route_group->add (track);
			}

			track->presentation_info ().set_trigger_track (trigger_visibility);

			new_routes.push_back (track);
			ret.push_back (track);
		}

		catch (failed_constructor &err) {
			error << _("Session: could not create new midi track.") << endmsg;
			goto failed;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {

			error << string_compose (_("A required port for MIDI I/O could not be created (%1).\nYou may need to restart the Audio/MIDI engine to fix this."), pfe.what()) << endmsg;
			goto failed;
		}

		--how_many;
	}

	failed:
	if (!new_routes.empty()) {
		ChanCount existing_inputs;
		ChanCount existing_outputs;
		count_existing_track_channels (existing_inputs, existing_outputs);

		add_routes (new_routes, input_auto_connect, !instrument, order);
		load_and_connect_instruments (new_routes, strict_io, instrument, pset, existing_outputs);
	}

	return ret;
}

RouteList
Session::new_midi_route (RouteGroup* route_group, uint32_t how_many, string name_template, bool strict_io,
                         std::shared_ptr<PluginInfo> instrument, Plugin::PresetRecord* pset,
                         PresentationInfo::Flag flag, PresentationInfo::order_t order)
{
	string bus_name;
	uint32_t bus_id = 0;
	string port;
	RouteList ret;

	bool const use_number = (how_many != 1) || name_template.empty () || name_template == _("Midi Bus");

	while (how_many) {
		if (!find_route_name (name_template.empty () ? _("Midi Bus") : name_template, ++bus_id, bus_name, use_number)) {
			error << "cannot find name for new midi bus" << endmsg;
			goto failure;
		}

		try {
			std::shared_ptr<Route> bus (new Route (*this, bus_name, flag, DataType::AUDIO)); // XXX Editor::add_routes is not ready for ARDOUR::DataType::MIDI

			if (bus->init ()) {
				goto failure;
			}

			if (strict_io) {
				bus->set_strict_io (true);
			}

			BOOST_MARK_ROUTE(bus);

			{
				Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

				if (bus->input()->ensure_io (ChanCount(DataType::MIDI, 1), false, this)) {
					error << _("cannot configure new midi bus input") << endmsg;
					goto failure;
				}


				if (bus->output()->ensure_io (ChanCount(DataType::MIDI, 1), false, this)) {
					error << _("cannot configure new midi bus output") << endmsg;
					goto failure;
				}
			}

			if (route_group) {
				route_group->add (bus);
			}

			bus->add_internal_return ();
			ret.push_back (bus);
		}

		catch (failed_constructor &err) {
			error << _("Session: could not create new MIDI bus.") << endmsg;
			goto failure;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {
			error << pfe.what() << endmsg;
			goto failure;
		}

		--how_many;
	}

	failure:
	if (!ret.empty()) {
		ChanCount existing_inputs;
		ChanCount existing_outputs;
		count_existing_track_channels (existing_inputs, existing_outputs);

		add_routes (ret, false, !instrument, order);
		load_and_connect_instruments (ret, strict_io, instrument, pset, existing_outputs);
	}

	return ret;

}


void
Session::midi_output_change_handler (IOChange change, void * /*src*/, std::weak_ptr<Route> wr)
{
	std::shared_ptr<Route> midi_route (wr.lock());

	if (!midi_route) {
		return;
	}

	if ((change.type & IOChange::ConfigurationChanged) && Config->get_output_auto_connect() != ManualConnect) {

		if (change.after.n_audio() <= change.before.n_audio()) {
			return;
		}

		/* new audio ports: make sure the audio goes somewhere useful,
		 * unless the user has no-auto-connect selected.
		 *
		 * The existing ChanCounts don't matter for this call as they are only
		 * to do with matching input and output indices, and we are only changing
		 * outputs here.
		 */
		auto_connect_route (midi_route, false, !midi_route->instrument_fanned_out (), ChanCount(), change.before);
	}
}

bool
Session::ensure_stripable_sort_order ()
{
	StripableList sl;
	get_stripables (sl);
	sl.sort (Stripable::Sorter ());

	bool change = false;
	PresentationInfo::order_t order = 0;

	for (StripableList::iterator si = sl.begin(); si != sl.end(); ++si) {
		std::shared_ptr<Stripable> s (*si);
		assert (!s->is_auditioner ()); // XXX remove me
		if (s->is_monitor () || s->is_surround_master ()) {
			continue;
		}
		if (order != s->presentation_info().order()) {
			s->set_presentation_order (order);
			change = true;
		}
		++order;
	}
	return change;
}

void
Session::ensure_route_presentation_info_gap (PresentationInfo::order_t first_new_order, uint32_t how_many)
{
	DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("ensure order gap starting at %1 for %2\n", first_new_order, how_many));

	if (first_new_order == PresentationInfo::max_order) {
		/* adding at end, no worries */
		return;
	}

	/* create a gap in the presentation info to accommodate @p how_many
	 * new objects.
	 */
	StripableList sl;
	get_stripables (sl);

	for (StripableList::iterator si = sl.begin(); si != sl.end(); ++si) {
		std::shared_ptr<Stripable> s (*si);

		if (s->presentation_info().special (false)) {
			continue;
		}

		if (!s->presentation_info().order_set()) {
			continue;
		}

		if (s->presentation_info().order () >= first_new_order) {
			s->set_presentation_order (s->presentation_info().order () + how_many);
		}
	}
}

/** Caller must not hold process lock
 *  @param name_template string to use for the start of the name, or "" to use "Audio".
 */
list< std::shared_ptr<AudioTrack> >
Session::new_audio_track (int input_channels, int output_channels, RouteGroup* route_group,
                          uint32_t how_many, string name_template, PresentationInfo::order_t order,
                          TrackMode mode, bool input_auto_connect,
                          bool trigger_visibility)
{
	string track_name;
	uint32_t track_id = 0;
	string port;
	RouteList new_routes;
	list<std::shared_ptr<AudioTrack> > ret;

	const string name_pattern = default_track_name_pattern (DataType::AUDIO);
	bool const use_number = (how_many != 1) || name_template.empty () || (name_template == name_pattern);

	while (how_many) {

		if (!find_route_name (name_template.empty() ? _(name_pattern.c_str()) : name_template, ++track_id, track_name, use_number)) {
			error << "cannot find name for new audio track" << endmsg;
			goto failed;
		}

		std::shared_ptr<AudioTrack> track;

		try {
			track.reset (new AudioTrack (*this, track_name, mode));

			if (track->init ()) {
				goto failed;
			}

			if (Profile->get_mixbus ()) {
				track->set_strict_io (true);
			}

			BOOST_MARK_TRACK (track);

			{
				Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

				if (track->input()->ensure_io (ChanCount(DataType::AUDIO, input_channels), false, this)) {
					error << string_compose (
						_("cannot configure %1 in/%2 out configuration for new audio track"),
						input_channels, output_channels)
					      << endmsg;
					goto failed;
				}

				if (track->output()->ensure_io (ChanCount(DataType::AUDIO, output_channels), false, this)) {
					error << string_compose (
						_("cannot configure %1 in/%2 out configuration for new audio track"),
						input_channels, output_channels)
					      << endmsg;
					goto failed;
				}
			}

			if (route_group) {
				route_group->add (track);
			}

			track->presentation_info ().set_trigger_track (trigger_visibility);

			new_routes.push_back (track);
			ret.push_back (track);
		}

		catch (failed_constructor &err) {
			error << _("Session: could not create new audio track.") << endmsg;
			goto failed;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {

			error << pfe.what() << endmsg;
			goto failed;
		}

		--how_many;
	}

	failed:
	if (!new_routes.empty()) {
		add_routes (new_routes, input_auto_connect, true, order);
	}

	return ret;
}

/** Caller must not hold process lock.
 *  @param name_template string to use for the start of the name, or "" to use "Bus".
 */
RouteList
Session::new_audio_route (int input_channels, int output_channels, RouteGroup* route_group, uint32_t how_many, string name_template,
                          PresentationInfo::Flag flags, PresentationInfo::order_t order)
{
	string bus_name;
	uint32_t bus_id = 0;
	string port;
	RouteList ret;

	bool const use_number = (how_many != 1) || name_template.empty () || name_template == _("Bus");

	while (how_many) {
		if (!find_route_name (name_template.empty () ? _("Bus") : name_template, ++bus_id, bus_name, use_number)) {
			error << "cannot find name for new audio bus" << endmsg;
			goto failure;
		}

		try {
			std::shared_ptr<Route> bus (new Route (*this, bus_name, flags, DataType::AUDIO));

			if (bus->init ()) {
				goto failure;
			}

			if (Profile->get_mixbus ()) {
				bus->set_strict_io (true);
			}

			BOOST_MARK_ROUTE(bus);

			{
				Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

				if (bus->input()->ensure_io (ChanCount(DataType::AUDIO, input_channels), false, this)) {
					error << string_compose (_("cannot configure %1 in/%2 out configuration for new audio track"),
								 input_channels, output_channels)
					      << endmsg;
					goto failure;
				}


				if (bus->output()->ensure_io (ChanCount(DataType::AUDIO, output_channels), false, this)) {
					error << string_compose (_("cannot configure %1 in/%2 out configuration for new audio track"),
								 input_channels, output_channels)
					      << endmsg;
					goto failure;
				}
			}

			if (route_group) {
				route_group->add (bus);
			}

			bus->add_internal_return ();
			ret.push_back (bus);
		}

		catch (failed_constructor &err) {
			error << _("Session: could not create new audio bus.") << endmsg;
			goto failure;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {
			error << pfe.what() << endmsg;
			goto failure;
		}


		--how_many;
	}

	failure:
	if (!ret.empty()) {
		if (flags == PresentationInfo::FoldbackBus) {
			add_routes (ret, false, false, order); // no autoconnect
		} else {
			add_routes (ret, false, true, order); // autoconnect // outputs only
		}
	}

	return ret;

}

RouteList
Session::new_route_from_template (uint32_t how_many, PresentationInfo::order_t insert_at, const std::string& template_path, const std::string& name_base,
                                  PlaylistDisposition pd)
{
	XMLTree tree;

	if (!tree.read (template_path.c_str())) {
		return RouteList();
	}

	return new_route_from_template (how_many, insert_at, *tree.root(), name_base, pd);
}

RouteList
Session::new_route_from_template (uint32_t how_many, PresentationInfo::order_t insert_at, XMLNode& node, const std::string& name_base, PlaylistDisposition pd)
{
	RouteList ret;
	uint32_t number = 0;
	const uint32_t being_added = how_many;
	/* This will prevent the use of any existing XML-provided PBD::ID
	   values by Stateful.
	*/
	Stateful::ForceIDRegeneration force_ids;

	/* New v6 templates do have a version in the Route-Template,
	 * we assume that all older, unversioned templates are
	 * from Ardour 5.x
	 * when Stateful::loading_state_version was 3002
	 */
	int version = 3002;
	node.get_property (X_("version"), version);

	while (how_many) {

		/* We're going to modify the node contents a bit so take a
		 * copy. The node may be re-used when duplicating more than once.
		 */

		XMLNode node_copy (node);
		std::vector<std::shared_ptr<Playlist> > shared_playlists;

		try {
			string name;

			if (!name_base.empty()) {

				/* if we're adding more than one routes, force
				 * all the names of the new routes to be
				 * numbered, via the final parameter.
				 */

				if (!find_route_name (name_base.c_str(), ++number, name, (being_added > 1))) {
					fatal << _("Session: Failed to create unique ID for track from template.") << endmsg;
					abort(); /*NOTREACHED*/
				}

			} else {

				string const route_name  = node_copy.property(X_("name"))->value ();

				/* generate a new name by adding a number to the end of the template name */
				if (!find_route_name (route_name, ++number, name, true)) {
					fatal << _("Session: Failed to generate unique name and ID for track from template.") << endmsg;
					abort(); /*NOTREACHED*/
				}
			}

			/* figure out the appropriate playlist setup. The track
			 * (if the Route we're creating is a track) will find
			 * playlists via ID.
			 */

			if (pd == CopyPlaylist) {

				PBD::ID playlist_id;

				if (node_copy.get_property (X_("audio-playlist"), playlist_id)) {
					std::shared_ptr<Playlist> playlist = _playlists->by_id (playlist_id);
					playlist = PlaylistFactory::create (playlist, string_compose ("%1.1", name));
					playlist->reset_shares ();
					node_copy.set_property (X_("audio-playlist"), playlist->id());
				}

				if (node_copy.get_property (X_("midi-playlist"), playlist_id)) {
					std::shared_ptr<Playlist> playlist = _playlists->by_id (playlist_id);
					playlist = PlaylistFactory::create (playlist, string_compose ("%1.1", name));
					playlist->reset_shares ();
					node_copy.set_property (X_("midi-playlist"), playlist->id());
				}

			} else if (pd == SharePlaylist) {
				PBD::ID playlist_id;

				if (node_copy.get_property (X_("audio-playlist"), playlist_id)) {
					std::shared_ptr<Playlist> playlist = _playlists->by_id (playlist_id);
					shared_playlists.push_back (playlist);
				}

				if (node_copy.get_property (X_("midi-playlist"), playlist_id)) {
					std::shared_ptr<Playlist> playlist = _playlists->by_id (playlist_id);
					shared_playlists.push_back (playlist);
				}

			} else { /* NewPlaylist */

				PBD::ID pid;
				std::string default_type;
				node.get_property(X_("default-type"), default_type);

				if (node_copy.get_property (X_("audio-playlist"), pid) || (version < 5000 && default_type == "audio")) {
					std::shared_ptr<Playlist> playlist = PlaylistFactory::create (DataType::AUDIO, *this, name, false);
					node_copy.set_property (X_("audio-playlist"), playlist->id());
				}

				if (node_copy.get_property (X_("midi-playlist"), pid) || (version < 5000 && default_type == "midi")) {
					std::shared_ptr<Playlist> playlist = PlaylistFactory::create (DataType::MIDI, *this, name, false);
					node_copy.set_property (X_("midi-playlist"), playlist->id());
				}
			}

			/* Fix up new name in the XML node */

			Route::set_name_in_state (node_copy, name);

			/* trim bitslots from listen sends so that new ones are used */
			XMLNodeList children = node_copy.children ();
			for (XMLNodeList::iterator i = children.begin(); i != children.end(); ++i) {
				if ((*i)->name() == X_("Processor")) {
					/* ForceIDRegeneration does not catch the following */
					XMLProperty const * role = (*i)->property (X_("role"));
					XMLProperty const * type = (*i)->property (X_("type"));

					if (role && role->value() == X_("Aux")) {
						/* Check if the target bus exists.
						 * This is mainly useful when duplicating tracks
						 * (aux-sends should not be saved in templates).
						 */
						XMLProperty const * target = (*i)->property (X_("target"));
						if (!target) {
							(*i)->set_property ("type", "dangling-aux-send");
							continue;
						}
						std::shared_ptr<Route> r = route_by_id (target->value());
						if (!r || std::dynamic_pointer_cast<Track>(r)) {
							(*i)->set_property ("type", "dangling-aux-send");
							continue;
						}
					}

					if (role && role->value() == X_("Listen")) {
						(*i)->remove_property (X_("bitslot"));
					}
					else if (role && (role->value() == X_("Send") || role->value() == X_("Aux"))) {
						Delivery::Role xrole;
						uint32_t bitslot = 0;
						xrole = Delivery::Role (string_2_enum (role->value(), xrole));

						/* generate new bitslot ID */
						std::string name = Send::name_and_id_new_send(*this, xrole, bitslot, false);
						(*i)->remove_property (X_("bitslot"));
						(*i)->set_property ("bitslot", bitslot);

						/* external sends need unique names */
						if (role->value() == X_("Send")) {
							(*i)->remove_property (X_("name"));
							(*i)->set_property ("name", name);

							XMLNodeList io_kids = (*i)->children ();
							for (XMLNodeList::iterator j = io_kids.begin(); j != io_kids.end(); ++j) {
								if ((*j)->name() != X_("IO")) {
									continue;
								}
								(*j)->remove_property (X_("name"));
								(*j)->set_property ("name", name);
							}
						}
					}
					else if (type && type->value() == X_("intreturn")) {
						(*i)->remove_property (X_("bitslot"));
						(*i)->set_property ("ignore-bitslot", "1");
					}
					else if (type && type->value() == X_("return")) {
						// Return::set_state() generates a new one
						(*i)->remove_property (X_("bitslot"));
					}
					else if (type && type->value() == X_("port")) {
						IOProcessor::prepare_for_reset (**i, name);
					}
				}
			}

			/* new routes start off unsoloed to avoid issues related to
			   upstream / downstream buses.
			*/
			node_copy.remove_node_and_delete (X_("Controllable"), X_("name"), X_("solo"));

			std::shared_ptr<Route> route;

			if (version < 3000) {
				route = XMLRouteFactory_2X (node_copy, version);
			} else if (version < 5000) {
				route = XMLRouteFactory_3X (node_copy, version);
			} else {
				route = XMLRouteFactory (node_copy, version);
			}

			if (route == 0) {
				error << _("Session: cannot create track/bus from template description") << endmsg;
				goto out;
			}

			{
				PresentationInfo& rpi = route->presentation_info ();
				rpi.set_flags (PresentationInfo::Flag (rpi.flags() & ~PresentationInfo::OrderSet));
			}

			/* Fix up sharing of playlists with the new Route/Track */

			for (vector<std::shared_ptr<Playlist> >::iterator sp = shared_playlists.begin(); sp != shared_playlists.end(); ++sp) {
				(*sp)->share_with (route->id());
			}

			if (std::dynamic_pointer_cast<Track>(route)) {
				/* force input/output change signals so that the new diskstream
				   picks up the configuration of the route. During session
				   loading this normally happens in a different way.
				*/

				Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

				IOChange change (IOChange::Type (IOChange::ConfigurationChanged | IOChange::ConnectionsChanged));
				change.after = route->input()->n_ports();
				route->input()->changed (change, this);
				change.after = route->output()->n_ports();
				route->output()->changed (change, this);
			}

			ret.push_back (route);
		}

		catch (failed_constructor &err) {
			error << _("Session: could not create new track/bus from template") << endmsg;
			goto out;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {
			error << pfe.what() << endmsg;
			goto out;
		}

		catch (...) {
			throw;
		}

		--how_many;
	}

	out:
	if (!ret.empty()) {
		add_routes (ret, false, false, insert_at);
	}

	if (!ret.empty()) {
		/* set/unset monitor-send */
		Glib::Threads::Mutex::Lock lm (_engine.process_lock());
		for (RouteList::iterator x = ret.begin(); x != ret.end(); ++x) {
			if ((*x)->can_monitor ()) {
				if (_monitor_out) {
					(*x)->enable_monitor_send ();
				} else {
					/* this may happen with old templates */
					(*x)->remove_monitor_send ();
				}
			}
			if (_surround_master) {
				(*x)->enable_surround_send();
			} else {
				(*x)->remove_surround_send();
			}
			/* reconnect ports using information from state */
			for (auto const& wio : (*x)->all_inputs ()) {
				std::shared_ptr<IO> io = wio.lock();
				if (!io) {
					continue;
				}
				for (auto const& p : *io->ports()) {
					p->reconnect ();
				}
			}
			for (auto const& wio : (*x)->all_outputs ()) {
				std::shared_ptr<IO> io = wio.lock();
				if (!io) {
					continue;
				}
				for (auto const& p : *io->ports()) {
					p->reconnect ();
				}
			}
		}
	}

	return ret;
}

void
Session::add_routes (RouteList& new_routes, bool input_auto_connect, bool output_auto_connect, PresentationInfo::order_t order)
{
	try {
		PBD::Unwinder<bool> aip (_adding_routes_in_progress, true);
		add_routes_inner (new_routes, input_auto_connect, output_auto_connect, order);

	} catch (...) {
		error << _("Adding new tracks/busses failed") << endmsg;
	}

	/* During the route additions there will have been potentially several
	 * signals emitted to indicate the new graph. ::graph_reordered() will
	 * have ignored all of them because _adding_routes_in_progress was
	 * true.
	 *
	 * We still need the effects of ::graph_reordered(), but we didn't want
	 * it called multiple times during the addition of multiple routes. Now
	 * that the addition is done, call it explicitly.
	 */

	graph_reordered (false);

	set_dirty();

	update_route_record_state ();

	/* Nobody should hear about changes to PresentationInfo
	 * (e.g. selection) until all handlers of RouteAdded have executed
	 */

	PresentationInfo::ChangeSuspender cs;
	RouteAdded (new_routes); /* EMIT SIGNAL */
}

void
Session::add_routes_inner (RouteList& new_routes, bool input_auto_connect, bool output_auto_connect, PresentationInfo::order_t order)
{
	ChanCount existing_inputs;
	ChanCount existing_outputs;
	uint32_t n_routes;
	uint32_t added = 0;

	count_existing_track_channels (existing_inputs, existing_outputs);

	{
		RCUWriter<RouteList> writer (routes);
		std::shared_ptr<RouteList> r = writer.get_copy ();
		n_routes = r->size();
		r->insert (r->end(), new_routes.begin(), new_routes.end());

		/* if there is no control out and we're not in the middle of loading,
		 * resort the graph here. if there is a control out, we will resort
		 * toward the end of this method. if we are in the middle of loading,
		 * we will resort when done.
		 */

		if (!_monitor_out && !loading() && !input_auto_connect && !output_auto_connect) {
			resort_routes_using (r);
		}
	}

	/* monitor is not part of the order */
	if (_monitor_out) {
		assert (n_routes > 0);
		--n_routes;
	}

	{
		PresentationInfo::ChangeSuspender cs;
		ensure_route_presentation_info_gap (order, new_routes.size());

		for (RouteList::iterator x = new_routes.begin(); x != new_routes.end(); ++x, ++added) {

			std::weak_ptr<Route> wpr (*x);
			std::shared_ptr<Route> r (*x);

			r->solo_control()->Changed.connect_same_thread (*this, std::bind (&Session::route_solo_changed, this, _1, _2,wpr));
			r->solo_isolate_control()->Changed.connect_same_thread (*this, std::bind (&Session::route_solo_isolated_changed, this, wpr));
			r->mute_control()->Changed.connect_same_thread (*this, std::bind (&Session::route_mute_changed, this));

			r->processors_changed.connect_same_thread (*this, std::bind (&Session::route_processors_changed, this, _1));
			r->processor_latency_changed.connect_same_thread (*this, std::bind (&Session::queue_latency_recompute, this));

			if (r->is_master()) {
				_master_out = r;
			}

			if (r->is_monitor()) {
				_monitor_out = r;
			}

			if (r->is_surround_master()) {
				_surround_master = r;
			}

			std::shared_ptr<Track> tr = std::dynamic_pointer_cast<Track> (r);
			if (tr) {
				tr->PlaylistChanged.connect_same_thread (*this, std::bind (&Session::track_playlist_changed, this, std::weak_ptr<Track> (tr)));
				track_playlist_changed (std::weak_ptr<Track> (tr));
				tr->rec_enable_control()->Changed.connect_same_thread (*this, std::bind (&Session::update_route_record_state, this));

				std::shared_ptr<MidiTrack> mt = std::dynamic_pointer_cast<MidiTrack> (tr);
				if (mt) {
					mt->StepEditStatusChange.connect_same_thread (*this, std::bind (&Session::step_edit_status_change, this, _1));
					mt->presentation_info().PropertyChanged.connect_same_thread (*this, std::bind (&Session::midi_track_presentation_info_changed, this, _1, std::weak_ptr<MidiTrack>(mt)));
				}
			}

			if (r->triggerbox()) {
				r->triggerbox()->EmptyStatusChanged.connect_same_thread (*this, std::bind (&Session::handle_slots_empty_status, this, wpr));
				if (!r->triggerbox()->empty()) {
					tb_with_filled_slots++;
				}
			}

			if (!r->presentation_info().special (false)) {

				DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("checking PI state for %1\n", r->name()));

				/* presentation info order may already have been set from XML */

				if (!r->presentation_info().order_set()) {
					if (order == PresentationInfo::max_order) {
						/* just add to the end */
						r->set_presentation_order (n_routes + added);
						DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("group order not set, set to NR %1 + %2 = %3\n", n_routes, added, n_routes + added));
					} else {
						r->set_presentation_order (order + added);
						DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("group order not set, set to %1 + %2 = %3\n", order, added, order + added));
					}
				} else {
					DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("group order already set to %1\n", r->presentation_info().order()));
				}
			}

			DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("added route %1, pi %2\n", r->name(), r->presentation_info()));

			if (input_auto_connect || output_auto_connect) {
				auto_connect_route (r, input_auto_connect, output_auto_connect, ChanCount (), ChanCount (), existing_inputs, existing_outputs);
				if (input_auto_connect) {
					existing_inputs += r->n_inputs();
				}
				if (output_auto_connect) {
					existing_outputs += r->n_outputs();
				}
			}

			ARDOUR::GUIIdle ();
		}
		ensure_stripable_sort_order ();
	}

	if (_monitor_out && !loading()) {
		Glib::Threads::Mutex::Lock lm (_engine.process_lock());

		for (RouteList::iterator x = new_routes.begin(); x != new_routes.end(); ++x) {
			if ((*x)->can_monitor ()) {
				(*x)->enable_monitor_send ();
			}
		}
	}

	if (_surround_master && !loading()) {
		Glib::Threads::Mutex::Lock lm (_engine.process_lock());
		for (auto & r : new_routes) {
			r->enable_surround_send ();
		}
	}

	reassign_track_numbers ();
}

void
Session::load_and_connect_instruments (RouteList& new_routes, bool strict_io, std::shared_ptr<PluginInfo> instrument, Plugin::PresetRecord* pset, ChanCount& existing_outputs)
{
	if (instrument) {
		for (RouteList::iterator r = new_routes.begin(); r != new_routes.end(); ++r) {
			PluginPtr plugin = instrument->load (*this);
			if (!plugin) {
				warning << "Failed to add Synth Plugin to newly created track." << endmsg;
				continue;
			}
			if (pset) {
				plugin->load_preset (*pset);
			}
			std::shared_ptr<PluginInsert> pi (new PluginInsert (*this, **r, plugin));
			if (strict_io) {
				pi->set_strict_io (true);
			}

			(*r)->add_processor (pi, PreFader);

			if (Profile->get_mixbus () && pi->configured () && pi->output_streams().n_audio() > 2) {
				(*r)->move_instrument_down (false);
			}

			/* Route::add_processors -> Delivery::configure_io -> IO::ensure_ports
			 * should have registered the ports, so now we can call.. */
			if (!(*r)->instrument_fanned_out ()) {
				auto_connect_route (*r, false, true, ChanCount (), ChanCount (), ChanCount (), existing_outputs);
				existing_outputs += (*r)->n_outputs();
			}
		}
	}
	for (RouteList::iterator r = new_routes.begin(); r != new_routes.end(); ++r) {
		(*r)->output()->changed.connect_same_thread (*this, std::bind (&Session::midi_output_change_handler, this, _1, _2, std::weak_ptr<Route>(*r)));
	}
}

void
Session::globally_set_send_gains_to_zero (std::shared_ptr<Route> dest)
{
	std::shared_ptr<RouteList const> r = routes.reader ();
	std::shared_ptr<Send> s;

	for (auto const& i : *r) {
		if ((s = i->internal_send_for (dest)) != 0) {
			s->gain_control()->set_value (GAIN_COEFF_ZERO, Controllable::NoGroup);
		}
	}
}

void
Session::globally_set_send_gains_to_unity (std::shared_ptr<Route> dest)
{
	std::shared_ptr<RouteList const> r = routes.reader ();
	std::shared_ptr<Send> s;

	for (auto const& i : *r) {
		if ((s = i->internal_send_for (dest)) != 0) {
			s->gain_control()->set_value (GAIN_COEFF_UNITY, Controllable::NoGroup);
		}
	}
}

void
Session::globally_set_send_gains_from_track(std::shared_ptr<Route> dest)
{
	std::shared_ptr<RouteList const> r = routes.reader ();
	std::shared_ptr<Send> s;

	for (auto const& i : *r) {
		if ((s = i->internal_send_for (dest)) != 0) {
			s->gain_control()->set_value (i->gain_control()->get_value(), Controllable::NoGroup);
		}
	}
}

/** @param include_buses true to add sends to buses and tracks, false for just tracks */
void
Session::globally_add_internal_sends (std::shared_ptr<Route> dest, Placement p, bool include_buses)
{
	std::shared_ptr<RouteList const> r = routes.reader ();
	std::shared_ptr<RouteList> t (new RouteList);

	for (auto const& i : *r) {
		/* no MIDI sends because there are no MIDI busses yet */
		if (include_buses || std::dynamic_pointer_cast<AudioTrack>(i)) {
			t->push_back (i);
		}
	}

	add_internal_sends (dest, p, t);
}

void
Session::add_internal_sends (std::shared_ptr<Route> dest, Placement p, std::shared_ptr<RouteList> senders)
{
	for (RouteList::iterator i = senders->begin(); i != senders->end(); ++i) {
		add_internal_send (dest, (*i)->before_processor_for_placement (p), *i);
	}
}

void
Session::add_internal_send (std::shared_ptr<Route> dest, int index, std::shared_ptr<Route> sender)
{
	add_internal_send (dest, sender->before_processor_for_index (index), sender);
}

void
Session::add_internal_send (std::shared_ptr<Route> dest, std::shared_ptr<Processor> before, std::shared_ptr<Route> sender)
{
	if (sender->is_singleton() || sender == dest || dest->is_singleton()) {
		return;
	}

	if (!dest->internal_return()) {
		dest->add_internal_return ();
	}

	sender->add_aux_send (dest, before);
}

void
Session::remove_routes (std::shared_ptr<RouteList> routes_to_remove)
{
	bool mute_changed = false;
	bool send_selected = false;

	{ // RCU Writer scope
		PBD::Unwinder<bool> uw_flag (_route_deletion_in_progress, true);
		RCUWriter<RouteList> writer (routes);
		std::shared_ptr<RouteList> rs = writer.get_copy ();

		for (RouteList::iterator iter = routes_to_remove->begin(); iter != routes_to_remove->end(); ++iter) {

			if (_selection->selected (*iter)) {
				send_selected = true;
			}

			if (*iter == _master_out) {
				continue;
			}

			/* speed up session deletion, don't do the solo dance */
			if (!deletion_in_progress ()) {
				/* Do not postpone set_value as rt-event via AC::check_rt,
				 * The route will be deleted by then, and the Controllable gone.
				 */
				(*iter)->solo_control()->clear_flag (Controllable::RealTime);
				(*iter)->solo_control()->set_value (0.0, Controllable::NoGroup);
			}

			if ((*iter)->mute_control()->muted ()) {
				mute_changed = true;
			}

			rs->remove (*iter);

			/* deleting the master out seems like a dumb
			   idea, but its more of a UI policy issue
			   than our concern.
			*/

			if (*iter == _master_out) {
				_master_out = std::shared_ptr<Route> ();
			}

			if (*iter == _monitor_out) {
				_monitor_out.reset ();
			}

			if (*iter == _surround_master) {
				_surround_master.reset ();
			}

			// We need to disconnect the route's inputs and outputs

			(*iter)->input()->disconnect (0);
			(*iter)->output()->disconnect (0);

			/* if the route had internal sends sending to it, remove them */

			if (!deletion_in_progress () && (*iter)->internal_return()) {

				std::shared_ptr<RouteList const> r = routes.reader ();
				for (auto const& i : *r) {
					std::shared_ptr<Send> s = i->internal_send_for (*iter);
					if (s) {
						i->remove_processor (s);
					}
				}
			}

			/* if the monitoring section had a pointer to this route, remove it */
			if (!deletion_in_progress () && _monitor_out && (*iter)->can_monitor ()) {
				Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
				ProcessorChangeBlocker pcb (this, false);
				(*iter)->remove_monitor_send ();
			}

			std::shared_ptr<MidiTrack> mt = std::dynamic_pointer_cast<MidiTrack> (*iter);
			if (mt && mt->step_editing()) {
				if (_step_editors > 0) {
					_step_editors--;
				}
			}
		}

		/* writer goes out of scope, forces route list update */

	} // end of RCU Writer scope

	if (mute_changed) {
		MuteChanged (); /* EMIT SIGNAL */
	}

	update_route_solo_state ();
	update_latency_compensation (false, false);
	set_dirty();

	/* Re-sort routes to remove the graph's current references to the one that is
	 * going away, then flush old references out of the graph.
	 */

	resort_routes ();

	/* get rid of it from the dead wood collection in the route list manager */
	/* XXX i think this is unsafe as it currently stands, but i am not sure. (pd, october 2nd, 2006) */
	routes.flush ();

	/* remove these routes from the selection if appropriate, and signal
	 * the change *before* we call DropReferences for them.
	 */

	if (send_selected && !deletion_in_progress()) {
		for (RouteList::iterator iter = routes_to_remove->begin(); iter != routes_to_remove->end(); ++iter) {
			_selection->remove_stripable_by_id ((*iter)->id());
		}
		PropertyChange pc;
		pc.add (Properties::selected);
		PresentationInfo::Change (pc);
	}

	/* try to cause everyone to drop their references
	 * and unregister ports from the backend
	 */

	for (RouteList::iterator iter = routes_to_remove->begin(); iter != routes_to_remove->end(); ++iter) {
		(*iter)->drop_references ();
	}

	if (deletion_in_progress()) {
		return;
	}

	/* really drop reference to the Surround Master to
	 * unload the vapor plugin. While the RCU keeps a refecent the
	 * SurroundMaster, a new SurroundMaster cannot be added.
	 */
	std::shared_ptr<RouteList const> r = routes.reader ();
	for (auto const& rt : *r) {
		rt->flush_graph_activision_rcu ();
	}

	PropertyChange pc;
	pc.add (Properties::order);
	PresentationInfo::Change (pc);

	update_route_record_state ();
}

void
Session::remove_route (std::shared_ptr<Route> route)
{
	std::shared_ptr<RouteList> rl (new RouteList);
	rl->push_back (route);
	remove_routes (rl);
}

void
Session::route_mute_changed ()
{
	MuteChanged (); /* EMIT SIGNAL */
	set_dirty ();
}

void
Session::route_listen_changed (Controllable::GroupControlDisposition group_override, std::weak_ptr<Route> wpr)
{
	std::shared_ptr<Route> route (wpr.lock());

	if (!route) {
		return;
	}

	assert (Config->get_solo_control_is_listen_control());

	if (route->solo_control()->soloed_by_self_or_masters()) {

		if (Config->get_exclusive_solo()) {

			_engine.monitor_port().clear_ports (false);

			RouteGroup* rg = route->route_group ();
			const bool group_already_accounted_for = (group_override == Controllable::ForGroup);

			std::shared_ptr<RouteList const> r = routes.reader ();

			for (auto const& i : *r) {
				if (i == route) {
					/* already changed */
					continue;
				}

				if (i->solo_isolate_control()->solo_isolated() || !i->can_monitor()) {
					/* route does not get solo propagated to it */
					continue;
				}

				if ((group_already_accounted_for && i->route_group() && i->route_group() == rg)) {
					/* this route is a part of the same solo group as the route
					 * that was changed. Changing that route did change or will
					 * change all group members appropriately, so we can ignore it
					 * here
					 */
					continue;
				}
				i->solo_control()->set_value (0.0, Controllable::NoGroup);
			}
		}

		_listen_cnt++;

	} else if (_listen_cnt > 0) {

		_listen_cnt--;
	}
}

void
Session::route_solo_isolated_changed (std::weak_ptr<Route> wpr)
{
	std::shared_ptr<Route> route (wpr.lock());

	if (!route) {
		return;
	}

	bool send_changed = false;

	if (route->solo_isolate_control()->solo_isolated()) {
		if (_solo_isolated_cnt == 0) {
			send_changed = true;
		}
		_solo_isolated_cnt++;
	} else if (_solo_isolated_cnt > 0) {
		_solo_isolated_cnt--;
		if (_solo_isolated_cnt == 0) {
			send_changed = true;
		}
	}

	if (send_changed) {
		IsolatedChanged (); /* EMIT SIGNAL */
	}
}

void
Session::route_solo_changed (bool self_solo_changed, Controllable::GroupControlDisposition group_override,  std::weak_ptr<Route> wpr)
{
	DEBUG_TRACE (DEBUG::Solo, string_compose ("route solo change, self = %1, update\n", self_solo_changed));

	std::shared_ptr<Route> route (wpr.lock());

	if (!route) {
		return;
	}

	if (Config->get_solo_control_is_listen_control()) {
		route_listen_changed (group_override, wpr);
		return;
	}

	DEBUG_TRACE (DEBUG::Solo, string_compose ("%1: self %2 masters %3 transition %4\n", route->name(), route->self_soloed(), route->solo_control()->get_masters_value(), route->solo_control()->transitioned_into_solo()));

	if (route->solo_control()->transitioned_into_solo() == 0) {
		/* route solo changed by upstream/downstream or clear all solo state; not interesting
		   to Session.
		*/
		DEBUG_TRACE (DEBUG::Solo, string_compose ("%1 not self-soloed nor soloed by master (%2), ignoring\n", route->name(), route->solo_control()->get_masters_value()));
		return;
	}

	std::shared_ptr<RouteList const> r = routes.reader ();
	int32_t delta = route->solo_control()->transitioned_into_solo ();

	/* the route may be a member of a group that has shared-solo
	 * semantics. If so, then all members of that group should follow the
	 * solo of the changed route. But ... this is optional, controlled by a
	 * Controllable::GroupControlDisposition.
	 *
	 * The first argument to the signal that this method is connected to is the
	 * GroupControlDisposition value that was used to change solo.
	 *
	 * If the solo change was done with group semantics (either InverseGroup
	 * (force the entire group to change even if the group shared solo is
	 * disabled) or UseGroup (use the group, which may or may not have the
	 * shared solo property enabled)) then as we propagate the change to
	 * the entire session we should IGNORE THE GROUP that the changed route
	 * belongs to.
	 */

	RouteGroup* rg = route->route_group ();
	const bool group_already_accounted_for = (group_override == Controllable::ForGroup);

	DEBUG_TRACE (DEBUG::Solo, string_compose ("propagate to session, group accounted for ? %1\n", group_already_accounted_for));

	if (delta == 1 && Config->get_exclusive_solo()) {

		/* new solo: disable all other solos, but not the group if its solo-enabled */
		_engine.monitor_port().clear_ports (false);

		for (auto const& i : *r) {

			if (i == route) {
				/* already changed */
				continue;
			}

			if (i->solo_isolate_control()->solo_isolated() || !i->can_solo()) {
				/* route does not get solo propagated to it */
				continue;
			}

			if ((group_already_accounted_for && i->route_group() && i->route_group() == rg)) {
				/* this route is a part of the same solo group as the route
				 * that was changed. Changing that route did change or will
				 * change all group members appropriately, so we can ignore it
				 * here
				 */
				continue;
			}

			i->solo_control()->set_value (0.0, group_override);
		}
	}

	DEBUG_TRACE (DEBUG::Solo, string_compose ("propagate solo change, delta = %1\n", delta));

	RouteList uninvolved;

	DEBUG_TRACE (DEBUG::Solo, string_compose ("%1\n", route->name()));

	for (auto const& i : *r) {
		bool in_signal_flow;

		if (i == route) {
			/* already changed */
			continue;
		}

		if (i->solo_isolate_control()->solo_isolated() || !i->can_solo()) {
			/* route does not get solo propagated to it */
			DEBUG_TRACE (DEBUG::Solo, string_compose ("%1 excluded from solo because iso = %2 can_solo = %3\n", i->name(), i->solo_isolate_control()->solo_isolated(), i->can_solo()));
			continue;
		}

		if ((group_already_accounted_for && i->route_group() && i->route_group() == rg)) {
			/* this route is a part of the same solo group as the route
			 * that was changed. Changing that route did change or will
			 * change all group members appropriately, so we can ignore it
			 * here
			 */
			continue;
		}

		in_signal_flow = false;

		DEBUG_TRACE (DEBUG::Solo, string_compose ("check feed from %1\n", i->name()));

		if (i->feeds (route)) {
			DEBUG_TRACE (DEBUG::Solo, string_compose ("\tthere is a feed from %1\n", i->name()));
			if (!route->soloed_by_others_upstream()) {
				i->solo_control()->mod_solo_by_others_downstream (delta);
			} else {
				DEBUG_TRACE (DEBUG::Solo, "\talready soloed by others upstream\n");
			}
			in_signal_flow = true;
		} else {
			DEBUG_TRACE (DEBUG::Solo, string_compose ("\tno feed from %1\n", i->name()));
		}

		DEBUG_TRACE (DEBUG::Solo, string_compose ("check feed to %1\n", i->name()));

		if (route->feeds (i)) {
			DEBUG_TRACE (DEBUG::Solo, string_compose ("%1 feeds %2 sboD %3 sboU %4\n",
			                                          route->name(),
			                                          i->name(),
			                                          route->soloed_by_others_downstream(),
			                                          route->soloed_by_others_upstream()));
			//NB. Triggers Invert Push, which handles soloed by downstream
			DEBUG_TRACE (DEBUG::Solo, string_compose ("\tmod %1 by %2\n", i->name(), delta));
			i->solo_control()->mod_solo_by_others_upstream (delta);
			in_signal_flow = true;
		} else {
			DEBUG_TRACE (DEBUG::Solo, string_compose("\tno feed to %1\n", i->name()) );
		}

		if (!in_signal_flow) {
			uninvolved.push_back (i);
		}
	}

	DEBUG_TRACE (DEBUG::Solo, "propagation complete\n");

	/* now notify that the mute state of the routes not involved in the signal
	   pathway of the just-solo-changed route may have altered.
	*/

	for (RouteList::iterator i = uninvolved.begin(); i != uninvolved.end(); ++i) {
		DEBUG_TRACE (DEBUG::Solo, string_compose ("mute change for %1, which neither feeds or is fed by %2\n", (*i)->name(), route->name()));
		(*i)->act_on_mute ();
		/* Session will emit SoloChanged() after all solo changes are
		 * complete, which should be used by UIs to update mute status
		 */
	}
}

void
Session::update_route_solo_state (std::shared_ptr<RouteList const> r)
{
	/* now figure out if anything that matters is soloed (or is "listening")*/

	bool something_soloed = false;
	bool something_listening = false;
	uint32_t listeners = 0;
	uint32_t isolated = 0;

	if (!r) {
		r = routes.reader();
	}

	for (auto const& i : *r) {
		if (i->can_monitor() && Config->get_solo_control_is_listen_control()) {
			if (i->solo_control()->soloed_by_self_or_masters()) {
				listeners++;
				something_listening = true;
			}
		} else if (i->can_solo()) {
			i->set_listen (false);
			if (i->can_solo() && i->solo_control()->soloed_by_self_or_masters()) {
				something_soloed = true;
			}
		}

		if (i->solo_isolate_control()->solo_isolated()) {
			isolated++;
		}
	}

	if (something_soloed != _non_soloed_outs_muted) {
		_non_soloed_outs_muted = something_soloed;
		SoloActive (_non_soloed_outs_muted); /* EMIT SIGNAL */
	}

	if (something_listening != _listening) {
		_listening = something_listening;
		SoloActive (_listening);
	}

	_listen_cnt = listeners;

	if (isolated != _solo_isolated_cnt) {
		_solo_isolated_cnt = isolated;
		IsolatedChanged (); /* EMIT SIGNAL */
	}

	DEBUG_TRACE (DEBUG::Solo, string_compose ("solo state updated by session, soloed? %1 listeners %2 isolated %3\n",
						  something_soloed, listeners, isolated));


	SoloChanged (); /* EMIT SIGNAL */
	set_dirty();
}

bool
Session::muted () const
{
	// TODO consider caching the value on every MuteChanged signal,
	// Note that API users may also subscribe to MuteChanged and hence
	// this method needs to be called first.
	bool muted = false;
	StripableList all;
	get_stripables (all);
	for (StripableList::const_iterator i = all.begin(); i != all.end(); ++i) {
		assert (!(*i)->is_auditioner()); // XXX remove me
		if ((*i)->is_monitor()) {
			continue;
		}
		std::shared_ptr<Route> r = std::dynamic_pointer_cast<Route>(*i);
		if (r && !r->active()) {
			continue;
		}
		std::shared_ptr<MuteControl> mc = (*i)->mute_control();
		if (mc && mc->muted ()) {
			muted = true;
			break;
		}
	}
	return muted;
}

std::vector<std::weak_ptr<AutomationControl> >
Session::cancel_all_mute ()
{
	StripableList all;
	get_stripables (all);
	std::vector<std::weak_ptr<AutomationControl> > muted;
	std::shared_ptr<AutomationControlList> cl (new AutomationControlList);
	for (StripableList::const_iterator i = all.begin(); i != all.end(); ++i) {
		assert (!(*i)->is_auditioner());
		if ((*i)->is_monitor()) {
			continue;
		}
		std::shared_ptr<Route> r = std::dynamic_pointer_cast<Route> (*i);
		if (r && !r->active()) {
			continue;
		}
		std::shared_ptr<AutomationControl> ac = (*i)->mute_control();
		if (ac && ac->get_value () > 0) {
			cl->push_back (ac);
			muted.push_back (std::weak_ptr<AutomationControl>(ac));
		}
	}
	if (!cl->empty ()) {
		set_controls (cl, 0.0, PBD::Controllable::UseGroup);
	}
	return muted;
}

void
Session::get_stripables (StripableList& sl, PresentationInfo::Flag fl) const
{
	std::shared_ptr<RouteList const> r = routes.reader ();
	for (auto const& i : *r) {
		if (i->presentation_info ().flags () & fl) {
			sl.push_back (i);
		}
	}

	if (fl & PresentationInfo::VCA) {
		VCAList v = _vca_manager->vcas ();
		sl.insert (sl.end(), v.begin(), v.end());
	}
}

StripableList
Session::get_stripables () const
{
	PresentationInfo::Flag fl = PresentationInfo::AllStripables;
	StripableList rv;
	Session::get_stripables (rv, fl);
	rv.sort (Stripable::Sorter ());
	return rv;
}

RouteList
Session::get_routelist (bool mixer_order, PresentationInfo::Flag fl) const
{
	std::shared_ptr<RouteList const> r = routes.reader ();
	RouteList rv;
	for (auto const& i : *r) {
		if (i->presentation_info ().flags () & fl) {
			rv.push_back (i);
		}
	}
	rv.sort (Stripable::Sorter (mixer_order));
	return rv;
}

std::shared_ptr<RouteList>
Session::get_routes_with_internal_returns() const
{
	std::shared_ptr<RouteList const> r = routes.reader ();
	std::shared_ptr<RouteList> rl (new RouteList);

	for (auto const& i : *r) {
		if (i->internal_return ()) {
			rl->push_back (i);
		}
	}
	return rl;
}

bool
Session::io_name_is_legal (const std::string& name) const
{
	std::shared_ptr<RouteList const> r = routes.reader ();

	for (map<string,bool>::const_iterator reserved = reserved_io_names.begin(); reserved != reserved_io_names.end(); ++reserved) {
		if (name == reserved->first) {
			if (!route_by_name (reserved->first)) {
				/* first instance of a reserved name is allowed for some */
				return reserved->second;
			}
			/* all other instances of a reserved name are not allowed */
			return false;
		}
	}

	for (auto const& i : *r) {
		if (i->name() == name) {
			return false;
		}

		if (i->has_io_processor_named (name)) {
			return false;
		}
	}

	std::shared_ptr<IOPlugList const> iop (_io_plugins.reader ());
	for (auto const& i : *iop) {
		if (i->io_name () == name) {
			return false;
		}
	}

	return true;
}

void
Session::set_exclusive_input_active (std::shared_ptr<RouteList> rl, bool onoff, bool flip_others)
{
	RouteList rl2;
	vector<string> connections;

	/* if we are passed only a single route and we're not told to turn
	 * others off, then just do the simple thing.
	 */

	if (flip_others == false && rl->size() == 1) {
		std::shared_ptr<MidiTrack> mt = std::dynamic_pointer_cast<MidiTrack> (rl->front());
		if (mt) {
			mt->set_input_active (onoff);
			return;
		}
	}

	for (RouteList::iterator rt = rl->begin(); rt != rl->end(); ++rt) {

		for (auto const& p : *(*rt)->input()->ports()) {
			p->get_connections (connections);
		}

		for (vector<string>::iterator s = connections.begin(); s != connections.end(); ++s) {
			routes_using_input_from (*s, rl2);
		}

		/* scan all relevant routes to see if others are on or off */

		bool others_are_already_on = false;

		for (RouteList::iterator r = rl2.begin(); r != rl2.end(); ++r) {

			std::shared_ptr<MidiTrack> mt = std::dynamic_pointer_cast<MidiTrack> (*r);

			if (!mt) {
				continue;
			}

			if ((*r) != (*rt)) {
				if (mt->input_active()) {
					others_are_already_on = true;
				}
			} else {
				/* this one needs changing */
				mt->set_input_active (onoff);
			}
		}

		if (flip_others) {

			/* globally reverse other routes */

			for (RouteList::iterator r = rl2.begin(); r != rl2.end(); ++r) {
				if ((*r) != (*rt)) {
					std::shared_ptr<MidiTrack> mt = std::dynamic_pointer_cast<MidiTrack> (*r);
					if (mt) {
						mt->set_input_active (!others_are_already_on);
					}
				}
			}
		}
	}
}

void
Session::routes_using_input_from (const string& str, RouteList& rl)
{
	std::shared_ptr<RouteList const> r = routes.reader();

	for (auto const& i : *r) {
		if (i->input()->connected_to (str)) {
			rl.push_back (i);
		}
	}
}

std::shared_ptr<Route>
Session::route_by_name (string name) const
{
	std::shared_ptr<RouteList const> r = routes.reader();

	for (auto const& i : *r) {
		if (i->name() == name) {
			return i;
		}
	}

	return nullptr;
}

std::shared_ptr<Stripable>
Session::stripable_by_name (string name) const
{
	StripableList sl;
	get_stripables (sl);

	for (auto & s : sl) {
		if (s->name() == name) {
			return s;
		}
	}
	return nullptr;
}

std::shared_ptr<Route>
Session::route_by_id (PBD::ID id) const
{
	std::shared_ptr<RouteList const> r = routes.reader ();

	for (auto const& i : *r) {
		if (i->id() == id) {
			return i;
		}
	}

	return std::shared_ptr<Route> ((Route*) 0);
}


std::shared_ptr<Stripable>
Session::stripable_by_id (PBD::ID id) const
{
	StripableList sl;
	get_stripables (sl);

	for (StripableList::const_iterator s = sl.begin(); s != sl.end(); ++s) {
		if ((*s)->id() == id) {
			return *s;
		}
	}

	return std::shared_ptr<Stripable>();
}

std::shared_ptr<Trigger>
Session::trigger_by_id (PBD::ID id) const
{
	std::shared_ptr<RouteList const> r = routes.reader ();
	for (auto const& i : *r) {
		std::shared_ptr<TriggerBox> box = i->triggerbox();
		if (box) {
			TriggerPtr trigger = box->trigger_by_id(id);
			if (trigger) {
				return trigger;
			}
		}
	}

	return std::shared_ptr<Trigger> ();
}

std::shared_ptr<Processor>
Session::processor_by_id (PBD::ID id) const
{
	std::shared_ptr<RouteList const> r = routes.reader ();

	for (auto const& i : *r) {
		std::shared_ptr<Processor> p = i->Route::processor_by_id (id);
		if (p) {
			return p;
		}
	}

	return std::shared_ptr<Processor> ();
}

std::shared_ptr<Route>
Session::get_remote_nth_route (PresentationInfo::order_t n) const
{
	return std::dynamic_pointer_cast<Route> (get_remote_nth_stripable (n, PresentationInfo::Route));
}

std::shared_ptr<Stripable>
Session::get_remote_nth_stripable (PresentationInfo::order_t n, PresentationInfo::Flag flags) const
{
	StripableList sl;
	PresentationInfo::order_t match_cnt = 0;

	get_stripables (sl);
	sl.sort (Stripable::Sorter());

	for (StripableList::const_iterator s = sl.begin(); s != sl.end(); ++s) {

		if ((*s)->presentation_info().hidden()) {
			/* if the caller didn't explicitly ask for hidden
			   stripables, ignore hidden ones. This matches
			   the semantics of the pre-PresentationOrder
			   "get by RID" logic of Ardour 4.x and earlier.

			   XXX at some point we should likely reverse
			   the logic of the flags, because asking for "the
			   hidden stripables" is not going to be common,
			   whereas asking for visible ones is normal.
			*/

			if (! (flags & PresentationInfo::Hidden)) {
				continue;
			}
		}

		if ((*s)->presentation_info().flag_match (flags)) {
			if (match_cnt++ == n) {
				return *s;
			}
		}
	}

	/* there is no nth stripable that matches the given flags */
	return std::shared_ptr<Stripable>();
}

std::shared_ptr<Route>
Session::route_by_selected_count (uint32_t id) const
{
	RouteList r (*(routes.reader ()));
	r.sort (Stripable::Sorter());

	RouteList::iterator i;

	for (i = r.begin(); i != r.end(); ++i) {
		if ((*i)->is_selected()) {
			if (id == 0) {
				return *i;
			}
			--id;
		}
	}

	return std::shared_ptr<Route> ();
}

void
Session::reassign_track_numbers ()
{
	int64_t tn = 0;
	int64_t bn = 0;
	uint32_t trigger_order = 0;
	RouteList r (*(routes.reader ()));
	r.sort (Stripable::Sorter());

	StateProtector sp (this);

	for (RouteList::iterator i = r.begin(); i != r.end(); ++i) {
		assert (!(*i)->is_auditioner());
		if (std::dynamic_pointer_cast<Track> (*i)) {
			(*i)->set_track_number(++tn);
		} else if (!(*i)->is_main_bus ()) {
			(*i)->set_track_number(--bn);
		}

		std::shared_ptr<TriggerBox> tb = (*i)->triggerbox();
		if (tb) {
			tb->set_order (trigger_order);
			trigger_order++;
		}
	}
	const uint32_t decimals = ceilf (log10f (tn + 1));
	const bool decimals_changed = _track_number_decimals != decimals;
	_track_number_decimals = decimals;

	if (decimals_changed && config.get_track_name_number ()) {
		for (RouteList::iterator i = r.begin(); i != r.end(); ++i) {
			std::shared_ptr<Track> t = std::dynamic_pointer_cast<Track> (*i);
			if (t) {
				t->resync_take_name ();
			}
		}
		// trigger GUI re-layout
		config.ParameterChanged("track-name-number");
	}

#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::OrderKeys)) {
		std::shared_ptr<RouteList const> rl = routes.reader ();
		for (auto const& i : *rl) {
			DEBUG_TRACE (DEBUG::OrderKeys, string_compose ("%1 numbered %2\n", i->name(), i->track_number()));
		}
	}
#endif /* NDEBUG */

}

void
Session::playlist_region_added (std::weak_ptr<Region> w)
{
	std::shared_ptr<Region> r = w.lock ();
	if (!r) {
		return;
	}

	/* These are the operations that are currently in progress... */
	list<GQuark> curr = _current_trans_quarks;
	curr.sort ();

	/* ...and these are the operations during which we want to update
	   the session range location markers.
	*/
	list<GQuark> ops;
	ops.push_back (Operations::capture);
	ops.push_back (Operations::paste);
	ops.push_back (Operations::duplicate_region);
	ops.push_back (Operations::insert_file);
	ops.push_back (Operations::insert_region);
	ops.push_back (Operations::drag_region_brush);
	ops.push_back (Operations::region_drag);
	ops.push_back (Operations::selection_grab);
	ops.push_back (Operations::region_fill);
	ops.push_back (Operations::fill_selection);
	ops.push_back (Operations::create_region);
	ops.push_back (Operations::region_copy);
	ops.push_back (Operations::fixed_time_region_copy);
	ops.sort ();

	/* See if any of the current operations match the ones that we want */
	list<GQuark> in;
	set_intersection (_current_trans_quarks.begin(), _current_trans_quarks.end(), ops.begin(), ops.end(), back_inserter (in));

	/* If so, update the session range markers */
	if (!in.empty ()) {
		maybe_update_session_range (r->position (), r->end ());
	}
}

/** Update the session range markers if a is before the current start or
 *  b is after the current end.
 */
void
Session::maybe_update_session_range (timepos_t const & a, timepos_t const & b)
{
	if (loading ()) {
		return;
	}

	samplepos_t session_end_marker_shift_samples = session_end_shift * nominal_sample_rate ();

	if (_session_range_location == 0) {

		set_session_extents (a, b + timepos_t (session_end_marker_shift_samples));

	} else {

		if (_session_range_is_free && (a < _session_range_location->start())) {
			_session_range_location->set_start (a);
		}

		if (_session_range_is_free && (b > _session_range_location->end())) {
			_session_range_location->set_end (b);
		}
	}
}

void
Session::set_session_range_is_free (bool yn)
{
	_session_range_is_free = yn;
}

void
Session::playlist_ranges_moved (list<Temporal::RangeMove> const & ranges)
{
	for (list<Temporal::RangeMove>::const_iterator i = ranges.begin(); i != ranges.end(); ++i) {
		maybe_update_session_range (i->from, i->to);
	}
}

void
Session::playlist_regions_extended (list<Temporal::Range> const & ranges)
{
	for (list<Temporal::Range>::const_iterator i = ranges.begin(); i != ranges.end(); ++i) {
		maybe_update_session_range (i->start(), i->end());
	}
}

/* Region management */

std::shared_ptr<Region>
Session::find_whole_file_parent (std::shared_ptr<Region const> child) const
{
	const RegionFactory::RegionMap& regions (RegionFactory::regions());
	RegionFactory::RegionMap::const_iterator i;
	std::shared_ptr<Region> region;

	Glib::Threads::Mutex::Lock lm (region_lock);

	for (i = regions.begin(); i != regions.end(); ++i) {

		region = i->second;

		if (region->whole_file()) {

			if (child->source_equivalent (region)) {
				return region;
			}
		}
	}

	return std::shared_ptr<Region> ();
}

int
Session::destroy_sources (list<std::shared_ptr<Source> > const& srcs)
{
	set<std::shared_ptr<Region> > relevant_regions;

	for (list<std::shared_ptr<Source> >::const_iterator s = srcs.begin(); s != srcs.end(); ++s) {
		RegionFactory::get_regions_using_source (*s, relevant_regions);
	}

	for (set<std::shared_ptr<Region> >::iterator r = relevant_regions.begin(); r != relevant_regions.end(); ) {
		set<std::shared_ptr<Region> >::iterator tmp;

		tmp = r;
		++tmp;

		_playlists->destroy_region (*r);
		RegionFactory::map_remove (*r);

		(*r)->drop_sources ();
		(*r)->drop_references ();

		relevant_regions.erase (r);

		r = tmp;
	}

	for (list<std::shared_ptr<Source> >::const_iterator s = srcs.begin(); s != srcs.end(); ++s) {

		{
			Glib::Threads::Mutex::Lock ls (source_lock);
			/* remove from the main source list */
			sources.erase ((*s)->id());
		}

		(*s)->mark_for_remove ();
		(*s)->drop_references ();
		SourceRemoved (std::weak_ptr<Source> (*s)); /* EMIT SIGNAL */
	}

	return 0;
}

int
Session::remove_last_capture ()
{
	list<std::shared_ptr<Source>> srcs;
	last_capture_sources (srcs);

	destroy_sources (srcs);

	/* save state so we don't end up with a session file
	 * referring to non-existent sources.
	 *
	 * Note: save_state calls reset_last_capture_sources ();
	 */

	save_state ();

	return 0;
}

void
Session::last_capture_sources (std::list<std::shared_ptr<Source>>& srcs) const
{
	std::shared_ptr<RouteList const> rl = routes.reader ();
	for (auto const& i : *rl) {
		std::shared_ptr<Track> tr = std::dynamic_pointer_cast<Track> (i);
		if (!tr) {
			continue;
		}

		list<std::shared_ptr<Source>> const& l = tr->last_capture_sources();
		srcs.insert (srcs.end(), l.begin(), l.end());
	}
}

bool
Session::have_last_capture_sources () const
{
	std::shared_ptr<RouteList const> rl = routes.reader ();
	for (auto const& i : *rl) {
		std::shared_ptr<Track> tr = std::dynamic_pointer_cast<Track> (i);
		if (!tr) {
			continue;
		}

		if (!tr->last_capture_sources().empty ()) {
			return true;
		}
	}
	return false;
}

void
Session::reset_last_capture_sources ()
{
	std::shared_ptr<RouteList const> rl = routes.reader ();
	for (auto const& i : *rl) {
		std::shared_ptr<Track> tr = std::dynamic_pointer_cast<Track> (i);
		if (!tr) {
			continue;
		}
		tr->reset_last_capture_sources ();
	}
	ClearedLastCaptureSources (); /* EMIT SIGNAL */
}

/* Source Management */

void
Session::add_source (std::shared_ptr<Source> source)
{
	pair<SourceMap::key_type, SourceMap::mapped_type> entry;
	pair<SourceMap::iterator,bool> result;

	entry.first = source->id();
	entry.second = source;

	{
		Glib::Threads::Mutex::Lock lm (source_lock);
		result = sources.insert (entry);
	}

	if (result.second) {

		/* yay, new source */

		std::shared_ptr<FileSource> fs = std::dynamic_pointer_cast<FileSource> (source);

		if (fs) {
			if (!fs->within_session()) {
				ensure_search_path_includes (Glib::path_get_dirname (fs->path()), fs->type());
			}
		}

		set_dirty();

		std::shared_ptr<AudioFileSource> afs;

		if ((afs = std::dynamic_pointer_cast<AudioFileSource>(source)) != 0) {
			if (Config->get_auto_analyse_audio()) {
				Analyser::queue_source_for_analysis (source, false);
			}
		}

		source->DropReferences.connect_same_thread (*this, std::bind (&Session::remove_source, this, std::weak_ptr<Source> (source), false));

		SourceAdded (std::weak_ptr<Source> (source)); /* EMIT SIGNAL */
	} else {
		/* If this happens there is a duplicate PBD::ID */
		assert (0);
		fatal << string_compose (_("programming error: %1"), "Failed to add source to source-list") << endmsg;
	}
}

void
Session::remove_source (std::weak_ptr<Source> src, bool drop_references)
{
	if (deletion_in_progress ()) {
		return;
	}

	SourceMap::iterator i;
	std::shared_ptr<Source> source = src.lock();

	if (!source) {
		return;
	}

	{
		Glib::Threads::Mutex::Lock lm (source_lock);

		if ((i = sources.find (source->id())) != sources.end()) {
			sources.erase (i);
		} else {
			return;
		}
	}

	SourceRemoved (src); /* EMIT SIGNAL */
	if (drop_references) {
		source->drop_references ();
		/* Removing a Source cannot be undone.
		 * We need to clear all undo commands that reference the
		 * removed source - or just clear all of the undo history.
		 */
		_history.clear();
	}

	if (source->empty ()) {
		/* No need to save when empty sources are removed.
		 * This is likely due to disk-writer initial dummies
		 * where files don't even exist on disk.
		 */
		return;
	}

	if (!in_cleanup () && !loading ()) {
		/* save state so we don't end up with a session file
		 * referring to non-existent sources.
		 */
		save_state ();
	}
}

std::shared_ptr<Source>
Session::source_by_id (const PBD::ID& id)
{
	Glib::Threads::Mutex::Lock lm (source_lock);
	SourceMap::iterator i;
	std::shared_ptr<Source> source;

	if ((i = sources.find (id)) != sources.end()) {
		source = i->second;
	}

	return source;
}

std::shared_ptr<AudioFileSource>
Session::audio_source_by_path_and_channel (const string& path, uint16_t chn) const
{
	/* Restricted to audio files because only audio sources have channel
	   as a property.
	*/

	Glib::Threads::Mutex::Lock lm (source_lock);

	for (SourceMap::const_iterator i = sources.begin(); i != sources.end(); ++i) {
		std::shared_ptr<AudioFileSource> afs
			= std::dynamic_pointer_cast<AudioFileSource>(i->second);

		if (afs && afs->path() == path && chn == afs->channel()) {
			return afs;
		}
	}

	return std::shared_ptr<AudioFileSource>();
}

std::shared_ptr<MidiSource>
Session::midi_source_by_path (const std::string& path, bool need_source_lock) const
{
	/* Restricted to MIDI files because audio sources require a channel
	   for unique identification, in addition to a path.
	*/

	Glib::Threads::Mutex::Lock lm (source_lock, Glib::Threads::NOT_LOCK);
	if (need_source_lock) {
		lm.acquire ();
	}

	for (SourceMap::const_iterator s = sources.begin(); s != sources.end(); ++s) {
		std::shared_ptr<MidiSource> ms
			= std::dynamic_pointer_cast<MidiSource>(s->second);
		std::shared_ptr<FileSource> fs
			= std::dynamic_pointer_cast<FileSource>(s->second);

		if (ms && fs && fs->path() == path) {
			return ms;
		}
	}

	return std::shared_ptr<MidiSource>();
}

uint32_t
Session::count_sources_by_origin (const string& path)
{
	uint32_t cnt = 0;
	Glib::Threads::Mutex::Lock lm (source_lock);

	for (SourceMap::const_iterator i = sources.begin(); i != sources.end(); ++i) {
		std::shared_ptr<FileSource> fs
			= std::dynamic_pointer_cast<FileSource>(i->second);

		if (fs && fs->origin() == path) {
			++cnt;
		}
	}

	return cnt;
}

static string
peak_file_helper (const string& peak_path, const string& file_path, const string& file_base, bool hash) {
	if (hash) {
		std::string checksum = Glib::Checksum::compute_checksum(Glib::Checksum::CHECKSUM_SHA1, file_path + G_DIR_SEPARATOR + file_base);
		return Glib::build_filename (peak_path, checksum + peakfile_suffix);
	} else {
		return Glib::build_filename (peak_path, file_base + peakfile_suffix);
	}
}

string
Session::construct_peak_filepath (const string& filepath, const bool in_session, const bool old_peak_name) const
{
	string interchange_dir_string = string (interchange_dir_name) + G_DIR_SEPARATOR;

	if (Glib::path_is_absolute (filepath)) {

		/* rip the session dir from the audiofile source */

		string session_path;
		bool in_another_session = true;

		if (filepath.find (interchange_dir_string) != string::npos) {

			session_path = Glib::path_get_dirname (filepath); /* now ends in audiofiles */
			session_path = Glib::path_get_dirname (session_path); /* now ends in session name */
			session_path = Glib::path_get_dirname (session_path); /* now ends in interchange */
			session_path = Glib::path_get_dirname (session_path); /* now has session path */

			/* see if it is within our session */

			for (vector<space_and_path>::const_iterator i = session_dirs.begin(); i != session_dirs.end(); ++i) {
				if (i->path == session_path) {
					in_another_session = false;
					break;
				}
			}
		} else {
			in_another_session = false;
		}


		if (in_another_session) {
			SessionDirectory sd (session_path);
			return peak_file_helper (sd.peak_path(), "", Glib::path_get_basename (filepath), !old_peak_name);
		}
	}

	/* 1) if file belongs to this session
	 * it may be a relative path (interchange/...)
	 * or just basename (session_state, remove source)
	 * -> just use the basename
	 */
	std::string filename = Glib::path_get_basename (filepath);
	std::string path;

	/* 2) if the file is outside our session dir:
	 * (imported but not copied) add the path for check-summming */
	if (!in_session) {
		path = Glib::path_get_dirname (filepath);
	}

	return peak_file_helper (_session_dir->peak_path(), path, Glib::path_get_basename (filepath), !old_peak_name);
}

string
Session::new_audio_source_path_for_embedded (const std::string& path)
{
	/* embedded source:
	 *
	 * we know that the filename is already unique because it exists
	 * out in the filesystem.
	 *
	 * However, when we bring it into the session, we could get a
	 * collision.
	 *
	 * Eg. two embedded files:
	 *
	 *          /foo/bar/baz.wav
	 *          /frob/nic/baz.wav
	 *
	 * When merged into session, these collide.
	 *
	 * There will not be a conflict with in-memory sources
	 * because when the source was created we already picked
	 * a unique name for it.
	 *
	 * This collision is not likely to be common, but we have to guard
	 * against it.  So, if there is a collision, take the md5 hash of the
	 * the path, and use that as the filename instead.
	 */

	SessionDirectory sdir (get_best_session_directory_for_new_audio());
	string base = Glib::path_get_basename (path);
	string newpath = Glib::build_filename (sdir.sound_path(), base);

	if (Glib::file_test (newpath, Glib::FILE_TEST_EXISTS)) {

		MD5 md5;

		md5.digestString (path.c_str());
		md5.writeToString ();
		base = md5.digestChars;

		string ext = get_suffix (path);

		if (!ext.empty()) {
			base += '.';
			base += ext;
		}

		newpath = Glib::build_filename (sdir.sound_path(), base);

		/* if this collides, we're screwed */

		if (Glib::file_test (newpath, Glib::FILE_TEST_EXISTS)) {
			error << string_compose (_("Merging embedded file %1: name collision AND md5 hash collision!"), path) << endmsg;
			return string();
		}

	}

	return newpath;
}

/** Return true if there are no audio file sources that use @p name as
 * the filename component of their path.
 *
 * Return false otherwise.
 *
 * This method MUST ONLY be used to check in-session, mono files since it
 * hard-codes the channel of the audio file source we are looking for as zero.
 *
 * If/when Ardour supports native files in non-mono formats, the logic here
 * will need to be revisited.
 */
bool
Session::audio_source_name_is_unique (const string& name)
{
	std::vector<string> sdirs = source_search_path (DataType::AUDIO);
	uint32_t existing = 0;

	for (vector<string>::const_iterator i = sdirs.begin(); i != sdirs.end(); ++i) {

		/* note that we search *without* the extension so that
		   we don't end up both "Audio 1-1.wav" and "Audio 1-1.caf"
		   in the event that this new name is required for
		   a file format change.
		*/

		const string spath = *i;

		if (matching_unsuffixed_filename_exists_in (spath, name)) {
			existing++;
			break;
		}

		/* it is possible that we have the path already
		 * assigned to a source that has not yet been written
		 * (ie. the write source for a diskstream). we have to
		 * check this in order to make sure that our candidate
		 * path isn't used again, because that can lead to
		 * two Sources point to the same file with different
		 * notions of their removability.
		 */


		string possible_path = Glib::build_filename (spath, name);

		if (audio_source_by_path_and_channel (possible_path, 0)) {
			existing++;
			break;
		}
	}

	return (existing == 0);
}

string
Session::format_audio_source_name (const string& legalized_base, uint32_t nchan, uint32_t chan, bool take_required, uint32_t cnt, bool related_exists)
{
	ostringstream sstr;
	const string ext = native_header_format_extension (config.get_native_file_header_format(), DataType::AUDIO);

	sstr << legalized_base;

	if (take_required || related_exists) {
		sstr << '-';
		sstr << cnt;
	}

	if (nchan == 2) {
		if (chan == 0) {
			sstr << "%L";
		} else {
			sstr << "%R";
		}
	} else if (nchan > 2) {
		if (nchan <= 26) {
			sstr << '%';
			sstr << static_cast<char>('a' + chan);
		} else {
			/* XXX what? more than 26 channels! */
			sstr << '%';
			sstr << chan+1;
		}
	}

	sstr << ext;

	return sstr.str();
}

/** Return a unique name based on \a base for a new internal audio source */
string
Session::new_audio_source_path (const string& base, uint32_t nchan, uint32_t chan, bool take_required)
{
	uint32_t cnt;
	string possible_name;
	const uint32_t limit = 9999; // arbitrary limit on number of files with the same basic name
	string legalized;
	bool some_related_source_name_exists = false;

	legalized = legalize_for_path (base);

	// Find a "version" of the base name that doesn't exist in any of the possible directories.

	for (cnt = 1; cnt <= limit; ++cnt) {

		possible_name = format_audio_source_name (legalized, nchan, chan, take_required, cnt, some_related_source_name_exists);

		if (audio_source_name_is_unique (possible_name)) {
			break;
		}

		some_related_source_name_exists = true;

		if (cnt > limit) {
			error << string_compose(
					_("There are already %1 recordings for %2, which I consider too many."),
					limit, base) << endmsg;
			destroy ();
			throw failed_constructor();
		}
	}

	/* We've established that the new name does not exist in any session
	 * directory, so now find out which one we should use for this new
	 * audio source.
	 */

	SessionDirectory sdir (get_best_session_directory_for_new_audio());

	std::string s = Glib::build_filename (sdir.sound_path(), possible_name);

	return s;
}

/** Return a unique name based on `base` for a new internal MIDI source */
string
Session::new_midi_source_path (const string& base, bool need_lock)
{
	string possible_path;
	string possible_name;

	possible_name = legalize_for_path (base);

	// Find a "version" of the file name that doesn't exist in any of the possible directories.
	std::vector<string> sdirs = source_search_path(DataType::MIDI);

	/* - the main session folder is the first in the vector.
	 * - after checking all locations for file-name uniqueness,
	 *   we keep the one from the last iteration as new file name
	 * - midi files are small and should just be kept in the main session-folder
	 *
	 * -> reverse the array, check main session folder last and use that as location
	 *    for MIDI files.
	 */
	std::reverse(sdirs.begin(), sdirs.end());

	while (true) {
		possible_name = bump_name_once (possible_name, '-');

		uint32_t existing = 0;

		for (vector<string>::const_iterator i = sdirs.begin(); i != sdirs.end(); ++i) {

			possible_path = Glib::build_filename (*i, possible_name + ".mid");

			if (Glib::file_test (possible_path, Glib::FILE_TEST_EXISTS)) {
				existing++;
			}

			if (midi_source_by_path (possible_path, need_lock)) {
				existing++;
			}
		}

		if (possible_path.size () >= PATH_MAX) {
			error << string_compose(
					_("There are already many recordings for %1, resulting in a too long file-path %2."),
					base, possible_path) << endmsg;
			destroy ();
			return 0;
		}

		if (existing == 0) {
			break;
		}
	}

	/* No need to "find best location" for software/app-based RAID, because
	   MIDI is so small that we always put it in the same place.
	*/

	return possible_path;
}


/** Create a new within-session audio source */
std::shared_ptr<AudioFileSource>
Session::create_audio_source_for_session (size_t n_chans, string const & base, uint32_t chan)
{
	const string path = new_audio_source_path (base, n_chans, chan, true);

	if (!path.empty()) {
		return std::dynamic_pointer_cast<AudioFileSource> (SourceFactory::createWritable (DataType::AUDIO, *this, path, sample_rate(), true, true));
	} else {
		throw failed_constructor ();
	}
}

/** Create a new within-session MIDI source */
std::shared_ptr<MidiSource>
Session::create_midi_source_for_session (string const & basic_name)
{
	const string path = new_midi_source_path (basic_name);

	if (!path.empty()) {
		return std::dynamic_pointer_cast<SMFSource> (SourceFactory::createWritable (DataType::MIDI, *this, path, sample_rate()));
	} else {
		throw failed_constructor ();
	}
}

/** Create a new within-session MIDI source */
std::shared_ptr<MidiSource>
Session::create_midi_source_by_stealing_name (std::shared_ptr<Track> track)
{
	/* the caller passes in the track the source will be used in,
	   so that we can keep the numbering sane.

	   Rationale: a track with the name "Foo" that has had N
	   captures carried out so far will ALREADY have a write source
	   named "Foo-N+1.mid" waiting to be used for the next capture.

	   If we call new_midi_source_name() we will get "Foo-N+2". But
	   there is no region corresponding to "Foo-N+1", so when
	   "Foo-N+2" appears in the track, the gap presents the user
	   with odd behaviour - why did it skip past Foo-N+1?

	   We could explain this to the user in some odd way, but
	   instead we rename "Foo-N+1.mid" as "Foo-N+2.mid", and then
	   use "Foo-N+1" here.

	   If that attempted rename fails, we get "Foo-N+2.mid" anyway.
	*/

	std::shared_ptr<MidiTrack> mt = std::dynamic_pointer_cast<MidiTrack> (track);
	assert (mt);
	std::string name = track->steal_write_source_name ();

	if (name.empty()) {
		return std::shared_ptr<MidiSource>();
	}

	/* MIDI files are small, just put them in the first location of the
	   session source search path.
	*/

	const string path = Glib::build_filename (source_search_path (DataType::MIDI).front(), name);

	return std::dynamic_pointer_cast<SMFSource> (SourceFactory::createWritable (DataType::MIDI, *this, path, sample_rate()));
}

bool
Session::playlist_is_active (std::shared_ptr<Playlist> playlist)
{
	Glib::Threads::Mutex::Lock lm (_playlists->lock);
	for (PlaylistSet::iterator i = _playlists->playlists.begin(); i != _playlists->playlists.end(); i++) {
		if ( (*i) == playlist ) {
			return true;
		}
	}
	return false;
}

void
Session::add_playlist (std::shared_ptr<Playlist> playlist)
{
	if (playlist->hidden()) {
		return;
	}

	_playlists->add (playlist);

	set_dirty();
}

void
Session::remove_playlist (std::weak_ptr<Playlist> weak_playlist)
{
	if (deletion_in_progress ()) {
		return;
	}

	std::shared_ptr<Playlist> playlist (weak_playlist.lock());

	if (!playlist) {
		return;
	}

	_playlists->remove (playlist);

	set_dirty();
}

void
Session::set_audition (std::shared_ptr<Region> r)
{
	pending_audition_region = r;
	add_post_transport_work (PostTransportAudition);
	_butler->schedule_transport_work ();
}

void
Session::audition_playlist ()
{
	SessionEvent* ev = new SessionEvent (SessionEvent::Audition, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0);
	ev->region.reset ();
	queue_event (ev);
}

void
Session::load_io_plugin (std::shared_ptr<IOPlug> ioplugin)
{
	{
		RCUWriter<IOPlugList> writer (_io_plugins);
		std::shared_ptr<IOPlugList> iop = writer.get_copy ();
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		ioplugin->ensure_io ();
		iop->push_back (ioplugin);
		ioplugin->LatencyChanged.connect_same_thread (*this, std::bind (&Session::update_latency_compensation, this, true, false));
	}
	IOPluginsChanged (); /* EMIT SIGNAL */
	set_dirty();
}

bool
Session::unload_io_plugin (std::shared_ptr<IOPlug> ioplugin)
{
	{
		RCUWriter<IOPlugList> writer (_io_plugins);
		std::shared_ptr<IOPlugList> iop = writer.get_copy ();
		auto i = find (iop->begin (), iop->end (), ioplugin);
		if (i == iop->end ()) {
			return false;
		}
		(*i)->drop_references ();
		iop->erase (i);
	}
	IOPluginsChanged (); /* EMIT SIGNAL */
	set_dirty();
	_io_plugins.flush ();
	return true;
}

void
Session::register_lua_function (
		const std::string& name,
		const std::string& script,
		const LuaScriptParamList& args
		)
{
	Glib::Threads::Mutex::Lock lm (lua_lock);

	lua_State* L = lua.getState();

	const std::string& bytecode = LuaScripting::get_factory_bytecode (script);
	luabridge::LuaRef tbl_arg (luabridge::newTable(L));
	for (LuaScriptParamList::const_iterator i = args.begin(); i != args.end(); ++i) {
		if ((*i)->optional && !(*i)->is_set) { continue; }
		tbl_arg[(*i)->name] = (*i)->value;
	}
	(*_lua_add)(name, bytecode, tbl_arg); // throws luabridge::LuaException
	lm.release();

	LuaScriptsChanged (); /* EMIT SIGNAL */
	set_dirty();
}

void
Session::unregister_lua_function (const std::string& name)
{
	Glib::Threads::Mutex::Lock lm (lua_lock);
	(*_lua_del)(name); // throws luabridge::LuaException
	lua.collect_garbage ();
	lm.release();

	LuaScriptsChanged (); /* EMIT SIGNAL */
	set_dirty();
}

std::vector<std::string>
Session::registered_lua_functions ()
{
	Glib::Threads::Mutex::Lock lm (lua_lock);
	std::vector<std::string> rv;

	try {
		luabridge::LuaRef list ((*_lua_list)());
		for (luabridge::Iterator i (list); !i.isNil (); ++i) {
			if (!i.key ().isString ()) { assert(0); continue; }
			rv.push_back (i.key ().cast<std::string> ());
		}
	} catch (...) { }
	return rv;
}

static void _lua_print (std::string s) {
#ifndef NDEBUG
	std::cout << "LuaSession: " << s << "\n";
#endif
	PBD::info << "LuaSession: " << s << endmsg;
}

void
Session::try_run_lua (pframes_t nframes)
{
	if (_n_lua_scripts == 0) return;
	Glib::Threads::Mutex::Lock tm (lua_lock, Glib::Threads::TRY_LOCK);
	if (tm.locked ()) {
		try { (*_lua_run)(nframes); } catch (...) { }
		lua.collect_garbage_step ();
	}
}

void
Session::setup_lua ()
{
	lua.Print.connect (&_lua_print);
	lua.do_command (
			"function ArdourSession ()"
			"  local self = { scripts = {}, instances = {} }"
			""
			"  local remove = function (n)"
			"   self.scripts[n] = nil"
			"   self.instances[n] = nil"
			"   Session:scripts_changed()" // call back
			"  end"
			""
			"  local addinternal = function (n, f, a)"
			"   assert(type(n) == 'string', 'function-name must be string')"
			"   assert(type(f) == 'function', 'Given script is a not a function')"
			"   assert(type(a) == 'table' or type(a) == 'nil', 'Given argument is invalid')"
			"   assert(self.scripts[n] == nil, 'Callback \"'.. n ..'\" already exists.')"
			"   self.scripts[n] = { ['f'] = f, ['a'] = a }"
			"   local env = { print = print, tostring = tostring, assert = assert, ipairs = ipairs, error = error, select = select, string = string, type = type, tonumber = tonumber, collectgarbage = collectgarbage, pairs = pairs, math = math, table = table, pcall = pcall, bit32=bit32, Session = Session, PBD = PBD, Temporal = Temporal, Timecode = Timecode, Evoral = Evoral, C = C, ARDOUR = ARDOUR }"
			"   self.instances[n] = load (string.dump(f, true), nil, nil, env)(a)"
			"   Session:scripts_changed()" // call back
			"  end"
			""
			"  local add = function (n, b, a)"
			"   assert(type(b) == 'string', 'ByteCode must be string')"
			"   load (b)()" // assigns f
			"   assert(type(f) == 'string', 'Assigned ByteCode must be string')"
			"   addinternal (n, load(f), a)"
			"  end"
			""
			"  local run = function (...)"
			"   for n, s in pairs (self.instances) do"
			"     local status, err = pcall (s, ...)"
			"     if not status then"
			"       print ('fn \"'.. n .. '\": ', err)"
			"       remove (n)"
			"      end"
			"   end"
			"   collectgarbage(\"step\")"
			"  end"
			""
			"  local cleanup = function ()"
			"   self.scripts = nil"
			"   self.instances = nil"
			"  end"
			""
			"  local list = function ()"
			"   local rv = {}"
			"   for n, _ in pairs (self.scripts) do"
			"     rv[n] = true"
			"   end"
			"   return rv"
			"  end"
			""
			"  local function basic_serialize (o)"
			"    if type(o) == \"number\" then"
			"     return tostring(o)"
			"    else"
			"     return string.format(\"%q\", o)"
			"    end"
			"  end"
			""
			"  local function serialize (name, value)"
			"   local rv = name .. ' = '"
			"   collectgarbage()"
			"   if type(value) == \"number\" or type(value) == \"string\" or type(value) == \"nil\" then"
			"    return rv .. basic_serialize(value) .. ' '"
			"   elseif type(value) == \"table\" then"
			"    rv = rv .. '{} '"
			"    for k,v in pairs(value) do"
			"     local fieldname = string.format(\"%s[%s]\", name, basic_serialize(k))"
			"     rv = rv .. serialize(fieldname, v) .. ' '"
			"     collectgarbage()" // string concatenation allocates a new string :(
			"    end"
			"    return rv;"
			"   elseif type(value) == \"function\" then"
			"     return rv .. string.format(\"%q\", string.dump(value, true))"
			"   else"
			"    error('cannot save a ' .. type(value))"
			"   end"
			"  end"
			""
			""
			"  local save = function ()"
			"   return (serialize('scripts', self.scripts))"
			"  end"
			""
			"  local restore = function (state)"
			"   self.scripts = {}"
			"   load (state)()"
			"   for n, s in pairs (scripts) do"
			"    addinternal (n, load(s['f']), s['a'])"
			"   end"
			"  end"
			""
			" return { run = run, add = add, remove = remove,"
		  "          list = list, restore = restore, save = save, cleanup = cleanup}"
			" end"
			" "
			" sess = ArdourSession ()"
			" ArdourSession = nil"
			" "
			"function ardour () end"
			);

	lua_State* L = lua.getState();

	try {
		luabridge::LuaRef lua_sess = luabridge::getGlobal (L, "sess");
		lua.do_command ("sess = nil"); // hide it.
		lua.do_command ("collectgarbage()");

		_lua_run = new luabridge::LuaRef(lua_sess["run"]);
		_lua_add = new luabridge::LuaRef(lua_sess["add"]);
		_lua_del = new luabridge::LuaRef(lua_sess["remove"]);
		_lua_list = new luabridge::LuaRef(lua_sess["list"]);
		_lua_save = new luabridge::LuaRef(lua_sess["save"]);
		_lua_load = new luabridge::LuaRef(lua_sess["restore"]);
		_lua_cleanup = new luabridge::LuaRef(lua_sess["cleanup"]);
	} catch (luabridge::LuaException const& e) {
		fatal << string_compose (_("programming error: %1"),
				std::string ("Failed to setup session Lua interpreter") + e.what ())
			<< endmsg;
		abort(); /*NOTREACHED*/
	} catch (...) {
		fatal << string_compose (_("programming error: %1"),
				X_("Failed to setup session Lua interpreter"))
			<< endmsg;
		abort(); /*NOTREACHED*/
	}

	lua_mlock (L, 1);
	LuaBindings::stddef (L);
	LuaBindings::common (L);
	LuaBindings::dsp (L);
	luabindings_session_rt (L);

	lua_mlock (L, 0);
	luabridge::push <Session *> (L, this);
	lua_setglobal (L, "Session");
}

void
Session::scripts_changed ()
{
	assert (!lua_lock.trylock()); // must hold lua_lock

	try {
		luabridge::LuaRef list ((*_lua_list)());
		int cnt = 0;
		for (luabridge::Iterator i (list); !i.isNil (); ++i) {
			if (!i.key ().isString ()) { assert(0); continue; }
			++cnt;
		}
		_n_lua_scripts = cnt;
	} catch (luabridge::LuaException const& e) {
		fatal << string_compose (_("programming error: %1"),
				std::string ("Indexing Lua Session Scripts failed.") + e.what ())
			<< endmsg;
		abort(); /*NOTREACHED*/
	} catch (...) {
		fatal << string_compose (_("programming error: %1"),
				X_("Indexing Lua Session Scripts failed."))
			<< endmsg;
		abort(); /*NOTREACHED*/
	}
}

void
Session::non_realtime_set_audition ()
{
	assert (pending_audition_region);
	auditioner->audition_region (pending_audition_region);
	pending_audition_region.reset ();
	AuditionActive (true); /* EMIT SIGNAL */
}

void
Session::audition_region (std::shared_ptr<Region> r)
{
	SessionEvent* ev = new SessionEvent (SessionEvent::Audition, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0);
	ev->region = r;
	queue_event (ev);
}

void
Session::cancel_audition ()
{
	if (!auditioner) {
		return;
	}
	if (auditioner->auditioning()) {
		auditioner->cancel_audition ();
		AuditionActive (false); /* EMIT SIGNAL */
	}
}

bool
Session::is_auditioning () const
{
	/* can be called before we have an auditioner object */
	if (auditioner) {
		return auditioner->auditioning();
	} else {
		return false;
	}
}

void
Session::graph_reordered (bool called_from_backend)
{
	/* don't do this stuff if we are setting up connections
	   from a set_state() call or creating new tracks. Ditto for deletion.
	*/

	if (inital_connect_or_deletion_in_progress () || _adding_routes_in_progress || _reconnecting_routes_in_progress || _route_deletion_in_progress) {
		return;
	}

	resort_routes ();

	/* force all diskstreams to update their capture offset values to
	 * reflect any changes in latencies within the graph.
	 *
	 * XXX: Is this required? When the graph-order callback
	 * is initiated by the backend, it is always followed by
	 * a latency callback.
	 */
	update_latency_compensation (true, called_from_backend);
}

/** @return Number of samples that there is disk space available to write,
 *  if known.
 */
std::optional<samplecnt_t>
Session::available_capture_duration ()
{
	Glib::Threads::Mutex::Lock lm (space_lock);

	if (_total_free_4k_blocks_uncertain) {
		return std::optional<samplecnt_t> ();
	}

	float sample_bytes_on_disk = 4.0; // keep gcc happy

	switch (config.get_native_file_data_format()) {
	case FormatFloat:
		sample_bytes_on_disk = 4.0;
		break;

	case FormatInt24:
		sample_bytes_on_disk = 3.0;
		break;

	case FormatInt16:
		sample_bytes_on_disk = 2.0;
		break;

	default:
		/* impossible, but keep some gcc versions happy */
		fatal << string_compose (_("programming error: %1"),
					 X_("illegal native file data format"))
		      << endmsg;
		abort(); /*NOTREACHED*/
	}

	double scale = 4096.0 / sample_bytes_on_disk;

	if (_total_free_4k_blocks * scale > (double) max_samplecnt) {
		return max_samplecnt;
	}

	return (samplecnt_t) floor (_total_free_4k_blocks * scale);
}

void
Session::tempo_map_changed ()
{
	clear_clicks ();
	sync_cues ();

	foreach_route (&Route::tempo_map_changed);

	_playlists->update_after_tempo_map_change ();

	set_dirty ();
}

void
Session::ensure_buffers_unlocked (ChanCount howmany)
{
	if (_required_thread_buffers >= howmany) {
		return;
	}
	Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
	ensure_buffers (howmany);
}

/** Ensures that all buffers (scratch, send, silent, etc) are allocated for
 * the given count with the current block size.
 * Must be called with the process-lock held
 */
void
Session::ensure_buffers (ChanCount howmany)
{
	size_t want_size = bounce_processing() ? bounce_chunk_size : 0;
	if (howmany.n_total () == 0) {
		howmany = _required_thread_buffers;
	}
	if (_required_thread_buffers >= howmany && _required_thread_buffersize == want_size) {
		return;
	}
	_required_thread_buffers = ChanCount::max (_required_thread_buffers, howmany);
	_required_thread_buffersize = want_size;
	BufferManager::ensure_buffers (_required_thread_buffers, _required_thread_buffersize);
}

uint32_t
Session::next_insert_id ()
{
	/* this doesn't really loop forever. just think about it */

	while (true) {
		for (boost::dynamic_bitset<uint32_t>::size_type n = 1; n < insert_bitset.size(); ++n) {
			if (!insert_bitset[n]) {
				insert_bitset[n] = true;
				return n;

			}
		}

		/* none available, so resize and try again */

		insert_bitset.resize (insert_bitset.size() + 16, false);
	}
}

uint32_t
Session::next_send_id ()
{
	/* this doesn't really loop forever. just think about it */

	while (true) {
		for (boost::dynamic_bitset<uint32_t>::size_type n = 1; n < send_bitset.size(); ++n) {
			if (!send_bitset[n]) {
				send_bitset[n] = true;
				return n;

			}
		}

		/* none available, so resize and try again */

		send_bitset.resize (send_bitset.size() + 16, false);
	}
}

uint32_t
Session::next_surround_send_id ()
{
	/* this doesn't really loop forever. just think about it */

	while (true) {
		for (boost::dynamic_bitset<uint32_t>::size_type n = 1; n < surround_send_bitset.size(); ++n) {
			if (!surround_send_bitset[n]) {
				surround_send_bitset[n] = true;
				return n;

			}
		}

		/* none available, so resize and try again */

		surround_send_bitset.resize (surround_send_bitset.size() + 16, false);
	}
}

uint32_t
Session::next_aux_send_id ()
{
	/* this doesn't really loop forever. just think about it */

	while (true) {
		for (boost::dynamic_bitset<uint32_t>::size_type n = 1; n < aux_send_bitset.size(); ++n) {
			if (!aux_send_bitset[n]) {
				aux_send_bitset[n] = true;
				return n;

			}
		}

		/* none available, so resize and try again */

		aux_send_bitset.resize (aux_send_bitset.size() + 16, false);
	}
}

uint32_t
Session::next_return_id ()
{
	/* this doesn't really loop forever. just think about it */

	while (true) {
		for (boost::dynamic_bitset<uint32_t>::size_type n = 1; n < return_bitset.size(); ++n) {
			if (!return_bitset[n]) {
				return_bitset[n] = true;
				return n;

			}
		}

		/* none available, so resize and try again */

		return_bitset.resize (return_bitset.size() + 16, false);
	}
}

void
Session::mark_send_id (uint32_t id)
{
	if (id >= send_bitset.size()) {
		send_bitset.resize (id+16, false);
	}
	if (send_bitset[id]) {
		warning << string_compose (_("send ID %1 appears to be in use already"), id) << endmsg;
	}
	send_bitset[id] = true;
}

void
Session::mark_aux_send_id (uint32_t id)
{
	if (id >= aux_send_bitset.size()) {
		aux_send_bitset.resize (id+16, false);
	}
	if (aux_send_bitset[id]) {
		warning << string_compose (_("aux send ID %1 appears to be in use already"), id) << endmsg;
	}
	aux_send_bitset[id] = true;
}

void
Session::mark_surround_send_id (uint32_t id)
{
	if (id >= surround_send_bitset.size()) {
		surround_send_bitset.resize (id+16, false);
	}
	if (surround_send_bitset[id]) {
		warning << string_compose (_("surround send ID %1 appears to be in use already"), id) << endmsg;
	}
	surround_send_bitset[id] = true;
}

void
Session::mark_return_id (uint32_t id)
{
	if (id >= return_bitset.size()) {
		return_bitset.resize (id+16, false);
	}
	if (return_bitset[id]) {
		warning << string_compose (_("return ID %1 appears to be in use already"), id) << endmsg;
	}
	return_bitset[id] = true;
}

void
Session::mark_insert_id (uint32_t id)
{
	if (id >= insert_bitset.size()) {
		insert_bitset.resize (id+16, false);
	}
	if (insert_bitset[id]) {
		warning << string_compose (_("insert ID %1 appears to be in use already"), id) << endmsg;
	}
	insert_bitset[id] = true;
}

void
Session::unmark_send_id (uint32_t id)
{
	if (deletion_in_progress ()) {
		return;
	}
	if (id < send_bitset.size()) {
		send_bitset[id] = false;
	}
}

void
Session::unmark_aux_send_id (uint32_t id)
{
	if (deletion_in_progress ()) {
		return;
	}
	if (id < aux_send_bitset.size()) {
		aux_send_bitset[id] = false;
	}
}

void
Session::unmark_surround_send_id (uint32_t id)
{
	if (deletion_in_progress ()) {
		return;
	}
	if (id < surround_send_bitset.size()) {
		surround_send_bitset[id] = false;
	}
}

void
Session::unmark_return_id (uint32_t id)
{
	if (deletion_in_progress ()) {
		return;
	}
	if (id < return_bitset.size()) {
		return_bitset[id] = false;
	}
}

void
Session::unmark_insert_id (uint32_t id)
{
	if (deletion_in_progress ()) {
		return;
	}
	if (id < insert_bitset.size()) {
		insert_bitset[id] = false;
	}
}

void
Session::reset_native_file_format ()
{
	std::shared_ptr<RouteList const> rl = routes.reader ();

	for (auto const& i : *rl) {
		std::shared_ptr<Track> tr = std::dynamic_pointer_cast<Track> (i);
		if (tr) {
			/* don't save state as we do this, there's no point */
			_state_of_the_state = StateOfTheState (_state_of_the_state | InCleanup);
			tr->reset_write_sources (false);
			_state_of_the_state = StateOfTheState (_state_of_the_state & ~InCleanup);
		}
	}
}

bool
Session::route_name_unique (string n) const
{
	std::shared_ptr<RouteList const> rl = routes.reader ();

	for (auto const& i : *rl) {
		if (i->name() == n) {
			return false;
		}
	}

	return true;
}

bool
Session::route_name_internal (string n) const
{
	if (auditioner && auditioner->name() == n) {
		return true;
	}

	if (_click_io && _click_io->name() == n) {
		return true;
	}

	return false;
}

int
Session::freeze_all (InterThreadInfo& itt)
{
	std::shared_ptr<RouteList const> r = routes.reader ();

	for (auto const& i : *r) {

		std::shared_ptr<Track> t;

		if ((t = std::dynamic_pointer_cast<Track>(i)) != 0) {
			/* XXX this is wrong because itt.progress will keep returning to zero at the start
			   of every track.
			*/
			t->freeze_me (itt);
		}
	}

	return 0;
}

struct MidiSourceLockMap
{
	std::shared_ptr<MidiSource> src;
	Source::WriterLock lock;

	MidiSourceLockMap (std::shared_ptr<MidiSource> midi_source) : src (midi_source), lock (src->mutex()) {}
};

std::shared_ptr<Region>
Session::write_one_track (Track& track, samplepos_t start, samplepos_t end,
                          bool /*overwrite*/, vector<std::shared_ptr<Source> >& srcs,
                          InterThreadInfo& itt,
                          std::shared_ptr<Processor> endpoint, bool include_endpoint,
                          bool for_export, bool for_freeze,
                          std::string const& source_name, std::string const& region_name)
{
	std::shared_ptr<Region> result;
	std::shared_ptr<Playlist> playlist;
	std::shared_ptr<Source> source;
	ChanCount diskstream_channels (track.n_channels());
	samplepos_t position;
	samplecnt_t this_chunk;
	samplepos_t to_do;
	samplepos_t latency_skip;
	samplepos_t out_pos;
	BufferSet buffers;
	samplepos_t len = end - start;
	bool need_block_size_reset = false;
	ChanCount const max_proc = track.max_processor_streams ();
	string legal_name;
	string possible_path;
	MidiBuffer resolved (256);
	MidiNoteTracker tracker;
	DataType data_type = track.data_type();
	std::vector<MidiSourceLockMap*> midi_source_locks;

	if (end <= start) {
		error << string_compose (_("Cannot write a range where end <= start (e.g. %1 <= %2)"),
					 end, start) << endmsg;
		return result;
	}

	diskstream_channels = track.bounce_get_output_streams (diskstream_channels, endpoint,
			include_endpoint, for_export, for_freeze);

	if (data_type == DataType::MIDI && endpoint && !for_export && !for_freeze && diskstream_channels.n(DataType::AUDIO) > 0) {
		data_type = DataType::AUDIO;
	}

	if (diskstream_channels.n(data_type) < 1) {
		error << _("Cannot write a range with no data.") << endmsg;
		return result;
	}

	/* block all process callback handling, so that thread-buffers
	 * are available here.
	 */
	block_processing ();

	_bounce_processing_active = true;

	/* call tree *MUST* hold route_lock */

	if ((playlist = track.playlist()) == 0) {
		goto out;
	}

	if (source_name.length() > 0) {
		/*if the user passed in a name, we will use it, and also prepend the resulting sources with that name*/
		legal_name = legalize_for_path (source_name);
	} else {
		legal_name = legalize_for_path (playlist->name ());
	}

	for (uint32_t chan_n = 0; chan_n < diskstream_channels.n(data_type); ++chan_n) {

		string path = ((data_type == DataType::AUDIO)
		               ? new_audio_source_path (legal_name, diskstream_channels.n_audio(), chan_n, false)
		               : new_midi_source_path (legal_name));

		if (path.empty()) {
			goto out;
		}

		try {
			source = SourceFactory::createWritable (data_type, *this, path, sample_rate());
		}

		catch (failed_constructor& err) {
			error << string_compose (_("cannot create new file \"%1\" for %2"), path, track.name()) << endmsg;
			goto out;
		}

		source->set_captured_for(track.name());

		time_t now;
		time (&now);
		Glib::DateTime tm (Glib::DateTime::create_now_local (now));
		source->set_take_id (tm.format ("%F %H.%M.%S"));

		srcs.push_back (source);
	}

	/* tell redirects that care that we are about to use a much larger
	 * blocksize. this will flush all plugins too, so that they are ready
	 * to be used for this process.
	 */

	need_block_size_reset = true;
	track.set_block_size (bounce_chunk_size);
	_engine.main_thread()->get_buffers ();

	position = start;
	to_do = len;
	latency_skip = track.bounce_get_latency (endpoint, include_endpoint, for_export, for_freeze);

	/* create a set of reasonably-sized buffers */
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		buffers.ensure_buffers(*t, max_proc.get(*t), bounce_chunk_size);
	}
	buffers.set_count (max_proc);

	/* prepare MIDI files */

	for (vector<std::shared_ptr<Source> >::iterator src = srcs.begin(); src != srcs.end(); ++src) {

		std::shared_ptr<MidiSource> ms = std::dynamic_pointer_cast<MidiSource>(*src);

		if (ms) {
			midi_source_locks.push_back (new MidiSourceLockMap (ms));
			ms->mark_streaming_write_started (midi_source_locks.back()->lock);
		}
	}

	/* prepare audio files */

	for (vector<std::shared_ptr<Source> >::iterator src = srcs.begin(); src != srcs.end(); ++src) {
		std::shared_ptr<AudioFileSource> afs = std::dynamic_pointer_cast<AudioFileSource>(*src);
		if (afs) {
			afs->prepare_for_peakfile_writes ();
		}
	}

	/* process */
	out_pos = start;

	while (to_do && !itt.cancel) {

		this_chunk = min (to_do, bounce_chunk_size);

		if (track.export_stuff (buffers, start, this_chunk, endpoint, include_endpoint, for_export, for_freeze, tracker)) {
			goto out;
		}

		start += this_chunk;
		to_do -= this_chunk;
		itt.progress = (float) (1.0 - ((double) to_do / len));

		if (latency_skip >= bounce_chunk_size) {
			latency_skip -= bounce_chunk_size;
			continue;
		}

		const samplecnt_t current_chunk = this_chunk - latency_skip;

		uint32_t n = 0;
		for (vector<std::shared_ptr<Source> >::iterator src=srcs.begin(); src != srcs.end(); ++src, ++n) {
			std::shared_ptr<AudioFileSource> afs = std::dynamic_pointer_cast<AudioFileSource>(*src);
			std::shared_ptr<MidiSource> ms;

			if (afs) {
				if (afs->write (buffers.get_audio(n).data(latency_skip), current_chunk) != current_chunk) {
					goto out;
				}
			}
		}

		/* XXX NUTEMPO fix this to not use samples */

		for (vector<MidiSourceLockMap*>::iterator m = midi_source_locks.begin(); m != midi_source_locks.end(); ++m) {
				const MidiBuffer& buf = buffers.get_midi(0);
				for (MidiBuffer::const_iterator i = buf.begin(); i != buf.end(); ++i) {
					Evoral::Event<samplepos_t> ev = *i;
					if (!endpoint || for_export) {
						ev.set_time(ev.time() - position);
					} else {
						/* MidiTrack::export_stuff moves event to the current cycle */
						ev.set_time(ev.time() + out_pos - position);
					}
					(*m)->src->append_event_samples ((*m)->lock, ev, (*m)->src->natural_position().samples());
				}
		}
		out_pos += current_chunk;
		latency_skip = 0;
	}

	tracker.resolve_notes (resolved, end-1);

	if (!resolved.empty()) {

		for (vector<MidiSourceLockMap*>::iterator m = midi_source_locks.begin(); m != midi_source_locks.end(); ++m) {

			for (MidiBuffer::iterator i = resolved.begin(); i != resolved.end(); ++i) {
				Evoral::Event<samplepos_t> ev = *i;
				if (!endpoint || for_export) {
					ev.set_time(ev.time() - position);
				}
				(*m)->src->append_event_samples ((*m)->lock, ev, (*m)->src->natural_position().samples());
			}
		}
	}

	for (vector<MidiSourceLockMap*>::iterator m = midi_source_locks.begin(); m != midi_source_locks.end(); ++m) {
		delete *m;
	}

	midi_source_locks.clear ();

	/* post-roll, pick up delayed processor output */
	latency_skip = track.bounce_get_latency (endpoint, include_endpoint, for_export, for_freeze);

	while (latency_skip && !itt.cancel) {
		this_chunk = min (latency_skip, bounce_chunk_size);
		latency_skip -= this_chunk;

		buffers.silence (this_chunk, 0);
		track.bounce_process (buffers, start, this_chunk, endpoint, include_endpoint, for_export, for_freeze);

		start += this_chunk;

		uint32_t n = 0;
		for (vector<std::shared_ptr<Source> >::iterator src=srcs.begin(); src != srcs.end(); ++src, ++n) {
			std::shared_ptr<AudioFileSource> afs = std::dynamic_pointer_cast<AudioFileSource>(*src);

			if (afs) {
				if (afs->write (buffers.get_audio(n).data(), this_chunk) != this_chunk) {
					goto out;
				}
			}
		}

		/* XXX NUTEMPO fix this to not use samples */

		for (vector<MidiSourceLockMap*>::iterator m = midi_source_locks.begin(); m != midi_source_locks.end(); ++m) {
				const MidiBuffer& buf = buffers.get_midi(0);
				for (MidiBuffer::const_iterator i = buf.begin(); i != buf.end(); ++i) {
					Evoral::Event<samplepos_t> ev = *i;
					if (!endpoint || for_export) {
						ev.set_time(ev.time() - position);
					} else {
						ev.set_time(ev.time() + out_pos - position);
					}
					(*m)->src->append_event_samples ((*m)->lock, ev, (*m)->src->natural_position().samples());
				}
		}
		out_pos += this_chunk;
	}

	tracker.resolve_notes (resolved, end-1);

	if (!resolved.empty()) {

		for (vector<MidiSourceLockMap*>::iterator m = midi_source_locks.begin(); m != midi_source_locks.end(); ++m) {

			for (MidiBuffer::iterator i = resolved.begin(); i != resolved.end(); ++i) {
				Evoral::Event<samplepos_t> ev = *i;
				if (!endpoint || for_export) {
					ev.set_time(ev.time() - position);
				} else {
					ev.set_time(ev.time() + out_pos - position);
				}
				(*m)->src->append_event_samples ((*m)->lock, ev, (*m)->src->natural_position().samples());
			}
		}
	}

	for (vector<MidiSourceLockMap*>::iterator m = midi_source_locks.begin(); m != midi_source_locks.end(); ++m) {
		delete *m;
	}

	midi_source_locks.clear ();

	if (!itt.cancel) {

		PropertyList plist;

		time_t now;
		struct tm* xnow;
		time (&now);
		xnow = localtime (&now);

		/* XXX we may want to round this up to the next beat or bar */

		const timecnt_t duration (end - start);

		for (vector<std::shared_ptr<Source> >::iterator src=srcs.begin(); src != srcs.end(); ++src) {
			std::shared_ptr<AudioFileSource> afs = std::dynamic_pointer_cast<AudioFileSource>(*src);
			std::shared_ptr<MidiSource> ms;

			if (afs) {
				afs->update_header (position, *xnow, now);
				afs->flush_header ();
				afs->mark_immutable ();
				plist.add (Properties::start, timepos_t (0));
			} else if ((ms = std::dynamic_pointer_cast<MidiSource>(*src))) {
				Source::WriterLock lock (ms->mutex());
				ms->mark_streaming_write_completed (lock, duration);
				plist.add (Properties::start, timepos_t (Beats()));
			}
		}

		/* construct a whole-file region to represent the bounced material */

		plist.add (Properties::whole_file, true);
		plist.add (Properties::length, len); //ToDo: in nutempo, if the Range is snapped to bbt, this should be in bbt (?)
		plist.add (Properties::name, region_name_from_path (srcs.front()->name(), true)); // TODO: allow custom region-name when consolidating 
		plist.add (Properties::tags, "(bounce)");

		result = RegionFactory::create (srcs, plist, true);

		if (region_name.empty ()) {
			/* setting name in the properties didn't seem to work, but this does */
			result->set_name(legal_name);
		} else {
			result->set_name(region_name);
		}
	}

	out:
	if (!result) {
		for (vector<std::shared_ptr<Source> >::iterator src = srcs.begin(); src != srcs.end(); ++src) {
			(*src)->mark_for_remove ();
			(*src)->drop_references ();
		}

	} else {
		for (vector<std::shared_ptr<Source> >::iterator src = srcs.begin(); src != srcs.end(); ++src) {
			std::shared_ptr<AudioFileSource> afs = std::dynamic_pointer_cast<AudioFileSource>(*src);

			if (afs)
				afs->done_with_peakfile_writes ();
		}
	}

	_bounce_processing_active = false;

	if (need_block_size_reset) {
		_engine.main_thread()->drop_buffers ();
		track.set_block_size (get_block_size());
	}

	unblock_processing ();

	return result;
}

gain_t*
Session::gain_automation_buffer() const
{
	return ProcessThread::gain_automation_buffer ();
}

gain_t*
Session::trim_automation_buffer() const
{
	return ProcessThread::trim_automation_buffer ();
}

gain_t*
Session::send_gain_automation_buffer() const
{
	return ProcessThread::send_gain_automation_buffer ();
}

gain_t*
Session::scratch_automation_buffer() const
{
	return ProcessThread::scratch_automation_buffer ();
}

pan_t**
Session::pan_automation_buffer() const
{
	return ProcessThread::pan_automation_buffer ();
}

BufferSet&
Session::get_silent_buffers (ChanCount count)
{
	return ProcessThread::get_silent_buffers (count);
}

BufferSet&
Session::get_scratch_buffers (ChanCount count, bool silence)
{
	return ProcessThread::get_scratch_buffers (count, silence);
}

BufferSet&
Session::get_noinplace_buffers (ChanCount count)
{
	return ProcessThread::get_noinplace_buffers (count);
}

BufferSet&
Session::get_route_buffers (ChanCount count, bool silence)
{
	return ProcessThread::get_route_buffers (count, silence);
}


BufferSet&
Session::get_mix_buffers (ChanCount count)
{
	return ProcessThread::get_mix_buffers (count);
}

uint32_t
Session::ntracks () const
{
	/* XXX Could be optimized by caching */

	uint32_t n = 0;
	std::shared_ptr<RouteList const> r = routes.reader ();

	for (auto const& i : *r) {
		if (std::dynamic_pointer_cast<Track> (i)) {
			++n;
		}
	}

	return n;
}

uint32_t
Session::naudiotracks () const
{
	/* XXX Could be optimized by caching */

	uint32_t n = 0;
	std::shared_ptr<RouteList const> r = routes.reader ();

	for (auto const& i : *r) {
		if (std::dynamic_pointer_cast<AudioTrack> (i)) {
			++n;
		}
	}

	return n;
}

uint32_t
Session::nbusses () const
{
	uint32_t n = 0;
	std::shared_ptr<RouteList const> r = routes.reader ();

	for (auto const& i : *r) {
		if (std::dynamic_pointer_cast<Track>(i) == 0) {
			++n;
		}
	}

	return n;
}

uint32_t
Session::nstripables (bool with_monitor) const
{
	uint32_t rv = routes.reader()->size ();
	rv += _vca_manager->vcas ().size ();

	if (with_monitor) {
		return rv;
	}

	if (_monitor_out) {
		assert (rv > 0);
		--rv;
	}
	return rv;
}

bool
Session::plot_process_graph (std::string const& file_name) const {
	return _graph_chain ? _graph_chain->plot (file_name) : false;
}

void
Session::add_automation_list(AutomationList *al)
{
	automation_lists[al->id()] = al;
}

/** @return true if there is at least one record-enabled track, otherwise false */
bool
Session::have_rec_enabled_track () const
{
	return _have_rec_enabled_track.load () == 1;
}

bool
Session::have_rec_disabled_track () const
{
	return _have_rec_disabled_track.load () == 1;
}

/** Update the state of our rec-enabled tracks flag */
void
Session::update_route_record_state ()
{
	std::shared_ptr<RouteList const> rl = routes.reader ();
	RouteList::const_iterator i = rl->begin();
	while (i != rl->end ()) {

		std::shared_ptr<Track> tr = std::dynamic_pointer_cast<Track> (*i);
	                            if (tr && tr->rec_enable_control()->get_value()) {
			break;
		}

		++i;
	}

	int const old = _have_rec_enabled_track.load ();

	_have_rec_enabled_track.store (i != rl->end () ? 1 : 0);

	if (_have_rec_enabled_track.load () != old) {
		RecordStateChanged (); /* EMIT SIGNAL */
	}

	for (i = rl->begin(); i != rl->end (); ++i) {
		std::shared_ptr<Track> tr = std::dynamic_pointer_cast<Track> (*i);
		if (tr && !tr->rec_enable_control()->get_value()) {
			break;
		}
	}

	_have_rec_disabled_track.store (i != rl->end () ? 1 : 0);

	bool record_arm_state_changed = (old != _have_rec_enabled_track.load () );

	if (record_status() == Recording && record_arm_state_changed ) {
		RecordArmStateChanged ();
	}

	UpdateRouteRecordState ();
}

void
Session::listen_position_changed ()
{
	if (loading ()) {
		/* skip duing session restore (already taken care of) */
		return;
	}
	ProcessorChangeBlocker pcb (this);
	std::shared_ptr<RouteList const> r = routes.reader ();
	for (auto const& i : *r) {
		i->listen_position_changed ();
	}
}

void
Session::solo_control_mode_changed ()
{
	if (soloing() || listening()) {
		if (loading()) {
			/* We can't use ::clear_all_solo_state() here because during
			   session loading at program startup, that will queue a call
			   to rt_clear_all_solo_state() that will not execute until
			   AFTER solo states have been established (thus throwing away
			   the session's saved solo state). So just explicitly turn
			   them all off.
			*/
			set_controls (route_list_to_control_list (get_routes(), &Stripable::solo_control), 0.0, Controllable::NoGroup);
		} else {
			clear_all_solo_state (get_routes());
		}
	}
}

/** Called when a property of one of our route groups changes */
void
Session::route_group_property_changed (RouteGroup* rg)
{
	RouteGroupPropertyChanged (rg); /* EMIT SIGNAL */
}

/** Called when a route is added to one of our route groups */
void
Session::route_added_to_route_group (RouteGroup* rg, std::weak_ptr<Route> r)
{
	RouteAddedToRouteGroup (rg, r);
}

/** Called when a route is removed from one of our route groups */
void
Session::route_removed_from_route_group (RouteGroup* rg, std::weak_ptr<Route> r)
{
	update_route_record_state ();
	RouteRemovedFromRouteGroup (rg, r); /* EMIT SIGNAL */

	if (!rg->has_control_master () && !rg->has_subgroup () && rg->empty()) {
		remove_route_group (*rg);
	}
}

std::shared_ptr<RouteList>
Session::get_tracks () const
{
	std::shared_ptr<RouteList const> rl = routes.reader ();
	std::shared_ptr<RouteList> tl (new RouteList);

	for (auto const& r : *rl) {
		if (std::dynamic_pointer_cast<Track> (r)) {
			tl->push_back (r);
		}
	}
	return tl;
}

std::shared_ptr<RouteList>
Session::get_routes_with_regions_at (timepos_t const & p) const
{
	std::shared_ptr<RouteList const> r = routes.reader ();
	std::shared_ptr<RouteList> rl (new RouteList);

	for (auto const& i : *r) {
		std::shared_ptr<Track> tr = std::dynamic_pointer_cast<Track> (i);
		if (!tr) {
			continue;
		}

		std::shared_ptr<Playlist> pl = tr->playlist ();
		if (!pl) {
			continue;
		}

		if (pl->has_region_at (p)) {
			rl->push_back (i);
		}
	}

	return rl;
}

void
Session::goto_end ()
{
	if (_session_range_location) {
		request_locate (_session_range_location->end().samples(), false, MustStop);
	} else {
		request_locate (0, MustStop);
	}
}

void
Session::goto_start (bool and_roll)
{
	if (_session_range_location) {
		request_locate (_session_range_location->start().samples(), false, and_roll ? MustRoll : RollIfAppropriate);
	} else {
		request_locate (0, false, and_roll ? MustRoll : RollIfAppropriate);
	}
}

samplepos_t
Session::current_start_sample () const
{
	return _session_range_location ? _session_range_location->start().samples() : 0;
}

samplepos_t
Session::current_end_sample () const
{
	return _session_range_location ? _session_range_location->end().samples() : 0;
}

timepos_t
Session::current_start () const
{
	return _session_range_location ? _session_range_location->start() : timepos_t::max (Temporal::AudioTime);
}

timepos_t
Session::current_end () const
{
	return _session_range_location ? _session_range_location->end() : timepos_t::max (Temporal::AudioTime);
}

void
Session::step_edit_status_change (bool yn)
{
	bool send = false;

	bool val = false;
	if (yn) {
		send = (_step_editors == 0);
		val = true;

		_step_editors++;
	} else {
		send = (_step_editors == 1);
		val = false;

		if (_step_editors > 0) {
			_step_editors--;
		}
	}

	if (send) {
		StepEditStatusChange (val);
	}
}


void
Session::start_time_changed (samplepos_t old)
{
	/* Update the auto loop range to match the session range
	   (unless the auto loop range has been changed by the user)
	*/

	Location* s = _locations->session_range_location ();
	if (s == 0) {
		return;
	}

	Location* l = _locations->auto_loop_location ();

	if (l && l->start() == old && l->end() > s->start()) {
		l->set_start (s->start(), true);
	}
	set_dirty ();
}

void
Session::end_time_changed (samplepos_t old)
{
	/* Update the auto loop range to match the session range
	   (unless the auto loop range has been changed by the user)
	*/

	Location* s = _locations->session_range_location ();
	if (s == 0) {
		return;
	}

	Location* l = _locations->auto_loop_location ();

	if (l && l->end() == old && l->start () < s->end()) {
		l->set_end (s->end(), true);
	}
	set_dirty ();
}

std::vector<std::string>
Session::source_search_path (DataType type) const
{
	Searchpath sp;

	if (session_dirs.size() == 1) {
		switch (type) {
		case DataType::AUDIO:
			sp.push_back (_session_dir->sound_path());
			break;
		case DataType::MIDI:
			sp.push_back (_session_dir->midi_path());
			break;
		}
	} else {
		for (vector<space_and_path>::const_iterator i = session_dirs.begin(); i != session_dirs.end(); ++i) {
			SessionDirectory sdir (i->path);
			switch (type) {
			case DataType::AUDIO:
				sp.push_back (sdir.sound_path());
				break;
			case DataType::MIDI:
				sp.push_back (sdir.midi_path());
				break;
			}
		}
	}

	if (type == DataType::AUDIO) {
		const string sound_path_2X = _session_dir->sound_path_2X();
		if (Glib::file_test (sound_path_2X, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_DIR)) {
			if (find (sp.begin(), sp.end(), sound_path_2X) == sp.end()) {
				sp.push_back (sound_path_2X);
			}
		}
	}

	// now check the explicit (possibly user-specified) search path

	switch (type) {
	case DataType::AUDIO:
		sp += Searchpath(config.get_audio_search_path ());
		break;
	case DataType::MIDI:
		sp += Searchpath(config.get_midi_search_path ());
		break;
	}

	return sp;
}

void
Session::ensure_search_path_includes (const string& path, DataType type)
{
	Searchpath sp;

	if (path == ".") {
		return;
	}

	switch (type) {
	case DataType::AUDIO:
		sp += Searchpath(config.get_audio_search_path ());
		break;
	case DataType::MIDI:
		sp += Searchpath (config.get_midi_search_path ());
		break;
	}

	for (vector<std::string>::iterator i = sp.begin(); i != sp.end(); ++i) {
		/* No need to add this new directory if it has the same inode as
		   an existing one; checking inode rather than name prevents duplicated
		   directories when we are using symlinks.

		   On Windows, I think we could just do if (*i == path) here.
		*/
		if (PBD::equivalent_paths (*i, path)) {
			return;
		}
	}

	sp += path;

	switch (type) {
	case DataType::AUDIO:
		config.set_audio_search_path (sp.to_string());
		break;
	case DataType::MIDI:
		config.set_midi_search_path (sp.to_string());
		break;
	}
}

void
Session::remove_dir_from_search_path (const string& dir, DataType type)
{
	Searchpath sp;

	switch (type) {
	case DataType::AUDIO:
		sp = Searchpath(config.get_audio_search_path ());
		break;
	case DataType::MIDI:
		sp = Searchpath (config.get_midi_search_path ());
		break;
	}

	sp -= dir;

	switch (type) {
	case DataType::AUDIO:
		config.set_audio_search_path (sp.to_string());
		break;
	case DataType::MIDI:
		config.set_midi_search_path (sp.to_string());
		break;
	}

}

std::shared_ptr<Speakers>
Session::get_speakers()
{
	return _speakers;
}

list<string>
Session::unknown_processors () const
{
	list<string> p;

	std::shared_ptr<RouteList const> r = routes.reader ();
	for (auto const& i : *r) {
		list<string> t = i->unknown_processors ();
		copy (t.begin(), t.end(), back_inserter (p));
	}

	p.sort ();
	p.unique ();

	return p;
}

list<string>
Session::missing_filesources (DataType dt) const
{
	list<string> p;
	for (SourceMap::const_iterator i = sources.begin(); i != sources.end(); ++i) {
		if (dt == DataType::AUDIO && 0 != std::dynamic_pointer_cast<SilentFileSource> (i->second)) {
			p.push_back (i->second->name());
		}
		else if (dt == DataType::MIDI && 0 != std::dynamic_pointer_cast<SMFSource> (i->second) && (i->second->flags() & Source::Missing) != 0) {
			p.push_back (i->second->name());
		}
	}
	p.sort ();
	return p;
}

void
Session::setup_engine_resampling ()
{
	if (_base_sample_rate != AudioEngine::instance()->sample_rate ()) {
		Port::setup_resampler (std::max<uint32_t>(65, Config->get_port_resampler_quality ()));
	} else {
		Port::setup_resampler (Config->get_port_resampler_quality ());
	}
	Port::set_engine_ratio (_base_sample_rate,  AudioEngine::instance()->sample_rate ());
}

void
Session::initialize_latencies ()
{
	block_processing ();
	setup_engine_resampling ();
	update_latency (false);
	update_latency (true);
	unblock_processing ();
}

void
Session::send_latency_compensation_change ()
{
	/* As a result of Send::set_output_latency()
	 * or InternalReturn::set_playback_offset ()
	 * the send's own latency can change (source track
	 * is aligned with target bus).
	 *
	 * This can only happen be triggered by
	 * Route::update_signal_latency ()
	 * when updating the processor latency.
	 *
	 * We need to walk the graph again to take those changes into account
	 * (we should probably recurse or process the graph in a 2 step process).
	 */
	++_send_latency_changes;
}

void
Session::update_send_delaylines ()
{
	/* called in rt-thread, if send latency changed */
	_update_send_delaylines = true;
}

bool
Session::update_route_latency (bool playback, bool apply_to_delayline, bool* delayline_update_needed)
{
	/* apply_to_delayline can no be called concurrently with processing
	 * caller must hold process lock when apply_to_delayline == true */
	assert (!apply_to_delayline || !AudioEngine::instance()->process_lock().trylock());

	DEBUG_TRACE (DEBUG::LatencyCompensation , string_compose ("update_route_latency: %1 apply_to_delayline? %2)\n", (playback ? "PLAYBACK" : "CAPTURE"), (apply_to_delayline ? "yes" : "no")));

	/* Note: RouteList is process-graph sorted */
	RouteList r = *routes.reader ();

	if (playback) {
		/* reverse the list so that we work backwards from the last route to run to the first,
		 * this is not needed, but can help to reduce the iterations for aux-sends.
		 */
		reverse (r.begin(), r.end());
	}

	bool changed = false;
	int bailout = 0;
restart:
	_send_latency_changes = 0;
	_worst_route_latency = 0;

	for (auto const& i : r) {
		// if (!(*i)->active()) { continue ; } // TODO
		samplecnt_t l;
		if (i->signal_latency () != (l = i->update_signal_latency (apply_to_delayline, delayline_update_needed))) {
			changed = true;
		}
		_worst_route_latency = std::max (l, _worst_route_latency);
	}

	if (_send_latency_changes > 0) {
		/* One extra iteration might be needed since we allow u level of aux-sends.
		 * Except mixbus that allows up to 3 (aux-sends, sends to mixbusses 1-8, sends to mixbusses 9-12,
		 * and then there's JACK */
		if (++bailout < 5) {
			DEBUG_TRACE (DEBUG::LatencyCompensation, string_compose ("restarting update. send changes: %1, iteration: %2\n", _send_latency_changes, bailout));
			goto restart;
		}
	}

	DEBUG_TRACE (DEBUG::LatencyCompensation , string_compose ("update_route_latency: worst proc latency: %1 (changed? %2) recursions: %3\n", _worst_route_latency, (changed ? "yes" : "no"), bailout));

	return changed;
}

void
Session::set_owned_port_public_latency (bool playback)
{
	/* special routes or IO or ports owned by the session */
	if (auditioner) {
		samplecnt_t latency = auditioner->set_private_port_latencies (playback);
		auditioner->set_public_port_latencies (latency, playback, true);
	}
	if (_click_io) {
		_click_io->set_public_port_latencies (_click_io->connected_latency (playback), playback);
	}

	std::shared_ptr<IOPlugList const> iop (_io_plugins.reader ());
	for (auto const& i : *iop) {
		i->set_public_latency (playback);
	}

	if (_midi_ports) {
		_midi_ports->set_public_latency (playback);
	}
}

void
Session::update_latency (bool playback)
{
	/* called only from AudioEngine::latency_callback.
	 * but may indirectly be triggered from
	 * Session::update_latency_compensation -> _engine.update_latencies
	 */
	DEBUG_TRACE (DEBUG::LatencyCompensation, string_compose ("Engine latency callback: %1 (initial/deletion: %2 adding: %3 deletion: %4)\n",
				(playback ? "PLAYBACK" : "CAPTURE"),
				inital_connect_or_deletion_in_progress(),
				_adding_routes_in_progress,
				_route_deletion_in_progress
				));

	if (inital_connect_or_deletion_in_progress () || _adding_routes_in_progress || _route_deletion_in_progress) {
		_engine.queue_latency_update (playback);
		return;
	}
	if (!_engine.running() || _exporting) {
		return;
	}

#ifndef NDEBUG
	Timing t;
#endif

	/* Session::new_midi_track -> Route::add_processors -> Delivery::configure_io
	 * -> IO::ensure_ports -> PortManager::register_output_port
	 *  may run currently (adding many ports) while the backend
	 *  already emits AudioEngine::latency_callback() for previously
	 *  added ports.
	 *
	 *  Route::set_public_port_latencies() -> IO::latency may try
	 *  to lookup ports that don't yet exist.
	 *  IO::* uses  BLOCK_PROCESS_CALLBACK to prevent concurrency,
	 *  so the same has to be done here to prevent a race.
	 */
	Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock (), Glib::Threads::TRY_LOCK);
	if (!lm.locked()) {
		/* IO::ensure_ports() calls jack_port_register() while holding the process-lock,
		 * JACK2 may block and call JACKAudioBackend::_latency_callback() which
		 * ends up here. https://pastebin.com/mitGBwpq
		 *
		 * This is a stopgap to be able to use 6.0 with JACK2's insane threading.
		 * Yes JACK can also concurrently process (using the old graph) yet emit
		 * a latency-callback (for which we do need the lock).
		 *
		 * One alternative is to use _adding_routes_in_progress and
		 * call graph_reordered (false); however various entry-points
		 * to ensure_io don't originate from Session.
		 *
		 * Eventually Ardour will probably need to be changed to
		 * register ports lock-free, and mark those ports as "pending",
		 * and skip them during process and all other callbacks.
		 *
		 * Then clear the pending flags in the rt-process context after
		 * a port-registraion callback.
		 */
		DEBUG_TRACE (DEBUG::LatencyCompensation, "Engine latency callback: called with process-lock held. queue for later.\n");
		queue_latency_recompute ();
		return;
	}

	/* Note; RouteList is sorted as process-graph */
	RouteList r = *routes.reader ();

	if (playback) {
		/* reverse the list so that we work backwards from the last route to run to the first */
		reverse (r.begin(), r.end());
	}
	for (auto const& i : r) {
		/* private port latency includes plugin and I/O delay,
		 * but no latency compensation delaylines.
		 */
		samplecnt_t latency = i->set_private_port_latencies (playback);
		/* However we also need to reset the latency of connected external
		 * ports, since those includes latency compensation delaylines.
		 */
		i->set_public_port_latencies (latency, playback, false);
	}

	set_owned_port_public_latency (playback);

	if (playback) {
		/* Processing needs to be blocked while re-configuring delaylines.
		 *
		 * With internal backends, AudioEngine::latency_callback () -> this method
		 * is called from the main_process_thread (so the lock is not contended).
		 * However jack2 can concurrently process and reconfigure port latencies.
		 * -> keep the process-lock.
		 */

		/* prevent any concurrent latency updates */
		Glib::Threads::Mutex::Lock lx (_update_latency_lock);
		update_route_latency (true, /*apply_to_delayline*/ true, NULL);

		/* release before emitting signals */
		lm.release ();

	} else {
		/* process lock is not needed to update worst-case latency */
		lm.release ();
		Glib::Threads::Mutex::Lock lx (_update_latency_lock);
		update_route_latency (false, false, NULL);
	}

	for (auto const& i : r) {
		/* Publish port latency. This includes latency-compensation
		 * delaylines in the direction of signal flow.
		 */
		samplecnt_t latency = i->set_private_port_latencies (playback);
		i->set_public_port_latencies (latency, playback, true);
	}

	/* now handle non-route ports that we are responsible for */
	set_owned_port_public_latency (playback);

	if (playback) {
		Glib::Threads::Mutex::Lock lx (_update_latency_lock);
		set_worst_output_latency ();
	} else {
		Glib::Threads::Mutex::Lock lx (_update_latency_lock);
		set_worst_input_latency ();
	}


	DEBUG_TRACE (DEBUG::LatencyCompensation, "Engine latency callback: DONE\n");
	LatencyUpdated (playback); /* EMIT SIGNAL */

#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::TopologyTiming)) {
		t.update ();
		std::cerr << string_compose ("Session::update_latency for %1 took %2ms ; DSP %3 %%\n",
				playback ? "playback" : "capture", t.elapsed () / 1000.,
				100.0 * t.elapsed () / _engine.usecs_per_cycle ());
	}
#endif
}

void
Session::set_worst_output_latency ()
{
	if (inital_connect_or_deletion_in_progress ()) {
		return;
	}

	_worst_output_latency = 0;
	_io_latency = 0;

	if (!_engine.running()) {
		return;
	}

	std::shared_ptr<RouteList const> r = routes.reader ();

	for (auto const& i : *r) {
		_worst_output_latency = max (_worst_output_latency, i->output()->latency());
		_io_latency = max (_io_latency, i->output()->latency() + i->input()->latency());
	}

	if (_click_io) {
		_worst_output_latency = max (_worst_output_latency, _click_io->latency());
	}

	DEBUG_TRACE (DEBUG::LatencyCompensation, string_compose ("Worst output latency: %1\n", _worst_output_latency));
}

void
Session::set_worst_input_latency ()
{
	if (inital_connect_or_deletion_in_progress ()) {
		return;
	}

	_worst_input_latency = 0;

	if (!_engine.running()) {
		return;
	}

	std::shared_ptr<RouteList const> r = routes.reader ();

	for (auto const& i : *r) {
		_worst_input_latency = max (_worst_input_latency, i->input()->latency());
	}

	DEBUG_TRACE (DEBUG::LatencyCompensation, string_compose ("Worst input latency: %1\n", _worst_input_latency));
}

void
Session::update_latency_compensation (bool force_whole_graph, bool called_from_backend)
{
	/* Called to update Ardour's internal latency values and compensation
	 * planning. Typically case is from within ::graph_reordered()
	 */

	if (inital_connect_or_deletion_in_progress ()) {
		return;
	}

	/* this lock is not usually contended, but under certain conditions,
	 * update_latency_compensation may be called concurrently.
	 * e.g. drag/drop copy a latent plugin while rolling.
	 * GUI thread (via route_processors_changed) and
	 * auto_connect_thread_run may race.
	 */
	Glib::Threads::Mutex::Lock lx (_update_latency_lock, Glib::Threads::TRY_LOCK);
	if (!lx.locked()) {
		/* no need to do this twice */
		return;
	}

	DEBUG_TRACE (DEBUG::LatencyCompensation, string_compose ("update_latency_compensation%1.\n", (force_whole_graph ? " of whole graph" : "")));

	bool delayline_update_needed = false;
	bool some_track_latency_changed = update_route_latency (false, false, &delayline_update_needed);

	if (some_track_latency_changed || force_whole_graph)  {

		/* cannot hold lock while engine initiates a full latency callback */

		lx.release ();

		/* next call will ask the backend up update its latencies.
		 *
		 * The semantics of how the backend does this are not well
		 * defined (Oct 2019).
		 *
		 * In all cases, eventually AudioEngine::latency_callback() is
		 * invoked, which will call Session::update_latency().
		 *
		 * Some backends will do that asynchronously with respect to
		 * this call. Others (JACK1) will do so synchronously, and in
		 * those cases this call will return until the backend latency
		 * callback is complete.
		 *
		 * Further, if this is called as part of a backend callback,
		 * then we have to follow the JACK1 rule that we cannot call
		 * back into the backend during such a callback (otherwise
		 * deadlock ensues).
		 */

		if (!called_from_backend) {
			DEBUG_TRACE (DEBUG::LatencyCompensation, "update_latency_compensation: delegate to engine\n");
			_engine.update_latencies ();
		} else {
			DEBUG_TRACE (DEBUG::LatencyCompensation, "update_latency_compensation called from engine, don't call back into engine\n");
		}
	} else if (delayline_update_needed) {
		DEBUG_TRACE (DEBUG::LatencyCompensation, "update_latency_compensation: directly apply to routes\n");
		lx.release (); // XXX cannot hold this lock when acquiring process_lock ?!
#ifndef MIXBUS
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock (), Glib::Threads::NOT_LOCK);
#endif
		lm.acquire ();

		std::shared_ptr<RouteList const> r = routes.reader ();
		for (auto const& i : *r) {
			i->apply_latency_compensation ();
		}
	}
	DEBUG_TRACE (DEBUG::LatencyCompensation, "update_latency_compensation: complete\n");
}

const std::string
Session::session_name_is_legal (const string& path)
{
	static const char illegal_chars[] = { '/', '\\', ':', ';' };

	for (size_t i = 0; i < sizeof (illegal_chars); ++i) {
		if (path.find (illegal_chars[i]) != string::npos) {
			return std::string (1, illegal_chars[i]);
		}
	}

	for (size_t i = 0; i < path.length(); ++i)  {
		if (iscntrl (path[i])) {
			return _("Control Char");
		}
	}
	return std::string ();
}

void
Session::notify_presentation_info_change (PropertyChange const& what_changed)
{
	if (deletion_in_progress() || _route_reorder_in_progress) {
		return;
	}

	if (what_changed.contains (Properties::order)) {
		PBD::Unwinder<bool> uw (_route_reorder_in_progress, true);
		ensure_stripable_sort_order ();
		reassign_track_numbers ();
		set_dirty ();
	}
}

void
Session::controllable_touched (std::weak_ptr<PBD::Controllable> c)
{
	_recently_touched_controllable = c;
}

std::shared_ptr<PBD::Controllable>
Session::recently_touched_controllable () const
{
	return _recently_touched_controllable.lock ();
}

void
Session::reconnect_ltc_output ()
{
	if (_ltc_output_port) {
		string src = Config->get_ltc_output_port();

		_ltc_output_port->disconnect_all ();

		if (src != _("None") && !src.empty())  {
			_ltc_output_port->connect (src);
		}
	}
}

void
Session::set_range_selection (timepos_t const & start, timepos_t const & end)
{
	_range_selection = Temporal::Range (start, end);
}

void
Session::set_object_selection (timepos_t const & start, timepos_t const & end)
{
	_object_selection = Temporal::Range (start, end);
}

void
Session::clear_range_selection ()
{
	_range_selection = Temporal::Range (timepos_t::max (Temporal::AudioTime), timepos_t::max (Temporal::AudioTime));
}

void
Session::clear_object_selection ()
{
	_object_selection = Temporal::Range (timepos_t::max (Temporal::AudioTime), timepos_t::max (Temporal::AudioTime));
}

void
Session::cut_copy_section (timepos_t const& start_, timepos_t const& end_, timepos_t const& to_, SectionOperation const op)
{
	timepos_t start = timepos_t::from_superclock (start_.superclocks());
	timepos_t end   = timepos_t::from_superclock (end_.superclocks());
	timepos_t to    = timepos_t::from_superclock (to_.superclocks());

#ifndef NDEBUG
	cout << "Session::cut_copy_section " << start << " - " << end << " << to " << to << " op = " << op << "\n";
#endif

	std::list<TimelineRange> ltr;
	TimelineRange tlr (start, end, 0);
	ltr.push_back (tlr);

	switch (op) {
		case CopyPasteSection:
			begin_reversible_command (_("Copy Section"));
			break;
		case CutPasteSection:
			begin_reversible_command (_("Move Section"));
			break;
		case InsertSection:
			begin_reversible_command (_("Insert Section"));
			break;
		case DeleteSection:
			begin_reversible_command (_("Delete Section"));
			break;
	}

	{
		/* disable DiskReader::playlist_ranges_moved moving automation */
		bool automation_follows = Config->get_automation_follows_regions ();
		Config->set_automation_follows_regions (false);

		vector<std::shared_ptr<Playlist>> playlists;
		_playlists->get (playlists);

		for (auto& pl : playlists) {
			pl->freeze ();
			pl->clear_changes ();
			pl->clear_owned_changes ();

			std::shared_ptr<Playlist> p;
			if (op == CopyPasteSection) {
				p = pl->copy (ltr);
			} else if (op == CutPasteSection || op == DeleteSection) {
				p = pl->cut (ltr);
			}

			if (op == CutPasteSection || op == DeleteSection) {
				pl->ripple (start, end.distance(start), NULL);
			}

			if (op != DeleteSection) {
				/* Commit changes so far, to retain undo sequence.
				 * split() may create a new region replacing the already
				 * rippled of regions, the length/position of which
				 * would not be saved/restored.
				 */
				pl->rdiff_and_add_command (this);
				pl->clear_changes ();
				pl->clear_owned_changes ();

				/* now make space at the insertion-point */
				pl->split (to);
				pl->ripple (to, start.distance(end), NULL);
			}

			if (op == CopyPasteSection || op == CutPasteSection) {
				pl->paste (p, to, 1);
			}

			pl->rdiff_and_add_command (this);
		}

		for (auto& pl : playlists) {
			pl->thaw ();
		}

		Config->set_automation_follows_regions (automation_follows);
	}

	/* automation */
	for (auto& r : *(routes.reader())) {
		r->cut_copy_section (start, end, to, op);
	}

	{
		XMLNode &before = _locations->get_state();
		_locations->cut_copy_section (start, end, to, op);
		XMLNode &after = _locations->get_state();
		add_command (new MementoCommand<Locations> (*_locations, &before, &after));
	}

	TempoMap::WritableSharedPtr wmap = TempoMap::write_copy ();
	TempoMapCutBuffer* tmcb;
	XMLNode& tm_before (wmap->get_state());

	switch (op) {
	case CopyPasteSection:
		if ((tmcb = wmap->copy (start, end))) {
			tmcb->dump (std::cerr);
			wmap->paste (*tmcb, to, true);
		}
		break;
	case CutPasteSection:
		if ((tmcb = wmap->cut (start, end, true))) {
			tmcb->dump (std::cerr);
			wmap->paste (*tmcb, to, true);
		}
		break;
	default:
		tmcb = nullptr;
		break;
	}

	if (tmcb && !tmcb->empty()) {
		TempoMap::update (wmap);
		delete tmcb;
		XMLNode& tm_after (wmap->get_state());
		add_command (new TempoCommand (_("cut tempo map"), &tm_before, &tm_after));
	} else {
		delete &tm_before;
		TempoMap::abort_update ();
		TempoMap::SharedPtr tmap (TempoMap::fetch());
	}

	if (abort_empty_reversible_command ()) {
		return;
	}

	commit_reversible_command ();
}

void
Session::auto_connect_route (std::shared_ptr<Route> route,
		bool connect_inputs,
		bool connect_outputs,
		const ChanCount& input_start,
		const ChanCount& output_start,
		const ChanCount& input_offset,
		const ChanCount& output_offset)
{
	Glib::Threads::Mutex::Lock lx (_auto_connect_queue_lock);

	DEBUG_TRACE (DEBUG::PortConnectAuto,
	             string_compose ("Session::auto_connect_route '%1' ci: %2 co: %3 is=(%4) os=(%5) io=(%6) oo=(%7)\n",
	             route->name(), connect_inputs, connect_outputs,
	             input_start, output_start, input_offset, output_offset));

	_auto_connect_queue.push (AutoConnectRequest (route,
				connect_inputs, connect_outputs,
				input_start, output_start,
				input_offset, output_offset));

	lx.release (); // XXX check try-lock + pthread_cond_wait
	auto_connect_thread_wakeup ();
}

void
Session::auto_connect_thread_wakeup ()
{
	if (pthread_mutex_trylock (&_auto_connect_mutex) == 0) {
		pthread_cond_signal (&_auto_connect_cond);
		pthread_mutex_unlock (&_auto_connect_mutex);
	}
}

void
Session::queue_latency_recompute ()
{
	_latency_recompute_pending.fetch_add (1);
	auto_connect_thread_wakeup ();
}

void
Session::auto_connect (const AutoConnectRequest& ar)
{
	std::shared_ptr<Route> route = ar.route.lock();

	if (!route) { return; }

	if (loading()) {
		return;
	}

	/* If both inputs and outputs are auto-connected to physical ports,
	 * use the max of input and output offsets to ensure auto-connected
	 * port numbers always match up (e.g. the first audio input and the
	 * first audio output of the route will have the same physical
	 * port number).  Otherwise just use the lowest input or output
	 * offset possible.
	 */

	const bool in_out_physical =
		(Config->get_input_auto_connect() & AutoConnectPhysical)
		&& (Config->get_output_auto_connect() & AutoConnectPhysical)
		&& ar.connect_inputs;

	const ChanCount in_offset = in_out_physical
		? ChanCount::max(ar.input_offset, ar.output_offset)
		: ar.input_offset;

	const ChanCount out_offset = in_out_physical
		? ChanCount::max(ar.input_offset, ar.output_offset)
		: ar.output_offset;

	DEBUG_TRACE (DEBUG::PortConnectAuto,
	             string_compose ("Session::auto_connect '%1' iop: %2 is=(%3) os=(%4) Eio=(%5) Eoo=(%6)\n",
	             route->name(), in_out_physical, ar.input_start, ar.output_start, in_offset, out_offset));

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		vector<string> physinputs;
		vector<string> physoutputs;


		/* for connecting track inputs we only want MIDI ports marked
		 * for "music".
		 */

		get_physical_ports (physinputs, physoutputs, *t, MidiPortMusic);

		DEBUG_TRACE (DEBUG::PortConnectAuto,
		             string_compose ("Physical MidiPortMusic %1 Ports count in: %2 out %3\n",
		             (*t).to_string(), physinputs.size(), physoutputs.size()));

		if (!physinputs.empty() && ar.connect_inputs) {
			uint32_t nphysical_in = physinputs.size();

			for (uint32_t i = ar.input_start.get(*t); i < route->n_inputs().get(*t) && i < nphysical_in; ++i) {
				string port;

				if (Config->get_input_auto_connect() & AutoConnectPhysical) {
					port = physinputs[(in_offset.get(*t) + i) % nphysical_in];
				}

				if (!port.empty() && route->input()->connect (route->input()->ports()->port(*t, i), port, this)) {
					DEBUG_TRACE (DEBUG::PortConnectAuto, "Failed to auto-connect input.");
					break;
				}
			}
		}

		if (!physoutputs.empty() && ar.connect_outputs) {
			DEBUG_TRACE (DEBUG::PortConnectAuto,
			             string_compose ("Connect %1 outputs # %2 .. %3\n",
			             (*t).to_string(), ar.output_start.get(*t), route->n_outputs().get(*t)));

			uint32_t nphysical_out = physoutputs.size();
			for (uint32_t i = ar.output_start.get(*t); i < route->n_outputs().get(*t); ++i) {
				string port;

				if ((*t) == DataType::MIDI && (Config->get_output_auto_connect() & AutoConnectPhysical)) {
					port = physoutputs[(out_offset.get(*t) + i) % nphysical_out];
				} else if ((*t) == DataType::AUDIO && (Config->get_output_auto_connect() & AutoConnectMaster)) {
					/* master bus is audio only */
					if (_master_out && _master_out->n_inputs().get(*t) > 0) {
						port = _master_out->input()->ports()->port(*t,
								i % _master_out->input()->n_ports().get(*t))->name();
					}
				}

				if (!port.empty() && route->output()->connect (route->output()->ports()->port(*t, i), port, this)) {
					DEBUG_TRACE (DEBUG::PortConnectAuto, "Failed to auto-connect output.");
					break;
				}
			}
		}
	}
}

void
Session::auto_connect_thread_start ()
{
	if (_ac_thread_active.load ()) {
		return;
	}

	Glib::Threads::Mutex::Lock lx (_auto_connect_queue_lock);
	while (!_auto_connect_queue.empty ()) {
		_auto_connect_queue.pop ();
	}
	lx.release ();

	_ac_thread_active.store (1);
	if (pthread_create_and_store ("AutoConnect", &_auto_connect_thread, auto_connect_thread, this, 0)) {
		_ac_thread_active.store (0);
		fatal << "Cannot create 'session auto connect' thread" << endmsg;
		abort(); /* NOTREACHED*/
	}
}

void
Session::auto_connect_thread_terminate ()
{
	if (!_ac_thread_active.load ()) {
		return;
	}

	{
		Glib::Threads::Mutex::Lock lx (_auto_connect_queue_lock);
		while (!_auto_connect_queue.empty ()) {
			_auto_connect_queue.pop ();
		}
	}

	/* cannot use auto_connect_thread_wakeup() because that is allowed to
	 * fail to wakeup the thread.
	 */

	pthread_mutex_lock (&_auto_connect_mutex);
	_ac_thread_active.store (0);
	pthread_cond_signal (&_auto_connect_cond);
	pthread_mutex_unlock (&_auto_connect_mutex);

	void *status;
	pthread_join (_auto_connect_thread, &status);
}

void *
Session::auto_connect_thread (void *arg)
{
	Session *s = static_cast<Session *>(arg);
	s->auto_connect_thread_run ();
	return 0;
}

void
Session::auto_connect_thread_run ()
{
	SessionEvent::create_per_thread_pool (X_("autoconnect"), 1024);
	PBD::notify_event_loops_about_thread_creation (pthread_self(), X_("autoconnect"), 1024);
	pthread_mutex_lock (&_auto_connect_mutex);

	Glib::Threads::Mutex::Lock lx (_auto_connect_queue_lock);
	while (_ac_thread_active.load ()) {

		if (!_auto_connect_queue.empty ()) {
			/* Why would we need the process lock?
			 *
			 * A: if ports are added while connections change,
			 * the backend's iterator may be invalidated:
			 *   graph_order_callback() -> resort_routes() -> direct_feeds_according_to_reality () -> backend::connected_to()
			 * Ardour::IO uses the process-lock to avoid concurrency, too
			 */
			Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

			while (!_auto_connect_queue.empty ()) {
				const AutoConnectRequest ar (_auto_connect_queue.front());
				_auto_connect_queue.pop ();
				lx.release ();
				auto_connect (ar);
				lx.acquire ();
			}
		}
		lx.release ();

		if (!actively_recording ()) { // might not be needed,
			/* this is only used for updating plugin latencies, the
			 * graph does not change. so it's safe in general.
			 * BUT..
			 * update_latency_compensation ()
			 * calls DiskWriter::set_capture_offset () which
			 * modifies the capture-offset, which can be a problem.
			 */
			while (_latency_recompute_pending.fetch_and (0)) {
				update_latency_compensation (false, false);
				if (_latency_recompute_pending.load ()) {
					Glib::usleep (1000);
				}
			}
		}

		if (_midi_ports && _update_pretty_names.load ()) {
			std::shared_ptr<Port> ap = std::dynamic_pointer_cast<Port> (vkbd_output_port ());
			if (ap->pretty_name () != _("Virtual Keyboard")) {
				ap->set_pretty_name (_("Virtual Keyboard"));
			}
			_update_pretty_names.store (0);
		}

		if (_engine.port_deletions_pending ().read_space () > 0) {
			// this may call ARDOUR::Port::drop ... jack_port_unregister ()
			// jack1 cannot cope with removing ports while processing
			Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
			_engine.clear_pending_port_deletions ();
		}

		lx.acquire ();
		if (_auto_connect_queue.empty ()) {
			lx.release ();
			pthread_cond_wait (&_auto_connect_cond, &_auto_connect_mutex);
			lx.acquire ();
		}
	}
	lx.release ();
	pthread_mutex_unlock (&_auto_connect_mutex);
}

void
Session::cancel_all_solo ()
{
	StripableList sl;

	get_stripables (sl);

	set_controls (stripable_list_to_control_list (sl, &Stripable::solo_control), 0.0, Controllable::NoGroup);
	clear_all_solo_state (routes.reader());

	_engine.monitor_port().clear_ports (false);
}

bool
Session::listening () const
{
	if (_listen_cnt > 0) {
		return true;
	}

	if (_monitor_out && _engine.monitor_port().monitoring ()) {
		return true;
	}

	return false;
}

void
Session::maybe_update_tempo_from_midiclock_tempo (float bpm)
{
	TempoMap::WritableSharedPtr tmap (TempoMap::write_copy());

	if (tmap->n_tempos() == 1) {
		Temporal::TempoMetric const & metric (tmap->metric_at (timepos_t (0)));
		if (fabs (metric.tempo().note_types_per_minute() - bpm) >= Config->get_midi_clock_resolution()) {
			/* fix note type as quarters, because that's how MIDI clock works */
			tmap->change_tempo (metric.get_editable_tempo(), Tempo (bpm, bpm, 4.0));
			TempoMap::update (tmap);
			return;
		}
	}

	TempoMap::abort_update ();
}

void
Session::send_mclk_for_cycle (samplepos_t start_sample, samplepos_t end_sample, pframes_t n_samples, samplecnt_t pre_roll)
{
	midi_clock->tick (start_sample, end_sample, n_samples, pre_roll);
}

void
Session::set_had_destructive_tracks (bool yn)
{
	_had_destructive_tracks = yn;
}

bool
Session::had_destructive_tracks() const
{
	return _had_destructive_tracks;
}

bool
Session::nth_mixer_scene_valid (size_t nth) const
{
	Glib::Threads::RWLock::ReaderLock lm (_mixer_scenes_lock);
	if (_mixer_scenes.size () <= nth) {
		return false;
	}
	if (!_mixer_scenes[nth]) {
		return false;
	}
	return !_mixer_scenes[nth]->empty ();
}

bool
Session::apply_nth_mixer_scene (size_t nth)
{
	std::shared_ptr<MixerScene> scene;
	{
		Glib::Threads::RWLock::ReaderLock lm (_mixer_scenes_lock);
		if (_mixer_scenes.size () <= nth) {
			return false;
		}
		if (!_mixer_scenes[nth]) {
			return false;
		}
		scene = _mixer_scenes[nth];
	}
	assert (scene);

	_last_touched_mixer_scene_idx = nth;
	return scene->apply ();
}

bool
Session::apply_nth_mixer_scene (size_t nth, RouteList const& rl)
{
	std::shared_ptr<MixerScene> scene;
	{
		Glib::Threads::RWLock::ReaderLock lm (_mixer_scenes_lock);
		if (_mixer_scenes.size () <= nth) {
			return false;
		}
		if (!_mixer_scenes[nth]) {
			return false;
		}
		scene = _mixer_scenes[nth];
	}
	assert (scene);

	ControllableSet acs;
	for (auto const& r : rl) {
		r->automatables (acs);
	}

	_last_touched_mixer_scene_idx = nth;
	return scene->apply (acs);
}

void
Session::store_nth_mixer_scene (size_t nth)
{
	std::shared_ptr<MixerScene> scn = nth_mixer_scene (nth, true);

	_last_touched_mixer_scene_idx = nth;
	scn->snapshot ();

	//calling code is expected to set a name, but we need to initalize with 'something'
	if (scn->name().length()==0) {
		std::string str = Glib::DateTime::create_now_local().format ("%FT%H.%M.%S");
		scn->set_name(str);
	}
}

std::shared_ptr<MixerScene>
Session::nth_mixer_scene (size_t nth, bool create_if_missing)
{
	Glib::Threads::RWLock::ReaderLock lm (_mixer_scenes_lock);
	if (create_if_missing) {
		if (_mixer_scenes.size() > nth && _mixer_scenes[nth]) {
			return _mixer_scenes[nth];
		}
		lm.release ();
		Glib::Threads::RWLock::WriterLock lw (_mixer_scenes_lock);
		if (_mixer_scenes.size() <= nth) {
			_mixer_scenes.resize (nth + 1);
		}
		_mixer_scenes[nth] = std::shared_ptr<MixerScene> (new MixerScene (*this));
		return _mixer_scenes[nth];
	}
	if (_mixer_scenes.size () <= nth) {
		return std::shared_ptr<MixerScene> ();
	}
	return _mixer_scenes[nth];
}

std::vector<std::shared_ptr<MixerScene>>
Session::mixer_scenes () const
{
	Glib::Threads::RWLock::ReaderLock lm (_mixer_scenes_lock);
	return _mixer_scenes;
}

Session::ProcessorChangeBlocker::ProcessorChangeBlocker (Session* s, bool rc)
	: _session (s)
	, _reconfigure_on_delete (rc)
{
	PBD::atomic_inc (s->_ignore_route_processor_changes);
}

Session::ProcessorChangeBlocker::~ProcessorChangeBlocker ()
{
	if (PBD::atomic_dec_and_test (_session->_ignore_route_processor_changes)) {
		RouteProcessorChange::Type type = (RouteProcessorChange::Type) _session->_ignored_a_processor_change.fetch_and (0);
		if (_reconfigure_on_delete) {
			if (type & RouteProcessorChange::GeneralChange) {
				_session->route_processors_changed (RouteProcessorChange ());
			} else {
				if (type & RouteProcessorChange::MeterPointChange) {
					_session->route_processors_changed (RouteProcessorChange (RouteProcessorChange::MeterPointChange));
				}
				if (type & RouteProcessorChange::RealTimeChange) {
					_session->route_processors_changed (RouteProcessorChange (RouteProcessorChange::RealTimeChange));
				}
			}
		}
	}
}

void
Session::foreach_route (void (Route::*method)())
{
	for (auto & r : *(routes.reader())) {
		((r.get())->*method) ();
	}
}

bool
Session::have_external_connections_for_current_backend (bool tracks_only) const
{
	std::shared_ptr<RouteList const> rl = routes.reader();
	for (auto const& r : *rl) {
		if (tracks_only && !std::dynamic_pointer_cast<Track> (r)) {
			continue;
		}
		if (r->is_singleton ()) {
			continue;
		}
		for (auto const& p : *r->input()->ports()) {
			if (p->has_ext_connection ()) {
				return true;
			}
		}
		for (auto const& p : *r->output()->ports()) {
			if (p->has_ext_connection ()) {
				return true;
			}
		}
	}
	return false;
}

std::shared_ptr<TriggerBox>
Session::armed_triggerbox () const
{
	std::shared_ptr<TriggerBox> armed_tb;
	std::shared_ptr<RouteList const> rl = routes.reader();

	for (auto const & r : *rl) {
		std::shared_ptr<TriggerBox> tb = r->triggerbox();
		if (tb && tb->armed()) {
			armed_tb = tb;
			break;
		}
	}

	return armed_tb;
}
