import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../online/online_service.dart';
import '../online/social_api.dart';
import '../settings/app_settings.dart';
import '../ui2/anix_kit.dart';
import '../ui2/luxe.dart';
import '../../worlds/orbit_network/state/orbit_controller.dart';
import '../../worlds/orbit_network/ui/orbit_chat_page.dart';
import 'user_avatar.dart';

/// A local explorer id, room title or bot name is not an auth user. Never send
/// one to a UUID-typed social RPC (nor pretend a local follow succeeded).
bool isProfileUserId(String id) => RegExp(
        r'^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$',
      ).hasMatch(id);

/// One entry point from NOVA, Orbit, rooms, rankings, lobbies and matches.
/// The sheet stays on the current page; only a confirmed direct-chat id opens
/// Orbit's existing conversation screen. It does not fabricate offline users.
Future<void> showProfilePreview(BuildContext context, String userId,
    {String? name, String? avatar, Future<void> Function()? onReport}) async {
  final _ProfileSelection? result = await showAnixSheet<_ProfileSelection>(
    context: context,
    useSafeArea: true,
    isScrollControlled: true,
    constraints: BoxConstraints(maxHeight: MediaQuery.sizeOf(context).height * 0.85),
    builder: (_) => ProfilePreview(userId: userId, name: name, avatar: avatar,
        reportable: onReport != null),
  );
  if (!context.mounted) return;
  if (result?.report == true) {
    await onReport?.call();
  } else if (result?.chatId != null) {
    await Navigator.of(context).push<void>(MaterialPageRoute<void>(
      builder: (_) => OrbitChatPage(chatId: result!.chatId!),
    ));
  }
}

class _ProfileSelection {
  const _ProfileSelection.chat(this.chatId) : report = false;
  const _ProfileSelection.report() : chatId = null, report = true;
  final String? chatId;
  final bool report;
}

class ProfilePreview extends ConsumerStatefulWidget {
  const ProfilePreview({super.key, required this.userId, this.name, this.avatar,
    this.reportable = false});
  final String userId;
  final String? name, avatar;
  final bool reportable;

  @override
  ConsumerState<ProfilePreview> createState() => _ProfilePreviewState();
}

class _ProfilePreviewState extends ConsumerState<ProfilePreview> {
  NxSocialProfile? _profile;
  bool _loading = false, _busy = false;
  String? _error;

  SocialApi? get _api {
    final c = ref.read(onlineProvider).client;
    return c?.auth.currentSession == null ? null : SocialApi(c!);
  }

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final SocialApi? api = _api;
    if (api == null || !isProfileUserId(widget.userId)) return;
    setState(() { _loading = true; _error = null; });
    try {
      final NxSocialProfile profile = await api.socialProfile(widget.userId);
      if (mounted) setState(() => _profile = profile);
    } catch (_) {
      if (mounted) setState(() => _error = context.tx('پروفایل در دسترس نیست؛ دوباره تلاش کن.',
          'Profile unavailable. Try again.'));
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  Future<void> _follow() async {
    final SocialApi? api = _api;
    final NxSocialProfile? p = _profile;
    if (api == null || p == null || _busy || p.isBlocked || p.isMe) return;
    setState(() => _busy = true);
    try {
      // A pending request is cancelled by the same RPC used for unfollowing.
      final NxSocialProfile next = await api.setFollow(p.id, !p.isFollowing && !p.isPending);
      if (mounted) setState(() => _profile = next);
    } catch (_) {
      _notice('دنبال‌کردن ممکن نشد.', 'Could not update follow.');
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _message() async {
    if (_busy || _profile?.isBlocked == true) return;
    setState(() => _busy = true);
    try {
      final String? id = await ref.read(orbitControllerProvider).openDirect(widget.userId);
      if (!mounted) return;
      if (id == null) {
        _notice('گفت‌وگوی آنلاین در دسترس نیست.', 'Online chat is unavailable.');
      } else {
        Navigator.of(context).pop(_ProfileSelection.chat(id));
      }
    } catch (_) {
      _notice('باز کردن گفت‌وگو ممکن نشد.', 'Could not open the chat.');
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  void _notice(String fa, String en) {
    if (mounted) ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(context.tx(fa, en))));
  }

  @override
  Widget build(BuildContext context) {
    final NxSocialProfile? p = _profile;
    final bool online = _api != null && isProfileUserId(widget.userId);
    final bool self = p?.isMe == true || widget.userId == ref.read(onlineProvider).uid;
    final String name = p?.username.isNotEmpty == true ? p!.username : (widget.name ?? 'Pilot');
    return SafeArea(
      top: false,
      child: SingleChildScrollView(
        child: Padding(
        padding: const EdgeInsets.fromLTRB(14, 6, 14, 18),
        child: LuxeSurface(
          radius: 22,
          padding: const EdgeInsets.all(18),
          child: Column(mainAxisSize: MainAxisSize.min, crossAxisAlignment: CrossAxisAlignment.stretch,
            children: <Widget>[
              Row(children: <Widget>[
                NxUserAvatar(avatar: p?.avatar ?? widget.avatar, name: name, size: 60, ring: Luxe.gold),
                const SizedBox(width: 14),
                Expanded(child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: <Widget>[
                  Text(name, maxLines: 1, overflow: TextOverflow.ellipsis,
                      style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w800, color: Luxe.ink)),
                  if (self) Text(context.tx('پروفایل تو', 'Your profile'), style: const TextStyle(color: Luxe.mute))
                  else if (p?.isBlocked == true)
                    Text(context.tx('نمایش پروفایل محدود است', 'Profile unavailable'), style: const TextStyle(color: Luxe.mute))
                  else if (p?.isPrivate == true)
                    Text(context.tx('حساب خصوصی', 'Private profile'), style: const TextStyle(color: Luxe.mute)),
                ])),
              ]),
              if (p != null && !p.isBlocked) ...<Widget>[
                if (p.bio.isNotEmpty) ...<Widget>[
                  const SizedBox(height: 10),
                  Text(p.bio, maxLines: 3, overflow: TextOverflow.ellipsis,
                      style: const TextStyle(color: Luxe.mute)),
                ],
                const SizedBox(height: 14),
                Wrap(spacing: 14, runSpacing: 8, children: <Widget>[
                  Text('${p.followers} ${context.tx('دنبال‌کننده', 'followers')}',
                      style: const TextStyle(color: Luxe.ink)),
                  Text('${p.following} ${context.tx('دنبال‌شونده', 'following')}',
                      style: const TextStyle(color: Luxe.mute)),
                  Text('${p.posts} ${context.tx('پست', 'posts')}',
                      style: const TextStyle(color: Luxe.mute)),
                ]),
                if (p.isPrivate && !p.canView) ...<Widget>[
                  const SizedBox(height: 10),
                  Text(context.tx('پست‌ها فقط برای دنبال‌کنندگان تأییدشده نمایش داده می‌شوند.',
                      'Posts are visible to approved followers only.'),
                      style: const TextStyle(color: Luxe.mute)),
                ],
              ],
              if (_loading) const Padding(padding: EdgeInsets.only(top: 12),
                  child: LinearProgressIndicator()),
              if (!online || _error != null) ...<Widget>[
                const SizedBox(height: 12),
                Text(_error ?? context.tx('برای دنبال‌کردن یا پیام، آنلاین شو.',
                    'Go online to follow or message.'), style: const TextStyle(color: Luxe.mute)),
                if (_error != null) TextButton.icon(onPressed: _loading ? null : _load,
                    icon: const Icon(Icons.refresh_rounded), label: Text(context.tx('تلاش دوباره', 'Retry'))),
              ],
              if (!self && online) ...<Widget>[
                const SizedBox(height: 16),
                Wrap(spacing: 8, runSpacing: 8, children: <Widget>[
                  FilledButton.icon(
                    key: const ValueKey<String>('profile-follow'),
                    onPressed: p == null || _busy || p.isBlocked ? null : _follow,
                    icon: Icon(p?.isPending == true ? Icons.hourglass_top_rounded
                        : p?.isFollowing == true ? Icons.check_rounded : Icons.person_add_alt_1_rounded, size: 18),
                    label: Text(p?.isPending == true ? context.tx('درخواست فرستاده شد', 'Requested')
                        : p?.isFollowing == true ? context.tx('دنبال می‌کنی', 'Following')
                        : context.tx('دنبال کن', 'Follow')),
                  ),
                  OutlinedButton.icon(
                    key: const ValueKey<String>('profile-message'),
                    onPressed: p == null || _busy || p.isBlocked ? null : _message,
                    icon: const Icon(Icons.chat_bubble_outline_rounded, size: 18),
                    label: Text(context.tx('پیام', 'Message')),
                  ),
                  if (widget.reportable) TextButton.icon(
                    onPressed: () => Navigator.of(context).pop(const _ProfileSelection.report()),
                    icon: const Icon(Icons.flag_outlined, size: 18),
                    label: Text(context.tx('گزارش', 'Report')),
                  ),
                ]),
              ],
            ],
          ),
        ),
      ),
      ),
    );
  }
}
