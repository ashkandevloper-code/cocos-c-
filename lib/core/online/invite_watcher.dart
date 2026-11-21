import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:supabase_flutter/supabase_flutter.dart';

import '../../games/game_registry.dart';
import '../settings/app_settings.dart';
import 'online_service.dart';

/// Shows real-time game invitations from friends anywhere in the app.
class InviteWatcher {
  InviteWatcher(this._ref, this._messenger, this._open, this._openTalk);

  final WidgetRef _ref;
  final GlobalKey<ScaffoldMessengerState> _messenger;

  /// Opens the online lobby for (gameId, code) or a talk room by its UUID.
  final void Function(String gameId, String code) _open;
  final void Function(String roomId) _openTalk;

  RealtimeChannel? _ch;
  Timer? _poll;
  String? _uid;
  bool _catchingUp = false;
  final Set<int> _seen = <int>{};

  void sync() {
    final OnlineService o = _ref.read(onlineProvider);
    final String? uid = o.isOnline ? o.uid : null;
    if (uid == _uid) return;
    _detach();
    _uid = uid;
    _seen.clear();
    final SupabaseClient? c = o.games;
    if (uid == null || c == null) return;
    _ch = c
        .channel('nxinv:$uid')
        .onPostgresChanges(
          event: PostgresChangeEvent.insert,
          schema: 'public',
          table: 'nx_invites',
          filter: PostgresChangeFilter(type: PostgresChangeFilterType.eq, column: 'to_user', value: uid),
          callback: (PostgresChangePayload p) => _show(p.newRecord),
        )
        .subscribe((RealtimeSubscribeStatus status, Object? error) {
          if (status == RealtimeSubscribeStatus.subscribed && _uid == uid) {
            unawaited(_pending(c, uid));
          }
          if (error != null) debugPrint('invite realtime: $error (polling continues)');
        });
    unawaited(_pending(c, uid));
    // Realtime can be interrupted while the user stays on another page; a
    // bounded pending-invite query preserves the invitation instead of
    // silently losing it. The id set prevents repeat notifications.
    _poll = Timer.periodic(const Duration(seconds: 15), (_) => unawaited(_pending(c, uid)));
  }

  Future<void> _pending(SupabaseClient c, String uid) async {
    if (_uid != uid || _catchingUp) return;
    _catchingUp = true;
    try {
      final List<dynamic> rows = await c
          .from('nx_invites')
          .select()
          .eq('to_user', uid)
          .eq('status', 'pending')
          .gte('created_at', DateTime.now().toUtc().subtract(const Duration(minutes: 10)).toIso8601String())
          .order('created_at', ascending: false)
          .limit(10);
      if (_uid != uid) return;
      for (final dynamic r in rows.reversed) {
        _show(Map<String, dynamic>.from(r as Map));
      }
    } catch (_) {
      // The next poll or subscription recovery retries the read.
    } finally {
      _catchingUp = false;
    }
  }

  void _show(Map<String, dynamic> r) {
    final int id = (r['id'] as num?)?.toInt() ?? -1;
    if (id < 0 || _uid == null || '${r['to_user']}' != _uid || _seen.contains(id)) return;
    final Map<String, dynamic> p = Map<String, dynamic>.from((r['payload'] as Map?) ?? const <String, dynamic>{});
    final bool talk = r['kind'] == 'talk';
    final String roomId = '${r['room_id'] ?? ''}';
    final String game = '${p['game'] ?? ''}';
    final String code = '${p['code'] ?? ''}';
    final String from = '${p['from_name'] ?? 'Pilot'}';
    final GameInfo? g = gameById(game);
    final bool fa = _ref.read(settingsProvider).isFa;
    final String gameName = talk ? '${p['title'] ?? (fa ? 'اتاق گفتگو' : 'Talk room')}'
        : g == null ? game : (fa ? g.nameFa : g.nameEn);
    final ScaffoldMessengerState? m = _messenger.currentState;
    if (m == null || (talk ? roomId.isEmpty || roomId == 'null' : code.isEmpty)) return;
    _seen.add(id);
    m.showSnackBar(SnackBar(
      duration: const Duration(seconds: 12),
      content: Text(fa ? '$from تو را به $gameName دعوت کرد' : '$from invited you to $gameName'),
      action: SnackBarAction(
        label: fa ? 'ورود' : 'Join',
        onPressed: () {
          final SupabaseClient? c = _ref.read(onlineProvider).games;
          if (c != null && id >= 0) {
            unawaited(c.from('nx_invites').update(<String, dynamic>{'status': 'accepted'}).eq('id', id).then((_) {}, onError: (Object _) {}));
          }
          if (talk) {
            _openTalk(roomId);
          } else {
            _open(game, code);
          }
        },
      ),
    ));
  }

  void _detach({bool disposing = false}) {
    _poll?.cancel();
    _poll = null;
    _uid = null; // ignore in-flight reads/callbacks after a sign-out or dispose
    final RealtimeChannel? ch = _ch;
    _ch = null;
    if (ch == null) return;
    // On dispose the app's `ref` is already unusable (Riverpod throws), so only unsubscribe the channel itself.
    if (disposing) {
      unawaited(ch.unsubscribe());
      return;
    }
    final SupabaseClient? c = _ref.read(onlineProvider).games;
    if (c != null) unawaited(c.removeChannel(ch));
  }

  void dispose() => _detach(disposing: true);
}
