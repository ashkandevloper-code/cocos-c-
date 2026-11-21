import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/audio/music_service.dart';
import '../../../core/audio/sfx_service.dart';
import '../../../core/online/cloud_profile_sync.dart';
import '../../../core/online/match_link.dart';
import '../../../core/online/online_service.dart';
import '../../../core/online/room_voice.dart';
import '../../../core/widgets/profile_preview.dart';
import '../../../core/profile/profile_service.dart';
import '../../../core/settings/app_settings.dart';
import '../../../core/theme/nx_theme.dart';
import '../../../games/game_registry.dart';
import '../../oracle_core/persona/oracle_persona.dart';
import '../../oracle_core/persona/oracle_persona_widgets.dart';
import 'settled_match_result.dart';

import '../../../core/ui2/anix_kit.dart';
/// Hosts one online match: opens the move link, builds the game in online mode,
/// rebuilds + replays the log on reconnect/resync, reports the result and shows it.
class OnlineMatchPage extends ConsumerStatefulWidget {
  const OnlineMatchPage(
      {super.key,
      required this.matchId,
      required this.gameId,
      this.tournamentMatchId});
  final String matchId;
  final String gameId;

  /// Set when this game belongs to a tournament match: the result is also reported to the tournament
  /// (nx_t_report — both players' reports must agree, otherwise it becomes a dispute).
  final String? tournamentMatchId;

  @override
  ConsumerState<OnlineMatchPage> createState() => _OnlineMatchPageState();
}

class _OnlineMatchPageState extends ConsumerState<OnlineMatchPage>
    with WidgetsBindingObserver {
  SupabaseMatchLink? _link;
  String? _error;
  int _generation = 0;
  bool _finished = false;
  bool _withdrawn = false;
  bool _reporting = false;
  GameResult? _localResult;
  GameResult? _result;
  MatchEnd? _serverEnd;
  String? _oracleLine;
  String? _resultNoteFa;
  String? _resultNoteEn;
  bool _recorded = false;
  bool _graceStarted = false;

  @override
  void initState() {
    super.initState();
    MusicService.instance.enterGame();
    WidgetsBinding.instance.addObserver(this);
    unawaited(_open());
  }

  Future<void> _open() async {
    final OnlineService o = ref.read(onlineProvider);
    final client = o.games;
    final String? uid = o.uid;
    if (client == null || uid == null) {
      setState(() => _error = 'offline');
      return;
    }
    try {
      final SupabaseMatchLink link =
          await SupabaseMatchLink.open(client, widget.matchId, uid: uid);
      if (!mounted) {
        link.dispose();
        return;
      }
      link.addListener(_onLinkChanged);
      link.resyncRequests.addListener(_onResync);
      setState(() => _link = link);
      if (link.isOver) _onLinkChanged();
    } catch (e) {
      if (mounted) setState(() => _error = '$e');
    }
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    // Coming back from background: rebuild from the server log so this phone catches up exactly.
    if (state == AppLifecycleState.resumed && _link != null && !_finished) {
      _onResync();
    }
  }

  void _onResync() {
    if (!mounted || _finished) return;
    setState(() => _generation++);
  }

  void _onLinkChanged() {
    final SupabaseMatchLink? l = _link;
    if (l == null || !mounted) return;
    final MatchEnd? end = l.end;
    if (end == null) {
      if (!_finished) setState(() {});
      return;
    }
    if (_serverEnd != null) return;
    if (!_finished && end.rewardsGranted && !end.isForfeit && !_graceStarted) {
      // A normal finish arrived first: give the local animation a moment to complete.
      _graceStarted = true;
      Future<void>.delayed(const Duration(seconds: 8), () {
        if (mounted && !_finished) _onLinkChanged();
      });
      setState(() {});
      return;
    }
    _finished = true;
    _applyServerEnd(end);
  }

  List<int> _winnersFor(GameResult r, SupabaseMatchLink l) {
    if (r.winnerSeats != null) return r.winnerSeats!;
    if (r.outcome == GameOutcome.draw) return <int>[];
    if (r.outcome == GameOutcome.win) return <int>[l.mySeat];
    return <int>[
      for (int s = 0; s < l.seatCount; s++)
        if (s != l.mySeat) s
    ];
  }

  void _onGameFinish(GameResult r) {
    if (_finished) return;
    _finished = true;
    _withdrawn = !r.endsMatch; // self-forfeit while teammates keep playing
    _localResult = r;
    final SupabaseMatchLink? l = _link;
    if (l != null && r.endsMatch) unawaited(_submitResult(l, r));
    Future<void>.delayed(Duration(milliseconds: r.holdMs ?? 900), () {
      if (mounted && _serverEnd == null) setState(() => _result = r);
    });
  }

  Future<void> _submitResult(SupabaseMatchLink l, GameResult r) async {
    if (_reporting || _serverEnd != null) return;
    _reporting = true;
    try {
      final MatchEnd? end = await l.report(_winnersFor(r, l), r.scores);
      if (mounted && end != null) _applyServerEnd(end);
    } finally {
      _reporting = false;
      if (mounted) setState(() {});
    }
  }

  void _applyServerEnd(MatchEnd end) {
    if (!mounted || _serverEnd != null) return;
    _serverEnd = end;
    final SupabaseMatchLink? l = _link;
    if (end.rewardsGranted && l != null) {
      final GameResult r = settledMatchResult(end, l.mySeat);
      _result = r;
      final int? fs = end.forfeitSeat;
      final String who = fs != null && fs >= 0 && fs < l.names.length ? l.names[fs] : '';
      _resultNoteFa = fs == l.mySeat ? 'از بازی خارج شدی' : (fs != null ? '$who از بازی خارج شد' : null);
      _resultNoteEn = fs == l.mySeat ? 'You left the match' : (fs != null ? '$who left the match' : null);
      if (!_recorded) {
        _recorded = true;
        _oracleLine = OraclePersona.instance.resultReaction(
            outcome: switch (r.outcome) { GameOutcome.win => 'win', GameOutcome.loss => 'loss', GameOutcome.draw => 'draw' },
            myScore: r.score,
            oppScore: r.opponentScore);
        final GameInfo? g = gameById(widget.gameId);
        ref.read(profileProvider).recordOnlineGame(
              gameId: widget.gameId,
              gameNameFa: g?.nameFa ?? widget.gameId,
              gameNameEn: g?.nameEn ?? widget.gameId,
              outcome: r.outcome.name,
              score: r.score,
            );
        if (r.outcome != GameOutcome.draw) NxSfx.instance.play(r.outcome == GameOutcome.win ? 'win' : 'lose');
        if (ref.read(settingsProvider).haptics) HapticFeedback.heavyImpact();
        if (widget.tournamentMatchId != null && _localResult != null && !end.isForfeit) {
          unawaited(_reportTournament(r, l));
        }
      }
    } else {
      // A dispute, abandonment or administrative closure is NOT a draw and carries NO reward or local record.
      _result = const GameResult(outcome: GameOutcome.draw);
      _oracleLine = null;
    }
    unawaited(ref.read(cloudSyncProvider).refreshWallet());
    setState(() {});
  }

  /// Tournament: this phone's own report of the game (seat 0 = home). Retries a few times on bad network.
  Future<void> _reportTournament(GameResult r, SupabaseMatchLink l) async {
    final List<int> winners = _winnersFor(r, l);
    final int winnerSeat = winners.length == 1 ? winners.first : -1;
    final List<int> s = List<int>.filled(l.seatCount < 2 ? 2 : l.seatCount, 0);
    final List<int>? scores = r.scores;
    if (scores != null && scores.length >= 2) {
      s[0] = scores[0];
      s[1] = scores[1];
    } else if (l.mySeat >= 0 && l.mySeat < 2) {
      s[l.mySeat] = r.score;
      s[1 - l.mySeat] = r.opponentScore;
    }
    // keep the report consistent with the winner (the server rejects contradictions)
    if (winnerSeat == 0 && s[0] < s[1]) s[0] = s[1];
    if (winnerSeat == 1 && s[1] < s[0]) s[1] = s[0];
    if (winnerSeat == -1 && s[0] != s[1]) s[1] = s[0];
    for (int attempt = 0; attempt < 4; attempt++) {
      try {
        await ref
            .read(onlineProvider)
            .games
            ?.rpc('nx_t_report', params: <String, dynamic>{
          'p_game': widget.matchId,
          'p_winner_seat': winnerSeat,
          'p_s0': s[0],
          'p_s1': s[1],
        });
        return;
      } catch (e) {
        debugPrint('tournament report failed (try $attempt): $e');
        await Future<void>.delayed(Duration(seconds: 2 * (attempt + 1)));
      }
    }
  }

  Future<bool> _confirmLeave() async {
    if (_finished || _link == null) return true;
    final bool? ok = await showDialog<bool>(
      context: context,
      builder: (BuildContext ctx) => AnixDialog(
        title: Text(ctx.tx('ترک بازی آنلاین؟', 'Leave the online match?')),
        content: Text(ctx.tx(
            'خروج یعنی تسلیم: در بازی دونفره حریف برنده می‌شود.',
            'Leaving counts as a forfeit: in a 2-player game your opponent wins.')),
        actions: <Widget>[
          TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: Text(ctx.tx('ادامه', 'Keep playing'))),
          FilledButton(
            style: FilledButton.styleFrom(backgroundColor: NxColors.danger),
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(ctx.tx('تسلیم و خروج', 'Forfeit & leave')),
          ),
        ],
      ),
    );
    if (ok == true) {
      final bool accepted = await _link?.forfeit() ?? false;
      if (accepted) {
        _finished = true;
        return true;
      }
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(
            content: Text(context.tx('سرور انصراف را تأیید نکرد. پس از اتصال دوباره تلاش کن.',
                'The server did not confirm the forfeit. Reconnect and try again.'))));
      }
    }
    return false;
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    final SupabaseMatchLink? l = _link;
    if (l != null) {
      l.removeListener(_onLinkChanged);
      l.resyncRequests.removeListener(_onResync);
      l.dispose();
    }
    MusicService.instance.exitGame();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final AppSettings s = ref.watch(settingsProvider);
    final SupabaseMatchLink? l = _link;
    final GameInfo? game = gameById(widget.gameId);
    Widget body;
    if (_error != null) {
      body = Center(
          child: Text(
              context.tx('ورود به بازی ممکن نشد', 'Could not open the match')));
    } else if (l == null || game == null) {
      body = const Center(child: CircularProgressIndicator());
    } else {
      final GameLaunch launch = GameLaunch(
        gameId: widget.gameId,
        mode: GameMode.online,
        difficulty: BotDifficulty.normal,
        playerName: l.mySeat >= 0 && l.mySeat < l.names.length
            ? l.names[l.mySeat]
            : 'Explorer',
        isFa: s.isFa,
        fancyEffects: s.fancyEffects,
        reducedMotion: s.reducedMotion,
        soundOn: s.sound,
        hapticsOn: s.haptics,
        playerCount: l.seatCount,
        skins: Map<String, String>.from(ref.read(profileProvider).equipped),
        seed: l.seed,
        net: l,
        onFinish: _onGameFinish,
      );
      final Widget gameView = KeyedSubtree(
        key: ValueKey<int>(_generation),
        child: game.builder(launch),
      );
      body = gameView;
    }
    return PopScope(
      canPop: false,
      onPopInvokedWithResult: (bool didPop, _) async {
        if (didPop) return;
        final NavigatorState nav = Navigator.of(context);
        if (await _confirmLeave()) nav.pop();
      },
      child: Scaffold(
        backgroundColor: NxColors.navyDeep,
        body: Stack(children: <Widget>[
          Positioned.fill(child: body),
          PositionedDirectional(
            top: MediaQuery.paddingOf(context).top + 6,
            start: 6,
            child: Material(
              color: Colors.black.withValues(alpha: 0.35),
              shape: const CircleBorder(),
              child: IconButton(
                icon: const Icon(Icons.close_rounded, color: Colors.white),
                onPressed: () async {
                  final NavigatorState nav = Navigator.of(context);
                  if (await _confirmLeave()) nav.pop();
                },
              ),
            ),
          ),
          if (l != null && _result == null)
            PositionedDirectional(
                top: MediaQuery.paddingOf(context).top + 10,
                end: 10,
                child: _presenceBar(l)),
          if (_result == null)
            Positioned(
              top: MediaQuery.paddingOf(context).top + 58,
              left: 0,
              right: 0,
              child: OracleTutorialBanner(gameId: widget.gameId),
            ),
          // In-match voice: mic mute/unmute + speaker on/off (same LiveKit room as the lobby).
          if (_result == null)
            PositionedDirectional(
              bottom: MediaQuery.paddingOf(context).bottom + 10,
              end: 10,
              child: RoomVoiceBar(id: widget.matchId, match: true, compact: true),
            ),
          if (_result != null)
            Positioned.fill(child: _overlay(context, _result!)),
        ]),
      ),
    );
  }

  Widget _presenceBar(SupabaseMatchLink l) {
    return Material(
      color: Colors.black.withValues(alpha: 0.55),
      borderRadius: BorderRadius.circular(18),
      child: InkWell(
        borderRadius: BorderRadius.circular(18),
        onTap: () => showAnixSheet<void>(context: context, builder: (BuildContext sheet) =>
          SafeArea(child: Column(mainAxisSize: MainAxisSize.min, children: <Widget>[
            Text(sheet.tx('بازیکنان', 'Players'), style: const TextStyle(fontWeight: FontWeight.w800)),
            for (int s = 0; s < l.seatCount; s++)
              ListTile(
                leading: Icon(Icons.circle, size: 13, color: l.forfeitedSeats.contains(s)
                    ? NxColors.danger : l.connectedSeats.contains(s) ? NxColors.success : NxColors.gold),
                title: Text(l.names[s]),
                subtitle: Text(l.realtimePresenceReady
                    ? sheet.tx('جایگاه ${s + 1}', 'Seat ${s + 1}')
                    : sheet.tx('حضور در حال بازیابی · حرکت‌ها همگام می‌شوند',
                        'Presence reconnecting · moves still sync')),
                onTap: () => showProfilePreview(sheet, l.playerIds[s], name: l.names[s]),
              ),
          ]))),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 9),
          child: Row(mainAxisSize: MainAxisSize.min, children: <Widget>[
            Icon(l.realtimePresenceReady ? Icons.people_outline_rounded : Icons.sync_problem_rounded,
                size: 17, color: l.realtimePresenceReady ? Colors.white : NxColors.gold),
            const SizedBox(width: 5),
            for (int s = 0; s < l.seatCount; s++)
              Padding(
                padding: const EdgeInsets.only(left: 3),
                child: Container(
                  width: 9,
                  height: 9,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: l.forfeitedSeats.contains(s)
                        ? NxColors.danger
                        : l.connectedSeats.contains(s) ? NxColors.success : NxColors.gold,
                    border: s == l.mySeat ? Border.all(color: Colors.white, width: 1.5) : null,
                  ),
                ),
              ),
          ]),
        ),
      ),
    );
  }

  Widget _overlay(BuildContext context, GameResult r) {
    final MatchEnd? end = _serverEnd;
    final MatchSettlementView view = matchSettlementView(end, withdrawalRequested: _withdrawn);
    final bool settled = view == MatchSettlementView.settled;
    final (String title, Color color, IconData icon) = switch (view) {
      MatchSettlementView.waiting =>
        (context.tx('در انتظار تأیید نتیجه', 'Waiting for match settlement'), NxColors.turquoise, Icons.hourglass_top_rounded),
      MatchSettlementView.withdrawal =>
        (context.tx('انصراف ثبت شد', 'Withdrawal recorded'), NxColors.gold, Icons.exit_to_app_rounded),
      MatchSettlementView.disputed =>
        (context.tx('نتیجه مورد اختلاف است', 'Result disputed'), NxColors.danger, Icons.report_problem_rounded),
      MatchSettlementView.closed =>
        (context.tx('مسابقه بدون نتیجه بسته شد', 'Match closed without a result'), NxColors.danger, Icons.report_problem_rounded),
      MatchSettlementView.settled => switch (r.outcome) {
          GameOutcome.win => (context.tx('پیروزی!', 'Victory!'), NxColors.gold, Icons.emoji_events_rounded),
          GameOutcome.draw => (context.tx('مساوی', 'Draw'), NxColors.turquoise, Icons.handshake_rounded),
          GameOutcome.loss => (context.tx('شکست', 'Defeat'), NxColors.danger, Icons.sentiment_dissatisfied_rounded),
        },
    };
    final String summary = settled ? (_resultNoteFa == null ? '' : context.tx(_resultNoteFa!, _resultNoteEn ?? '')) : '';
    return Container(
      color: Colors.black.withValues(alpha: 0.72),
      alignment: Alignment.center,
      padding: const EdgeInsets.all(24),
      child: Column(mainAxisSize: MainAxisSize.min, children: <Widget>[
        Icon(icon, color: color, size: 72),
        const SizedBox(height: 10),
        Text(title,
            style: TextStyle(
                fontSize: 34, fontWeight: FontWeight.w900, color: color)),
        if (summary.isNotEmpty) ...<Widget>[
          const SizedBox(height: 8),
          Text(summary,
              textAlign: TextAlign.center,
              style: const TextStyle(color: NxColors.iceWhite)),
        ],
        if (_oracleLine != null) OracleResultLine(text: _oracleLine!),
        const SizedBox(height: 10),
        Text(
          view == MatchSettlementView.withdrawal
              ? context.tx('هم‌تیمی‌ها بازی را ادامه می‌دهند. نتیجهٔ این مسابقه بعداً در سرور ثبت می‌شود؛ فعلاً پاداشی دریافت نمی‌کنی.',
                  'Your teammates continue playing. The server will settle the match later; no reward is granted yet.')
              : end == null
                  ? context.tx('تا ثبت نتیجه در سرور، رکورد یا پاداشی ثبت نمی‌شود.',
                      'No record or reward is granted until the server settles the match.')
              : settled
                  ? context.tx('نتیجه در سرور ثبت شد؛ کیف پول در حال همگام‌سازی است.',
                      'Server result recorded; wallet is syncing.')
                  : context.tx('این مسابقه پاداش ندارد. برای بررسی اختلاف با پشتیبانی تماس بگیر.',
                      'No reward for this match. Contact support to review the dispute.'),
          textAlign: TextAlign.center,
          style: const TextStyle(color: NxColors.textDim),
        ),
        if (widget.tournamentMatchId != null && settled)
          Padding(
            padding: const EdgeInsets.only(top: 6),
            child: Text(context.tx('وضعیت تورنمنت را پس از بازگشت بررسی کن.',
                'Check tournament status after returning.'),
                textAlign: TextAlign.center,
                style: const TextStyle(color: NxColors.gold, fontSize: 12)),
          ),
        if (end == null && !_reporting && _localResult?.endsMatch == true && _link != null)
          TextButton.icon(
            onPressed: () => unawaited(_submitResult(_link!, _localResult!)),
            icon: const Icon(Icons.refresh_rounded),
            label: Text(context.tx('تلاش دوباره برای ثبت نتیجه', 'Retry settlement')),
          ),
        const SizedBox(height: 22),
        FilledButton.icon(
          onPressed: () => Navigator.of(context).pop(),
          icon: Icon(widget.tournamentMatchId != null
              ? Icons.emoji_events_rounded
              : Icons.meeting_room_rounded),
          label: Text(widget.tournamentMatchId != null
              ? context.tx('بازگشت به تورنمنت', 'Back to the tournament')
              : context.tx('بازگشت به اتاق (بازی دوباره)',
                  'Back to the room (rematch)')),
        ),
      ]),
    );
  }
}
