import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../core/lux/lux.dart';
import '../../core/profile/profile_service.dart';
import '../../core/settings/app_settings.dart';
import '../../core/ui2/nx2_widgets.dart';
import '../../core/widgets/nx_widgets.dart';
import '../../features/admin/admin_access.dart';
import 'luxe_worlds.dart';

/// Opens a Station item: a real route, or a real in-app action.
Future<void> luxeOpen(BuildContext context, WidgetRef ref, LuxeItem it) async {
  if (it.action == 'daily') {
    await luxeClaimDaily(context, ref);
    return;
  }
  final String? r = it.route;
  if (r != null) context.push(r);
}

/// Claims today's reward through ProfileService (server-synced award path).
Future<void> luxeClaimDaily(BuildContext context, WidgetRef ref) async {
  final ProfileService p = ref.read(profileProvider);
  final AwardOutcome? o = p.claimDaily();
  if (!context.mounted) return;
  if (o == null) {
    LuxToast.show(context, context.tx('جایزه‌ی امروز را گرفته‌ای. فردا برگرد!', 'Already claimed today. Come back tomorrow!'),
        icon: Icons.schedule_rounded);
    return;
  }
  await showAwardOutcome(context, o);
}

Nx2Tab luxeDockTab(String k) => switch (k) {
      'games' => Nx2Tab.games,
      'social' || 'chat' => Nx2Tab.social,
      'compete' || 'tournaments' || 'events' => Nx2Tab.tournaments,
      'profile' => Nx2Tab.profile,
      'shop' => Nx2Tab.shop,
      'oracle' => Nx2Tab.oracle,
      'rewards' => Nx2Tab.rewards,
      _ => Nx2Tab.home,
    };

/// A Station (category hub): medallion header, gold pill tabs, and a two-column grid of glass tiles. Used for Games,
/// Social, Compete, Shop and Profile; every tile opens a real screen.
class LuxeStationPage extends ConsumerStatefulWidget {
  const LuxeStationPage({super.key, required this.k, this.initialTab = 0});
  final String k;
  final int initialTab;

  @override
  ConsumerState<LuxeStationPage> createState() => _LuxeStationPageState();
}

class _LuxeStationPageState extends ConsumerState<LuxeStationPage> {
  late int _tab = widget.initialTab;

  @override
  Widget build(BuildContext context) {
    final LuxeWorld? w = luxeWorld(widget.k);
    if (w == null) {
      return LuxPage(
        title: context.tx('پیدا نشد', 'Not found'),
        body: LuxEmpty(title: context.tx('این بخش پیدا نشد', 'Section not found'), icon: Icons.explore_off_rounded),
      );
    }
    final bool staff = ref.watch(adminAccessProvider).isAdmin;
    final List<String> tabs = context.isFa ? w.tabsFa : w.tabsEn;
    final int tab = _tab.clamp(0, tabs.length - 1).toInt();
    final List<LuxeItem> items = w.items.where((LuxeItem i) => i.tab == tab && (!i.staffOnly || staff)).toList();
    final LuxPalette p = LuxPalette.of(context);
    return LuxPage(
      key: ValueKey<String>('station-${w.k}'),
      title: context.tx(w.fa, w.en),
      kicker: w.en,
      iconAsset: 'assets/luxe/icons/${w.icon}.webp',
      accent: w.color,
      showBack: Navigator.of(context).canPop(),
      bottom: LuxDock(current: luxeDockTab(w.k)),
      tabs: tabs.length > 1
          ? LuxPillTabs(labels: tabs, index: tab, onChanged: (int i) => setState(() => _tab = i))
          : null,
      body: AnimatedSwitcher(
        duration: LuxMotion.normal,
        switchInCurve: LuxMotion.ease,
        transitionBuilder: (Widget c, Animation<double> a) => FadeTransition(
          opacity: a,
          child: SlideTransition(position: Tween<Offset>(begin: const Offset(0, .03), end: Offset.zero).animate(a), child: c),
        ),
        child: ListView(
          key: ValueKey<int>(tab),
          padding: const EdgeInsets.fromLTRB(LuxSpace.page, 6, LuxSpace.page, 24),
          physics: const BouncingScrollPhysics(),
          children: <Widget>[
            Padding(
              padding: const EdgeInsets.only(bottom: 12, left: 4, right: 4),
              child: Text(context.tx(w.tagFa, w.tagEn), style: LuxType.label.copyWith(color: p.mute)),
            ),
            if (items.isEmpty)
              LuxEmpty(title: context.tx('چیزی اینجا نیست', 'Nothing here yet'), icon: Icons.inbox_rounded)
            else
              LuxTileGrid(children: <Widget>[
                for (int i = 0; i < items.length; i++)
                  LuxTile(
                    key: ValueKey<String>('station-item-${items[i].route ?? items[i].action}'),
                    index: i,
                    title: context.tx(items[i].fa, items[i].en),
                    subtitle: items[i].subFa == null ? null : context.tx(items[i].subFa!, items[i].subEn ?? items[i].subFa!),
                    asset: items[i].image,
                    accent: w.color,
                    badge: items[i].online ? 'ONLINE' : (items[i].staffOnly ? 'STAFF' : null),
                    icon: Icons.auto_awesome_rounded,
                    onTap: () => luxeOpen(context, ref, items[i]),
                  )._withMedallion(items[i].icon),
              ]),
          ],
        ),
      ),
    );
  }
}

extension on LuxTile {
  /// Tiles that use the owner's luxe icon art show it as a jewel medallion instead of a plain orb.
  Widget _withMedallion(String? icon) {
    if (icon == null) return this;
    return _MedallionTile(tile: this, icon: icon);
  }
}

class _MedallionTile extends StatelessWidget {
  const _MedallionTile({required this.tile, required this.icon});
  final LuxTile tile;
  final String icon;
  @override
  Widget build(BuildContext context) {
    final LuxPalette p = LuxPalette.of(context);
    final Widget card = LuxPanel(
      onTap: tile.onTap,
      padding: const EdgeInsets.fromLTRB(12, 18, 12, 16),
      child: Column(mainAxisSize: MainAxisSize.min, children: <Widget>[
        Stack(clipBehavior: Clip.none, children: <Widget>[
          LuxMedallion(asset: 'assets/luxe/icons/$icon.webp', size: 70, glow: tile.accent),
          if (tile.badge != null)
            PositionedDirectional(
              top: -4,
              end: -14,
              child: Container(
                padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 2),
                decoration: BoxDecoration(gradient: p.goldGradient, borderRadius: BorderRadius.circular(99)),
                child: Text(tile.badge!, style: LuxType.caption.copyWith(color: p.onGold, fontWeight: FontWeight.w700, fontSize: 9.5)),
              ),
            ),
        ]),
        const SizedBox(height: 12),
        Text(tile.title,
            textAlign: TextAlign.center, maxLines: 1, overflow: TextOverflow.ellipsis, style: LuxType.bodyStrong.copyWith(color: p.ink, fontSize: 14.5)),
        if (tile.subtitle != null) ...<Widget>[
          const SizedBox(height: 3),
          Text(tile.subtitle!,
              textAlign: TextAlign.center,
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
              style: LuxType.caption.copyWith(color: p.mute, fontSize: 10.5)),
        ],
      ]),
    );
    if (LuxMotion.reduced(context)) return card;
    return TweenAnimationBuilder<double>(
      tween: Tween<double>(begin: 0, end: 1),
      duration: Duration(milliseconds: 520 + 70 * tile.index.clamp(0, 10)),
      curve: LuxMotion.ease,
      builder: (_, double t, Widget? c) => Opacity(opacity: t, child: Transform.translate(offset: Offset(0, 16 * (1 - t)), child: c)),
      child: card,
    );
  }
}
