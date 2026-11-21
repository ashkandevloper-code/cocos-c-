import 'dart:math' as math;
import 'dart:ui' show ImageFilter;

import 'package:flutter/material.dart';

import '../audio/sfx_service.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../profile/profile_service.dart';
import '../settings/app_settings.dart';
import '../lux/lux.dart';
import 'anix_kit.dart';
import 'nx2_assets.dart';
import '../../worlds/oracle_core/persona/oracle_tour.dart';
import '../../worlds/hub/hub_search.dart';

/// NX2 colour tokens, re-based on the ANIX PRO Station Luxe palette (deep black, gold, champagne).
class Nx2 {
  Nx2._();
  static const Color deep = Color(0xFF07080C);
  static const Color night = Color(0xFF11100C);
  static const Color plum = Color(0xFF1C1912);
  static const Color violet = Color(0xFF8A6A2C);
  static const Color magenta = Color(0xFFD9B872);
  static const Color cyan = Color(0xFFF6E7BD);
  static const Color gold = Color(0xFFD9B872);
  static const Color goldDeep = Color(0xFF8A6A2C);
  static const Color text = Color(0xFFF4EAD2);
  static const Color muted = Color(0xFFB9AE95);
  static const Color success = Color(0xFF4BE3A0);
  static const Color danger = Color(0xFFFF5A7A);

  static const LinearGradient neon =
      LinearGradient(colors: <Color>[cyan, violet, magenta]);
  static const LinearGradient goldGrad =
      LinearGradient(colors: <Color>[Color(0xFFF6E7BD), gold, goldDeep]);

  static const TextStyle title = TextStyle(
      fontSize: 18,
      fontWeight: FontWeight.w900,
      letterSpacing: 0.8,
      color: text,
      shadows: <Shadow>[Shadow(color: Color(0xAA8B3DFF), blurRadius: 12)]);
  static const TextStyle label =
      TextStyle(fontSize: 12, fontWeight: FontWeight.w800, color: text);
}

/// Scene art behind a page (kept for call-site compatibility; every scene now renders the Station Luxe backdrop with
/// its own artwork faded in).
enum Nx2Scene { castle, room, hub, social, arcade, voiceStage, trophyHall, panorama }

String _sceneArt(Nx2Scene s) => switch (s) {
      Nx2Scene.castle || Nx2Scene.hub || Nx2Scene.panorama => 'assets/luxe/hub.webp',
      Nx2Scene.room => Nx2Assets.bgBgRoom,
      Nx2Scene.social => AnixAssets.bgSocial,
      Nx2Scene.arcade => AnixAssets.bgArcade,
      Nx2Scene.voiceStage => AnixAssets.sceneVoiceStage,
      Nx2Scene.trophyHall => AnixAssets.sceneTrophyHall,
    };

/// Page backdrop: the Station Luxe scene (warm light, faded art, gold dust) in night and day.
class Nx2Background extends StatelessWidget {
  const Nx2Background({super.key, this.child, this.room = false, this.dim = 0.45, this.scene});
  final Widget? child;
  final bool room;
  final double dim;
  final Nx2Scene? scene;

  @override
  Widget build(BuildContext context) =>
      LuxScene(art: _sceneArt(scene ?? (room ? Nx2Scene.room : Nx2Scene.hub)), child: child);
}

/// Legacy painter (chamfered frame) kept only for old call sites; draws a rounded Luxe hairline frame.
class Nx2FramePainter extends CustomPainter {
  Nx2FramePainter({
    required this.cut,
    this.fill,
    this.border = Nx2.goldGrad,
    this.strokeWidth = 1.2,
    this.glow = 0.0,
    this.gems = false,
  });
  final double cut;
  final Gradient? fill;
  final Gradient border;
  final double strokeWidth;
  final double glow;
  final bool gems;

  static Path chamfer(Rect r, double c) =>
      Path()..addRRect(RRect.fromRectAndRadius(r, Radius.circular(math.min(c * 1.6, math.min(r.width, r.height) / 2))));

  @override
  void paint(Canvas canvas, Size size) {
    final Rect r = Offset.zero & size;
    final Path p = chamfer(r.deflate(strokeWidth / 2), cut);
    if (fill != null) canvas.drawPath(p, Paint()..shader = fill!.createShader(r));
    canvas.drawPath(
        p,
        Paint()
          ..style = PaintingStyle.stroke
          ..strokeWidth = strokeWidth
          ..shader = border.createShader(r));
  }

  @override
  bool shouldRepaint(covariant Nx2FramePainter old) => old.cut != cut || old.fill != fill || old.border != border;
}

/// Glass panel (Station Luxe): frosted glass, hairline gold border; [gold] = the featured / highlighted panel.
class Nx2Panel extends StatelessWidget {
  const Nx2Panel({
    super.key,
    required this.child,
    this.padding = const EdgeInsets.all(14),
    this.onTap,
    this.cut = 14,
    this.gold = false,
    this.tint,
    this.borderColor,
    this.glow = 0.35,
    this.gems = false,
  });
  final Widget child;
  final EdgeInsetsGeometry padding;
  final VoidCallback? onTap;
  final double cut;
  final bool gold;
  final Color? tint;
  final Color? borderColor;
  final double glow;
  final bool gems;

  @override
  Widget build(BuildContext context) => LuxPanel(
        padding: padding,
        radius: math.max(18, cut * 1.6),
        highlight: gold || gems,
        glow: tint ?? borderColor,
        onTap: onTap == null
            ? null
            : () {
                NxSfx.instance.play('tick', gain: 0.6);
                onTap!();
              },
        child: child,
      );
}

/// Scale-down-on-press with a light haptic and the UI tick.
class _Pressable extends StatelessWidget {
  const _Pressable({required this.onTap, required this.child});
  final VoidCallback onTap;
  final Widget child;
  @override
  Widget build(BuildContext context) => LuxPress(
      onTap: () {
        NxSfx.instance.play('tick', gain: 0.6);
        onTap();
      },
      child: child);
}

/// Public wrapper of the press animation for art tiles.
class Nx2Pressable extends StatelessWidget {
  const Nx2Pressable({super.key, required this.onTap, required this.child});
  final VoidCallback onTap;
  final Widget child;
  @override
  Widget build(BuildContext context) => _Pressable(onTap: onTap, child: child);
}

enum Nx2ButtonKind { pink, violet, cyan }

/// App button: 3D gold (primary), glass (secondary) or ghost — the Station Luxe button family.
class Nx2Button extends StatelessWidget {
  const Nx2Button({
    super.key,
    required this.label,
    required this.onTap,
    this.icon,
    this.kind = Nx2ButtonKind.violet,
    this.height = 52,
    this.fontSize = 16,
    this.loading = false,
  });
  final String label;
  final VoidCallback? onTap;
  final IconData? icon;
  final Nx2ButtonKind kind;
  final double height;
  final double fontSize;
  final bool loading;

  @override
  Widget build(BuildContext context) => LuxButton(
        label: label,
        onPressed: onTap,
        icon: icon,
        busy: loading,
        expand: true,
        compact: height < 46,
        kind: switch (kind) {
          Nx2ButtonKind.pink => LuxButtonKind.gold,
          Nx2ButtonKind.violet => LuxButtonKind.gold,
          Nx2ButtonKind.cyan => LuxButtonKind.glass,
        },
      );
}

/// Title plate: hairline gold rules either side of the title (Station Luxe).
class Nx2Banner extends StatelessWidget {
  const Nx2Banner({super.key, required this.text, this.height = 58, this.wings = false});
  final String text;
  final double height;
  final bool wings;

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return SizedBox(
      height: height,
      child: Row(children: <Widget>[
        Expanded(child: Container(height: 1, decoration: BoxDecoration(gradient: LinearGradient(colors: <Color>[p.gold.withValues(alpha: 0), p.gold])))),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 14),
          child: Text(text, style: LuxType.title.copyWith(color: p.ink, fontSize: height * .34)),
        ),
        Expanded(child: Container(height: 1, decoration: BoxDecoration(gradient: LinearGradient(colors: <Color>[p.gold, p.gold.withValues(alpha: 0)])))),
      ]),
    );
  }
}

/// Avatar initial inside a gold ring.
class Nx2AvatarRing extends StatelessWidget {
  const Nx2AvatarRing({super.key, this.size = 64, this.initial = '?', this.purple = false});
  final double size;
  final String initial;
  final bool purple;

  @override
  Widget build(BuildContext context) => LuxAvatar(size: size * .7, initials: initial);
}

/// Thin gold progress bar.
class Nx2Progress extends StatelessWidget {
  const Nx2Progress({super.key, required this.value, this.height = 9, this.gold = false});
  final double value;
  final double height;
  final bool gold;

  @override
  Widget build(BuildContext context) => LuxProgress(value: value, height: math.min(height, 6));
}

/// Gem colour (الماس) used by every gem balance / price.
const Color kGemColor = Color(0xFF8FD3E8);

/// Gem balance chip.
class Nx2GemStat extends StatelessWidget {
  const Nx2GemStat({super.key, required this.gems, this.onTap});
  final int gems;
  final VoidCallback? onTap;
  @override
  Widget build(BuildContext context) => Nx2Stat(value: '$gems', icon: Icons.diamond_rounded, color: kGemColor, onTap: onTap);
}

/// Currency / level chip (glass pill, jewel icon).
class Nx2Stat extends StatelessWidget {
  const Nx2Stat({super.key, required this.value, this.icon = Icons.toll_rounded, this.color = Nx2.gold, this.onTap, this.wiggle});
  final String value;
  final IconData icon;
  final Color color;
  final VoidCallback? onTap;

  /// When it ticks, the pill gives a little shake (Oracle pokes the coins).
  final Listenable? wiggle;

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    final Widget pill = Container(
      padding: const EdgeInsetsDirectional.fromSTEB(5, 4, 11, 4),
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(99),
        gradient: p.glass,
        color: p.isDark ? null : p.panel,
        border: Border.all(color: p.line),
      ),
      child: Row(mainAxisSize: MainAxisSize.min, children: <Widget>[
        Container(
          width: 20,
          height: 20,
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            gradient: RadialGradient(center: const Alignment(-.35, -.4), colors: <Color>[Color.lerp(color, Colors.white, .55)!, color, Color.lerp(color, Colors.black, .35)!]),
          ),
          child: Icon(icon, size: 12, color: const Color(0xFF241A08)),
        ),
        const SizedBox(width: 6),
        Text(value, style: LuxType.label.copyWith(color: p.ink, fontWeight: FontWeight.w700)),
      ]),
    );
    Widget w = pill;
    if (wiggle != null) w = _Wiggle(listenable: wiggle!, child: pill);
    if (onTap != null) w = _Pressable(onTap: onTap!, child: w);
    return w;
  }
}

class _Wiggle extends StatefulWidget {
  const _Wiggle({required this.listenable, required this.child});
  final Listenable listenable;
  final Widget child;
  @override
  State<_Wiggle> createState() => _WiggleState();
}

class _WiggleState extends State<_Wiggle> with SingleTickerProviderStateMixin {
  late final AnimationController _c = AnimationController(vsync: this, duration: const Duration(milliseconds: 600));
  @override
  void initState() {
    super.initState();
    widget.listenable.addListener(_go);
  }

  void _go() {
    if (mounted) _c.forward(from: 0);
  }

  @override
  void dispose() {
    widget.listenable.removeListener(_go);
    _c.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
        animation: _c,
        builder: (_, Widget? child) {
          final double t = _c.value;
          final double a = math.sin(t * math.pi * 6) * (1 - t) * 0.18;
          return Transform.rotate(angle: a, child: Transform.scale(scale: 1 + 0.12 * math.sin(t * math.pi), child: child));
        },
        child: widget.child,
      );
}

/// Art tile (medallion art + label).
class Nx2Tile extends StatelessWidget {
  const Nx2Tile({super.key, required this.asset, required this.label, required this.onTap, this.badge = false, this.size = 74});
  final String asset;
  final String label;
  final VoidCallback onTap;
  final bool badge;
  final double size;

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return _Pressable(
      onTap: onTap,
      child: Column(mainAxisSize: MainAxisSize.min, children: <Widget>[
        SizedBox(
          width: size,
          height: size,
          child: Stack(clipBehavior: Clip.none, children: <Widget>[
            Positioned.fill(child: LuxMedallion(asset: asset, size: size)),
            if (badge)
              PositionedDirectional(
                top: 2,
                end: 2,
                child: Container(
                  width: 12,
                  height: 12,
                  decoration: BoxDecoration(color: p.danger, shape: BoxShape.circle, border: Border.all(color: p.bg, width: 2)),
                ),
              ),
          ]),
        ),
        const SizedBox(height: 6),
        Text(label, maxLines: 1, overflow: TextOverflow.ellipsis, textAlign: TextAlign.center, style: LuxType.label.copyWith(color: p.ink)),
      ]),
    );
  }
}

/// Section header: gold hairline + title + optional action.
class Nx2SectionHeader extends StatelessWidget {
  const Nx2SectionHeader({super.key, required this.title, this.action, this.onAction});
  final String title;
  final String? action;
  final VoidCallback? onAction;

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    return LuxSectionTitle(
      title,
      trailing: action == null
          ? null
          : GestureDetector(
              onTap: onAction,
              child: Text(action!, style: LuxType.label.copyWith(color: p.isDark ? p.gold2 : p.gold3, fontWeight: FontWeight.w700)),
            ),
    );
  }
}

/// Destinations of the bottom dock: Home · Social · PLAY · Compete · Profile (+ shop, rewards, oracle reached from
/// the hub and the profile station).
enum Nx2Tab { home, social, oracle, games, shop, tournaments, rewards, profile }

String nx2TabPath(Nx2Tab t) => switch (t) {
      Nx2Tab.home => '/hub',
      Nx2Tab.social => '/social',
      Nx2Tab.oracle => '/oracle',
      Nx2Tab.games => '/play',
      Nx2Tab.shop => '/shop',
      Nx2Tab.tournaments => '/compete',
      Nx2Tab.rewards => '/rewards',
      Nx2Tab.profile => '/profile',
    };

/// Goes to a dock destination: Home is the root; other tabs replace each other on top of it.
void nx2GoTab(BuildContext context, Nx2Tab tab, Nx2Tab? current) {
  if (tab == current) return;
  final String path = nx2TabPath(tab);
  if (tab == Nx2Tab.home) {
    context.go(path);
  } else if (current == null || current == Nx2Tab.home) {
    context.push(path);
  } else {
    context.pushReplacement(path);
  }
}

/// The Station Luxe dock (see [LuxDock]).
class Nx2Dock extends StatelessWidget {
  const Nx2Dock({super.key, required this.current});
  final Nx2Tab? current;

  @override
  Widget build(BuildContext context) => LuxDock(current: current);
}

/// Floating glass dock: Home · Social · [PLAY medallion] · Compete · Profile.
class LuxDock extends StatelessWidget {
  const LuxDock({super.key, required this.current});
  final Nx2Tab? current;

  Nx2Tab? get _on => switch (current) {
        Nx2Tab.shop || Nx2Tab.rewards || Nx2Tab.oracle => Nx2Tab.home,
        _ => current,
      };

  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    final double bottom = MediaQuery.paddingOf(context).bottom;
    final Nx2Tab? on = _on;

    Widget cell(String id, Nx2Tab tab, String fa, String en, String icon, IconData glyph) {
      final bool sel = on == tab;
      return Expanded(
        child: OracleTourTarget(
          id: id,
          child: Semantics(
            selected: sel,
            button: true,
            label: context.tx(fa, en),
            child: LuxPress(
              onTap: () {
                NxSfx.instance.play('tick', gain: 0.6);
                nx2GoTab(context, tab, current);
              },
              child: AnimatedContainer(
                duration: LuxMotion.normal,
                curve: LuxMotion.ease,
                padding: const EdgeInsets.symmetric(vertical: 4),
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(22),
                  gradient: sel
                      ? LinearGradient(
                          begin: Alignment.topCenter,
                          end: Alignment.bottomCenter,
                          colors: <Color>[p.gold.withValues(alpha: .22), p.gold.withValues(alpha: .03)])
                      : null,
                  border: Border.all(color: sel ? p.lineStrong : Colors.transparent),
                ),
                child: ExcludeSemantics(
                  child: Column(mainAxisSize: MainAxisSize.min, children: <Widget>[
                    SizedBox(
                      width: 30,
                      height: 30,
                      child: DecoratedBox(
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          boxShadow: sel ? <BoxShadow>[BoxShadow(color: p.gold.withValues(alpha: .55), blurRadius: 12)] : const <BoxShadow>[],
                        ),
                        child: ClipOval(
                          child: Image.asset('assets/luxe/icons/$icon.webp',
                              cacheWidth: 96,
                              fit: BoxFit.cover,
                              errorBuilder: (_, __, ___) => Icon(glyph, color: sel ? p.gold2 : p.mute, size: 22)),
                        ),
                      ),
                    ),
                    const SizedBox(height: 2),
                    SizedBox(
                      height: 14,
                      child: FittedBox(
                        fit: BoxFit.scaleDown,
                        child: Text(context.tx(fa, en),
                            maxLines: 1,
                            style: LuxType.caption.copyWith(
                                fontSize: 10, color: sel ? (p.isDark ? p.gold2 : p.gold3) : p.mute, fontWeight: sel ? FontWeight.w700 : FontWeight.w500)),
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

    const double center = 78;
    final bool playOn = on == Nx2Tab.games;
    return OracleTourTarget(
      id: 'dock',
      child: SizedBox(
        height: 72 + bottom + 30,
        child: Stack(clipBehavior: Clip.none, children: <Widget>[
          Positioned(
            left: 14,
            right: 14,
            bottom: bottom + 10,
            height: 70,
            child: ClipRRect(
              borderRadius: BorderRadius.circular(30),
              child: BackdropFilter(
                filter: ImageFilter.blur(sigmaX: 22, sigmaY: 22),
                child: Container(
                  padding: const EdgeInsets.all(5),
                  decoration: BoxDecoration(
                    color: p.isDark ? const Color(0x8C0E0E12) : p.panel.withValues(alpha: .86),
                    borderRadius: BorderRadius.circular(30),
                    border: Border.all(color: p.line),
                  ),
                  child: Row(children: <Widget>[
                    cell('dock.home', Nx2Tab.home, 'خانه', 'Home', 'r1c6', Icons.home_rounded),
                    cell('dock.social', Nx2Tab.social, 'اجتماعی', 'Social', 'r3c3', Icons.chat_bubble_rounded),
                    const SizedBox(width: center),
                    cell('dock.tournaments', Nx2Tab.tournaments, 'رقابت', 'Compete', 'r2c5', Icons.emoji_events_rounded),
                    cell('dock.profile', Nx2Tab.profile, 'پروفایل', 'Profile', 'r4c4', Icons.person_rounded),
                  ]),
                ),
              ),
            ),
          ),
          Positioned(
            bottom: bottom + 24,
            left: 0,
            right: 0,
            child: Center(
              child: OracleTourTarget(
                id: 'dock.games',
                child: LuxPress(
                  onTap: () {
                    NxSfx.instance.play('tick', gain: 0.6);
                    nx2GoTab(context, Nx2Tab.games, current);
                  },
                  child: Semantics(
                    button: true,
                    selected: playOn,
                    label: context.tx('بازی', 'PLAY'),
                    child: Container(
                      width: center,
                      height: center,
                      decoration: BoxDecoration(
                        shape: BoxShape.circle,
                        gradient: const RadialGradient(center: Alignment(0, -.4), colors: <Color>[Color(0xFF2C2619), Color(0xFF0A0A0D)], stops: <double>[0, .72]),
                        border: Border.all(color: p.gold, width: 1.5),
                        boxShadow: <BoxShadow>[
                          BoxShadow(color: p.bg.withValues(alpha: .9), spreadRadius: 5),
                          BoxShadow(color: p.gold.withValues(alpha: playOn ? .85 : .5), blurRadius: playOn ? 40 : 24),
                        ],
                      ),
                      child: ExcludeSemantics(
                        child: Column(mainAxisAlignment: MainAxisAlignment.center, children: <Widget>[
                          Image.asset('assets/luxe/icons/r4c5.webp',
                              width: 44,
                              height: 40,
                              cacheWidth: 140,
                              errorBuilder: (_, __, ___) => const Icon(Icons.sports_esports_rounded, color: Nx2.cyan)),
                          Text(context.tx('بازی', 'PLAY'),
                              style: LuxType.latin.copyWith(fontSize: 10, letterSpacing: 1.6, color: const Color(0xFFF6E7BD))),
                        ]),
                      ),
                    ),
                  ),
                ),
              ),
            ),
          ),
        ]),
      ),
    );
  }
}

/// Top bar for world pages: round back / home button, title, search, wallet chips (Station Luxe header).
class Nx2TopBar extends ConsumerWidget implements PreferredSizeWidget {
  const Nx2TopBar({super.key, required this.title, this.actions, this.showWallet = true});
  final String title;
  final List<Widget>? actions;
  final bool showWallet;

  @override
  Size get preferredSize => const Size.fromHeight(64);

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final ProfileService p = ref.watch(profileProvider);
    final LuxPalette lp = LuxPalette.of(context);
    final bool canPop = Navigator.of(context).canPop();
    return SafeArea(
      bottom: false,
      child: SizedBox(
        height: 64,
        child: Row(children: <Widget>[
          const SizedBox(width: 12),
          if (canPop)
            LuxIconButton(
              icon: Directionality.of(context) == TextDirection.rtl ? Icons.arrow_forward_rounded : Icons.arrow_back_rounded,
              tooltip: context.tx('بازگشت', 'Back'),
              onPressed: () => Navigator.of(context).maybePop(),
            )
          else
            LuxIconButton(icon: Icons.home_rounded, tooltip: context.tx('خانه', 'Home'), onPressed: () => context.go('/hub')),
          const SizedBox(width: 10),
          Expanded(
            child: Text(title, maxLines: 1, overflow: TextOverflow.ellipsis, style: LuxType.title.copyWith(color: lp.ink, fontSize: 19)),
          ),
          LuxIconButton(
            icon: Icons.search_rounded,
            size: 38,
            tooltip: context.tx('جستجوی سراسری', 'Open global search'),
            onPressed: () => showSearch<void>(
              context: context,
              delegate: HubSearchDelegate(isFa: Localizations.localeOf(context).languageCode == 'fa'),
            ),
          ),
          ...?actions,
          if (showWallet) ...<Widget>[
            const SizedBox(width: 6),
            Nx2Stat(value: '${p.coins}'),
            const SizedBox(width: 4),
            Nx2GemStat(gems: p.gems),
          ],
          const SizedBox(width: 12),
        ]),
      ),
    );
  }
}

/// Small round icon button (Station Luxe).
class Nx2IconButton extends StatelessWidget {
  const Nx2IconButton({super.key, required this.icon, required this.onTap, required this.tooltip});
  final IconData icon;
  final VoidCallback onTap;
  final String tooltip;
  @override
  Widget build(BuildContext context) => LuxIconButton(icon: icon, onPressed: onTap, tooltip: tooltip);
}

/// Speech bubble for the mascot / hub logo.
class Nx2Bubble extends StatelessWidget {
  const Nx2Bubble({super.key, required this.text, this.maxWidth = 220});
  final String text;
  final double maxWidth;
  @override
  Widget build(BuildContext context) => AnixPopover(maxWidth: maxWidth, child: Text(text));
}

/// Gold pill tabs bound to the nearest DefaultTabController.
class Nx2SegmentTabs extends StatelessWidget {
  const Nx2SegmentTabs({super.key, required this.labels});
  final List<String> labels;

  @override
  Widget build(BuildContext context) => SizedBox(
        height: 42,
        child: TabBar(
          isScrollable: true,
          tabAlignment: TabAlignment.start,
          labelPadding: const EdgeInsets.symmetric(horizontal: 16),
          padding: EdgeInsets.zero,
          tabs: <Widget>[for (final String l in labels) Tab(height: 38, text: l)],
        ),
      );
}
