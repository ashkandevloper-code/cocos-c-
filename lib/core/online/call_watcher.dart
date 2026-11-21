import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:supabase_flutter/supabase_flutter.dart';

import '../settings/app_settings.dart';
import 'online_service.dart';

/// Rings anywhere in the app when someone starts an Orbit Network call in one of my chats
/// (nx_calls inserts reach only chat members through RLS).
class CallWatcher {
  CallWatcher(this._ref, this._messenger, this._open);

  final WidgetRef _ref;
  final GlobalKey<ScaffoldMessengerState> _messenger;

  /// Opens the call page: (chatId, callId, video, title).
  final void Function(String chatId, String callId, bool video, String title) _open;

  RealtimeChannel? _ch;
  Timer? _poll;
  String? _uid;
  bool _catchingUp = false;
  final Set<String> _seen = <String>{};
  final Set<String> _ringing = <String>{};

  void sync() {
    final OnlineService o = _ref.read(onlineProvider);
    final String? uid = o.isOnline ? o.uid : null;
    if (uid == _uid) return;
    _detach();
    _uid = uid;
    _seen.clear();
    final SupabaseClient? c = o.client;
    if (uid == null || c == null) return;
    _ch = c
        .channel('nxcall:$uid')
        .onPostgresChanges(
          event: PostgresChangeEvent.insert,
          schema: 'public',
          table: 'nx_calls',
          callback: (PostgresChangePayload p) => unawaited(_ring(c, uid, p.newRecord)),
        )
        .subscribe((RealtimeSubscribeStatus status, Object? error) {
          if (status == RealtimeSubscribeStatus.subscribed && _uid == uid) {
            unawaited(_pending(c, uid));
          }
          if (error != null) debugPrint('call realtime: $error (polling continues)');
        });
    unawaited(_pending(c, uid));
    _poll = Timer.periodic(const Duration(seconds: 12), (_) => unawaited(_pending(c, uid)));
  }

  Future<void> _pending(SupabaseClient c, String uid) async {
    if (_uid != uid || _catchingUp) return;
    _catchingUp = true;
    try {
      final List<dynamic> rows = await c.from('nx_calls').select()
          .isFilter('ended_at', null)
          .neq('caller', uid)
          .gte('created_at', DateTime.now().toUtc().subtract(const Duration(seconds: 60)).toIso8601String())
          .order('created_at', ascending: false).limit(10);
      if (_uid != uid) return;
      for (final dynamic r in rows.reversed) {
        await _ring(c, uid, Map<String, dynamic>.from(r as Map));
      }
    } catch (_) {
      // Realtime or the next poll retries after an offline interval.
    } finally {
      _catchingUp = false;
    }
  }

  Future<void> _ring(SupabaseClient c, String uid, Map<String, dynamic> r) async {
    final String id = '${r['id']}';
    final String caller = '${r['caller']}';
    if (_uid != uid || caller == uid || _seen.contains(id) || !_ringing.add(id)) return;
    try {
    final String chatId = '${r['chat_id']}';
    final bool video = r['video'] == true;
    String who = 'Pilot';
    String title = who;
    try {
      final Map<String, dynamic>? p = await c.from('profiles').select('username').eq('id', caller).maybeSingle();
      who = '${p?['username'] ?? 'Pilot'}';
      final Map<String, dynamic>? ch = await c.from('nx_chats').select('kind, name').eq('id', chatId).maybeSingle();
      title = ch == null || ch['kind'] == 'direct' ? who : '${ch['name']}';
    } catch (_) {}
    if (_uid != uid) return;
    final ScaffoldMessengerState? m = _messenger.currentState;
    if (m == null) return;
    final bool fa = _ref.read(settingsProvider).isFa;
    _seen.add(id);
    m.showSnackBar(SnackBar(
      duration: const Duration(seconds: 30),
      content: Row(children: <Widget>[
        Icon(video ? Icons.videocam_rounded : Icons.call_rounded, color: Colors.white),
        const SizedBox(width: 10),
        Expanded(
          child: Text(title == who
              ? (fa ? '$who تماس ${video ? 'تصویری' : 'صوتی'} گرفته است' : '$who is calling you (${video ? 'video' : 'voice'})')
              : (fa ? '$who در «$title» تماس شروع کرد' : '$who started a call in "$title"')),
        ),
      ]),
      action: SnackBarAction(label: fa ? 'پاسخ' : 'Answer', onPressed: () => _open(chatId, id, video, title)),
    ));
    } finally {
      _ringing.remove(id);
    }
  }

  void _detach({bool disposing = false}) {
    _poll?.cancel();
    _poll = null;
    _uid = null;
    final RealtimeChannel? ch = _ch;
    _ch = null;
    if (ch == null) return;
    // On dispose the app's `ref` is already unusable (Riverpod throws), so only unsubscribe the channel itself.
    if (disposing) {
      unawaited(ch.unsubscribe());
      return;
    }
    final SupabaseClient? c = _ref.read(onlineProvider).client;
    if (c != null) unawaited(c.removeChannel(ch));
  }

  void dispose() => _detach(disposing: true);
}
