import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import 'package:supabase_flutter/supabase_flutter.dart';

import '../../core/online/online_service.dart';
import '../../core/settings/app_settings.dart';
import '../../core/lux/lux.dart';
import '../../core/ui2/nx2_widgets.dart';
import '../../games/game_registry.dart';
import '../../games/arcade_kit/arcade_icon.dart';

/// ROOMS — opened by the big GAME button in the dock.
///  * Quick Play: pick a game -> real matchmaking (`nx_quick`) in that game's lobby.
///  * Create Room: pick a game -> a new public room (`nx_create_room`), others can join from this list.
///  * Open rooms: every public lobby on the server right now (no placeholder rows: empty/offline states only).
///  * All games: the arcade list (offline vs-bot / pass-and-play stays available there).
class RoomsPage extends ConsumerStatefulWidget {
  const RoomsPage({super.key, this.tab});

  /// 'quick' | 'rooms' | 'talk' (Games -> Online & voice).
  final String? tab;

  @override
  ConsumerState<RoomsPage> createState() => _RoomsPageState();
}

class _RoomsPageState extends ConsumerState<RoomsPage> {
  late int _tab = switch (widget.tab) { 'rooms' => 1, 'talk' => 2, _ => 0 };
  List<Map<String, dynamic>> _rooms = <Map<String, dynamic>>[];
  List<Map<String, dynamic>> _talk = <Map<String, dynamic>>[];
  bool _loading = true;
  String? _error;
  Timer? _poll;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) => unawaited(_load()));
    _poll = Timer.periodic(const Duration(seconds: 8), (_) => unawaited(_load(silent: true)));
  }

  @override
  void dispose() {
    _poll?.cancel();
    super.dispose();
  }

  Future<void> _load({bool silent = false}) async {
    final OnlineService o = ref.read(onlineProvider);
    if (!silent && mounted) setState(() => _loading = true);
    try {
      if (!o.isOnline) await o.ensureSession();
      final SupabaseClient? c = o.games;
      if (c == null || o.client?.auth.currentSession == null) {
        if (mounted) {
          setState(() {
            _loading = false;
            _error = 'offline';
          });
        }
        return;
      }
      final List<dynamic> rows = await c
          .from('nx_rooms')
          .select('id, code, game, seats, host, created_at, nx_room_players(count)')
          .eq('status', 'lobby')
          .eq('is_private', false)
          .eq('quick', false)
          .eq('kind', 'game')
          .gte('updated_at', DateTime.now().toUtc().subtract(const Duration(minutes: 20)).toIso8601String())
          .order('created_at', ascending: false)
          .limit(40);
      List<dynamic> talk = const <dynamic>[];
      try {
        talk = (await c.rpc('nx_talk_rooms')) as List<dynamic>? ?? const <dynamic>[];
      } catch (_) {}
      if (!mounted) return;
      setState(() {
        _talk = talk.map((dynamic r) => Map<String, dynamic>.from(r as Map)).toList();
        _rooms = rows.map((dynamic r) => Map<String, dynamic>.from(r as Map)).toList();
        _loading = false;
        _error = null;
      });
    } catch (e) {
      if (mounted) {
        setState(() {
          _loading = false;
          _error = 'offline';
        });
      }
    }
  }

  Future<void> _pickGame(String action) async {
    final List<GameInfo> games = kGames.where((GameInfo g) => g.supportsOnline).toList();
    final GameInfo? g = await showModalBottomSheet<GameInfo>(
      context: context,
      backgroundColor: Colors.transparent,
      isScrollControlled: true,
      builder: (BuildContext ctx) => _GamePickerSheet(games: games, title: action == 'quick'
          ? ctx.tx('بازی سریع — کدام بازی؟', 'Quick Play — which game?')
          : ctx.tx('ساخت اتاق — کدام بازی؟', 'Create room — which game?')),
    );
    if (g == null || !mounted) return;
    await context.push('/online/${g.id}?action=$action');
    unawaited(_load(silent: true));
  }

  /// Opens a new conversation room (voice + chat; games are picked inside).
  Future<void> _openTalk() async {
    final TextEditingController name = TextEditingController();
    final String? title = await showLuxSheet<String>(
      context,
      title: context.tx('روم صوتی جدید', 'New voice room'),
      subtitle: context.tx('گفتگوی زنده؛ بازی را داخل روم انتخاب کن', 'Live talk; pick a game inside the room'),
      builder: (BuildContext ctx) => Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: <Widget>[
        LuxTextField(controller: name, autofocus: true, maxLength: 60, hint: ctx.tx('نام روم (اختیاری)', 'Room name (optional)'), icon: Icons.mic_rounded),
        const SizedBox(height: 14),
        LuxButton(label: ctx.tx('ساخت روم', 'Create room'), icon: Icons.headset_mic_rounded, expand: true, onPressed: () => Navigator.pop(ctx, name.text.trim())),
      ]),
    );
    if (title == null || !mounted) return;
    final OnlineService o = ref.read(onlineProvider);
    try {
      await o.gamesHello();
      final dynamic r = await o.games!.rpc('nx_room_open', params: <String, dynamic>{'p_title': title});
      if (!mounted) return;
      await context.push('/room/${(r as Map)['room_id']}');
      unawaited(_load(silent: true));
    } catch (e) {
      if (mounted) {
        LuxToast.show(context, context.tx('ساخت روم ممکن نشد. وارد حساب شده‌ای؟', 'Could not create the room. Are you signed in?'),
            icon: Icons.error_outline_rounded);
      }
    }
  }

  Widget _talkTile(Map<String, dynamic> r) {
    final GameInfo? g = gameById('${r['game']}');
    return Padding(
      padding: const EdgeInsets.only(bottom: 10),
      child: LuxPanel(
        padding: const EdgeInsets.fromLTRB(12, 10, 14, 10),
        onTap: () async {
          await context.push('/room/${r['id']}');
          unawaited(_load(silent: true));
        },
        child: LuxListRow(
          leading: const LuxMedallion(asset: 'assets/luxe/icons/r3c1.webp', size: 46),
          title: '${r['title'] ?? r['host_name'] ?? 'ANIX'}',
          subtitle: '${r['status'] == 'playing' ? context.tx('در حال بازی', 'Playing') : context.tx('گفتگو', 'Talking')}'
              '${g == null ? '' : ' · ${context.tx(g.nameFa, g.nameEn)}'}',
          trailing: LuxChip(label: '${r['members'] ?? 0}', icon: Icons.people_alt_rounded, live: true),
          chevron: false,
        ),
      ),
    );
  }

  int _count(Map<String, dynamic> r) {
    final dynamic p = r['nx_room_players'];
    if (p is List && p.isNotEmpty && p.first is Map) return ((p.first as Map)['count'] as num?)?.toInt() ?? 0;
    return 0;
  }

  @override
  Widget build(BuildContext context) {
    final bool signedIn = ref.watch(onlineProvider).client?.auth.currentSession != null;
    Widget status() {
      if (_error != null && !signedIn) {
        return LuxEmpty(
          icon: Icons.person_off_rounded,
          title: context.tx('برای بازی آنلاین وارد شو', 'Sign in to play online'),
          message: context.tx('بازی آفلاین و با ربات در بخش بازی‌ها در دسترس است.', 'Offline and vs-bot games are under Games.'),
          actionLabel: context.tx('ورود', 'Sign in'),
          action: () => context.push('/account'),
        );
      }
      if (_error != null) {
        return LuxEmpty(
          icon: Icons.wifi_off_rounded,
          title: context.tx('اتصال برقرار نشد', 'Could not reach the server'),
          message: context.tx('اینترنت را بررسی کن و دوباره تلاش کن.', 'Check your connection and try again.'),
          actionLabel: context.tx('تلاش دوباره', 'Retry'),
          action: _load,
        );
      }
      return const SizedBox.shrink();
    }

    final List<Widget> body;
    switch (_tab) {
      case 0:
        body = <Widget>[
          LuxPanel(
            highlight: true,
            padding: const EdgeInsets.all(18),
            child: Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: <Widget>[
              Row(children: <Widget>[
                const LuxMedallion(asset: 'assets/luxe/icons/r4c6.webp', size: 58),
                const SizedBox(width: 14),
                Expanded(
                  child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: <Widget>[
                    Text(context.tx('بازی سریع', 'Quick match'), style: LuxType.headline.copyWith(color: LuxPalette.of(context).ink)),
                    Text(context.tx('یک بازی انتخاب کن؛ حریف را برایت پیدا می‌کنیم.', 'Pick a game; we find you an opponent.'),
                        style: LuxType.caption.copyWith(color: LuxPalette.of(context).mute, fontSize: 12)),
                  ]),
                ),
              ]),
              const SizedBox(height: 16),
              LuxButton(
                  key: const ValueKey<String>('rooms-quick'),
                  label: context.tx('شروع بازی سریع', 'Start quick match'),
                  icon: Icons.bolt_rounded,
                  expand: true,
                  onPressed: () => _pickGame('quick')),
            ]),
          ),
          const SizedBox(height: 14),
          LuxSection(title: context.tx('همه‌ی بازی‌ها', 'All games'), children: <Widget>[
            LuxListRow(
              icon: Icons.smart_toy_rounded,
              title: context.tx('بازی با ربات و دونفره', 'Vs bot & pass-and-play'),
              subtitle: context.tx('بدون اینترنت', 'No internet needed'),
              onTap: () => context.push('/play'),
            ),
            LuxListRow(
              icon: Icons.leaderboard_rounded,
              title: context.tx('رده‌بندی', 'Rankings'),
              onTap: () => context.push('/leaderboard'),
            ),
          ]),
        ];
      case 1:
        body = <Widget>[
          Row(children: <Widget>[
            Expanded(
              child: LuxButton(
                  label: context.tx('ساخت روم', 'New room'), icon: Icons.add_rounded, expand: true, onPressed: () => _pickGame('create')),
            ),
            const SizedBox(width: 10),
            Expanded(
              child: LuxButton.glass(
                  label: context.tx('ورود با کد', 'Join by code'), icon: Icons.vpn_key_rounded, expand: true, onPressed: _joinByCode),
            ),
          ]),
          const SizedBox(height: 16),
          LuxSectionTitle(context.tx('روم‌های باز', 'Open rooms'), latin: 'LOBBIES'),
          if (_loading && _rooms.isEmpty)
            const Padding(padding: EdgeInsets.all(28), child: LuxLoading())
          else if (_error != null && _rooms.isEmpty)
            status()
          else if (_rooms.isEmpty)
            LuxEmpty(
              icon: Icons.meeting_room_outlined,
              title: context.tx('الان روم بازی باز نیست', 'No open rooms right now'),
              message: context.tx('اولین روم را بساز و دوستانت را دعوت کن.', 'Create the first one and invite friends.'),
            )
          else
            for (final Map<String, dynamic> r in _rooms) _roomTile(r),
        ];
      default:
        body = <Widget>[
          LuxButton(
              label: context.tx('روم صوتی جدید', 'New voice room'), icon: Icons.headset_mic_rounded, expand: true, onPressed: _openTalk),
          const SizedBox(height: 16),
          LuxSectionTitle(context.tx('روم‌های صوتی زنده', 'Live voice rooms'), latin: 'VOICE'),
          if (_loading && _talk.isEmpty)
            const Padding(padding: EdgeInsets.all(28), child: LuxLoading())
          else if (_error != null && _talk.isEmpty)
            status()
          else if (_talk.isEmpty)
            LuxEmpty(
              icon: Icons.mic_none_rounded,
              title: context.tx('روم صوتی فعالی نیست', 'No live voice rooms'),
              message: context.tx('یک روم بساز؛ دیگران می‌توانند بپیوندند.', 'Open one; others can join you.'),
            )
          else
            for (final Map<String, dynamic> r in _talk) _talkTile(r),
        ];
    }
    return LuxPage(
      title: context.tx('آنلاین و صوتی', 'Online & voice'),
      kicker: 'ONLINE',
      iconAsset: 'assets/luxe/icons/r4c5.webp',
      bottom: const LuxDock(current: Nx2Tab.games),
      tabs: LuxPillTabs(
        labels: <String>[context.tx('بازی سریع', 'Quick'), context.tx('روم‌ها', 'Rooms'), context.tx('صوتی', 'Voice')],
        icons: const <IconData>[Icons.bolt_rounded, Icons.meeting_room_rounded, Icons.mic_rounded],
        index: _tab,
        onChanged: (int i) => setState(() => _tab = i),
      ),
      body: RefreshIndicator(
        onRefresh: _load,
        child: ListView(
          key: ValueKey<int>(_tab),
          physics: const AlwaysScrollableScrollPhysics(parent: BouncingScrollPhysics()),
          padding: const EdgeInsets.fromLTRB(LuxSpace.page, 8, LuxSpace.page, 24),
          children: body,
        ),
      ),
    );
  }

  Future<void> _joinByCode() async {
    final TextEditingController code = TextEditingController();
    final String? c = await showLuxSheet<String>(
      context,
      title: context.tx('ورود با کد', 'Join by code'),
      builder: (BuildContext ctx) => Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: <Widget>[
        LuxTextField(controller: code, autofocus: true, maxLength: 8, hint: 'ABCD12', icon: Icons.vpn_key_rounded, textAlign: TextAlign.center),
        const SizedBox(height: 14),
        LuxButton(label: ctx.tx('ورود', 'Join'), expand: true, onPressed: () => Navigator.pop(ctx, code.text.trim().toUpperCase())),
      ]),
    );
    if (c == null || c.length < 4 || !mounted) return;
    // Resolve the code to its room's game first, so the lobby opens the right game.
    final SupabaseClient? gc = ref.read(onlineProvider).games;
    Map<String, dynamic>? room;
    try {
      room = gc == null ? null : await gc.from('nx_rooms').select('id, game, kind').eq('code', c).maybeSingle();
    } catch (_) {}
    if (!mounted) return;
    if (room == null) {
      LuxToast.show(context, context.tx('رومی با این کد پیدا نشد', 'No room with that code'), icon: Icons.search_off_rounded);
      return;
    }
    if (room['kind'] == 'talk') {
      await context.push('/room/${room['id']}');
    } else {
      await context.push('/online/${room['game']}?code=$c');
    }
    unawaited(_load(silent: true));
  }

  Widget _roomTile(Map<String, dynamic> r) {
    final GameInfo? g = gameById('${r['game']}');
    final int n = _count(r), seats = (r['seats'] as num?)?.toInt() ?? 2;
    return Padding(
      padding: const EdgeInsets.only(bottom: 10),
      child: LuxPanel(
        padding: const EdgeInsets.fromLTRB(12, 10, 14, 10),
        onTap: () async {
          await context.push('/online/${r['game']}?room=${r['id']}');
          unawaited(_load(silent: true));
        },
        child: LuxListRow(
          leading: LuxMedallion(asset: 'assets/arcade/pedestals/${r['game']}.webp', size: 46, glow: g?.accent),
          title: g == null ? '${r['game']}' : context.tx(g.nameFa, g.nameEn),
          subtitle: '${context.tx('کد', 'Code')} ${r['code']}',
          trailing: LuxChip(label: '$n/$seats', icon: Icons.event_seat_rounded),
          chevron: false,
        ),
      ),
    );
  }
}

class _GamePickerSheet extends StatelessWidget {
  const _GamePickerSheet({required this.games, required this.title});
  final List<GameInfo> games;
  final String title;

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return Container(
      padding: EdgeInsets.fromLTRB(18, 14, 18, 18 + MediaQuery.paddingOf(context).bottom),
      decoration: BoxDecoration(
        borderRadius: const BorderRadius.vertical(top: Radius.circular(LuxRadius.sheet)),
        gradient: LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: p.isDark ? <Color>[const Color(0xFF14130F), const Color(0xFF0A0A0D)] : <Color>[p.panelHi, p.panel]),
        border: Border(top: BorderSide(color: p.lineStrong)),
      ),
      child: Column(mainAxisSize: MainAxisSize.min, children: <Widget>[
        Container(width: 40, height: 4, margin: const EdgeInsets.only(bottom: 12), decoration: BoxDecoration(color: p.faint, borderRadius: BorderRadius.circular(4))),
        Text(title, style: LuxType.title.copyWith(color: p.ink, fontSize: 19)),
        const SizedBox(height: 14),
        Flexible(
          child: GridView.count(
            shrinkWrap: true,
            crossAxisCount: 3,
            mainAxisSpacing: 10,
            crossAxisSpacing: 10,
            childAspectRatio: .86,
            children: <Widget>[
              for (final GameInfo g in games)
                LuxPanel(
                  padding: const EdgeInsets.fromLTRB(6, 10, 6, 8),
                  radius: 22,
                  onTap: () => Navigator.pop(context, g),
                  child: Column(mainAxisAlignment: MainAxisAlignment.center, children: <Widget>[
                    Expanded(
                      child: Image.asset('assets/arcade/pedestals/${g.id}.webp',
                          errorBuilder: (_, __, ___) => SizedBox(height: 52, child: ArcadeMedallion(g.id))),
                    ),
                    const SizedBox(height: 6),
                    Text(context.tx(g.nameFa, g.nameEn),
                        maxLines: 1, overflow: TextOverflow.ellipsis, style: LuxType.label.copyWith(color: p.ink)),
                  ]),
                ),
            ],
          ),
        ),
      ]),
    );
  }
}
