import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:nexora/core/online/social_api.dart';
import 'package:nexora/core/widgets/profile_preview.dart';

void main() {
  test('private follow request is not mistaken for a completed follow', () {
    final NxSocialProfile p = NxSocialProfile.fromRow(<String, dynamic>{
      'id': '8d6c7071-086c-471e-9478-8549570094bf',
      'username': 'Friend', 'followers': 3, 'following': 5,
      'is_private': true, 'is_pending': true, 'is_following': false,
      'can_view': false, 'posts': 7,
    });
    expect(p.isPrivate, isTrue);
    expect(p.isPending, isTrue);
    expect(p.isFollowing, isFalse);
    expect(p.followers, 3);
    expect(p.posts, 7);
    expect(p.canView, isFalse, reason: 'a pending request does not grant private-post access');
    expect(isProfileUserId(p.id), isTrue);
    expect(isProfileUserId('local-explorer'), isFalse);
  });

  testWidgets('local/offline profile does not fabricate follow or messaging', (WidgetTester tester) async {
    await tester.pumpWidget(const ProviderScope(child: MaterialApp(
      home: Scaffold(body: ProfilePreview(userId: 'local-explorer', name: 'Local player')),
    )));
    expect(find.text('Local player'), findsOneWidget);
    expect(find.byKey(const ValueKey<String>('profile-follow')), findsNothing);
    expect(find.byKey(const ValueKey<String>('profile-message')), findsNothing);
    expect(find.text('Go online to follow or message.'), findsOneWidget);
    await tester.pumpWidget(const SizedBox.shrink());
  });
}
