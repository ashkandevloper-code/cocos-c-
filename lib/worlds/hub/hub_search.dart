import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../core/online/online_service.dart';
import '../../core/online/social_api.dart';
import '../../core/settings/app_settings.dart';
import '../../core/widgets/profile_preview.dart';
import '../../core/widgets/user_avatar.dart';
import '../../core/theme/nx_theme.dart';
import '../../games/game_registry.dart';
import '../world_registry.dart';

class _Hit {
  const _Hit(this.title, this.subtitle, this.icon, this.color, this.route,
      [this.searchAliases = const <String>[]]);
  final String title;
  final String subtitle;
  final IconData icon;
  final Color color;
  final String route;
  final List<String> searchAliases;
}

/// Global search across worlds, games and features (STEP_12).
class HubSearchDelegate extends SearchDelegate<void> {
  HubSearchDelegate({required this.isFa})
      : super(searchFieldLabel: isFa ? 'جستجو…' : 'Global search');
  final bool isFa;

  List<_Hit> _all() {
    final hits = <_Hit>[
      for (final w in kWorlds)
        _Hit(isFa ? w.nameFa : w.name, isFa ? w.taglineFa : w.taglineEn, w.icon,
            w.palette.primary, '/world/${w.id}'),
      for (final g in kGames)
        _Hit(isFa ? g.nameFa : g.nameEn, isFa ? g.taglineFa : g.taglineEn,
            Icons.sports_esports_rounded, g.accent, '/arcade/${g.id}',
            <String>[g.nameFa, g.nameEn, g.id]),
      _Hit(
          isFa ? 'تنظیمات' : 'Settings',
          isFa ? 'کیفیت، کاهش حرکت، زبان' : 'Quality, reduced motion, language',
          Icons.settings_rounded,
          NxColors.iceWhite,
          '/settings'),
      _Hit(
          isFa ? 'جایزه‌ی روزانه و چالش‌ها' : 'Daily reward & challenges',
          isFa ? 'در مدار شخصی' : 'In Personal Orbit',
          Icons.card_giftcard_rounded,
          NxColors.gold,
          '/world/personal_orbit'),
      _Hit(
          isFa ? 'دستاوردها' : 'Achievements',
          isFa ? 'نشان‌ها و جام‌ها' : 'Badges & trophies',
          Icons.emoji_events_rounded,
          NxColors.gold,
          '/world/personal_orbit'),
      _Hit(
          isFa ? 'صندوق مسابقات' : 'Tournament inbox',
          isFa ? 'دعوت‌ها و اعلان‌های مسابقات' : 'Invites and tournament alerts',
          Icons.inbox_rounded,
          NxColors.electricBlue,
          '/tournaments/inbox'),
    ];
    return hits;
  }

  List<_Hit> _filter() {
    final q = _searchKey(query);
    if (q.isEmpty) return _all();
    return _all()
        .where((h) => <String>[h.title, h.subtitle, h.route, ...h.searchAliases]
            .any((value) => _searchKey(value).contains(q)))
        .toList();
  }

  /// Match Persian/Arabic input consistently, including optional ZWNJ and
  /// whitespace differences such as «تخته نرد» vs «تخته‌نرد».
  String _searchKey(String value) => value
      .toLowerCase()
      .replaceAll('ي', 'ی')
      .replaceAll('ى', 'ی')
      .replaceAll('ك', 'ک')
      .replaceAll(RegExp(r'[\s\u200c\u200d]+'), '');

  @override
  ThemeData appBarTheme(BuildContext context) {
    final t = Theme.of(context);
    final visual = NxThemeColors.of(context);
    return t.copyWith(
      scaffoldBackgroundColor: visual.background,
      appBarTheme: t.appBarTheme.copyWith(
          backgroundColor: visual.backgroundMid, foregroundColor: visual.text),
      inputDecorationTheme:
          t.inputDecorationTheme.copyWith(fillColor: visual.surfaceRaised),
    );
  }

  @override
  List<Widget>? buildActions(BuildContext context) => [
        if (query.isNotEmpty)
          IconButton(onPressed: () => query = '', icon: const Icon(Icons.clear))
      ];

  @override
  Widget? buildLeading(BuildContext context) => IconButton(
      onPressed: () => close(context, null), icon: const BackButtonIcon());

  @override
  Widget buildResults(BuildContext context) => _list(context);

  @override
  Widget buildSuggestions(BuildContext context) => _list(context);

  Widget _list(BuildContext context) {
    final hits = _filter();
    final visual = NxThemeColors.of(context);
    return ListView.separated(
      padding: const EdgeInsets.fromLTRB(16, 12, 16, 20),
      itemCount: hits.isEmpty ? 2 : hits.length + 1,
      separatorBuilder: (_, __) => const SizedBox(height: 8),
      itemBuilder: (ctx, i) {
        if (i == 0) return _PeopleResults(query: query, onSelect: (NxUser user) {
          // Keep the search route mounted beneath the sheet. Its item context
          // has a Navigator and MediaQuery, and Back from a chat returns to
          // the same people search instead of an unrelated page.
          showProfilePreview(ctx, user.id, name: user.username, avatar: user.avatar);
        });
        if (hits.isEmpty) {
          return TextButton.icon(
            onPressed: () {
              final String q = query;
              close(context, null);
              context.push('/oracle?q=${Uri.encodeComponent(q)}');
            },
            icon: Icon(Icons.blur_circular_rounded, color: visual.accent),
            label: Text(isFa ? 'از ORACLE بپرس: «$query»' : 'Ask ORACLE: "$query"'),
          );
        }
        final h = hits[i - 1];
        return Material(
          color: visual.surface,
          borderRadius: BorderRadius.circular(16),
          child: ListTile(
            shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(16),
                side: BorderSide(color: visual.border.withValues(alpha: 0.48))),
            leading: CircleAvatar(
                backgroundColor: h.color.withValues(alpha: 0.14),
                child: Icon(h.icon, color: h.color)),
            title: Text(h.title,
                style:
                    TextStyle(color: visual.text, fontWeight: FontWeight.w800)),
            subtitle: Text(h.subtitle, style: TextStyle(color: visual.muted)),
            trailing: Icon(Icons.arrow_forward_ios_rounded,
                size: 15, color: visual.muted),
            onTap: () {
              close(context, null);
              context.push(h.route);
            },
          ),
        );
      },
    );
  }
}

/// User results share the existing global search, so following someone is
/// available from the Home Page and every world without another toolbar icon.
/// The debounce and stale-query check prevent a slow response from replacing
/// results for the latest query.
class _PeopleResults extends ConsumerStatefulWidget {
  const _PeopleResults({required this.query, required this.onSelect});
  final String query;
  final ValueChanged<NxUser> onSelect;

  @override
  ConsumerState<_PeopleResults> createState() => _PeopleResultsState();
}

class _PeopleResultsState extends ConsumerState<_PeopleResults> {
  Timer? _debounce;
  List<NxUser> _results = const <NxUser>[];
  bool _loading = false;
  int _request = 0;

  @override
  void initState() { super.initState(); _schedule(); }

  @override
  void didUpdateWidget(_PeopleResults oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.query != widget.query) _schedule();
  }

  void _schedule() {
    _debounce?.cancel();
    final int request = ++_request;
    if (widget.query.trim().length < 2) {
      _results = const <NxUser>[];
      _loading = false;
      return;
    }
    _loading = true;
    _debounce = Timer(const Duration(milliseconds: 280), () async {
      final c = ref.read(onlineProvider).client;
      if (c?.auth.currentSession == null) {
        if (mounted && request == _request) setState(() => _loading = false);
        return;
      }
      try {
        final List<NxUser> users = await SocialApi(c!).search(widget.query);
        if (mounted && request == _request) setState(() { _results = users; _loading = false; });
      } catch (_) {
        if (mounted && request == _request) setState(() => _loading = false);
      }
    });
  }

  @override
  void dispose() { _request++; _debounce?.cancel(); super.dispose(); }

  @override
  Widget build(BuildContext context) {
    if (widget.query.trim().length < 2) return const SizedBox.shrink();
    if (_loading) return const LinearProgressIndicator(minHeight: 2);
    if (_results.isEmpty) return const SizedBox.shrink();
    return Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: <Widget>[
      Padding(padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
        child: Text(context.tx('کاوشگران', 'People'),
          style: const TextStyle(fontWeight: FontWeight.w800))),
      for (final NxUser u in _results)
        ListTile(
          leading: NxUserAvatar(avatar: u.avatar, name: u.username),
          title: Text(u.username),
          subtitle: Text(context.tx('نمایش پروفایل · دنبال‌کردن · پیام',
              'View profile · follow · message')),
          onTap: () => widget.onSelect(u),
        ),
    ]);
  }
}

/// Standalone route used by deep links and older callers of the global search.
class NexoraSearchPage extends StatelessWidget {
  const NexoraSearchPage({super.key});

  @override
  Widget build(BuildContext context) => Scaffold(
        appBar: AppBar(title: const Text('Global search')),
        body: Center(
          child: FilledButton.icon(
            onPressed: () => showSearch<void>(
              context: context,
              delegate: HubSearchDelegate(
                isFa: Localizations.localeOf(context).languageCode == 'fa',
              ),
            ),
            icon: const Icon(Icons.search_rounded),
            label: const Text('Search'),
          ),
        ),
      );
}
