import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import 'package:supabase_flutter/supabase_flutter.dart';

import '../../core/audio/music_service.dart';
import '../../core/online/online_service.dart';
import '../../core/profile/profile_service.dart';
import '../../core/settings/app_settings.dart';
import '../../core/ui2/anix_kit.dart';
import '../../core/lux/lux.dart';
import '../../core/ui2/nx2_widgets.dart';
import 'hub_search.dart';
import 'luxe_station_page.dart';
import 'luxe_worlds.dart';

/// ANIX PRO HUB — native Flutter rebuild of the "Station Luxe" reference:
/// black/gold scene with drifting dust, profile + live-rooms HUD, the ANIX PRO mark,
/// a 3D ring of world cards (swipe to rotate, tap the front card to enter) and the Luxe dock.
/// Every number on screen is real (profile/wallet from ProfileService, room count from the games backend).
class HubPage extends ConsumerStatefulWidget {
  const HubPage({super.key});

  @override
  ConsumerState<HubPage> createState() => _HubPageState();
}

class _HubPageState extends ConsumerState<HubPage> with SingleTickerProviderStateMixin {
  late final List<LuxeWorld> _worlds = buildLuxeWorlds();
  late final PageController _pc = PageController(viewportFraction: 0.6, initialPage: _worlds.length * 200);
  late final AnimationController _sheen =
      AnimationController(vsync: this, duration: const Duration(seconds: 6));
  int _idx = 0;
  int? _liveRooms;
  Timer? _poll;

  @override
  void initState() {
    super.initState();
    MusicService.instance.start();
    unawaited(_loadRooms());
    _poll = Timer.periodic(const Duration(seconds: 30), (_) => unawaited(_loadRooms()));
  }

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (LuxMotion.reduced(context)) {
      _sheen.stop();
    } else if (!_sheen.isAnimating) {
      _sheen.repeat();
    }
  }

  @override
  void dispose() {
    _poll?.cancel();
    _pc.dispose();
    _sheen.dispose();
    super.dispose();
  }

  /// Live public talk rooms + open game lobbies on the games project. Offline/guest -> hidden.
  Future<void> _loadRooms() async {
    final OnlineService o = ref.read(onlineProvider);
    if (!o.isOnline) return;
    final SupabaseClient? c = o.games;
    if (c == null || o.client?.auth.currentSession == null) return;
    try {
      int n = 0;
      try {
        n += ((await c.rpc('nx_talk_rooms')) as List<dynamic>? ?? const <dynamic>[]).length;
      } catch (_) {}
      final List<dynamic> lobbies = await c
          .from('nx_rooms')
          .select('id')
          .eq('status', 'lobby')
          .eq('is_private', false)
          .gte('updated_at', DateTime.now().toUtc().subtract(const Duration(minutes: 20)).toIso8601String())
          .limit(99);
      n += lobbies.length;
      if (mounted) setState(() => _liveRooms = n);
    } catch (_) {
      // Keep the last known value; the chip simply stays as it was.
    }
  }

  int _wrap(int page) => page % _worlds.length;

  void _enter(LuxeWorld w) => context.push(w.route);

  @override
  Widget build(BuildContext context) {
    final ProfileService p = ref.watch(profileProvider);
    final LuxeWorld cur = _worlds[_idx];
    final bool fa = context.isFa;
    final LuxPalette lp = LuxPalette.of(context);
    return Scaffold(
      backgroundColor: lp.bg,
      extendBody: true,
      bottomNavigationBar: const LuxDock(current: Nx2Tab.home),
      body: LuxScene(
        accent: cur.color,
        artOpacity: lp.isDark ? .6 : .14,
        child: SafeArea(
          bottom: false,
          child: LayoutBuilder(builder: (BuildContext ctx, BoxConstraints box) {
            final double ringH = math.min(box.maxHeight * 0.5, 450);
            return Column(children: <Widget>[
              _Hud(profile: p, liveRooms: _liveRooms),
              const SizedBox(height: 4),
              const _Brand(),
              const Spacer(),
              SizedBox(
                height: ringH,
                child: PageView.builder(
                  key: const ValueKey<String>('hub-ring'),
                  controller: _pc,
                  onPageChanged: (int i) => setState(() => _idx = _wrap(i)),
                  itemBuilder: (BuildContext c, int i) {
                    final LuxeWorld w = _worlds[_wrap(i)];
                    return AnimatedBuilder(
                      animation: _pc,
                      builder: (BuildContext c, Widget? child) {
                        double page = _pc.initialPage.toDouble();
                        if (_pc.hasClients && _pc.position.haveDimensions) page = _pc.page ?? page;
                        final double d = (i - page).clamp(-2.0, 2.0);
                        final double a = d.abs().clamp(0.0, 1.0);
                        final Matrix4 m = Matrix4.identity()
                          ..setEntry(3, 2, 0.0011)
                          ..translateByDouble(0.0, a * 26, -a * 120, 1.0)
                          ..rotateY(-d * 0.62 * (fa ? -1 : 1));
                        return Opacity(
                          opacity: d.abs() > 1.15 ? 0.0 : (1 - a * 0.45).clamp(0.0, 1.0),
                          child: Transform(alignment: Alignment.center, transform: m, child: child),
                        );
                      },
                      child: _WorldCard(
                        world: w,
                        sheen: _sheen,
                        onTap: () {
                          if (_wrap(i) == _idx) {
                            _enter(w);
                          } else {
                            _pc.animateToPage(i, duration: const Duration(milliseconds: 700), curve: LuxMotion.ease);
                          }
                        },
                      ),
                    );
                  },
                ),
              ),
              const SizedBox(height: 16),
              AnimatedSwitcher(
                duration: LuxMotion.normal,
                child: Padding(
                  key: ValueKey<String>(cur.k),
                  padding: const EdgeInsets.symmetric(horizontal: 28),
                  child: Column(children: <Widget>[
                    Text(cur.en, style: LuxType.latin.copyWith(color: lp.isDark ? lp.gold : lp.gold3)),
                    const SizedBox(height: 4),
                    Text(fa ? cur.fa : cur.en, style: LuxType.title.copyWith(fontSize: 26, color: lp.ink)),
                    const SizedBox(height: 4),
                    Text(fa ? cur.tagFa : cur.tagEn, textAlign: TextAlign.center, style: LuxType.label.copyWith(color: lp.mute)),
                  ]),
                ),
              ),
              const SizedBox(height: 14),
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: List<Widget>.generate(_worlds.length, (int i) {
                  final bool on = i == _idx;
                  return AnimatedContainer(
                    duration: LuxMotion.normal,
                    curve: LuxMotion.ease,
                    margin: const EdgeInsets.symmetric(horizontal: 3.5),
                    width: on ? 22 : 6,
                    height: 6,
                    decoration: BoxDecoration(color: on ? lp.gold : lp.faint, borderRadius: BorderRadius.circular(9)),
                  );
                }),
              ),
              SizedBox(height: 118 + MediaQuery.paddingOf(context).bottom),
            ]);
          }),
        ),
      ),
    );
  }
}

class _Brand extends StatelessWidget {
  const _Brand();

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return SizedBox(
      height: 92,
      child: Column(mainAxisSize: MainAxisSize.min, children: <Widget>[
        Expanded(
          child: DecoratedBox(
            decoration: BoxDecoration(boxShadow: <BoxShadow>[BoxShadow(color: p.gold.withValues(alpha: .25), blurRadius: 30, spreadRadius: -6)]),
            child: Image.asset('assets/branding/anix_logo.webp',
                cacheWidth: 300,
                errorBuilder: (_, __, ___) => Text('ANIX', style: LuxType.latinDisplay.copyWith(color: p.gold))),
          ),
        ),
        ShaderMask(
          shaderCallback: (Rect r) => LinearGradient(colors: <Color>[p.gold2, p.gold, p.gold3]).createShader(r),
          child: Text('P R O', style: LuxType.latin.copyWith(fontSize: 13, color: Colors.white, letterSpacing: 4)),
        ),
      ]),
    );
  }
}

class _WorldCard extends StatelessWidget {
  const _WorldCard({required this.world, required this.sheen, required this.onTap});
  final LuxeWorld world;
  final Animation<double> sheen;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final bool fa = context.isFa;
    final LuxPalette p = LuxPalette.of(context);
    return Center(
      child: AspectRatio(
        aspectRatio: 3 / 4.3,
        child: Semantics(
          button: true,
          label: fa ? world.fa : world.en,
          child: LuxPress(
            onTap: onTap,
            child: Container(
              decoration: BoxDecoration(
                borderRadius: BorderRadius.circular(LuxRadius.card),
                border: Border.all(color: p.gold2.withValues(alpha: .28)),
                boxShadow: <BoxShadow>[
                  BoxShadow(color: p.shadow.withValues(alpha: p.isDark ? 1 : .35), blurRadius: 60, offset: const Offset(0, 40), spreadRadius: -28),
                  BoxShadow(color: world.color.withValues(alpha: 0.45), blurRadius: 50, spreadRadius: -14),
                ],
                gradient: LinearGradient(
                  begin: Alignment.topCenter,
                  end: Alignment.bottomCenter,
                  colors: <Color>[Color.lerp(const Color(0xFF1C1A14), world.color, 0.22)!, const Color(0xFF07080C)],
                ),
              ),
              child: ClipRRect(
                borderRadius: BorderRadius.circular(LuxRadius.card),
                child: Stack(fit: StackFit.expand, children: <Widget>[
                  DecoratedBox(
                    decoration: BoxDecoration(
                      gradient: RadialGradient(
                          center: const Alignment(0, -1), radius: 1.1, colors: <Color>[world.color.withValues(alpha: 0.23), Colors.transparent]),
                    ),
                  ),
                  const DecoratedBox(
                    decoration: BoxDecoration(
                      gradient: LinearGradient(
                        begin: Alignment.topCenter,
                        end: Alignment.bottomCenter,
                        stops: <double>[0, 0.85],
                        colors: <Color>[Color(0x0D07080C), Color(0xD907080C)],
                      ),
                    ),
                  ),
                  Align(
                    alignment: const Alignment(0, -0.5),
                    child: FractionallySizedBox(
                      widthFactor: 0.74,
                      child: AspectRatio(
                        aspectRatio: 1,
                        child: Container(
                          decoration: BoxDecoration(
                            borderRadius: BorderRadius.circular(22),
                            border: Border.all(color: const Color(0x59D9B872), width: 1.2),
                            boxShadow: <BoxShadow>[BoxShadow(color: world.color.withValues(alpha: 0.45), blurRadius: 34, spreadRadius: -8)],
                          ),
                          child: ClipRRect(
                            borderRadius: BorderRadius.circular(21),
                            child: Image.asset('assets/luxe/icons/${world.icon}.webp',
                                fit: BoxFit.cover, cacheWidth: 360, errorBuilder: (_, __, ___) => const SizedBox()),
                          ),
                        ),
                      ),
                    ),
                  ),
                  Positioned(
                    left: 8,
                    right: 8,
                    bottom: 22,
                    child: Column(children: <Widget>[
                      Text(fa ? world.fa : world.en,
                          textAlign: TextAlign.center, style: LuxType.headline.copyWith(fontSize: 19, color: const Color(0xFFF4EAD2))),
                      if (fa) const SizedBox(height: 2),
                      if (fa) Text(world.en, style: LuxType.latin.copyWith(fontSize: 10, color: const Color(0xFFF6E7BD))),
                    ]),
                  ),
                  if (!LuxMotion.reduced(context))
                    IgnorePointer(
                      child: AnimatedBuilder(
                        animation: sheen,
                        builder: (BuildContext c, _) {
                          final double t = Curves.easeInOut.transform((sheen.value / 0.6).clamp(0.0, 1.0));
                          return FractionalTranslation(
                            translation: Offset(-1.2 + t * 2.4, 0),
                            child: const DecoratedBox(
                              decoration: BoxDecoration(
                                gradient: LinearGradient(
                                  begin: Alignment(-1, -0.5),
                                  end: Alignment(1, 0.5),
                                  stops: <double>[0.4, 0.5, 0.6],
                                  colors: <Color>[Colors.transparent, Color(0x24FFFFFF), Colors.transparent],
                                ),
                              ),
                            ),
                          );
                        },
                      ),
                    ),
                ]),
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _Hud extends ConsumerWidget {
  const _Hud({required this.profile, required this.liveRooms});
  final ProfileService profile;
  final int? liveRooms;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final LuxPalette lp = LuxPalette.of(context);
    final String name = profile.username.isEmpty ? context.tx('کاوشگر', 'Explorer') : profile.username;
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 10, 16, 0),
      child: Row(children: <Widget>[
        Semantics(
          button: true,
          label: context.tx('پروفایل', 'Profile'),
          child: LuxPress(
            onTap: () => _profileSheet(context, ref),
            child: Stack(clipBehavior: Clip.none, children: <Widget>[
              Container(
                width: 46,
                height: 46,
                padding: const EdgeInsets.all(1.6),
                decoration: BoxDecoration(shape: BoxShape.circle, gradient: LinearGradient(colors: <Color>[lp.gold2, lp.gold3, lp.gold2])),
                child: ClipOval(
                  child: ColoredBox(
                    color: const Color(0xFF0C0D12),
                    child: AnixFramedAvatar(
                        avatarId: profile.portrait, photoUrl: profile.avatarUrl, tier: anixTierForLevel(profile.level), size: 43),
                  ),
                ),
              ),
              PositionedDirectional(
                bottom: -4,
                end: -4,
                child: Container(
                  padding: const EdgeInsets.symmetric(horizontal: 5, vertical: 1),
                  decoration: BoxDecoration(gradient: lp.goldGradient, borderRadius: BorderRadius.circular(8)),
                  child: Text('${profile.level}', style: LuxType.latin.copyWith(fontSize: 11, letterSpacing: 0, color: lp.onGold)),
                ),
              ),
            ]),
          ),
        ),
        const SizedBox(width: 10),
        Expanded(
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, mainAxisSize: MainAxisSize.min, children: <Widget>[
            Text(name, maxLines: 1, overflow: TextOverflow.ellipsis, style: LuxType.bodyStrong.copyWith(color: lp.ink)),
            Text(context.tx(profile.rankTitleFa, profile.rankTitleEn), maxLines: 1, style: LuxType.caption.copyWith(color: lp.mute)),
          ]),
        ),
        Semantics(
          label: context.tx('سکه', 'Coins'),
          child: Nx2Stat(value: _fmt(profile.coins), onTap: () => context.push('/shop')),
        ),
        const SizedBox(width: 6),
        Semantics(
          label: context.tx('جم', 'Gems'),
          child: Nx2GemStat(gems: profile.gems, onTap: () => context.push('/shop/teams')),
        ),
        if (liveRooms != null) ...<Widget>[
          const SizedBox(width: 6),
          Semantics(
            label: context.tx('روم‌های فعال', 'Live rooms'),
            child: LuxChip(label: '$liveRooms', live: true, onTap: () => context.push('/rooms?tab=talk')),
          ),
        ],
        const SizedBox(width: 6),
        LuxIconButton(
          icon: Icons.search_rounded,
          size: 36,
          tooltip: context.tx('جستجو', 'Search'),
          onPressed: () => showSearch<void>(context: context, delegate: HubSearchDelegate(isFa: context.isFa)),
        ),
      ]),
    );
  }

  static String _fmt(int v) {
    if (v >= 1000000) return '${(v / 1000000).toStringAsFixed(1)}M';
    if (v >= 10000) return '${(v / 1000).toStringAsFixed(1)}K';
    final String s = '$v';
    return s.length > 3 ? '${s.substring(0, s.length - 3)},${s.substring(s.length - 3)}' : s;
  }

  void _profileSheet(BuildContext context, WidgetRef ref) {
    showLuxSheet<void>(context, builder: (BuildContext ctx) {
      return Consumer(builder: (BuildContext ctx, WidgetRef ref, _) {
        final ProfileService p = ref.watch(profileProvider);
        final LuxPalette lp = LuxPalette.of(ctx);
        void go(String r) {
          Navigator.of(ctx).pop();
          context.push(r);
        }

        return Column(crossAxisAlignment: CrossAxisAlignment.stretch, mainAxisSize: MainAxisSize.min, children: <Widget>[
          Row(children: <Widget>[
            AnixFramedAvatar(avatarId: p.portrait, photoUrl: p.avatarUrl, tier: anixTierForLevel(p.level), size: 56),
            const SizedBox(width: 12),
            Expanded(
              child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: <Widget>[
                Text(p.username.isEmpty ? ctx.tx('کاوشگر', 'Explorer') : p.username, style: LuxType.headline.copyWith(color: lp.ink)),
                Text('${ctx.tx('سطح', 'Level')} ${p.level} · ${ctx.tx(p.rankTitleFa, p.rankTitleEn)}',
                    style: LuxType.caption.copyWith(color: lp.mute, fontSize: 12)),
              ]),
            ),
          ]),
          const SizedBox(height: 14),
          LuxProgress(value: p.levelProgress, height: 6),
          const SizedBox(height: 6),
          Text('XP ${p.xpIntoLevel}', style: LuxType.caption.copyWith(color: lp.mute)),
          const SizedBox(height: 6),
          LuxOption(label: ctx.tx('سکه: ${p.coins}', 'Coins: ${p.coins}'), icon: Icons.toll_rounded, onTap: () => go('/shop')),
          LuxOption(label: ctx.tx('الماس: ${p.gems}', 'Gems: ${p.gems}'), icon: Icons.diamond_rounded, onTap: () => go('/shop/teams')),
          if (p.canClaimDaily)
            LuxOption(
                label: ctx.tx('دریافت جایزه‌ی امروز', 'Claim today\'s reward'),
                asset: 'assets/luxe/icons/r3c6.webp',
                onTap: () {
                  Navigator.of(ctx).pop();
                  luxeClaimDaily(context, ref);
                }),
          LuxOption(label: ctx.tx('پروفایل من', 'My profile'), asset: 'assets/luxe/icons/r4c4.webp', onTap: () => go('/profile/me')),
          LuxOption(label: ctx.tx('جایزه‌ها', 'Rewards'), asset: 'assets/luxe/icons/r5c2.webp', onTap: () => go('/rewards')),
          LuxOption(label: ctx.tx('تنظیمات', 'Settings'), asset: 'assets/luxe/icons/r4c2.webp', onTap: () => go('/settings')),
        ]);
      });
    });
  }
}
