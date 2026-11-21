import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:nexora/core/economy/gem_shop.dart';
import 'package:nexora/core/storage/local_store.dart';
import 'package:nexora/games/finger_soccer/core/bm_engine.dart';
import 'package:nexora/games/finger_soccer/core/fs_custom_team.dart';
import 'package:nexora/games/finger_soccer/intro/fs_custom_photo.dart';
import 'package:nexora/games/finger_soccer/intro/fs_intro_ui.dart';
import 'package:nexora/games/finger_soccer/intro/fs_shop.dart';
import 'package:shared_preferences/shared_preferences.dart';

const String _uid = '12345678-1234-1234-1234-123456789abc';

void main() {
  test('custom item is one server-priced entitlement, not a locally unlocked team', () {
    final String id = fsCustomTeamId(_uid, 1795276800123);
    expect(fsCustomOwner(id), _uid);
    expect(teamItemId(id), kFsCustomItem);
    expect(teamItemId('custom'), kFsCustomItem);
    expect(defaultTeamPrice(id), 1000);
    expect(GemShop.instance.teamPrice(id), kFsCustomPrice);
    expect(GemShop.instance.customTeamId, isNull);
    expect(GemShop.instance.ownsTeam(id), isFalse);
    expect(GemShop.instance.usableTeam(id), kDefaultTeam);
    expect(fsCustomOwner('custom:../../private:1795276800123'), isNull);
    expect(fsCustomOwner('custom:$_uid:https://evil.example'), isNull);
    expect(fsCustomOwner('custom:$_uid:0'), isNull);
    expect(bmTeam(id).power, bmTeam(kDefaultTeam).power);
    expect(bmTeam(id).accuracy, bmTeam(kDefaultTeam).accuracy);
  });

  test('a gallery photo becomes a bounded, centred, real PNG', () async {
    final ui.PictureRecorder recorder = ui.PictureRecorder();
    final ui.Canvas canvas = ui.Canvas(recorder);
    canvas.drawRect(const ui.Rect.fromLTWH(0, 0, 400, 200), ui.Paint()..color = const ui.Color(0xFF117766));
    final ui.Picture picture = recorder.endRecording();
    final ui.Image source = await picture.toImage(400, 200);
    final ByteData? data = await source.toByteData(format: ui.ImageByteFormat.png);
    expect(data, isNotNull);
    final Uint8List png = await fsPrepareCustomPhoto(data!.buffer.asUint8List());
    expect(fsIsPng(png), isTrue);
    expect(png.length, lessThanOrEqualTo(kFsCustomMaxPngBytes));
    final ui.Codec codec = await ui.instantiateImageCodec(png);
    final ui.Image result = (await codec.getNextFrame()).image;
    expect(result.width, 256);
    expect(result.height, 256);
    result.dispose();
    codec.dispose();
    source.dispose();
    picture.dispose();
    await expectLater(fsPrepareCustomPhoto(Uint8List(8 * 1024 * 1024 + 1)), throwsFormatException);
  });

  setUp(() async {
    // ignore: invalid_use_of_visible_for_testing_member
    SharedPreferences.setMockInitialValues(<String, Object>{'settings.lang': 'fa'});
    await LocalStore.init();
  });

  void phone(WidgetTester tester) {
    tester.view.physicalSize = const Size(360, 800);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
  }

  testWidgets('the local shop displays the exact price and requires sign-in', (WidgetTester tester) async {
    phone(tester);
    await tester.pumpWidget(MaterialApp(home: Scaffold(body: FsShop(
      fa: true, team: kDefaultTeam, tab: 3, onTeam: (_) {}, onBack: () {}))));
    await tester.pump(const Duration(milliseconds: 300));
    expect(find.text('خرید ۱۰۰۰'), findsOneWidget);
    await tester.tap(find.text('خرید ۱۰۰۰'));
    await tester.pump();
    expect(find.text('برای خرید تیم اختصاصی وارد حساب کاربری شو.'), findsOneWidget);
  });

  testWidgets('the online shop cannot field or buy a custom photo', (WidgetTester tester) async {
    phone(tester);
    await tester.pumpWidget(MaterialApp(home: Scaffold(body: FsShop(
      fa: true, team: kDefaultTeam, tab: 3, allowCustom: false,
      onTeam: (_) => fail('online custom selection must be disabled'), onBack: () {}))));
    await tester.pump(const Duration(milliseconds: 300));
    expect(find.text('فقط بازی محلی'), findsOneWidget);
    final Finder press = find.ancestor(of: find.text('فقط بازی محلی'), matching: find.byType(FsPress));
    expect(tester.widget<FsPress>(press).enabled, isFalse);
  });
}
