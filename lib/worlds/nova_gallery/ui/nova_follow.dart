import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/settings/app_settings.dart';
import '../../../core/online/online_service.dart';
import '../../../core/online/social_api.dart';
import '../../../core/widgets/profile_preview.dart';
import '../../../core/theme/nx_theme.dart';
import '../data/supabase_nova_repository.dart';
import '../state/nova_controller.dart';
import 'nova_visuals.dart';

/// Followers / following of a real account (shared `follows` table), with a follow toggle for other people.
class NovaFollowBar extends ConsumerStatefulWidget {
  const NovaFollowBar({super.key, required this.uid, this.self = false});
  final String uid;
  final bool self;

  @override
  ConsumerState<NovaFollowBar> createState() => _NovaFollowBarState();
}

class _NovaFollowBarState extends ConsumerState<NovaFollowBar> {
  NxSocialProfile? _profile;
  bool _busy = false;

  // This bar also appears in talk rooms; it must not depend on a NOVA tab
  // having been opened first. Use the same privacy-aware RPC as the preview.
  SocialApi? get _api {
    final c = ref.read(onlineProvider).client;
    return c?.auth.currentSession == null ? null : SocialApi(c!);
  }

  @override
  void initState() {
    super.initState();
    _load();
  }

  @override
  void didUpdateWidget(NovaFollowBar oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.uid != widget.uid) {
      _profile = null;
      _load();
    }
  }

  Future<void> _load() async {
    final SocialApi? api = _api;
    if (api == null || !isProfileUserId(widget.uid)) return;
    try {
      final NxSocialProfile p = await api.socialProfile(widget.uid);
      if (mounted && p.id == widget.uid) setState(() => _profile = p);
    } catch (_) {}
  }

  @override
  Widget build(BuildContext context) {
    final NxSocialProfile? p = _profile;
    Widget stat(String v, String l) => Expanded(
          child: Column(children: <Widget>[
            Text(v, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w900)),
            Text(l, style: const TextStyle(color: NxColors.textDim, fontSize: 12)),
          ]),
        );
    return Row(children: <Widget>[
      stat(p == null ? '–' : '${p.followers}', context.tx('دنبال‌کننده', 'Followers')),
      stat(p == null ? '–' : '${p.following}', context.tx('دنبال‌شونده', 'Following')),
      if (!widget.self && p != null && !p.isMe)
        Expanded(
          child: p.isFollowing || p.isPending
              ? OutlinedButton(
                  onPressed: _busy || p.isBlocked ? null : () => _toggle(false),
                  child: Text(p.isPending ? context.tx('درخواست فرستاده شد', 'Requested')
                      : context.tx('دنبال می‌کنی', 'Following')),
                )
              : FilledButton(
                  onPressed: _busy || p.isBlocked ? null : () => _toggle(true),
                  child: Text(context.tx('دنبال کن', 'Follow')),
                ),
        ),
    ]);
  }

  Future<void> _toggle(bool on) async {
    final SocialApi? api = _api;
    if (api == null) return;
    setState(() => _busy = true);
    try {
      final NxSocialProfile next = await api.setFollow(widget.uid, on);
      if (mounted) setState(() => _profile = next);
    } catch (_) {
      if (mounted) ScaffoldMessenger.of(context).showSnackBar(SnackBar(
          content: Text(context.tx('عملیات دنبال‌کردن ناموفق بود.', 'Follow action failed.'))));
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }
}

/// Looks up a real account by username and shows its follow bar (online), or null when not found.
class NovaUserLookup extends ConsumerWidget {
  const NovaUserLookup({super.key, required this.username});
  final String username;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final SupabaseNovaRepository? r = ref.read(novaControllerProvider).online;
    if (r == null) return const SizedBox.shrink();
    return FutureBuilder<List<dynamic>>(
      future: r.c.from('profiles').select('id, username, level, bio, avatar_url').ilike('username', username.replaceAll('%', '')).limit(1),
      builder: (BuildContext c, AsyncSnapshot<List<dynamic>> s) {
        if (!s.hasData) return const Padding(padding: EdgeInsets.all(12), child: Center(child: CircularProgressIndicator()));
        if (s.data!.isEmpty) {
          return Text(c.tx('کاوشگری با این نام پیدا نشد.', 'No explorer with this name.'),
              style: const TextStyle(color: NxColors.textDim));
        }
        final Map<dynamic, dynamic> p = s.data!.first as Map;
        final String uid = '${p['id']}';
        return Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: <Widget>[
          NovaAvatar(name: '${p['username'] ?? username}', avatarUrl: p['avatar_url'] as String?, size: 64, ring: true),
          const SizedBox(height: 8),
          Text('Lv ${p['level'] ?? 1}${(p['bio'] ?? '').toString().isEmpty ? '' : ' · ${p['bio']}'}',
              style: const TextStyle(color: NxColors.textDim)),
          const SizedBox(height: 10),
          NovaFollowBar(uid: uid, self: uid == r.me),
          if (uid != r.me) TextButton.icon(
            onPressed: () => showProfilePreview(context, uid, name: '${p['username'] ?? username}'),
            icon: const Icon(Icons.chat_bubble_outline_rounded),
            label: Text(context.tx('پروفایل و پیام', 'Profile & message')),
          ),
        ]);
      },
    );
  }
}
